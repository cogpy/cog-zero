/*
 * standalone/src/Agent.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include <sstream>

#include "cog0/Agent.h"
#include "cog0/Logger.h"

namespace cog0 {

Agent::Agent(AgentConfig cfg)
    : _cfg(std::move(cfg))
    , _store(std::make_shared<AtomStore>())
    , _taskMgr(std::make_shared<TaskManager>(_store))
    , _reasoning(std::make_shared<ReasoningEngine>(_store))
    , _loop(std::make_shared<CognitiveLoop>(_store, _taskMgr, _reasoning))
{
    if (_cfg.verbose) logger().setLevel(LogLevel::DEBUG);

    _loop->setCycleInterval(_cfg.cycleInterval);
    _loop->setMaxCycles(_cfg.maxCycles);
    _loop->setMaxTasksPerCycle(_cfg.maxTasksPerCycle);
    _loop->enablePhase(_cfg.enablePercept,
                       _cfg.enableReasoning,
                       _cfg.enablePlanning,
                       _cfg.enableAction,
                       _cfg.enableReflection);

    buildDefaultRules();

    _actionExec = std::make_shared<ActionExecutor>(_store);
    _scheduler  = std::make_shared<ActionScheduler>(_store, _actionExec);
    _knowledge  = std::make_shared<KnowledgeIntegrator>(_store);
    _episodic   = std::make_shared<EpisodicMemory>(_store);

    logger().info("[Agent] Initialized: " + _cfg.name);
}

Agent::~Agent() { stop(); }

Goal::Ptr Agent::setGoal(const std::string& name,
                          const std::string& desc,
                          double priority) {
    return _taskMgr->setGoal(name, desc, priority);
}

Task::Ptr Agent::scheduleTask(const std::string& name,
                               const std::string& desc,
                               Priority prio,
                               std::function<bool()> action) {
    auto t = _taskMgr->createTask(name, desc, prio, std::move(action));
    _taskMgr->enqueue(t);
    return t;
}

void Agent::addPercept(const std::string& source,
                        const std::string& content,
                        double salience) {
    _loop->addPercept(PerceptInput{source, "text", content, salience});
}

void Agent::start() {
    logger().info("[Agent] Starting cognitive loop");
    _loop->start();
}

void Agent::stop() {
    if (_loop->isRunning())
        logger().info("[Agent] Stopping cognitive loop");
    _loop->stop(); // always join the thread, even if the loop stopped itself
}

void Agent::runCycles(size_t n) {
    for (size_t i = 0; i < n; ++i) _loop->runSingleCycle();
}

bool Agent::isRunning() const { return _loop->isRunning(); }

std::string Agent::statusReport() const {
    std::ostringstream oss;
    oss << "=== Agent Status: " << _cfg.name << " ===\n";
    oss << "Cycles run: " << _loop->cycleCount() << "\n";
    oss << "AtomStore size: " << _store->size() << "\n";
    oss << _taskMgr->statusReport();
    oss << _reasoning->statusReport();
    oss << _knowledge->statusReport();
    oss << _episodic->statusReport();
    oss << _actionExec->statusReport();
    return oss.str();
}

// -----------------------------------------------------------------------
// Default inference rules

void Agent::buildDefaultRules() {
    // Rule: if a goal atom exists, assert the agent is "goal-driven"
    _reasoning->addRule(
        "goal-driven-assertion",
        [](const AtomStore& store) {
            // Condition: at least one Goal:* concept node present
            auto goals = store.getByType(AtomType::CONCEPT);
            for (const auto& g : goals)
                if (g->name().rfind("Goal:", 0) == 0) return true;
            return false;
        },
        [](AtomStore& store) {
            store.addNode(AtomType::CONCEPT, "AgentProperty:goal-driven");
        },
        2.0);

    // Rule: if a percept with salience > 0.8 is received, mark as high-priority
    _reasoning->addRule(
        "high-salience-attention",
        [](const AtomStore& store) {
            auto percepts = store.getByType(AtomType::CONCEPT);
            for (const auto& p : percepts)
                if (p->name().rfind("Percept:", 0) == 0 && p->sti() > 0.8)
                    return true;
            return false;
        },
        [](AtomStore& store) {
            store.addNode(AtomType::CONCEPT, "AttentionFlag:high-salience");
        },
        1.5);

    // Rule: inheritance transitivity — if A->B and B->C, infer A->C
    _reasoning->addRule(
        "inheritance-transitivity",
        [](const AtomStore& store) {
            auto links = store.getByType(AtomType::INHERITANCE);
            for (const auto& ab : links) {
                if (ab->out().size() < 2) continue;
                const auto& B = ab->out()[1];
                // look for B->C
                for (const auto& bc : store.getByType(AtomType::INHERITANCE)) {
                    if (bc->out().size() < 2) continue;
                    if (bc->out()[0] == B) return true;
                }
            }
            return false;
        },
        [](AtomStore& store) {
            auto links = store.getByType(AtomType::INHERITANCE);
            for (const auto& ab : links) {
                if (ab->out().size() < 2) continue;
                const auto& A = ab->out()[0];
                const auto& B = ab->out()[1];
                for (const auto& bc : store.getByType(AtomType::INHERITANCE)) {
                    if (bc->out().size() < 2) continue;
                    if (bc->out()[0] != B) continue;
                    const auto& C = bc->out()[1];
                    // assert A->C if not already present
                    auto existing = store.getLink(AtomType::INHERITANCE, {A, C});
                    if (!existing) {
                        double s = ab->tv().strength * bc->tv().strength;
                        double c = ab->tv().confidence * bc->tv().confidence * 0.9;
                        auto link = store.addLink(AtomType::INHERITANCE, {A, C});
                        link->setTV(TruthValue{s, c});
                    }
                }
            }
        },
        1.0);
}

size_t Agent::addKnowledge(const std::string& content,
                            KnowledgeType type,
                            double strength) {
    return _knowledge->addKnowledge(content, type, strength);
}

size_t Agent::recordEpisode(const std::string& type,
                             const std::string& content,
                             double importance) {
    return _episodic->record(type, content, importance);
}

size_t Agent::scheduleAction(const std::string& actionType,
                              const std::map<std::string, std::string>& params,
                              std::chrono::milliseconds delay) {
    if (delay.count() == 0)
        return _scheduler->scheduleAt(actionType, params,
                                      std::chrono::steady_clock::now());
    return _scheduler->scheduleAfter(actionType, params, delay);
}

// -----------------------------------------------------------------------
// Serialization helpers for JSON-like string formatting (no external deps)

namespace {

std::string escapeJson(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 16);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;      break;
        }
    }
    return out;
}

std::string unescapeJson(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            char next = s[++i];
            switch (next) {
                case '"':  out += '"';  break;
                case '\\': out += '\\'; break;
                case 'n':  out += '\n'; break;
                case 'r':  out += '\r'; break;
                case 't':  out += '\t'; break;
                default:   out += next; break;
            }
        } else {
            out += s[i];
        }
    }
    return out;
}

// Simple JSON value extraction (for primitive values)
std::string extractValue(const std::string& json, const std::string& key) {
    std::string needle = "\"" + key + "\":";
    auto pos = json.find(needle);
    if (pos == std::string::npos) return "";
    
    pos += needle.size();
    // Skip whitespace
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) ++pos;
    
    if (pos >= json.size()) return "";
    
    if (json[pos] == '"') {
        // String value
        ++pos;
        size_t end = pos;
        while (end < json.size()) {
            if (json[end] == '"' && (end == pos || json[end-1] != '\\')) break;
            ++end;
        }
        return unescapeJson(json.substr(pos, end - pos));
    } else if (json[pos] == '[') {
        // Array value - find matching ]
        int depth = 1;
        size_t start = pos;
        ++pos;
        while (pos < json.size() && depth > 0) {
            if (json[pos] == '[') ++depth;
            else if (json[pos] == ']') --depth;
            else if (json[pos] == '"') {
                // Skip string
                ++pos;
                while (pos < json.size() && json[pos] != '"') {
                    if (json[pos] == '\\' && pos + 1 < json.size()) ++pos;
                    ++pos;
                }
            }
            ++pos;
        }
        return json.substr(start, pos - start);
    } else {
        // Numeric or boolean
        size_t end = pos;
        while (end < json.size() && json[end] != ',' && json[end] != '}' 
               && json[end] != ']' && json[end] != '\n') ++end;
        std::string val = json.substr(pos, end - pos);
        // Trim trailing whitespace
        while (!val.empty() && (val.back() == ' ' || val.back() == '\t')) 
            val.pop_back();
        return val;
    }
}

// Parse a JSON array of objects
std::vector<std::string> parseArray(const std::string& arr) {
    std::vector<std::string> items;
    if (arr.empty() || arr[0] != '[') return items;
    
    size_t pos = 1;
    while (pos < arr.size()) {
        // Skip whitespace
        while (pos < arr.size() && (arr[pos] == ' ' || arr[pos] == '\t' 
               || arr[pos] == '\n' || arr[pos] == ',')) ++pos;
        
        if (pos >= arr.size() || arr[pos] == ']') break;
        
        if (arr[pos] == '{') {
            // Object - find matching }
            int depth = 1;
            size_t start = pos;
            ++pos;
            while (pos < arr.size() && depth > 0) {
                if (arr[pos] == '{') ++depth;
                else if (arr[pos] == '}') --depth;
                else if (arr[pos] == '"') {
                    ++pos;
                    while (pos < arr.size() && arr[pos] != '"') {
                        if (arr[pos] == '\\' && pos + 1 < arr.size()) ++pos;
                        ++pos;
                    }
                }
                ++pos;
            }
            items.push_back(arr.substr(start, pos - start));
        } else {
            ++pos;
        }
    }
    return items;
}

} // anonymous namespace

// -----------------------------------------------------------------------
// Agent::serialize

std::string Agent::serialize() const {
    std::ostringstream oss;
    oss << "{\n";
    
    // Agent name
    oss << "  \"name\": \"" << escapeJson(_cfg.name) << "\",\n";
    
    // Atoms from AtomStore
    oss << "  \"atoms\": [\n";
    {
        auto atoms = _store->getByType(AtomType::CONCEPT);
        // Gather all atom types
        for (auto t : {AtomType::PREDICATE, AtomType::VARIABLE, AtomType::LIST,
                       AtomType::EVALUATION, AtomType::INHERITANCE, 
                       AtomType::SIMILARITY, AtomType::IMPLICATION,
                       AtomType::EXECUTION, AtomType::STATE, AtomType::CUSTOM}) {
            auto more = _store->getByType(t);
            atoms.insert(atoms.end(), more.begin(), more.end());
        }
        bool first = true;
        for (const auto& atom : atoms) {
            if (!atom->isNode()) continue; // Only serialize nodes for simplicity
            if (!first) oss << ",\n";
            first = false;
            oss << "    {\"name\": \"" << escapeJson(atom->name()) 
                << "\", \"type\": \"" << atomTypeName(atom->type())
                << "\", \"strength\": " << atom->tv().strength
                << ", \"confidence\": " << atom->tv().confidence
                << ", \"sti\": " << atom->sti()
                << ", \"lti\": " << atom->lti() << "}";
        }
    }
    oss << "\n  ],\n";
    
    // Goals from TaskManager
    oss << "  \"goals\": [\n";
    {
        const auto& goals = _taskMgr->goals();
        bool first = true;
        for (const auto& g : goals) {
            if (!first) oss << ",\n";
            first = false;
            oss << "    {\"name\": \"" << escapeJson(g->name)
                << "\", \"description\": \"" << escapeJson(g->description)
                << "\", \"priority\": " << g->priority
                << ", \"achieved\": " << (g->achieved ? "true" : "false") << "}";
        }
    }
    oss << "\n  ],\n";
    
    // Tasks from TaskManager
    oss << "  \"tasks\": [\n";
    {
        auto tasks = _taskMgr->pendingTasks();
        bool first = true;
        for (const auto& t : tasks) {
            if (!first) oss << ",\n";
            first = false;
            std::string status = t->completed ? "completed" : 
                                 (t->failed ? "failed" : "pending");
            oss << "    {\"name\": \"" << escapeJson(t->name)
                << "\", \"description\": \"" << escapeJson(t->description)
                << "\", \"priority\": \"" << priorityName(t->priority)
                << "\", \"status\": \"" << status << "\"}";
        }
    }
    oss << "\n  ],\n";
    
    // Episodes from EpisodicMemory
    oss << "  \"episodes\": [\n";
    {
        auto episodes = _episodic->recentEpisodes(1000); // Get all recent
        bool first = true;
        for (const auto& ep : episodes) {
            if (!first) oss << ",\n";
            first = false;
            // Convert timestamp to Unix-like offset (ms since epoch of type)
            auto dur = ep->timestamp.time_since_epoch();
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(dur).count();
            oss << "    {\"type\": \"" << escapeJson(ep->type)
                << "\", \"content\": \"" << escapeJson(ep->content)
                << "\", \"importance\": " << ep->importance
                << ", \"timestamp\": " << ms << "}";
        }
    }
    oss << "\n  ]\n";
    
    oss << "}";
    return oss.str();
}

// -----------------------------------------------------------------------
// Agent::deserialize

std::unique_ptr<Agent> Agent::deserialize(const std::string& data, AgentConfig cfg) {
    // Extract agent name
    std::string name = extractValue(data, "name");
    if (!name.empty()) {
        cfg.name = name;
    }
    
    auto agent = std::make_unique<Agent>(cfg);
    
    // Restore atoms
    std::string atomsArr = extractValue(data, "atoms");
    for (const auto& atomJson : parseArray(atomsArr)) {
        std::string atomName = extractValue(atomJson, "name");
        std::string typeStr  = extractValue(atomJson, "type");
        std::string strengthStr = extractValue(atomJson, "strength");
        std::string confStr = extractValue(atomJson, "confidence");
        std::string stiStr  = extractValue(atomJson, "sti");
        std::string ltiStr  = extractValue(atomJson, "lti");
        
        // Map type string to AtomType (handle both cases for compatibility)
        AtomType type = AtomType::CONCEPT;
        if (typeStr == "PREDICATE" || typeStr == "Predicate") type = AtomType::PREDICATE;
        else if (typeStr == "VARIABLE" || typeStr == "Variable") type = AtomType::VARIABLE;
        else if (typeStr == "LIST" || typeStr == "List") type = AtomType::LIST;
        else if (typeStr == "EVALUATION" || typeStr == "Evaluation") type = AtomType::EVALUATION;
        else if (typeStr == "INHERITANCE" || typeStr == "Inheritance") type = AtomType::INHERITANCE;
        else if (typeStr == "SIMILARITY" || typeStr == "Similarity") type = AtomType::SIMILARITY;
        else if (typeStr == "IMPLICATION" || typeStr == "Implication") type = AtomType::IMPLICATION;
        else if (typeStr == "EXECUTION" || typeStr == "Execution") type = AtomType::EXECUTION;
        else if (typeStr == "STATE" || typeStr == "State") type = AtomType::STATE;
        else if (typeStr == "CUSTOM" || typeStr == "Custom") type = AtomType::CUSTOM;
        
        if (!atomName.empty()) {
            auto atom = agent->atomStore().addNode(type, atomName);
            if (!strengthStr.empty() && !confStr.empty()) {
                atom->setTV(TruthValue{std::stod(strengthStr), std::stod(confStr)});
            }
            if (!stiStr.empty()) atom->setSTI(std::stod(stiStr));
            if (!ltiStr.empty()) atom->setLTI(std::stod(ltiStr));
        }
    }
    
    // Restore goals
    std::string goalsArr = extractValue(data, "goals");
    for (const auto& goalJson : parseArray(goalsArr)) {
        std::string goalName = extractValue(goalJson, "name");
        std::string desc     = extractValue(goalJson, "description");
        std::string prioStr  = extractValue(goalJson, "priority");
        std::string achievedStr = extractValue(goalJson, "achieved");
        
        double priority = prioStr.empty() ? 1.0 : std::stod(prioStr);
        bool achieved = (achievedStr == "true");
        
        if (!goalName.empty()) {
            auto goal = agent->setGoal(goalName, desc, priority);
            if (achieved && goal) {
                goal->achieved = true;
            }
        }
    }
    
    // Restore tasks
    std::string tasksArr = extractValue(data, "tasks");
    for (const auto& taskJson : parseArray(tasksArr)) {
        std::string taskName = extractValue(taskJson, "name");
        std::string desc     = extractValue(taskJson, "description");
        std::string prioStr  = extractValue(taskJson, "priority");
        
        // Map priority string to Priority enum
        Priority prio = Priority::NORMAL;
        if (prioStr == "CRITICAL") prio = Priority::CRITICAL;
        else if (prioStr == "HIGH") prio = Priority::HIGH;
        else if (prioStr == "LOW") prio = Priority::LOW;
        else if (prioStr == "BACKGROUND") prio = Priority::BACKGROUND;
        
        if (!taskName.empty()) {
            agent->scheduleTask(taskName, desc, prio);
        }
    }
    
    // Restore episodes
    std::string episodesArr = extractValue(data, "episodes");
    for (const auto& epJson : parseArray(episodesArr)) {
        std::string epType    = extractValue(epJson, "type");
        std::string content   = extractValue(epJson, "content");
        std::string impStr    = extractValue(epJson, "importance");
        
        double importance = impStr.empty() ? 0.5 : std::stod(impStr);
        
        if (!epType.empty() && !content.empty()) {
            agent->recordEpisode(epType, content, importance);
        }
    }
    
    return agent;
}

} // namespace cog0
