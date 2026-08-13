/*
 * agentzero-core/src/ActionScheduler.cpp
 */
#include "opencog/agentzero/ActionScheduler.h"
#include "opencog/agentzero/ActionExecutor.h"
#include "opencog/agentzero/AgentZeroCore.h"

#include <opencog/atoms/atom_types/types.h>
#include <opencog/atoms/truthvalue/SimpleTruthValue.h>
#include <opencog/util/Logger.h>

#include <algorithm>
#include <sstream>

using namespace opencog;
using namespace opencog::agentzero;

ActionScheduler::ActionScheduler(AgentZeroCore* agent_core, AtomSpacePtr atomspace)
    : _agent_core(agent_core)
    , _atomspace(std::move(atomspace))
{
    if (!_atomspace) {
        throw std::invalid_argument("ActionScheduler requires a valid AtomSpace");
    }
    _schedule_root = _atomspace->add_node(CONCEPT_NODE, "ActionScheduler");
}

ActionScheduler::~ActionScheduler() = default;

void ActionScheduler::setExecutor(std::shared_ptr<ActionExecutor> executor)
{
    std::lock_guard<std::mutex> lock(_mu);
    _executor = std::move(executor);
}

ActionScheduler::ScheduleResult
ActionScheduler::scheduleAction(const Handle& action_atom,
                                std::chrono::steady_clock::time_point when,
                                int priority)
{
    if (!action_atom) return ScheduleResult::INVALID;
    std::lock_guard<std::mutex> lock(_mu);
    for (const auto& sa : _scheduled) {
        if (sa.action_atom == action_atom) return ScheduleResult::DUPLICATE;
    }
    ScheduledAction sa;
    sa.action_atom = action_atom;
    sa.scheduled_time = when;
    sa.priority = priority;
    sa.state_atom = makeScheduleAtom(sa);
    _scheduled.push_back(sa);
    return ScheduleResult::SCHEDULED;
}

ActionScheduler::ScheduleResult
ActionScheduler::scheduleActionAfter(const Handle& action_atom, int delay_ms, int priority)
{
    auto when = std::chrono::steady_clock::now() + std::chrono::milliseconds(delay_ms);
    return scheduleAction(action_atom, when, priority);
}

ActionScheduler::ScheduleResult
ActionScheduler::scheduleActionBefore(const Handle& action_atom,
                                      std::chrono::steady_clock::time_point deadline,
                                      int priority)
{
    if (!action_atom) return ScheduleResult::INVALID;
    std::lock_guard<std::mutex> lock(_mu);
    ScheduledAction sa;
    sa.action_atom = action_atom;
    sa.scheduled_time = std::chrono::steady_clock::now();
    sa.deadline = deadline;
    sa.priority = priority;
    sa.state_atom = makeScheduleAtom(sa);
    _scheduled.push_back(sa);
    return ScheduleResult::SCHEDULED;
}

ActionScheduler::ScheduleResult
ActionScheduler::schedulePeriodicAction(const Handle& action_atom, int period_ms,
                                        int repeat_count, int priority)
{
    if (!action_atom) return ScheduleResult::INVALID;
    std::lock_guard<std::mutex> lock(_mu);
    ScheduledAction sa;
    sa.action_atom = action_atom;
    sa.scheduled_time = std::chrono::steady_clock::now();
    sa.priority = priority;
    sa.is_periodic = true;
    sa.repeat_count = repeat_count;
    sa.period = std::chrono::milliseconds(period_ms);
    sa.state_atom = makeScheduleAtom(sa);
    _scheduled.push_back(sa);
    return ScheduleResult::SCHEDULED;
}

ActionScheduler::ScheduleResult
ActionScheduler::scheduleActionWithDependencies(const Handle& action_atom,
                                                const std::vector<Handle>& dependencies,
                                                int priority)
{
    if (!action_atom) return ScheduleResult::INVALID;
    std::lock_guard<std::mutex> lock(_mu);
    ScheduledAction sa;
    sa.action_atom = action_atom;
    sa.scheduled_time = std::chrono::steady_clock::now();
    sa.priority = priority;
    sa.dependencies = dependencies;
    sa.state_atom = makeScheduleAtom(sa);
    _scheduled.push_back(sa);
    return ScheduleResult::SCHEDULED;
}

