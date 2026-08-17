/*
 * opencog/agentzero/planning/PlanningEngine.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * STRIPS / HTN plan generation and execution.
 */

#include <algorithm>
#include <queue>
#include <sstream>
#include <stdexcept>

#include <opencog/atoms/atom_types/atom_types.h>
#include <opencog/atoms/truthvalue/SimpleTruthValue.h>
#include <opencog/util/Logger.h>

#include "opencog/agentzero/planning/PlanningEngine.h"

using namespace opencog;
using namespace opencog::agentzero::planning;

PlanningEngine::PlanningEngine(AtomSpacePtr atomspace)
    : _atomspace(atomspace)
{
    if (!_atomspace)
        throw std::runtime_error("PlanningEngine requires a valid AtomSpace");
}

PlanningEngine::~PlanningEngine() = default;

bool PlanningEngine::initialize()
{
    if (_initialized) return true;
    createPlanningContexts();
    if (!_goal_hierarchy) {
        _goal_hierarchy = std::make_shared<GoalHierarchy>(_atomspace);
        _goal_hierarchy->initialize();
    }
    if (!_temporal_reasoner) {
        _temporal_reasoner = std::make_shared<TemporalReasoner>(_atomspace);
        _temporal_reasoner->initialize();
    }
    _last_planning_time = std::chrono::steady_clock::now();
    _initialized = true;
    logger().info() << "[PlanningEngine] Initialized";
    return true;
}

bool PlanningEngine::shutdown()
{
    for (auto& kv : _active_plans) {
        if (kv.second.status == ExecutionStatus::EXECUTING)
            kv.second.status = ExecutionStatus::CANCELLED;
    }
    _initialized = false;
    return true;
}

void PlanningEngine::createPlanningContexts()
{
    _planning_context = _atomspace->add_node(CONCEPT_NODE, "PlanningEngineContext");
    _temporal_context = _atomspace->add_node(CONCEPT_NODE, "TemporalPlanningContext");
    _goal_context = _atomspace->add_node(CONCEPT_NODE, "PlanningGoalContext");
    _action_context = _atomspace->add_node(CONCEPT_NODE, "PlanningActionContext");
    _planning_context->setTruthValue(SimpleTruthValue::createTV(1.0, 1.0));
    _atomspace->add_link(MEMBER_LINK, {_temporal_context, _planning_context});
    _atomspace->add_link(MEMBER_LINK, {_goal_context, _planning_context});
    _atomspace->add_link(MEMBER_LINK, {_action_context, _planning_context});
}

bool PlanningEngine::registerOperator(const StripsOperator& op)
{
    if (op.name.empty()) return false;
    StripsOperator copy = op;
    if (copy.operator_atom == Handle::UNDEFINED) {
        copy.operator_atom = _atomspace->add_node(SCHEMA_NODE, op.name);
    }
    _operators[op.name] = copy;
    _atomspace->add_link(MEMBER_LINK, {copy.operator_atom, _action_context});
    return true;
}

bool PlanningEngine::registerMethod(const HtnMethod& method)
{
    if (method.name.empty() || method.compound_task.empty()) return false;
    _methods[method.compound_task].push_back(method);
    return true;
}

bool PlanningEngine::setFluent(const std::string& name, bool value)
{
    if (value) _world_state.insert(name);
    else _world_state.erase(name);
    return true;
}

bool PlanningEngine::getFluent(const std::string& name) const
{
    return _world_state.count(name) > 0;
}

bool PlanningEngine::preconditionsMet(const std::vector<std::string>& pre) const
{
    for (const auto& p : pre)
        if (!_world_state.count(p)) return false;
    return true;
}

bool PlanningEngine::isPrimitive(const std::string& task) const
{
    return _operators.count(task) > 0;
}

std::string PlanningEngine::goalName(const Handle& goal_atom) const
{
    if (!goal_atom) return {};
    if (goal_atom->is_node()) return goal_atom->get_name();
    return goal_atom->to_short_string();
}

