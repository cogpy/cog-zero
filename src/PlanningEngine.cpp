/*
 * opencog/agentzero/PlanningEngine.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Planning Engine with Temporal Reasoning Implementation
 * Part of AZ-PLAN-002: Create PlanningEngine with temporal reasoning
 */

#include <opencog/agentzero/PlanningEngine.h>
#include <opencog/agentzero/GoalHierarchy.h>
#include <opencog/agentzero/TemporalReasoner.h>

#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>
#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/truthvalue/SimpleTruthValue.h>
#include <opencog/util/Logger.h>

#include <algorithm>
#include <sstream>
#include <iomanip>

using namespace opencog;
using namespace opencog::agentzero;

PlanningEngine::PlanningEngine(AtomSpacePtr atomspace)
    : _atomspace(atomspace)
    , _goal_hierarchy(std::make_shared<GoalHierarchy>(atomspace))
    , _temporal_reasoner(std::make_shared<TemporalReasoner>(atomspace))
#ifdef HAVE_SPACETIME
    , _time_server(nullptr)
    , _space_server(nullptr)
#endif
    , _planning_timeout(std::chrono::milliseconds(100))  // Target < 100ms for routine decisions
    , _max_plan_depth(10)
    , _max_actions_per_plan(50)
    , _min_confidence_threshold(0.3f)
    , _default_strategy(PlanningStrategy::HYBRID)
    , _enable_replanning(true)
    , _enable_temporal_optimization(true)
    , _average_planning_time(std::chrono::milliseconds(0))
    , _plans_generated(0)
    , _plans_successful(0)
{
    if (!_atomspace) {
        throw std::invalid_argument("PlanningEngine requires valid AtomSpace");
    }
    
    initializePlanningEngine();
    logger().info("PlanningEngine: Initialized with temporal reasoning capabilities");
}

PlanningEngine::~PlanningEngine()
{
    // Cancel all active plans
    for (auto& [goal_atom, plan] : _active_plans) {
        if (plan.status == ExecutionStatus::EXECUTING) {
            plan.status = ExecutionStatus::CANCELLED;
        }
    }
    
    logger().info("PlanningEngine: Destroyed - %d plans processed, %.1f%% success rate",
                 _plans_generated, getPlanningSuccessRate());
}

void PlanningEngine::initializePlanningEngine()
{
    createPlanningContexts();
    
    // Initialize performance tracking
    _last_planning_time = std::chrono::steady_clock::now();
    
    logger().debug("PlanningEngine: Initialization complete");
}

void PlanningEngine::createPlanningContexts()
{
    // Create AtomSpace contexts for organizing planning knowledge
    _planning_context = _atomspace->add_node(CONCEPT_NODE, "PlanningEngineContext");
    _temporal_context = _atomspace->add_node(CONCEPT_NODE, "TemporalPlanningContext");
    _goal_context = _atomspace->add_node(CONCEPT_NODE, "PlanningGoalContext");
    _action_context = _atomspace->add_node(CONCEPT_NODE, "PlanningActionContext");
    
    // Set initial truth values
    _planning_context->setTruthValue(SimpleTruthValue::createTV(1.0, 1.0));
    _temporal_context->setTruthValue(SimpleTruthValue::createTV(1.0, 1.0));
    _goal_context->setTruthValue(SimpleTruthValue::createTV(1.0, 1.0));
    _action_context->setTruthValue(SimpleTruthValue::createTV(1.0, 1.0));
    
    logger().debug("PlanningEngine: Created planning contexts");
}

bool PlanningEngine::validateGoal(const Handle& goal_atom)
{
    if (goal_atom == Handle::UNDEFINED) {
        logger().warn("PlanningEngine: Invalid goal atom - undefined handle");
        return false;
    }
    
    if (!_atomspace->is_valid_handle(goal_atom)) {
        logger().warn("PlanningEngine: Invalid goal atom - not in AtomSpace");
        return false;
    }
    
    // Check if goal has minimum required structure
    // Goals should have some form of condition or description
    auto incoming = goal_atom->getIncomingSet();
    if (incoming.empty()) {
        logger().debug("PlanningEngine: Goal atom has no incoming links - may be primitive goal");
    }
    
    return true;
}

