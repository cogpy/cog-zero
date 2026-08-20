/*
 * standalone/main.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * cog0 — Standalone C++ Agent-Zero application.
 *
 * Usage:
 *   cog0 [--verbose] [--demo] [--cycles N]
 *   cog0 --script <file> [--batch]
 *   cog0 --eval  "<command>" [--batch]
 *
 * Interactive mode (default):
 *   Commands: goal, task, percept, run, status, atoms, save, load, rule, help, quit
 */

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

// Portable TTY detection
#ifdef _WIN32
#  include <io.h>
static inline bool _cog0_stdout_is_tty() { return _isatty(_fileno(stdout)) != 0; }
#else
#  include <unistd.h>
static inline bool _cog0_stdout_is_tty() { return isatty(fileno(stdout)) != 0; }
#endif

// Optional readline for tab-completion — enabled at build time when
// libreadline-dev is present and USE_READLINE=ON (the cmake default).
#ifdef COG0_HAVE_READLINE
#  include <cstring>
#  include <readline/readline.h>
#  include <readline/history.h>
#endif

#include "cog0/Agent.h"
#include "cog0/Logger.h"

using namespace cog0;

// -----------------------------------------------------------------------
// Argument parsing helpers

struct CliArgs {
    bool   verbose  = false;
    bool   demo     = false;
    bool   batch    = false;   // emit JSON status after script/eval; no interactive output
    bool   noColor  = false;   // disable ANSI colour output
    size_t cycles   = 0;       // 0 = interactive
    std::string name   = "cog0";
    std::string script;         // path to an rc-style script file
    std::string eval;           // single command string to execute
};

CliArgs parseArgs(int argc, char** argv) {
    CliArgs a;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--verbose" || arg == "-v") { a.verbose = true; }
        else if (arg == "--demo" || arg == "-d") { a.demo = true; }
        else if (arg == "--batch" || arg == "-b") { a.batch = true; }
        else if (arg == "--no-color") { a.noColor = true; }
        else if ((arg == "--cycles" || arg == "-c") && i + 1 < argc) {
            a.cycles = static_cast<size_t>(std::stoul(argv[++i]));
        } else if ((arg == "--name" || arg == "-n") && i + 1 < argc) {
            a.name = argv[++i];
        } else if ((arg == "--script" || arg == "-s") && i + 1 < argc) {
            a.script = argv[++i];
        } else if ((arg == "--eval" || arg == "-e") && i + 1 < argc) {
            a.eval = argv[++i];
        } else if (arg == "--version" || arg == "-V") {
            // COG0_VERSION is injected by CMake from the top-level VERSION file.
#ifndef COG0_VERSION
#define COG0_VERSION "0.3.0"
#endif
            std::cout << "cog0 " << COG0_VERSION << "\n";
            std::exit(0);
        } else if (arg == "--help" || arg == "-h") {
            std::cout <<
                "cog0 — Standalone Agent-Zero C++ application\n"
                "\n"
                "Usage: cog0 [options]\n"
                "\n"
                "Options:\n"
                "  -v, --verbose          Enable debug logging\n"
                "  -d, --demo             Run built-in demo and exit\n"
                "  -c, --cycles <N>       Run N cognitive cycles then exit\n"
                "  -n, --name <name>      Set agent name (default: cog0)\n"
                "  -s, --script <file>    Execute commands from a script file\n"
                "  -e, --eval <cmd>       Execute command(s) and exit ('; separates commands)\n"
                "  -b, --batch            Emit JSON status after --script/--eval (no prompts)\n"
                "      --no-color         Disable ANSI colour output\n"
                "  -V, --version          Print version and exit\n"
                "  -h, --help             Show this help\n"
                "\n"
                "Interactive commands:\n"
                "  goal <name> [desc]     Set a new goal\n"
                "  goals                  List all goals with priority and status\n"
                "  task <name> [desc]     Schedule a task (always succeeds)\n"
                "  percept <text>         Inject a text percept\n"
                "  run [N]               Run N cognitive cycles (default: 1)\n"
                "  infer                  Run one forward-chaining inference pass\n"
                "  status                 Print agent status\n"
                "  atoms                  List all atoms in the store\n"
                "  save <file>            Save AtomStore snapshot to file\n"
                "  load <file>            Load AtomStore snapshot from file\n"
                "  rule <name> if-exists <concept> then-add <concept>\n"
                "                         Add an inference rule\n"
                "  help                   Show this help\n"
                "  quit / exit            Exit the application\n"
                "\n"
                "Script file format (rc-style):\n"
                "  One command per line; lines starting with '#' are comments.\n"
                "  Example:\n"
                "    # My cog0 script\n"
                "    goal explore-world Explore the environment\n"
                "    percept sky is blue\n"
                "    task analyse-sky Analyse sky colour\n"
                "    run 5\n"
                "    status\n";
            std::exit(0);
        }
    }
    return a;
}

