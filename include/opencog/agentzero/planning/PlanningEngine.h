/*
 * opencog/agentzero/planning/PlanningEngine.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * STRIPS / HTN plan generation and execution (Phase 4).
 */

#ifndef _OPENCOG_AGENTZERO_PLANNING_PLANNING_ENGINE_H
#define _OPENCOG_AGENTZERO_PLANNING_PLANNING_ENGINE_H

#include <chrono>
#include <map>
#include <memory>
#include <queue>
#include <set>
#include <string>
#include <vector>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Handle.h>
#include <opencog/util/Logger.h>

#include "GoalHierarchy.h"
#include "TemporalReasoner.h"

namespace opencog {
namespace agentzero {
namespace planning {

/**
 * PlanningEngine — STRIPS operators + HTN methods, plan generation & execution.
 */
class PlanningEngine
{
public:
    enum class PlanResult {
        SUCCESS,
        NO_SOLUTION,
        TIMEOUT,
        GOAL_INVALID,
        RESOURCES_UNAVAILABLE,
        TEMPORAL_CONFLICT,
        MEMORY_LIMIT,
        PRECONDITION_FAILURE
    };

    enum class ExecutionStatus {
        NOT_STARTED,
        EXECUTING,
        COMPLETED,
        FAILED,
        CANCELLED,
        REPLANNING
    };

    enum class PlanningStrategy {
        HIERARCHICAL,    // HTN top-down
        STRIPS,          // classical STRIPS forward search
        FORWARD_SEARCH,
        BACKWARD_SEARCH,
        HYBRID,
        TEMPORAL_FIRST,
        RESOURCE_OPTIMAL
    };

    /** STRIPS-style operator. */
    struct StripsOperator {
        std::string name;
        Handle operator_atom;
        std::vector<std::string> preconditions; // fluent names that must hold
        std::vector<std::string> add_effects;
        std::vector<std::string> delete_effects;
        float cost{1.0f};
        float confidence{1.0f};
    };

    /** HTN method: compound task → ordered subtasks (primitive or compound). */
    struct HtnMethod {
        std::string name;
        std::string compound_task;
        std::vector<std::string> subtasks;
        std::vector<std::string> preconditions;
        float confidence{1.0f};
    };

    struct Plan {
        Handle plan_atom;
        Handle goal_atom;
        std::vector<Handle> action_sequence;
        std::vector<std::string> action_names;
        std::vector<Handle> preconditions;
        std::vector<Handle> effects;
        std::chrono::steady_clock::time_point start_time;
        std::chrono::steady_clock::time_point end_time;
        std::chrono::milliseconds duration{0};
        ExecutionStatus status{ExecutionStatus::NOT_STARTED};
        float confidence{0.5f};
        int revision_count{0};
        size_t next_action_index{0};
        PlanningStrategy strategy_used{PlanningStrategy::HYBRID};
    };

    struct PlanQualityMetrics {
        float optimality_score{0.0f};
        float goal_coverage{0.0f};
        float temporal_satisfaction{0.0f};
        float resource_efficiency{0.0f};
        float robustness_score{0.0f};
        int action_count{0};
        float average_action_confidence{0.0f};
        std::chrono::milliseconds planning_time{0};

        float overallScore() const {
            return (optimality_score + goal_coverage + temporal_satisfaction
                    + resource_efficiency + robustness_score) / 5.0f;
        }

        void reset() {
            optimality_score = goal_coverage = temporal_satisfaction = 0.0f;
            resource_efficiency = robustness_score = average_action_confidence = 0.0f;
            action_count = 0;
            planning_time = std::chrono::milliseconds{0};
        }
    };

    explicit PlanningEngine(AtomSpacePtr atomspace);
    ~PlanningEngine();

    bool initialize();
    bool shutdown();
    bool isInitialized() const { return _initialized; }

    // Domain definition
    bool registerOperator(const StripsOperator& op);
    bool registerMethod(const HtnMethod& method);
    bool setFluent(const std::string& name, bool value);
    bool getFluent(const std::string& name) const;
    std::set<std::string> getWorldState() const { return _world_state; }
    size_t operatorCount() const { return _operators.size(); }
    size_t methodCount() const { return _methods.size(); }

    // Planning
    PlanResult createPlan(const Handle& goal_atom,
                          PlanningStrategy strategy = PlanningStrategy::HYBRID);
    PlanResult createPlanForTask(const std::string& task_name,
                                 PlanningStrategy strategy = PlanningStrategy::HIERARCHICAL);
    PlanResult createTemporalPlan(const Handle& goal_atom,
                                  const std::chrono::steady_clock::time_point& deadline,
                                  PlanningStrategy strategy = PlanningStrategy::TEMPORAL_FIRST);
    PlanResult createResourceConstrainedPlan(const Handle& goal_atom,
                                             const std::vector<Handle>& available_resources,
                                             PlanningStrategy strategy = PlanningStrategy::RESOURCE_OPTIMAL);