Handle PlanningEngine::createActionAtom(const std::string& name)
{
    Handle action = _atomspace->add_node(SCHEMA_NODE, name);
    Handle exec = _atomspace->add_link(EXECUTION_LINK, {action});
    _atomspace->add_link(MEMBER_LINK, {exec, _action_context});
    return exec;
}

bool PlanningEngine::validateGoal(const Handle& goal_atom) const
{
    if (goal_atom == Handle::UNDEFINED) return false;
    return _atomspace->is_valid_handle(goal_atom);
}

PlanningEngine::PlanResult
PlanningEngine::stripsPlanning(const std::vector<std::string>& goals, Plan& plan)
{
    // Goal-regression / forward BFS on fluents with registered operators.
    std::set<std::string> state = _world_state;
    auto goalsMet = [&](const std::set<std::string>& s) {
        for (const auto& g : goals)
            if (!s.count(g)) return false;
        return true;
    };
    if (goalsMet(state)) {
        plan.confidence = 1.0f;
        plan.strategy_used = PlanningStrategy::STRIPS;
        return PlanResult::SUCCESS;
    }

    struct Node {
        std::set<std::string> state;
        std::vector<std::string> actions;
        float cost;
    };

    std::queue<Node> q;
    q.push(Node{state, {}, 0.0f});
    std::set<std::set<std::string>> visited;
    visited.insert(state);

    int expansions = 0;
    const int max_expansions = 5000;

    while (!q.empty() && expansions < max_expansions) {
        Node cur = q.front();
        q.pop();
        ++expansions;

        if (static_cast<int>(cur.actions.size()) > _max_actions_per_plan)
            continue;

        for (const auto& kv : _operators) {
            const StripsOperator& op = kv.second;
            bool ok = true;
            for (const auto& p : op.preconditions) {
                if (!cur.state.count(p)) { ok = false; break; }
            }
            if (!ok) continue;

            std::set<std::string> next = cur.state;
            for (const auto& d : op.delete_effects) next.erase(d);
            for (const auto& a : op.add_effects) next.insert(a);

            if (visited.count(next)) continue;
            visited.insert(next);

            Node nxt;
            nxt.state = std::move(next);
            nxt.actions = cur.actions;
            nxt.actions.push_back(op.name);
            nxt.cost = cur.cost + op.cost;

            if (goalsMet(nxt.state)) {
                plan.action_names = nxt.actions;
                plan.action_sequence.clear();
                float conf = 1.0f;
                for (const auto& an : plan.action_names) {
                    plan.action_sequence.push_back(createActionAtom(an));
                    conf *= _operators[an].confidence;
                }
                plan.confidence = conf;
                plan.strategy_used = PlanningStrategy::STRIPS;
                return PlanResult::SUCCESS;
            }
            q.push(std::move(nxt));
        }
    }
    return PlanResult::NO_SOLUTION;
}