// -----------------------------------------------------------------------
// ANSI colour helpers — disabled when stdout is not a TTY or --no-color is set.
// All functions return plain strings when colour is off.

namespace col {
    static bool on = false;

    // Call once after argument parsing; forceOff=true from --no-color or --batch.
    inline void init(bool forceOff) {
        on = !forceOff && _cog0_stdout_is_tty();
    }

    inline std::string c(const char* code, const std::string& s) {
        if (!on) return s;
        return std::string(code) + s + "\033[0m";
    }

    inline std::string green     (const std::string& s) { return c("\033[0;32m", s); }
    inline std::string yellow    (const std::string& s) { return c("\033[0;33m", s); }
    inline std::string boldYellow(const std::string& s) { return c("\033[1;33m", s); }
    inline std::string boldCyan  (const std::string& s) { return c("\033[1;36m", s); }
    inline std::string red       (const std::string& s) { return c("\033[0;31m", s); }

    // The interactive REPL prompt string.
    inline std::string prompt() { return boldCyan("cog0>") + " "; }
} // namespace col

// -----------------------------------------------------------------------
// Optional readline tab-completion (compiled in when COG0_HAVE_READLINE=1).

#ifdef COG0_HAVE_READLINE
static const char* const s_rlWords[] = {
    "atoms", "exit", "goal", "goals", "help", "infer", "load", "percept", "quit",
    "rule", "run", "save", "status", "task", nullptr
};

static char* rl_generator(const char* text, int state) {
    static int    idx = 0;
    static size_t len = 0;
    if (state == 0) { idx = 0; len = std::strlen(text); }
    for (; s_rlWords[idx]; ) {
        const char* w = s_rlWords[idx++];
        if (std::strncmp(w, text, len) == 0) return ::strdup(w);
    }
    return nullptr;
}

static char** rl_completer(const char* text, int start, int /*end*/) {
    rl_attempted_completion_over = 1; // suppress default filename completion
    return start == 0 ? rl_completion_matches(text, rl_generator) : nullptr;
}
#endif // COG0_HAVE_READLINE

// -----------------------------------------------------------------------
// Demo scenario

