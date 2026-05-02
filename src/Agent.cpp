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

} // namespace cog0
