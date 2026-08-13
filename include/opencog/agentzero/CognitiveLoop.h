/*
 * opencog/agentzero/CognitiveLoop.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Perception-action-reflection cognitive loop with AtomSpace handles.
 * Part of AGENT-ZERO-GENESIS Phase 1.
 */
#ifndef _OPENCOG_AGENTZERO_COGNITIVE_LOOP_H
#define _OPENCOG_AGENTZERO_COGNITIVE_LOOP_H

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Handle.h>

namespace opencog {
namespace agentzero {

class AgentZeroCore;

/**
 * CognitiveLoop — perception → attention → reasoning → planning →
 * action → reflection cycle backed by AtomSpace handles.
 */
class CognitiveLoop {
public:
    struct Percept {
        std::string source;
        std::string modality;
        std::string content;
        double salience = 0.5;
        Handle atom = Handle::UNDEFINED;
    };

    struct CycleStats {
        size_t cycle_number = 0;
        size_t percepts_processed = 0;
        size_t conclusions = 0;
        size_t tasks_processed = 0;
        std::chrono::milliseconds duration{0};
    };

    CognitiveLoop(AgentZeroCore* agent_core, AtomSpacePtr atomspace);
    ~CognitiveLoop();

    bool start();
    bool stop();
    bool isRunning() const { return _running.load(); }

    void runSingleCycle();
    size_t getCycleCount() const { return _cycle_count.load(); }
    CycleStats lastStats() const;

    void addPercept(const Percept& p);
    void addPercept(const std::string& source, const std::string& content,
                    const std::string& modality = "text", double salience = 0.5);

    void setCycleInterval(std::chrono::milliseconds ms) { _cycle_interval = ms; }
    void setMaxCycles(size_t n) { _max_cycles = n; }

    Handle getLastPerceptAtom() const { return _last_percept_atom; }
    Handle getLastConclusionAtom() const { return _last_conclusion_atom; }

private:
    void loopBody();
    size_t phasePerception();
    size_t phaseReasoning();
    size_t phasePlanning();
    size_t phaseAction();
    void phaseReflection(const CycleStats& stats);

    Handle encodePercept(const Percept& p);
    Handle encodeConclusion(const std::string& name, double strength, double confidence);

    AgentZeroCore* _agent_core;
    AtomSpacePtr _atomspace;

    std::atomic<bool> _running{false};
    std::atomic<size_t> _cycle_count{0};
    size_t _max_cycles = 0;
    std::chrono::milliseconds _cycle_interval{50};
    std::thread _thread;

    mutable std::mutex _percept_mu;
    std::vector<Percept> _pending_percepts;

    mutable std::mutex _stats_mu;
    CycleStats _last_stats;

    Handle _last_percept_atom = Handle::UNDEFINED;
    Handle _last_conclusion_atom = Handle::UNDEFINED;
    Handle _cycle_root = Handle::UNDEFINED;
};

} // namespace agentzero
} // namespace opencog

#endif // _OPENCOG_AGENTZERO_COGNITIVE_LOOP_H