ActionScheduler::ScheduleResult
ActionScheduler::scheduleActionWithResources(const Handle& action_atom,
                                             const std::vector<std::string>& resources,
                                             int priority)
{
    if (!action_atom) return ScheduleResult::INVALID;
    std::lock_guard<std::mutex> lock(_mu);
    ScheduledAction sa;
    sa.action_atom = action_atom;
    sa.scheduled_time = std::chrono::steady_clock::now();
    sa.priority = priority;
    sa.required_resources = resources;
    sa.state_atom = makeScheduleAtom(sa);
    _scheduled.push_back(sa);
    return ScheduleResult::SCHEDULED;
}

std::vector<ActionScheduler::ScheduledAction>
ActionScheduler::getScheduledActions() const
{
    std::lock_guard<std::mutex> lock(_mu);
    return _scheduled;
}

std::chrono::steady_clock::time_point
ActionScheduler::getNextActionTime() const
{
    std::lock_guard<std::mutex> lock(_mu);
    if (_scheduled.empty()) return std::chrono::steady_clock::time_point{};
    auto it = std::min_element(_scheduled.begin(), _scheduled.end(),
        [](const ScheduledAction& a, const ScheduledAction& b) {
            return a.scheduled_time < b.scheduled_time;
        });
    return it->scheduled_time;
}

int ActionScheduler::dispatchDueActions()
{
    auto now = std::chrono::steady_clock::now();
    std::vector<Handle> due;
    {
        std::lock_guard<std::mutex> lock(_mu);
        std::vector<ScheduledAction> remaining;
        for (auto& sa : _scheduled) {
            if (sa.scheduled_time <= now && dependenciesSatisfied(sa)) {
                due.push_back(sa.action_atom);
                if (sa.is_periodic && sa.repeat_count > 1) {
                    sa.repeat_count--;
                    sa.scheduled_time = now + sa.period;
                    remaining.push_back(sa);
                }
            } else {
                remaining.push_back(sa);
            }
        }
        _scheduled.swap(remaining);
    }
    int n = 0;
    std::shared_ptr<ActionExecutor> exec;
    {
        std::lock_guard<std::mutex> lock(_mu);
        exec = _executor;
    }
    if (exec) {
        for (const auto& h : due) {
            if (exec->executeAction(h)) ++n;
        }
        exec->processActionQueue();
    }
    return n;
}

bool ActionScheduler::cancelScheduled(const Handle& action_atom)
{
    std::lock_guard<std::mutex> lock(_mu);
    auto before = _scheduled.size();
    _scheduled.erase(std::remove_if(_scheduled.begin(), _scheduled.end(),
                        [&](const ScheduledAction& sa) { return sa.action_atom == action_atom; }),
                     _scheduled.end());
    return _scheduled.size() < before;
}

size_t ActionScheduler::scheduledCount() const
{
    std::lock_guard<std::mutex> lock(_mu);
    return _scheduled.size();
}

std::string ActionScheduler::getStatusInfo() const
{
    std::lock_guard<std::mutex> lock(_mu);
    std::ostringstream oss;
    oss << "{\"scheduled\":" << _scheduled.size() << "}";
    return oss.str();
}

Handle ActionScheduler::makeScheduleAtom(const ScheduledAction& sa)
{
    Handle pred = _atomspace->add_node(PREDICATE_NODE, "scheduled_action");
    Handle prio = _atomspace->add_node(NUMBER_NODE, std::to_string(sa.priority));
    Handle list = _atomspace->add_link(LIST_LINK, HandleSeq{sa.action_atom, prio, _schedule_root});
    Handle eval = _atomspace->add_link(EVALUATION_LINK, HandleSeq{pred, list});
    SimpleTruthValue::setTV(eval, 1.0, 0.9);
    Handle state = _atomspace->add_link(
        STATE_LINK,
        HandleSeq{sa.action_atom, _atomspace->add_node(CONCEPT_NODE, "scheduled")});
    SimpleTruthValue::setTV(state, 1.0, 1.0);
    return eval;
}

bool ActionScheduler::dependenciesSatisfied(const ScheduledAction& sa) const
{
    // Without an external completion oracle, treat empty deps as satisfied.
    // Non-empty deps are considered satisfied if corresponding STATE completed exists.
    if (sa.dependencies.empty()) return true;
    for (const auto& dep : sa.dependencies) {
        if (!dep) return false;
        // Best-effort: presence of a completed state link
        auto states = _atomspace->get_handles_by_type(STATE_LINK);
        bool ok = false;
        for (const auto& st : states) {
            if (!st || st->get_arity() < 2) continue;
            auto out = st->getOutgoingSet();
            if (out[0] == dep && out[1] && out[1]->is_node() &&
                out[1]->get_name() == "completed") {
                ok = true;
                break;
            }
        }
        if (!ok) return false;
    }
    return true;
}