PlanningEngine::PlanResult
PlanningEngine::hierarchicalPlanning(const std::string& task, Plan& plan, int depth)
{
    if (depth > _max_plan_depth) return PlanResult::NO_SOLUTION;

    // Primitive STRIPS operator
    if (isPrimitive(task)) {
        const StripsOperator& op = _operators[task];
        if (!preconditionsMet(op.preconditions))
            return PlanResult::PRECONDITION_FAILURE;
        plan.action_names.push_back(task);
        plan.action_sequence.push_back(createActionAtom(task));
        plan.confidence *= op.confidence;
        // Simulate effects so later subtasks in the same HTN method see them
        for (const auto& d : op.delete_effects) _world_state.erase(d);
        for (const auto& a : op.add_effects) _world_state.insert(a);
        plan.strategy_used = PlanningStrategy::HIERARCHICAL;
        return PlanResult::SUCCESS;
    }

    // HTN methods
    auto mit = _methods.find(task);
    if (mit == _methods.end() || mit->second.empty()) {
        // Fallback: treat unknown task name as a goal fluent via STRIPS
        return stripsPlanning({task}, plan);
    }

    // Try methods in order; restore world state between method attempts
    const std::set<std::string> saved_world = _world_state;
    for (const HtnMethod& method : mit->second) {
        _world_state = saved_world;
        if (!preconditionsMet(method.preconditions)) continue;

        Plan sub;
        sub.confidence = method.confidence;
        bool ok = true;
        for (const auto& st : method.subtasks) {
            Plan part;
            part.confidence = 1.0f;
            PlanResult r = hierarchicalPlanning(st, part, depth + 1);
            if (r != PlanResult::SUCCESS) {
                ok = false;
                break;
            }
            sub.action_names.insert(sub.action_names.end(),
                                    part.action_names.begin(), part.action_names.end());
            sub.action_sequence.insert(sub.action_sequence.end(),
                                       part.action_sequence.begin(),
                                       part.action_sequence.end());
            sub.confidence *= part.confidence;
            if (static_cast<int>(sub.action_sequence.size()) > _max_actions_per_plan) {
                ok = false;
                break;
            }
        }
        if (!ok) {
            _world_state = saved_world;
            continue;
        }

        // Planning must not permanently mutate world state — execution does
        _world_state = saved_world;
        plan.action_names = sub.action_names;
        plan.action_sequence = sub.action_sequence;
        plan.confidence = sub.confidence;
        plan.strategy_used = PlanningStrategy::HIERARCHICAL;
        return PlanResult::SUCCESS;
    }
    _world_state = saved_world;
    return PlanResult::NO_SOLUTION;
}

PlanningEngine::PlanResult
PlanningEngine::temporalPlanning(const Handle& goal_atom, Plan& plan)
{
    PlanResult r = hierarchicalPlanning(goalName(goal_atom), plan, 0);
    if (r != PlanResult::SUCCESS) {
        // try STRIPS on goal name as fluent
        r = stripsPlanning({goalName(goal_atom)}, plan);
    }
    if (r != PlanResult::SUCCESS) return r;

    if (_temporal_reasoner) {
        _temporal_reasoner->assignSequentialSchedule(
            plan.action_sequence,
            std::chrono::steady_clock::now(),
            std::chrono::milliseconds(100));
        plan.action_sequence =
            _temporal_reasoner->optimizeTemporalSchedule(plan.action_sequence);
        auto conflicts =
            _temporal_reasoner->findTemporalConflicts(plan.action_sequence);
        if (!conflicts.empty()) {
            if (!_temporal_reasoner->resolveTemporalConflicts(conflicts))
                return PlanResult::TEMPORAL_CONFLICT;
        }
        if (!_temporal_reasoner->validateTemporalSchedule(plan.action_sequence))
            return PlanResult::TEMPORAL_CONFLICT;
    }
    plan.strategy_used = PlanningStrategy::TEMPORAL_FIRST;
    return PlanResult::SUCCESS;
}