void runDemo(Agent& agent) {
    logger().info("[Demo] Starting built-in demo scenario");
    std::cout << "\n=== cog0 Agent-Zero Demo ===\n\n";

    // 1. Set a goal
    std::cout << ">> Setting goal: 'learn-about-opencog'\n";
    agent.setGoal("learn-about-opencog",
                  "Understand the OpenCog cognitive architecture", 0.9);

    // 2. Build a small knowledge graph
    auto& store = agent.atomStore();
    auto opencog = store.addNode(AtomType::CONCEPT, "OpenCog");
    auto cogutil = store.addNode(AtomType::CONCEPT, "cogutil");
    auto atomspace = store.addNode(AtomType::CONCEPT, "AtomSpace");
    auto pln      = store.addNode(AtomType::CONCEPT, "PLN");
    auto cog0node = store.addNode(AtomType::CONCEPT, "cog0");

    // Inheritance hierarchy
    store.addLink(AtomType::INHERITANCE, {cogutil,  opencog})->setTV({0.9, 0.95});
    store.addLink(AtomType::INHERITANCE, {atomspace, opencog})->setTV({0.9, 0.95});
    store.addLink(AtomType::INHERITANCE, {pln,       opencog})->setTV({0.8, 0.9});
    store.addLink(AtomType::INHERITANCE, {cog0node,  opencog})->setTV({0.7, 0.85});

    // 3. Add percepts
    std::cout << ">> Injecting percepts…\n";
    agent.addPercept("environment", "OpenCog is a cognitive architecture", 0.9);
    agent.addPercept("environment", "AtomSpace is a hypergraph knowledge store", 0.85);
    agent.addPercept("user",        "Task: summarise key components",          0.95);

    // 4. Schedule tasks
    std::cout << ">> Scheduling tasks…\n";
    agent.scheduleTask("analyse-components",
                       "Analyse OpenCog components",
                       Priority::HIGH,
                       []() {
                           std::cout << "  [Task] Analysing OpenCog components… done\n";
                           return true;
                       });

    agent.scheduleTask("summarise-findings",
                       "Summarise key findings",
                       Priority::NORMAL,
                       []() {
                           std::cout << "  [Task] Summarising findings… done\n";
                           return true;
                       });

    agent.scheduleTask("update-knowledge",
                       "Update knowledge base with new facts",
                       Priority::LOW,
                       [&agent]() {
                           agent.atomStore().addNode(AtomType::CONCEPT,
                                                     "KnowledgeFact:cog0-is-standalone");
                           std::cout << "  [Task] Knowledge base updated… done\n";
                           return true;
                       });

    // 5. Run cognitive cycles
    std::cout << "\n>> Running 5 cognitive cycles…\n";
    agent.runCycles(5);

    // 6. Status
    std::cout << "\n" << agent.statusReport() << "\n";

    // 7. Reasoning query results
    auto& re = agent.reasoningEngine();
    std::cout << "=== Reasoning Queries ===\n";
    std::cout << "cogutil inherits OpenCog?  "
              << (re.queryInherits("cogutil", "OpenCog")  ? "yes" : "no") << "\n";
    std::cout << "pln inherits OpenCog?      "
              << (re.queryInherits("PLN", "OpenCog")       ? "yes" : "no") << "\n";
    std::cout << "AgentProperty:goal-driven present? "
              << (re.queryExists(AtomType::CONCEPT, "AgentProperty:goal-driven")
                      ? "yes" : "no") << "\n";

    std::cout << "\n=== Demo Complete ===\n";
}

// -----------------------------------------------------------------------
// Interactive CLI

void printHelp() {
    std::cout <<
        col::boldYellow("Commands:") << "\n"
        "  goal <name> [description]      Set a new goal\n"
        "  goals                          List all goals with priority and status\n"
        "  task <name> [description]      Schedule a task\n"
        "  percept <text>                 Inject a text percept\n"
        "  run [N]                        Run N cognitive cycles (default: 1)\n"
        "  infer                          Run one forward-chaining inference pass\n"
        "  status                         Print agent status\n"
        "  atoms                          List all atoms\n"
        "  save <file>                    Save AtomStore snapshot to file\n"
        "  load <file>                    Load AtomStore snapshot from file\n"
        "  rule <name> if-exists <concept> then-add <concept>\n"
        "                                 Add an inference rule\n"
        "  help                           Show this help\n"
        "  quit / exit                    Exit\n";
}

// -----------------------------------------------------------------------
// executeCommand — dispatch a single line of text to agent operations.
// Returns false if the line requests quit/exit, true otherwise.
// When quiet=true (batch mode) stdout output from commands is suppressed.