    PlanResult adaptPlan(const Handle& plan_atom);
    bool cancelPlan(const Handle& plan_atom);

    // Execution
    bool startExecution(const Handle& goal_atom);
    PlanResult stepExecution(const Handle& goal_atom);
    bool markActionCompleted(const Handle& plan_atom, const Handle& action_atom);
    /** Apply STRIPS effects of the named operator to the world state. */
    bool applyOperatorEffects(const std::string& operator_name);

    // Access
    const Plan* getPlan(const Handle& goal_atom) const;
    std::vector<Plan> getActivePlans() const;
    ExecutionStatus getPlanStatus(const Handle& plan_atom) const;
    Handle getNextAction(const Handle& plan_atom) const;

    void setGoalHierarchy(std::shared_ptr<GoalHierarchy> hierarchy) {
        _goal_hierarchy = std::move(hierarchy);
    }
    GoalHierarchy* getGoalHierarchy() const { return _goal_hierarchy.get(); }

    void setTemporalReasoner(std::shared_ptr<TemporalReasoner> tr) {
        _temporal_reasoner = std::move(tr);
    }
    TemporalReasoner* getTemporalReasoner() const { return _temporal_reasoner.get(); }

    void setPlanningTimeout(int timeout_ms) {
        _planning_timeout = std::chrono::milliseconds(timeout_ms);
    }
    void setMaxPlanDepth(int max_depth) { _max_plan_depth = max_depth; }
    void setMaxActionsPerPlan(int max_actions) { _max_actions_per_plan = max_actions; }
    void setMinConfidenceThreshold(float threshold) { _min_confidence_threshold = threshold; }
    void configureFeatures(bool replanning, bool temporal_opt) {
        _enable_replanning = replanning;
        _enable_temporal_optimization = temporal_opt;
    }

    Handle getPlanningContext() const { return _planning_context; }
    std::string getPerformanceStats() const;
    std::chrono::milliseconds getAveragePlanningTime() const { return _average_planning_time; }
    float getPlanningSuccessRate() const;
    PlanQualityMetrics computePlanQuality(const Plan& plan) const;
    void resetPerformanceStats();

    static std::string strategyToString(PlanningStrategy s);
    static std::string resultToString(PlanResult r);

private:
    AtomSpacePtr _atomspace;
    bool _initialized{false};
    std::shared_ptr<GoalHierarchy> _goal_hierarchy;
    std::shared_ptr<TemporalReasoner> _temporal_reasoner;

    std::map<Handle, Plan> _active_plans;
    std::map<std::string, StripsOperator> _operators;
    std::map<std::string, std::vector<HtnMethod>> _methods; // compound task → methods
    std::set<std::string> _world_state; // true fluents

    Handle _planning_context;
    Handle _temporal_context;
    Handle _goal_context;
    Handle _action_context;

    std::chrono::milliseconds _planning_timeout{100};
    int _max_plan_depth{10};
    int _max_actions_per_plan{50};
    float _min_confidence_threshold{0.3f};
    PlanningStrategy _default_strategy{PlanningStrategy::HYBRID};
    bool _enable_replanning{true};
    bool _enable_temporal_optimization{true};

    std::chrono::steady_clock::time_point _last_planning_time;
    std::chrono::milliseconds _average_planning_time{0};
    int _plans_generated{0};
    int _plans_successful{0};

    void createPlanningContexts();
    bool validateGoal(const Handle& goal_atom) const;
    PlanResult generatePlan(const Handle& goal_atom, PlanningStrategy strategy, Plan& out);
    PlanResult hierarchicalPlanning(const std::string& task, Plan& plan, int depth);
    PlanResult stripsPlanning(const std::vector<std::string>& goals, Plan& plan);
    PlanResult temporalPlanning(const Handle& goal_atom, Plan& plan);
    bool preconditionsMet(const std::vector<std::string>& pre) const;
    bool isPrimitive(const std::string& task) const;
    Handle createActionAtom(const std::string& name);
    Handle createPlanAtom(const Plan& plan);
    void recordPlanningMetrics(const Plan& plan, std::chrono::milliseconds planning_time);
    std::string goalName(const Handle& goal_atom) const;
};

} // namespace planning
} // namespace agentzero
} // namespace opencog

#endif // _OPENCOG_AGENTZERO_PLANNING_PLANNING_ENGINE_H