PlanningEngine::PlanResult
PlanningEngine::generatePlan(const Handle& goal_atom, PlanningStrategy strategy, Plan& out)
{
    auto start = std::chrono::steady_clock::now();
    out = Plan{};
    out.goal_atom = goal_atom;
    out.confidence = 1.0f;
    out.start_time = start;
    out.strategy_used = strategy;

    // HTN expansion may temporarily simulate operator effects; always restore.
    const std::set<std::string> world_snapshot = _world_state;

    if (!validateGoal(goal_atom)) return PlanResult::GOAL_INVALID;

    // Goal hierarchy decomposition into leaf tasks when available
    std::vector<std::string> tasks;
    if (_goal_hierarchy && _goal_hierarchy->hasGoal(goal_atom)) {
        auto leaves = _goal_hierarchy->getLeafGoals(goal_atom);
        if (leaves.empty()) leaves = {goal_atom};
        for (const Handle& leaf : leaves) {
            if (!_goal_hierarchy->areGoalDependenciesSatisfied(leaf) &&
                !_goal_hierarchy->getGoalDependencies(leaf).empty()) {
                // skip blocked leaves — try others
                continue;
            }
            tasks.push_back(goalName(leaf));
        }
        if (tasks.empty()) tasks.push_back(goalName(goal_atom));
    } else {
        tasks.push_back(goalName(goal_atom));
    }

    PlanResult result = PlanResult::NO_SOLUTION;

    auto runHybrid = [&]() {
        // Prefer HTN if methods exist for any task, else STRIPS
        bool any_method = false;
        for (const auto& t : tasks)
            if (_methods.count(t)) any_method = true;
        if (any_method) {
            Plan acc;
            acc.confidence = 1.0f;
            for (const auto& t : tasks) {
                Plan part;
                part.confidence = 1.0f;
                PlanResult r = hierarchicalPlanning(t, part, 0);
                if (r != PlanResult::SUCCESS) return r;
                acc.action_names.insert(acc.action_names.end(),
                                        part.action_names.begin(),
                                        part.action_names.end());
                acc.action_sequence.insert(acc.action_sequence.end(),
                                           part.action_sequence.begin(),
                                           part.action_sequence.end());
                acc.confidence *= part.confidence;
            }
            out = acc;
            out.goal_atom = goal_atom;
            out.strategy_used = PlanningStrategy::HYBRID;
            return PlanResult::SUCCESS;
        }
        return stripsPlanning(tasks, out);
    };

    switch (strategy) {
        case PlanningStrategy::HIERARCHICAL: {
            Plan acc;
            acc.confidence = 1.0f;
            for (const auto& t : tasks) {
                Plan part;
                part.confidence = 1.0f;
                PlanResult r = hierarchicalPlanning(t, part, 0);
                if (r != PlanResult::SUCCESS) { result = r; break; }
                acc.action_names.insert(acc.action_names.end(),
                                        part.action_names.begin(),
                                        part.action_names.end());
                acc.action_sequence.insert(acc.action_sequence.end(),
                                           part.action_sequence.begin(),
                                           part.action_sequence.end());
                acc.confidence *= part.confidence;
                result = PlanResult::SUCCESS;
            }
            if (result == PlanResult::SUCCESS) {
                out = acc;
                out.goal_atom = goal_atom;
                out.strategy_used = PlanningStrategy::HIERARCHICAL;
            }
            break;
        }
        case PlanningStrategy::STRIPS:
        case PlanningStrategy::FORWARD_SEARCH:
            result = stripsPlanning(tasks, out);
            out.goal_atom = goal_atom;
            break;
        case PlanningStrategy::BACKWARD_SEARCH:
            // same operator set; STRIPS BFS is bidirectional-ready; use same
            result = stripsPlanning(tasks, out);
            out.goal_atom = goal_atom;
            out.strategy_used = PlanningStrategy::BACKWARD_SEARCH;
            break;
        case PlanningStrategy::TEMPORAL_FIRST:
            result = temporalPlanning(goal_atom, out);
            break;
        case PlanningStrategy::RESOURCE_OPTIMAL:
            result = runHybrid();
            if (result == PlanResult::SUCCESS)
                out.strategy_used = PlanningStrategy::RESOURCE_OPTIMAL;
            break;
        case PlanningStrategy::HYBRID:
        default:
            result = runHybrid();
            break;
    }

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);

    _world_state = world_snapshot;

    if (elapsed > _planning_timeout * 10 && result != PlanResult::SUCCESS)
        return PlanResult::TIMEOUT;

    if (result == PlanResult::SUCCESS) {
        if (out.confidence < _min_confidence_threshold)
            return PlanResult::NO_SOLUTION;
        out.plan_atom = createPlanAtom(out);
        out.duration = elapsed;
        out.end_time = std::chrono::steady_clock::now();
        out.status = ExecutionStatus::NOT_STARTED;
        _active_plans[goal_atom] = out;
        recordPlanningMetrics(out, elapsed);
    }
    return result;
}