PlanningEngine::PlanResult PlanningEngine::createPlan(const Handle& goal_atom, 
                                                     PlanningStrategy strategy)
{
    auto start_time = std::chrono::steady_clock::now();
    
    if (!validateGoal(goal_atom)) {
        return PlanResult::GOAL_INVALID;
    }
    
    // Check if plan already exists for this goal
    if (_active_plans.find(goal_atom) != _active_plans.end()) {
        logger().debug("PlanningEngine: Plan already exists for goal, adapting existing plan");
        return adaptPlan(_active_plans[goal_atom].plan_atom);
    }
    
    Plan new_plan;
    new_plan.goal_atom = goal_atom;
    new_plan.start_time = start_time;
    
    PlanResult result = generatePlan(goal_atom, new_plan);
    
    if (result == PlanResult::SUCCESS) {
        new_plan.plan_atom = createPlanAtom(new_plan);
        _active_plans[goal_atom] = new_plan;
        _plans_successful++;
        
        logger().info("PlanningEngine: Successfully created plan for goal with %d actions", 
                     (int)new_plan.action_sequence.size());
    } else {
        logger().warn("PlanningEngine: Failed to create plan for goal - result: %d", (int)result);
    }
    
    _plans_generated++;
    auto planning_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start_time);
    recordPlanningMetrics(new_plan, planning_time);
    
    return result;
}

PlanningEngine::PlanResult PlanningEngine::createTemporalPlan(
    const Handle& goal_atom,
    const std::chrono::steady_clock::time_point& deadline,
    PlanningStrategy strategy)
{
    if (!validateGoal(goal_atom)) {
        return PlanResult::GOAL_INVALID;
    }
    
    // Add deadline constraint to temporal reasoner
    _temporal_reasoner->addDeadlineConstraint(goal_atom, deadline, 1.0f, true);
    
    // Use temporal-first strategy if not specified
    if (strategy == PlanningStrategy::HYBRID) {
        strategy = PlanningStrategy::TEMPORAL_FIRST;
    }
    
    return createPlan(goal_atom, strategy);
}

