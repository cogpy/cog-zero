/*
 * standalone/include/cog0/Agent.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Top-level Agent class — owns all subsystems and provides the public API
 * for the standalone cog0 application.
 */

#pragma once

#include <map>
#include <memory>
#include <string>

#include "AtomStore.h"
#include "CognitiveLoop.h"
#include "ReasoningEngine.h"
#include "TaskManager.h"
#include "ActionExecutor.h"
#include "ActionScheduler.h"
#include "EpisodicMemory.h"
#include "KnowledgeIntegrator.h"

namespace cog0 {

// -----------------------------------------------------------------------
// AgentConfig — tunable parameters

struct AgentConfig {
    std::string name          = "cog0-agent";
    std::chrono::milliseconds cycleInterval{500};
    size_t      maxCycles     = 0;        // 0 = unlimited
    bool        enablePercept = true;
    bool        enableReasoning = true;
    bool        enablePlanning  = true;
    bool        enableAction    = true;
    bool        enableReflection = true;
    size_t      maxTasksPerCycle = 3;  // max tasks executed per cognitive cycle
    bool        verbose       = false;
};

// -----------------------------------------------------------------------
// Agent — facade over all subsystems

class Agent {
public:
    explicit Agent(AgentConfig cfg = {});
    ~Agent();

    // Subsystem accessors
    AtomStore&      atomStore()      { return *_store; }
    TaskManager&    taskManager()    { return *_taskMgr; }
    ReasoningEngine& reasoningEngine() { return *_reasoning; }
    CognitiveLoop&  cognitiveLoop()  { return *_loop; }
    ActionExecutor&    actionExecutor()    { return *_actionExec; }
    ActionScheduler&   actionScheduler()   { return *_scheduler; }
    KnowledgeIntegrator& knowledgeIntegrator() { return *_knowledge; }
    EpisodicMemory&    episodicMemory()    { return *_episodic; }

    [[nodiscard]] const std::string& name() const { return _cfg.name; }

    // Convenience wrappers
    Goal::Ptr setGoal(const std::string& name, const std::string& desc = "",
                      double priority = 1.0);

    Task::Ptr scheduleTask(const std::string& name,
                           const std::string& desc = "",
                           Priority           prio = Priority::NORMAL,
                           std::function<bool()> action = nullptr);

    void addPercept(const std::string& source, const std::string& content,
                    double salience = 0.5);

    // Knowledge integration
    size_t addKnowledge(const std::string& content,
                        KnowledgeType type = KnowledgeType::FACTUAL,
                        double strength = 1.0);

    // Episodic memory
    size_t recordEpisode(const std::string& type, const std::string& content,
                         double importance = 0.5);

    // Action execution — returns the scheduled action ID, or 0 on failure.
    // Use the returned ID with actionScheduler() for cancellation or dependency
    // management.
    size_t scheduleAction(const std::string& actionType,
                          const std::map<std::string, std::string>& params = {},
                          std::chrono::milliseconds delay = std::chrono::milliseconds(0));

    // Lifecycle
    void start();              // Start background cognitive loop
    void stop();               // Stop loop
    void runCycles(size_t n);  // Run exactly n synchronous cycles

    [[nodiscard]] bool isRunning() const;

    // Status / diagnostics
    [[nodiscard]] std::string statusReport() const;

private:
    AgentConfig                      _cfg;
    std::shared_ptr<AtomStore>       _store;
    std::shared_ptr<TaskManager>     _taskMgr;
    std::shared_ptr<ReasoningEngine> _reasoning;
    std::shared_ptr<CognitiveLoop>   _loop;
    std::shared_ptr<ActionExecutor>    _actionExec;
    std::shared_ptr<ActionScheduler>   _scheduler;
    std::shared_ptr<KnowledgeIntegrator> _knowledge;
    std::shared_ptr<EpisodicMemory>    _episodic;

    void buildDefaultRules();
};

} // namespace cog0