Handle PlanningEngine::createPlanAtom(const Plan& plan)
{
    std::string name = "Plan_" + goalName(plan.goal_atom) + "_" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    Handle plan_atom = _atomspace->add_node(CONCEPT_NODE, name);
    Handle pred = _atomspace->add_node(PREDICATE_NODE, "plan-for-goal");
    _atomspace->add_link(EVALUATION_LINK, {
        pred,
        _atomspace->add_link(LIST_LINK, {plan_atom, plan.goal_atom})
    });
    if (!plan.action_sequence.empty()) {
        Handle list = _atomspace->add_link(LIST_LINK, plan.action_sequence);
        Handle ap = _atomspace->add_node(PREDICATE_NODE, "plan-actions");
        _atomspace->add_link(EVALUATION_LINK, {
            ap,
            _atomspace->add_link(LIST_LINK, {plan_atom, list})
        });
    }
    plan_atom->setTruthValue(SimpleTruthValue::createTV(plan.confidence, 1.0));
    _atomspace->add_link(MEMBER_LINK, {plan_atom, _planning_context});
    return plan_atom;
}

void PlanningEngine::recordPlanningMetrics(const Plan& plan,
                                           std::chrono::milliseconds planning_time)
{
    ++_plans_generated;
    if (plan.confidence >= _min_confidence_threshold) ++_plans_successful;
    if (_plans_generated == 1)
        _average_planning_time = planning_time;
    else
        _average_planning_time = std::chrono::milliseconds(
            (_average_planning_time.count() * (_plans_generated - 1) +
             planning_time.count()) / _plans_generated);
    _last_planning_time = std::chrono::steady_clock::now();
}

PlanningEngine::PlanResult
PlanningEngine::createPlan(const Handle& goal_atom, PlanningStrategy strategy)
{
    if (!_initialized) initialize();
    Plan plan;
    return generatePlan(goal_atom, strategy, plan);
}

PlanningEngine::PlanResult
PlanningEngine::createPlanForTask(const std::string& task_name,
                                  PlanningStrategy strategy)
{
    if (!_initialized) initialize();
    Handle goal = _atomspace->add_node(CONCEPT_NODE, task_name);
    return createPlan(goal, strategy);
}

PlanningEngine::PlanResult
PlanningEngine::createTemporalPlan(const Handle& goal_atom,
                                   const std::chrono::steady_clock::time_point& deadline,
                                   PlanningStrategy strategy)
{
    if (!_initialized) initialize();
    if (_temporal_reasoner)
        _temporal_reasoner->addDeadlineConstraint(goal_atom, deadline);
    (void)strategy;
    return createPlan(goal_atom, PlanningStrategy::TEMPORAL_FIRST);
}

PlanningEngine::PlanResult
PlanningEngine::createResourceConstrainedPlan(
    const Handle& goal_atom,
    const std::vector<Handle>& available_resources,
    PlanningStrategy strategy)
{
    if (!_initialized) initialize();
    if (available_resources.empty()) {
        // still allow planning; resources optional
    }
    (void)strategy;
    return createPlan(goal_atom, PlanningStrategy::RESOURCE_OPTIMAL);
}

PlanningEngine::PlanResult PlanningEngine::adaptPlan(const Handle& plan_atom)
{
    for (auto& kv : _active_plans) {
        if (kv.second.plan_atom == plan_atom) {
            kv.second.status = ExecutionStatus::REPLANNING;
            ++kv.second.revision_count;
            Plan fresh;
            PlanResult r = generatePlan(kv.second.goal_atom, kv.second.strategy_used, fresh);
            if (r == PlanResult::SUCCESS) {
                fresh.revision_count = kv.second.revision_count;
                fresh.status = ExecutionStatus::NOT_STARTED;
                kv.second = fresh;
            } else {
                kv.second.status = ExecutionStatus::FAILED;
            }
            return r;
        }
    }
    return PlanResult::GOAL_INVALID;
}

bool PlanningEngine::cancelPlan(const Handle& plan_atom)
{
    for (auto& kv : _active_plans) {
        if (kv.second.plan_atom == plan_atom) {
            kv.second.status = ExecutionStatus::CANCELLED;
            return true;
        }
    }
    return false;
}

bool PlanningEngine::startExecution(const Handle& goal_atom)
{
    auto it = _active_plans.find(goal_atom);
    if (it == _active_plans.end()) return false;
    it->second.status = ExecutionStatus::EXECUTING;
    it->second.next_action_index = 0;
    return true;
}