PlanningEngine::PlanResult PlanningEngine::generatePlan(const Handle& goal_atom, Plan& result_plan)
{
    auto start_time = std::chrono::steady_clock::now();
    
    try {
        switch (_default_strategy) {
            case PlanningStrategy::HIERARCHICAL:
                return hierarchicalPlanning(goal_atom, result_plan);
                
            case PlanningStrategy::FORWARD_SEARCH:
                return forwardSearchPlanning(goal_atom, result_plan);
                
            case PlanningStrategy::TEMPORAL_FIRST:
                return temporalPlanning(goal_atom, result_plan);
                
            case PlanningStrategy::HYBRID:
            default:
                // Try hierarchical first, fall back to forward search
                PlanResult result = hierarchicalPlanning(goal_atom, result_plan);
                if (result != PlanResult::SUCCESS) {
                    logger().debug("PlanningEngine: Hierarchical planning failed, trying forward search");
                    result = forwardSearchPlanning(goal_atom, result_plan);
                }
                return result;
        }
    } catch (const std::exception& e) {
        logger().error("PlanningEngine: Exception during planning - %s", e.what());
        return PlanResult::NO_SOLUTION;
    }
    
    // Check timeout
    auto elapsed = std::chrono::steady_clock::now() - start_time;
    if (elapsed > _planning_timeout) {
        logger().warn("PlanningEngine: Planning timeout exceeded (%d ms)", 
                     (int)std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
        return PlanResult::TIMEOUT;
    }
    
    return PlanResult::NO_SOLUTION;
}

PlanningEngine::PlanResult PlanningEngine::hierarchicalPlanning(const Handle& goal_atom, Plan& plan)
{
    logger().debug("PlanningEngine: Starting hierarchical planning for goal");
    
    // Decompose goal into subgoals
    auto subgoals = decomposeGoal(goal_atom);
    
    if (subgoals.empty()) {
        // No decomposition possible, treat as primitive goal
        auto actions = findApplicableActions(goal_atom);
        if (actions.empty()) {
            return PlanResult::NO_SOLUTION;
        }
        
        plan.action_sequence = actions;
        plan.confidence = 0.8f;  // Moderate confidence for primitive goals
        return PlanResult::SUCCESS;
    }
    
    // Plan for each subgoal recursively
    std::vector<Handle> all_actions;
    float combined_confidence = 1.0f;
    
    for (const Handle& subgoal : subgoals) {
        Plan subplan;
        PlanResult result = hierarchicalPlanning(subgoal, subplan);
        
        if (result != PlanResult::SUCCESS) {
            logger().debug("PlanningEngine: Failed to plan for subgoal");
            return result;
        }
        
        // Combine actions and confidence
        all_actions.insert(all_actions.end(), 
                          subplan.action_sequence.begin(), 
                          subplan.action_sequence.end());
        combined_confidence *= subplan.confidence;
        
        // Check plan size limits
        if (all_actions.size() > _max_actions_per_plan) {
            logger().warn("PlanningEngine: Plan size limit exceeded");
            return PlanResult::NO_SOLUTION;
        }
    }
    
    plan.action_sequence = all_actions;
    plan.confidence = combined_confidence;
    
    // Optimize plan if temporal optimization is enabled
    if (_enable_temporal_optimization) {
        optimizePlan(plan);
    }
    
    return PlanResult::SUCCESS;
}

PlanningEngine::PlanResult PlanningEngine::forwardSearchPlanning(const Handle& goal_atom, Plan& plan)
{
    logger().debug("PlanningEngine: Starting forward search planning for goal");
    
    // Find applicable actions for the goal
    auto actions = findApplicableActions(goal_atom);
    
    if (actions.empty()) {
        return PlanResult::NO_SOLUTION;
    }
    
    // Simple forward search - select best action based on preconditions
    std::vector<Handle> valid_actions;
    for (const Handle& action : actions) {
        if (checkPreconditions(action)) {
            valid_actions.push_back(action);
        }
    }
    
    if (valid_actions.empty()) {
        return PlanResult::NO_SOLUTION;
    }
    
    plan.action_sequence = valid_actions;
    plan.confidence = 0.6f;  // Lower confidence for simple forward search
    
    return PlanResult::SUCCESS;
}

PlanningEngine::PlanResult PlanningEngine::temporalPlanning(const Handle& goal_atom, Plan& plan)
{
    logger().debug("PlanningEngine: Starting temporal planning for goal");
    
    // Generate initial plan using hierarchical approach
    PlanResult result = hierarchicalPlanning(goal_atom, plan);
    
    if (result != PlanResult::SUCCESS) {
        return result;
    }
    
    // Apply temporal constraints and optimization
    if (!checkTemporalConstraints(plan)) {
        logger().debug("PlanningEngine: Initial plan violates temporal constraints");
        
        // Try to resolve temporal conflicts
        auto conflicts = _temporal_reasoner->findTemporalConflicts(plan.action_sequence);
        if (!conflicts.empty()) {
            if (_temporal_reasoner->resolveTemporalConflicts(conflicts)) {
                logger().debug("PlanningEngine: Resolved temporal conflicts");
            } else {
                return PlanResult::TEMPORAL_CONFLICT;
            }
        }
    }
    
    // Optimize temporal schedule
    plan.action_sequence = _temporal_reasoner->optimizeTemporalSchedule(plan.action_sequence);
    
    return PlanResult::SUCCESS;
}

std::vector<Handle> PlanningEngine::decomposeGoal(const Handle& goal_atom)
{
    std::vector<Handle> subgoals;
    
    // Use goal hierarchy if available
    if (_goal_hierarchy) {
        subgoals = _goal_hierarchy->getSubgoals(goal_atom);
        if (!subgoals.empty()) {
            return subgoals;
        }
    }
    
    // Analyze goal structure in AtomSpace for decomposition
    auto incoming = goal_atom->getIncomingSet();
    for (const Handle& link : incoming) {
        if (link->get_type() == AND_LINK) {
            // Goal with AND structure - all components are subgoals
            auto outgoing = link->getOutgoingSet();
            for (const Handle& component : outgoing) {
                if (component != goal_atom) {
                    subgoals.push_back(component);
                }
            }
        }
    }
    
    return subgoals;
}

std::vector<Handle> PlanningEngine::findApplicableActions(const Handle& goal_atom)
{
    std::vector<Handle> actions;
    
    // Search AtomSpace for actions that can achieve the goal
    // Look for ExecutionLinks or other action representations
    HandleSeq all_atoms;
    _atomspace->get_handles_by_type(all_atoms, EXECUTION_LINK);
    
    for (const Handle& exec_link : all_atoms) {
        // Check if this action can achieve the goal
        // This is a simplified heuristic - in practice would use more sophisticated matching
        auto outgoing = exec_link->getOutgoingSet();
        if (outgoing.size() >= 2) {
            // Check if action effects match goal
            bool applicable = false;
            auto effects_incoming = outgoing[1]->getIncomingSet();
            for (const Handle& effect_link : effects_incoming) {
                auto effect_outgoing = effect_link->getOutgoingSet();
                for (const Handle& effect_atom : effect_outgoing) {
                    if (effect_atom == goal_atom) {
                        applicable = true;
                        break;
                    }
                }
                if (applicable) break;
            }
            
            if (applicable) {
                actions.push_back(exec_link);
            }
        }
    }
    
    // If no specific actions found, create a generic action
    if (actions.empty()) {
        std::string action_name = "achieve-" + goal_atom->to_string();
        Handle action_node = _atomspace->add_node(CONCEPT_NODE, std::move(action_name));
        Handle execution = _atomspace->add_link(EXECUTION_LINK, {action_node, goal_atom});
        actions.push_back(execution);
    }
    
    return actions;
}

bool PlanningEngine::checkPreconditions(const Handle& action_atom)
{
    // Simplified precondition checking
    // In practice, would evaluate precondition expressions in AtomSpace
    return true;  // Assume preconditions are satisfied for now
}

bool PlanningEngine::checkTemporalConstraints(const Plan& plan)
{
    if (!_temporal_reasoner) {
        return true;
    }
    
    return _temporal_reasoner->validateTemporalSchedule(plan.action_sequence);
}

bool PlanningEngine::checkResourceConstraints(const Plan& plan)
{
    // Resource constraint checking would be integrated with resource management system
    // For now, assume resources are available
    return true;
}

void PlanningEngine::optimizePlan(Plan& plan)
{
    if (!_enable_temporal_optimization || !_temporal_reasoner) {
        return;
    }
    
    // Optimize action sequence for temporal efficiency
    plan.action_sequence = _temporal_reasoner->optimizeTemporalSchedule(plan.action_sequence);
    
    // Update plan duration based on optimized schedule
    if (!plan.action_sequence.empty()) {
        auto first_interval = _temporal_reasoner->getTemporalInterval(plan.action_sequence.front());
        auto last_interval = _temporal_reasoner->getTemporalInterval(plan.action_sequence.back());
        
        if (first_interval && last_interval) {
            plan.start_time = first_interval->start;
            plan.end_time = last_interval->end;
            plan.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                plan.end_time - plan.start_time);
        }
    }
    
    logger().debug("PlanningEngine: Optimized plan with %d actions, duration: %d ms",
                  (int)plan.action_sequence.size(), (int)plan.duration.count());
}