bool executeCommand(const std::string& line, Agent& agent, bool quiet = false) {
    std::istringstream iss(line);
    std::string cmd;
    iss >> cmd;
    if (cmd.empty() || cmd[0] == '#') return true; // blank line or comment

    // lower-case cmd
    std::transform(cmd.begin(), cmd.end(), cmd.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    if (cmd == "quit" || cmd == "exit") {
        return false;
    } else if (cmd == "help") {
        if (!quiet) printHelp();
    } else if (cmd == "status") {
        if (!quiet) std::cout << agent.statusReport();
    } else if (cmd == "atoms") {
        if (!quiet) {
            auto& store = agent.atomStore();
            std::cout << col::boldYellow("=== Atoms in store (size="
                      + std::to_string(store.size()) + ") ===") << "\n";
            for (auto t : {AtomType::CONCEPT, AtomType::PREDICATE, AtomType::INHERITANCE,
                            AtomType::EVALUATION, AtomType::STATE}) {
                auto atoms = store.getByType(t);
                for (const auto& a : atoms)
                    std::cout << "  " << a->toStr() << "\n";
            }
        }
    } else if (cmd == "goal") {
        std::string name, rest;
        iss >> name;
        std::getline(iss, rest);
        if (!rest.empty() && rest[0] == ' ') rest = rest.substr(1);
        if (name.empty()) { std::cerr << col::red("Usage: goal <name> [desc]") << "\n"; return true; }
        auto g = agent.setGoal(name, rest);
        if (!quiet) std::cout << col::green("Goal set: ") << g->toStr() << "\n";
    } else if (cmd == "goals") {
        if (!quiet) {
            const auto& goals = agent.taskManager().goals();
            if (goals.empty()) {
                std::cout << col::yellow("No goals set.") << "\n";
            } else {
                std::cout << col::boldYellow("=== Goals (" + std::to_string(goals.size()) + ") ===") << "\n";
                for (const auto& g : goals) {
                    std::string status = g->achieved ? col::green("[achieved]") : col::yellow("[active]");
                    std::cout << "  " << status << " [prio=" << g->priority << "] "
                              << g->name;
                    if (!g->description.empty())
                        std::cout << " — " << g->description;
                    std::cout << "\n";
                }
            }
        }
    } else if (cmd == "task") {
        std::string name, rest;
        iss >> name;
        std::getline(iss, rest);
        if (!rest.empty() && rest[0] == ' ') rest = rest.substr(1);
        if (name.empty()) { std::cerr << col::red("Usage: task <name> [desc]") << "\n"; return true; }
        auto t = agent.scheduleTask(name, rest, Priority::NORMAL,
                                     [name, quiet]() {
                                         if (!quiet)
                                             std::cout << "  [Task] " << name << " executed\n";
                                         return true;
                                     });
        if (!quiet) std::cout << col::green("Task scheduled: ") << t->toStr() << "\n";
    } else if (cmd == "percept") {
        std::string rest;
        std::getline(iss, rest);
        if (!rest.empty() && rest[0] == ' ') rest = rest.substr(1);
        if (rest.empty()) { std::cerr << col::red("Usage: percept <text>") << "\n"; return true; }
        agent.addPercept("cli", rest, 0.7);
        if (!quiet) std::cout << col::green("Percept injected.") << "\n";
    } else if (cmd == "run") {
        size_t n = 1;
        iss >> n;
        if (n == 0) n = 1;
        if (!quiet) std::cout << col::yellow("Running " + std::to_string(n) + " cycle(s)…") << "\n";
        agent.runCycles(n);
        if (!quiet)
            std::cout << col::green("Done.") << " Total cycles: "
                      << agent.cognitiveLoop().cycleCount() << "\n";
    } else if (cmd == "infer") {
        auto results = agent.reasoningEngine().runCycle();
        if (!quiet) {
            size_t fired = 0;
            for (const auto& r : results) if (r.fired) ++fired;
            std::cout << col::boldYellow("=== Inference pass (" + std::to_string(results.size())
                      + " rules, " + std::to_string(fired) + " fired) ===") << "\n";
            for (const auto& r : results) {
                if (r.fired)
                    std::cout << "  " << col::green("fired") << "  " << r.ruleName;
                else
                    std::cout << "  " << col::yellow("skip ") << "  " << r.ruleName;
                if (!r.notes.empty()) std::cout << "  (" << r.notes << ")";
                std::cout << "\n";
            }
            if (results.empty())
                std::cout << col::yellow("  No rules registered.") << "\n";
        }
    } else if (cmd == "save") {
        std::string path;
        iss >> path;
        if (path.empty()) { std::cerr << col::red("Usage: save <file>") << "\n"; return true; }
        if (agent.atomStore().saveToFile(path)) {
            if (!quiet) std::cout << col::green("Snapshot saved to: ") << path << "\n";
        } else {
            std::cerr << col::red("save: failed to write '" + path + "'") << "\n";
        }
    } else if (cmd == "load") {
        std::string path;
        iss >> path;
        if (path.empty()) { std::cerr << col::red("Usage: load <file>") << "\n"; return true; }
        if (agent.atomStore().loadFromFile(path)) {
            if (!quiet)
                std::cout << col::green("Snapshot loaded from: ") << path
                          << " (" << agent.atomStore().size() << " atoms)\n";
        } else {
            std::cerr << col::red("load: failed to read '" + path + "'") << "\n";
        }
    } else if (cmd == "rule") {
        // Syntax: rule <name> if-exists <concept> then-add <concept>
        std::string ruleName, ifKw, concept1, thenKw, concept2;
        iss >> ruleName >> ifKw >> concept1 >> thenKw >> concept2;
        if (ruleName.empty() || ifKw != "if-exists" || concept1.empty() ||
                thenKw != "then-add" || concept2.empty()) {
            std::cerr << col::red("Usage: rule <name> if-exists <concept> then-add <concept>") << "\n";
            return true;
        }
        agent.reasoningEngine().addRule(
            ruleName,
            [concept1](const AtomStore& store) {
                return store.getNode(AtomType::CONCEPT, concept1) != nullptr;
            },
            [concept2](AtomStore& store) {
                store.addNode(AtomType::CONCEPT, concept2);
            });
        if (!quiet) std::cout << col::green("Rule added: ") << ruleName << "\n";
    } else {
        std::cerr << col::red("Unknown command: ") << cmd << ". Type 'help'.\n";
    }
    return true;
}

// -----------------------------------------------------------------------
// jsonEscapeString — escape a string value for inclusion in a JSON literal.

static std::string jsonEscapeString(const std::string& s) {
    std::string r;
    r.reserve(s.size());
    for (char c : s) {
        if      (c == '"')  { r += "\\\""; }
        else if (c == '\\') { r += "\\\\"; }
        else if (c == '\n') { r += "\\n";  }
        else                 { r += c;      }
    }
    return r;
}

// -----------------------------------------------------------------------
// batchStatusJson — emit a JSON object with the current agent status.
// Used by --batch mode for programmatic consumption.

std::string batchStatusJson(Agent& agent) {
    std::ostringstream j;
    j << "{\n";
    j << "  \"status\": \"ok\",\n";
    j << "  \"agent\": \"" << jsonEscapeString(agent.name()) << "\",\n";
    j << "  \"cycles\": " << agent.cognitiveLoop().cycleCount() << ",\n";
    j << "  \"atoms\": " << agent.atomStore().size() << ",\n";

    // pending tasks count from taskManager
    j << "  \"tasks_pending\": " << agent.taskManager().pendingCount() << ",\n";

    // goals list
    j << "  \"goals\": [";
    const auto& goals = agent.taskManager().goals();
    for (size_t i = 0; i < goals.size(); ++i) {
        if (i) j << ", ";
        j << "\"" << jsonEscapeString(goals[i]->name) << "\"";
    }
    j << "],\n";

    // rules count
    j << "  \"rules\": " << agent.reasoningEngine().rules().size() << "\n";
    j << "}\n";
    return j.str();
}

// -----------------------------------------------------------------------
// runScript — execute commands from an rc-style script file.
// Lines beginning with '#' are comments and are ignored.
// Returns 0 on success, 1 if the file could not be opened.

int runScript(const std::string& path, Agent& agent, bool batch = false) {
    std::ifstream fs(path);
    if (!fs.is_open()) {
        std::cerr << "cog0: cannot open script: " << path << "\n";
        return 1;
    }
    if (!batch)
        std::cout << "cog0 executing script: " << path << "\n";
    std::string line;
    while (std::getline(fs, line)) {
        // Echo non-comment, non-blank lines so the user sees what ran
        if (!line.empty() && line[0] != '#') {
            if (!batch)
                std::cout << col::prompt() << line << "\n";
            if (!executeCommand(line, agent, batch)) break;
        }
    }
    return 0;
}

// -----------------------------------------------------------------------
// interactiveLoop

void interactiveLoop(Agent& agent) {
    std::cout << col::boldCyan("cog0") << " agent '" << agent.name()
              << "' ready. Type " << col::yellow("'help'") << " for commands.\n";

#ifdef COG0_HAVE_READLINE
    rl_attempted_completion_function = rl_completer;
    char* raw;
    while ((raw = ::readline(col::prompt().c_str())) != nullptr) {
        std::string line(raw);
        if (!line.empty()) ::add_history(raw);
        ::free(raw);
        if (!executeCommand(line, agent)) {
            std::cout << "Goodbye.\n";
            break;
        }
    }
    if (!raw) std::cout << "\n"; // newline after EOF (Ctrl-D)
#else
    std::string line;
    while (true) {
        std::cout << col::prompt() << std::flush;
        if (!std::getline(std::cin, line)) {
            std::cout << "\n";
            break;
        }
        if (!executeCommand(line, agent)) {
            std::cout << "Goodbye.\n";
            break;
        }
    }
#endif
}

// -----------------------------------------------------------------------
// main

int main(int argc, char** argv) {
    CliArgs args = parseArgs(argc, argv);

    AgentConfig cfg;
    cfg.name    = args.name;
    cfg.verbose = args.verbose;

    // In batch mode, suppress INFO/DEBUG log lines so only JSON goes to stdout
    if (args.batch) logger().setLevel(LogLevel::WARN);

    // Colour: disabled in batch mode, when stdout is not a TTY, or by --no-color.
    col::init(args.noColor || args.batch);

    Agent agent(cfg);

    if (args.demo) {
        runDemo(agent);
        return 0;
    }

    if (!args.script.empty()) {
        int rc = runScript(args.script, agent, args.batch);
        if (args.batch) std::cout << batchStatusJson(agent);
        return rc;
    }

    if (!args.eval.empty()) {
        // Support semicolon-separated commands: --eval "goal g; run 1; status"
        std::string eval = args.eval;
        size_t start = 0;
        while (start < eval.size()) {
            size_t sep = eval.find(';', start);
            std::string piece = (sep == std::string::npos)
                ? eval.substr(start)
                : eval.substr(start, sep - start);
            // Trim leading/trailing whitespace
            size_t b = piece.find_first_not_of(" \t\r\n");
            size_t e = piece.find_last_not_of(" \t\r\n");
            if (b != std::string::npos) {
                if (!executeCommand(piece.substr(b, e - b + 1), agent, args.batch))
                    break;
            }
            if (sep == std::string::npos) break;
            start = sep + 1;
        }
        if (args.batch) std::cout << batchStatusJson(agent);
        return 0;
    }

    if (args.cycles > 0) {
        logger().info("[main] Running " + std::to_string(args.cycles) + " cycles");
        agent.runCycles(args.cycles);
        std::cout << agent.statusReport();
        return 0;
    }

    // Interactive mode
    interactiveLoop(agent);
    return 0;
}