PlanningEngine::PlanResult PlanningEngine::stepExecution(const Handle& goal_atom)
{
    auto it = _active_plans.find(goal_atom);
    if (it == _active_plans.end()) return PlanResult::GOAL_INVALID;
    Plan& plan = it->second;
    if (plan.status != ExecutionStatus::EXECUTING)
        return PlanResult::NO_SOLUTION;

    if (plan.next_action_index >= plan.action_names.size()) {
        plan.status = ExecutionStatus::COMPLETED;
        return PlanResult::SUCCESS;
    }

    const std::string& op_name = plan.action_names[plan.next_action_index];
    if (!applyOperatorEffects(op_name)) {
        // if operator unknown, still advance
    }
    ++plan.next_action_index;
    if (plan.next_action_index >= plan.action_names.size()) {
        plan.status = ExecutionStatus::COMPLETED;
        // mark goal satisfied in hierarchy if present
        if (_goal_hierarchy && _goal_hierarchy->hasGoal(goal_atom))
            _goal_hierarchy->setGoalStatus(goal_atom, GoalHierarchy::GoalStatus::SATISFIED);
    }
    return PlanResult::SUCCESS;
}

bool PlanningEngine::applyOperatorEffects(const std::string& operator_name)
{
    auto it = _operators.find(operator_name);
    if (it == _operators.end()) return false;
    if (!preconditionsMet(it->second.preconditions)) return false;
    for (const auto& d : it->second.delete_effects) _world_state.erase(d);
    for (const auto& a : it->second.add_effects) _world_state.insert(a);
    return true;
}

bool PlanningEngine::markActionCompleted(const Handle& plan_atom,
                                         const Handle& action_atom)
{
    for (auto& kv : _active_plans) {
        if (kv.second.plan_atom != plan_atom) continue;
        auto& seq = kv.second.action_sequence;
        auto pos = std::find(seq.begin(), seq.end(), action_atom);
        if (pos == seq.end()) return false;
        size_t idx = static_cast<size_t>(std::distance(seq.begin(), pos));
        if (idx == kv.second.next_action_index) {
            if (idx < kv.second.action_names.size())
                applyOperatorEffects(kv.second.action_names[idx]);
            ++kv.second.next_action_index;
            if (kv.second.next_action_index >= seq.size())
                kv.second.status = ExecutionStatus::COMPLETED;
            return true;
        }
        return false;
    }
    return false;
}

const PlanningEngine::Plan* PlanningEngine::getPlan(const Handle& goal_atom) const
{
    auto it = _active_plans.find(goal_atom);
    if (it == _active_plans.end()) return nullptr;
    return &it->second;
}

std::vector<PlanningEngine::Plan> PlanningEngine::getActivePlans() const
{
    std::vector<Plan> out;
    for (const auto& kv : _active_plans) {
        if (kv.second.status == ExecutionStatus::EXECUTING ||
            kv.second.status == ExecutionStatus::NOT_STARTED ||
            kv.second.status == ExecutionStatus::REPLANNING)
            out.push_back(kv.second);
    }
    return out;
}

PlanningEngine::ExecutionStatus
PlanningEngine::getPlanStatus(const Handle& plan_atom) const
{
    for (const auto& kv : _active_plans)
        if (kv.second.plan_atom == plan_atom) return kv.second.status;
    return ExecutionStatus::FAILED;
}

Handle PlanningEngine::getNextAction(const Handle& plan_atom) const
{
    for (const auto& kv : _active_plans) {
        if (kv.second.plan_atom != plan_atom) continue;
        if (kv.second.next_action_index >= kv.second.action_sequence.size())
            return Handle::UNDEFINED;
        return kv.second.action_sequence[kv.second.next_action_index];
    }
    return Handle::UNDEFINED;
}

float PlanningEngine::getPlanningSuccessRate() const
{
    if (_plans_generated == 0) return 0.0f;
    return 100.0f * static_cast<float>(_plans_successful) /
           static_cast<float>(_plans_generated);
}