Handle PlanningEngine::createPlanAtom(const Plan& plan)
{
    // Create AtomSpace representation of the plan
    std::string plan_name = "plan-" + std::to_string(_plans_generated);
    Handle plan_atom = _atomspace->add_node(CONCEPT_NODE, std::move(plan_name));
    
    // Link plan to goal
    _atomspace->add_link(EVALUATION_LINK, {
        _atomspace->add_node(PREDICATE_NODE, "plan-for-goal"),
        _atomspace->add_link(LIST_LINK, {plan_atom, plan.goal_atom})
    });
    
    // Link plan to actions
    if (!plan.action_sequence.empty()) {
        HandleSeq action_copy = plan.action_sequence;  // Make a copy
        Handle action_list = _atomspace->add_link(LIST_LINK, std::move(action_copy));
        _atomspace->add_link(EVALUATION_LINK, {
            _atomspace->add_node(PREDICATE_NODE, "plan-actions"),
            _atomspace->add_link(LIST_LINK, {plan_atom, action_list})
        });
    }
    
    // Set plan confidence as truth value
    plan_atom->setTruthValue(SimpleTruthValue::createTV(plan.confidence, 1.0));
    
    return plan_atom;
}

void PlanningEngine::recordPlanningMetrics(const Plan& plan, 
                                         std::chrono::milliseconds planning_time)
{
    // Update average planning time
    if (_plans_generated > 0) {
        auto total_time = _average_planning_time * _plans_generated + planning_time;
        _average_planning_time = total_time / (_plans_generated + 1);
    } else {
        _average_planning_time = planning_time;
    }
    
    _last_planning_time = std::chrono::steady_clock::now();
    
    logger().debug("PlanningEngine: Planning took %d ms (avg: %d ms), confidence: %.2f",
                  (int)planning_time.count(), (int)_average_planning_time.count(), plan.confidence);
}

