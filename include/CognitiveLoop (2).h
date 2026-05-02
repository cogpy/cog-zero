/*
 * standalone/include/cog0/CognitiveLoop.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Perception → Attention → Reasoning → Planning → Action → Reflection cycle
 * for the standalone cog0 agent.
 */

#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

#include "AtomStore.h"
#include "TaskManager.h"
#include "ReasoningEngine.h"

namespace cog0 {

// -----------------------------------------------------------------------
// PerceptInput — a single percept delivered to the cognitive loop

struct PerceptInput {
    std::string source;     // e.g. "stdin", "sensor-1", "api"
    std::string modality;   // e.g. "text", "numeric", "event"
    std::string content;    // raw content string
    double      salience = 0.5; // attention priority [0,1]
};

// -----------------------------------------------------------------------
// CycleStats — metrics for a single cognitive cycle

struct CycleStats {
    size_t cycleNumber    = 0;
    size_t perceptsAdded  = 0;
    size_t rulesFired     = 0;
    size_t tasksExecuted  = 0;
    std::chrono::milliseconds duration{0};
};

// -----------------------------------------------------------------------
// CognitiveLoop

class CognitiveLoop {
public:
    // Callback types for cycle hooks
    using PerceptionHook   = std::function<void(const PerceptInput&, AtomStore&)>;
    using ReflectionHook   = std::function<void(const CycleStats&)>;

    CognitiveLoop(std::shared_ptr<AtomStore>     store,
                  std::shared_ptr<TaskManager>   taskMgr,
                  std::shared_ptr<ReasoningEngine> reasoning);

    ~CognitiveLoop();

    // --- Configuration ---
    void setCycleInterval(std::chrono::milliseconds ms) { _cycleInterval = ms; }
    void setMaxCycles(size_t n) { _maxCycles = n; } // 0 = unlimited
    void setMaxTasksPerCycle(size_t n) { _maxTasksPerCycle = n; } // tasks executed per cycle
    void enablePhase(bool perception, bool reasoning, bool planning,
                     bool action, bool reflection);

    // --- Percept injection (thread-safe) ---
    void addPercept(PerceptInput p);

    // --- Hooks ---
    void setPerceptionHook(PerceptionHook h) { _perceptionHook = std::move(h); }
    void setReflectionHook(ReflectionHook h) { _reflectionHook = std::move(h); }

    // --- Lifecycle ---
    void start();              // Start background loop thread
    void stop();               // Stop loop (waits for thread)
    void runSingleCycle();     // Run one synchronous cycle (useful for testing)
    [[nodiscard]] bool isRunning() const { return _running.load(); }

    // --- Stats ---
    [[nodiscard]] size_t cycleCount() const { return _cycleCount.load(); }
    [[nodiscard]] CycleStats lastStats() const { return _lastStats; }

private:
    void loopBody();

    // Cognitive phases
    size_t  phasePerception();          // add percepts to AtomStore
    size_t  phaseReasoning();           // run inference rules
    size_t  phasePlanning();            // update task priorities
    size_t  phaseAction();              // execute pending tasks
    void    phaseReflection(const CycleStats& stats); // meta-analysis

    std::shared_ptr<AtomStore>      _store;
    std::shared_ptr<TaskManager>    _taskMgr;
    std::shared_ptr<ReasoningEngine> _reasoning;

    // Percept queue (protected by mutex)
    std::mutex              _perceptMutex;
    std::vector<PerceptInput> _perceptQueue;

    // Control
    std::atomic<bool>   _running{false};
    std::atomic<size_t> _cycleCount{0};
    size_t              _maxCycles      = 0;
    std::chrono::milliseconds _cycleInterval{1000};
    std::thread         _loopThread;

    // Reflection bookkeeping — track previous cycle atoms so they can be
    // removed before recording the next cycle, preventing unbounded growth.
    Handle _prevCycleNode;
    Handle _prevDurationNode;
    Handle _prevStateLink;

    // Planning bookkeeping — track which tasks have already been promoted
    // to prevent per-cycle escalation (each task gets at most one boost).
    std::unordered_set<size_t> _promotedTaskIds;

    // Phase flags
    bool _enablePerception  = true;
    bool _enableReasoning   = true;
    bool _enablePlanning    = true;
    bool _enableAction      = true;
    bool _enableReflection  = true;

    size_t _maxTasksPerCycle = 3; // configurable via setMaxTasksPerCycle()

    // Hooks
    PerceptionHook  _perceptionHook;
    ReflectionHook  _reflectionHook;

    CycleStats _lastStats;
};

} // namespace cog0