PlanningEngine::PlanQualityMetrics
PlanningEngine::computePlanQuality(const Plan& plan) const
{
    PlanQualityMetrics m;
    m.action_count = static_cast<int>(plan.action_sequence.size());
    m.planning_time = plan.duration;
    m.average_action_confidence = plan.confidence;

    // Optimality: fewer actions is better (normalize vs max)
    if (_max_actions_per_plan > 0) {
        m.optimality_score = 1.0f - std::min(1.0f,
            static_cast<float>(m.action_count) / static_cast<float>(_max_actions_per_plan));
    }

    // Goal coverage: if goal fluent holds after simulating effects
    std::set<std::string> sim = _world_state;
    for (const auto& name : plan.action_names) {
        auto it = _operators.find(name);
        if (it == _operators.end()) continue;
        for (const auto& d : it->second.delete_effects) sim.erase(d);
        for (const auto& a : it->second.add_effects) sim.insert(a);
    }
    std::string gname = goalName(plan.goal_atom);
    if (!gname.empty() && sim.count(gname))
        m.goal_coverage = 1.0f;
    else if (!plan.action_sequence.empty())
        m.goal_coverage = 0.7f; // partial — plan exists
    else
        m.goal_coverage = 0.0f;

    // Temporal satisfaction
    if (_temporal_reasoner && !plan.action_sequence.empty()) {
        m.temporal_satisfaction =
            _temporal_reasoner->validateTemporalSchedule(plan.action_sequence) ? 1.0f : 0.4f;
    } else {
        m.temporal_satisfaction = 0.8f;
    }

    // Resource efficiency — shorter plans + higher confidence
    m.resource_efficiency = std::min(1.0f, plan.confidence *
        (m.action_count == 0 ? 1.0f : 1.0f / (0.25f * m.action_count + 0.75f)));

    // Robustness — prefer higher confidence and more alternatives (methods)
    m.robustness_score = std::min(1.0f, plan.confidence * 0.8f + 0.2f);

    return m;
}

void PlanningEngine::resetPerformanceStats()
{
    _plans_generated = 0;
    _plans_successful = 0;
    _average_planning_time = std::chrono::milliseconds{0};
}

std::string PlanningEngine::getPerformanceStats() const
{
    std::ostringstream oss;
    oss << "{\"plans_generated\":" << _plans_generated
        << ",\"success_rate\":" << getPlanningSuccessRate()
        << ",\"avg_ms\":" << _average_planning_time.count()
        << ",\"operators\":" << _operators.size()
        << ",\"methods\":" << _methods.size()
        << "}";
    return oss.str();
}

std::string PlanningEngine::strategyToString(PlanningStrategy s)
{
    switch (s) {
        case PlanningStrategy::HIERARCHICAL: return "hierarchical";
        case PlanningStrategy::STRIPS: return "strips";
        case PlanningStrategy::FORWARD_SEARCH: return "forward_search";
        case PlanningStrategy::BACKWARD_SEARCH: return "backward_search";
        case PlanningStrategy::HYBRID: return "hybrid";
        case PlanningStrategy::TEMPORAL_FIRST: return "temporal_first";
        case PlanningStrategy::RESOURCE_OPTIMAL: return "resource_optimal";
    }
    return "unknown";
}

std::string PlanningEngine::resultToString(PlanResult r)
{
    switch (r) {
        case PlanResult::SUCCESS: return "success";
        case PlanResult::NO_SOLUTION: return "no_solution";
        case PlanResult::TIMEOUT: return "timeout";
        case PlanResult::GOAL_INVALID: return "goal_invalid";
        case PlanResult::RESOURCES_UNAVAILABLE: return "resources_unavailable";
        case PlanResult::TEMPORAL_CONFLICT: return "temporal_conflict";
        case PlanResult::MEMORY_LIMIT: return "memory_limit";
        case PlanResult::PRECONDITION_FAILURE: return "precondition_failure";
    }
    return "unknown";
}