const PlanningEngine::Plan* PlanningEngine::getPlan(const Handle& goal_atom) const
{
    auto it = _active_plans.find(goal_atom);
    return (it != _active_plans.end()) ? &it->second : nullptr;
}

std::vector<PlanningEngine::Plan> PlanningEngine::getActivePlans() const
{
    std::vector<Plan> plans;
    for (const auto& [goal, plan] : _active_plans) {
        if (plan.status == ExecutionStatus::EXECUTING || 
            plan.status == ExecutionStatus::NOT_STARTED) {
            plans.push_back(plan);
        }
    }
    return plans;
}

PlanningEngine::ExecutionStatus PlanningEngine::getPlanStatus(const Handle& plan_atom) const
{
    for (const auto& [goal, plan] : _active_plans) {
        if (plan.plan_atom == plan_atom) {
            return plan.status;
        }
    }
    return ExecutionStatus::NOT_STARTED;
}

int PlanningEngine::updateTemporalReasoning()
{
    if (!_temporal_reasoner) {
        return 0;
    }
    
    return _temporal_reasoner->updateTemporalReasoning();
}

std::string PlanningEngine::getPerformanceStats() const
{
    std::ostringstream stats;
    stats << std::fixed << std::setprecision(2);
    stats << "{\n";
    stats << "  \"plans_generated\": " << _plans_generated << ",\n";
    stats << "  \"plans_successful\": " << _plans_successful << ",\n";
    stats << "  \"success_rate\": " << getPlanningSuccessRate() << ",\n";
    stats << "  \"average_planning_time_ms\": " << _average_planning_time.count() << ",\n";
    stats << "  \"active_plans\": " << _active_plans.size() << ",\n";
    stats << "  \"target_time_ms\": " << _planning_timeout.count() << "\n";
    stats << "}";
    return stats.str();
}

float PlanningEngine::getPlanningSuccessRate() const
{
    if (_plans_generated == 0) {
        return 0.0f;
    }
    return (float(_plans_successful) / float(_plans_generated)) * 100.0f;
}

void PlanningEngine::resetPerformanceStats()
{
    _plans_generated = 0;
    _plans_successful = 0;
    _average_planning_time = std::chrono::milliseconds(0);
    _last_planning_time = std::chrono::steady_clock::now();
    
    logger().info("PlanningEngine: Performance statistics reset");
}

