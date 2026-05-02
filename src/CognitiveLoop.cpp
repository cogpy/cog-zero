/*
 * standalone/src/CognitiveLoop.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include <chrono>
#include <sstream>
#include <stdexcept>

#include "cog0/CognitiveLoop.h"
#include "cog0/Logger.h"

namespace cog0 {

CognitiveLoop::CognitiveLoop(std::shared_ptr<AtomStore>      store,
                               std::shared_ptr<TaskManager>    taskMgr,
                               std::shared_ptr<ReasoningEngine> reasoning)
    : _store(std::move(store))
    , _taskMgr(std::move(taskMgr))
    , _reasoning(std::move(reasoning))
{}

CognitiveLoop::~CognitiveLoop() { stop(); }

void CognitiveLoop::enablePhase(bool perception, bool reasoning,
                                  bool planning, bool action, bool reflection) {
    _enablePerception  = perception;
    _enableReasoning   = reasoning;
    _enablePlanning    = planning;
    _enableAction      = action;
    _enableReflection  = reflection;
}

void CognitiveLoop::addPercept(PerceptInput p) {
    std::lock_guard<std::mutex> lk(_perceptMutex);
    _perceptQueue.push_back(std::move(p));
}

void CognitiveLoop::start() {
    if (_running.exchange(true)) return; // already running
    // Join any previous thread that finished naturally (e.g., via maxCycles)
    if (_loopThread.joinable()) _loopThread.join();
    _loopThread = std::thread([this] { loopBody(); });
}

void CognitiveLoop::stop() {
    _running.store(false);
    if (_loopThread.joinable()) _loopThread.join();
}

void CognitiveLoop::runSingleCycle() {
    CycleStats stats;
    stats.cycleNumber = _cycleCount.fetch_add(1) + 1;
    auto t0 = std::chrono::steady_clock::now();

    if (_enablePerception)  stats.perceptsAdded  = phasePerception();
    if (_enableReasoning)   stats.rulesFired     = phaseReasoning();
    if (_enablePlanning)    phasePlanning();
    if (_enableAction)      stats.tasksExecuted  = phaseAction();

    stats.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - t0);

    if (_enableReflection) phaseReflection(stats);
    _lastStats = stats;
}

// -----------------------------------------------------------------------
// Private — loop body

void CognitiveLoop::loopBody() {
    logger().info("[CognitiveLoop] Loop started");
    while (_running.load()) {
        runSingleCycle();
        if (_maxCycles > 0 && _cycleCount.load() >= _maxCycles) {
            _running.store(false);
            break;
        }
        std::this_thread::sleep_for(_cycleInterval);
    }
    logger().info("[CognitiveLoop] Loop stopped after "
                  + std::to_string(_cycleCount.load()) + " cycles");
}

// -----------------------------------------------------------------------
// Cognitive phases

size_t CognitiveLoop::phasePerception() {
    std::vector<PerceptInput> batch;
    {
        std::lock_guard<std::mutex> lk(_perceptMutex);
        batch.swap(_perceptQueue);
    }
    for (const auto& p : batch) {
        // Represent the percept as atoms in the store
        auto srcNode     = _store->addNode(AtomType::CONCEPT, "Source:" + p.source);
        auto modalNode   = _store->addNode(AtomType::CONCEPT, "Modality:" + p.modality);
        auto contentNode = _store->addNode(AtomType::CONCEPT, "Percept:" + p.content);
        contentNode->setSTI(p.salience);

        // EvaluationLink: (perceived, (Source:X, Modality:Y, Content:Z))
        auto predNode  = _store->addNode(AtomType::PREDICATE, "perceived");
        auto listLink  = _store->addLink(AtomType::LIST, {srcNode, modalNode, contentNode});
        auto evalLink  = _store->addLink(AtomType::EVALUATION, {predNode, listLink});
        evalLink->setTV(TruthValue{p.salience, 0.9});

        if (_perceptionHook) _perceptionHook(p, *_store);

        logger().debug("[CognitiveLoop] Percept added: [" + p.source + "] " + p.content);
    }
    return batch.size();
}

size_t CognitiveLoop::phaseReasoning() {
    auto results = _reasoning->runCycle();
    size_t fired = 0;
    for (const auto& r : results) if (r.fired) ++fired;
    return fired;
}

size_t CognitiveLoop::phasePlanning() {
    // Boost task priorities when their associated goal has high priority.
    // Tasks attached to the current (highest-priority) goal get a one-time
    // promotion of one priority level, ensuring goal-relevant work is
    // executed first without collapsing all priorities over repeated cycles.
    auto goal = _taskMgr->currentGoal();
    if (!goal) return 0;

    size_t promoted = 0;
    for (auto& task : goal->tasks) {
        if (task->completed || task->failed) continue;
        // Skip tasks that have already been promoted
        if (_promotedTaskIds.count(task->id)) continue;
        // Promote task priority by one level if it's below HIGH
        if (static_cast<int>(task->priority) > static_cast<int>(Priority::HIGH)) {
            task->priority = static_cast<Priority>(
                static_cast<int>(task->priority) - 1);
            _promotedTaskIds.insert(task->id);
            ++promoted;
        }
    }
    if (promoted > 0) {
        // Re-sort the task queue to reflect the new priorities
        _taskMgr->resortQueue();
        logger().debug("[CognitiveLoop] Planning promoted " +
                       std::to_string(promoted) + " task(s) for goal '" +
                       goal->name + "'");
    }
    return promoted;
}

size_t CognitiveLoop::phaseAction() {
    size_t executed = 0;
    while (executed < _maxTasksPerCycle && _taskMgr->executeNext()) ++executed;
    return executed;
}

void CognitiveLoop::phaseReflection(const CycleStats& stats) {
    // Remove the previous cycle's metadata atoms to prevent unbounded growth.
    // We keep only the most recent cycle's data in the store.
    if (_prevStateLink) _store->remove(_prevStateLink);
    if (_prevCycleNode) _store->remove(_prevCycleNode);
    if (_prevDurationNode) _store->remove(_prevDurationNode);

    // Record cycle metadata in the atom store
    _prevCycleNode = _store->addNode(AtomType::CONCEPT,
                                     "Cycle:" + std::to_string(stats.cycleNumber));
    _prevDurationNode = _store->addNode(AtomType::CONCEPT,
                                        "Duration:" + std::to_string(stats.duration.count()) + "ms");
    _prevStateLink = _store->addLink(AtomType::STATE, {_prevCycleNode, _prevDurationNode});

    if (_reflectionHook) _reflectionHook(stats);

    logger().debug("[CognitiveLoop] Cycle #" + std::to_string(stats.cycleNumber)
                   + " percepts=" + std::to_string(stats.perceptsAdded)
                   + " rules=" + std::to_string(stats.rulesFired)
                   + " tasks=" + std::to_string(stats.tasksExecuted)
                   + " " + std::to_string(stats.duration.count()) + "ms");
}

} // namespace cog0