PlanningEngine::PlanResult PlanningEngine::adaptPlan(const Handle& plan_atom)
{
    // Find the goal associated with this plan
    Handle goal_atom = Handle::UNDEFINED;
    for (const auto& [goal, plan] : _active_plans) {
        if (plan.plan_atom == plan_atom) {
            goal_atom = goal;
            break;
        }
    }
    
    if (goal_atom == Handle::UNDEFINED) {
        logger().warn("PlanningEngine: Cannot adapt plan - plan not found");
        return PlanResult::NO_SOLUTION;
    }
    
    // Mark current plan for replanning
    _active_plans[goal_atom].status = ExecutionStatus::REPLANNING;
    _active_plans[goal_atom].revision_count++;
    
    // Generate new plan
    Plan adapted_plan;
    adapted_plan.goal_atom = goal_atom;
    adapted_plan.revision_count = _active_plans[goal_atom].revision_count;
    
    PlanResult result = generatePlan(goal_atom, adapted_plan);
    
    if (result == PlanResult::SUCCESS) {
        adapted_plan.plan_atom = createPlanAtom(adapted_plan);
        _active_plans[goal_atom] = adapted_plan;
        
        logger().info("PlanningEngine: Successfully adapted plan (revision %d)", 
                     adapted_plan.revision_count);
    } else {
        // Restore original status if adaptation failed
        _active_plans[goal_atom].status = ExecutionStatus::FAILED;
        logger().warn("PlanningEngine: Plan adaptation failed");
    }
    
    return result;
}
PlanningEngine::PlanQualityMetrics
PlanningEngine::computePlanQuality(const Plan& plan) const
{
    PlanQualityMetrics metrics;

    // Action count and average action confidence
    metrics.action_count = static_cast<int>(plan.action_sequence.size());

    if (metrics.action_count > 0) {
        float conf_sum = 0.0f;
        for (const Handle& action : plan.action_sequence) {
            if (_atomspace->is_valid_handle(action)) {
                TruthValuePtr tv = action->getTruthValue();
                conf_sum += static_cast<float>(tv->get_confidence());
            }
        }
        metrics.average_action_confidence = conf_sum / float(metrics.action_count);
    }

    // Optimality score: penalise longer plans relative to a heuristic target depth
    const int target_depth = _max_plan_depth > 0 ? _max_plan_depth : 10;
    metrics.optimality_score = metrics.action_count > 0
        ? std::max(0.0f, 1.0f - float(metrics.action_count - 1) / float(target_depth))
        : 0.0f;

    // Goal coverage: ratio of satisfied effects to total expected effects
    if (!plan.effects.empty()) {
        int satisfied = 0;
        for (const Handle& effect : plan.effects) {
            if (_atomspace->is_valid_handle(effect)) {
                TruthValuePtr tv = effect->getTruthValue();
                if (tv->get_mean() >= 0.5) {
                    ++satisfied;
                }
            }
        }
        metrics.goal_coverage = float(satisfied) / float(plan.effects.size());
    } else {
        // No effects specified: assume full coverage when plan succeeded
        metrics.goal_coverage = (plan.status == ExecutionStatus::COMPLETED) ? 1.0f : 0.5f;
    }

    // Temporal satisfaction: use temporal reasoner to validate the plan schedule
    if (_temporal_reasoner && !plan.action_sequence.empty()) {
        Handle temporal_ctx = _temporal_reasoner->getTemporalContext();
        if (temporal_ctx != Handle::UNDEFINED) {
            // Query how many temporal constraints related to plan actions conflict
            auto conflicts = _temporal_reasoner->findTemporalConflicts(
                plan.action_sequence);
            int total_events = static_cast<int>(plan.action_sequence.size());
            int unsatisfied = static_cast<int>(conflicts.size());
            metrics.temporal_satisfaction = std::max(0.0f,
                1.0f - float(unsatisfied) / float(total_events));
        } else {
            metrics.temporal_satisfaction = 1.0f; // No constraints to violate
        }
    } else {
        metrics.temporal_satisfaction = 1.0f;
    }

    // Resource efficiency: derived from plan confidence and revision count
    float revision_penalty = std::min(1.0f, float(plan.revision_count) * 0.1f);
    metrics.resource_efficiency = std::max(0.0f, plan.confidence - revision_penalty);

    // Robustness: plans with fewer revisions and higher confidence are more robust
    metrics.robustness_score = plan.confidence * (1.0f - revision_penalty);

    // Record the wall-clock time spent generating the last plan
    metrics.planning_time = _average_planning_time;

    logger().debug("PlanningEngine: computePlanQuality -> overall=%.2f "
                   "actions=%d coverage=%.2f temporal=%.2f",
                   metrics.overallScore(),
                   metrics.action_count,
                   metrics.goal_coverage,
                   metrics.temporal_satisfaction);

    return metrics;
}
