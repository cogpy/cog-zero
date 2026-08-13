/*
 * agentzero-core/src/ActionExecutor.cpp
 */
#include "opencog/agentzero/ActionExecutor.h"
#include "opencog/agentzero/ActionScheduler.h"
#include "opencog/agentzero/AgentZeroCore.h"

#include <opencog/atoms/atom_types/types.h>
#include <opencog/atoms/truthvalue/SimpleTruthValue.h>
#include <opencog/util/Logger.h>

#include <algorithm>
#include <sstream>

using namespace opencog;
using namespace opencog::agentzero;

ActionExecutor::ActionExecutor(AgentZeroCore* agent_core, AtomSpacePtr atomspace)
    : _agent_core(agent_core)
    , _atomspace(std::move(atomspace))
{
    if (!_atomspace) {
        throw std::invalid_argument("ActionExecutor requires a valid AtomSpace");
    }
    _actions_root = _atomspace->add_node(CONCEPT_NODE, "ActionExecutor");

    // Default "basic" action
    registerAction("basic", [this](const Handle& action_atom,
                                   const std::map<std::string, Handle>& params) {
        return runDefaultAction(action_atom, params);
    });
}

ActionExecutor::~ActionExecutor() = default;

void ActionExecutor::setScheduler(std::shared_ptr<ActionScheduler> scheduler)
{
    std::lock_guard<std::mutex> lock(_mu);
    _scheduler = std::move(scheduler);
}

bool ActionExecutor::executeAction(const Handle& action_atom, Priority priority)
{
    if (!action_atom) return false;
    std::lock_guard<std::mutex> lock(_mu);
    _pending.push_back(action_atom);
    _priorities[action_atom] = priority;
    ActionResult r;
    r.status = ActionStatus::PENDING;
    r.message = "queued";
    _results[action_atom] = r;
    updateStatusAtom(action_atom, ActionStatus::PENDING);
    // Keep higher priority first
    std::stable_sort(_pending.begin(), _pending.end(), [this](const Handle& a, const Handle& b) {
        return static_cast<int>(_priorities[a]) > static_cast<int>(_priorities[b]);
    });
    return true;
}

ActionExecutor::ActionResult
ActionExecutor::executeActionSync(const Handle& action_atom, int /*timeout_ms*/)
{
    auto t0 = std::chrono::steady_clock::now();
    ActionResult result = runDefaultAction(action_atom, {});
    auto t1 = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0);
    if (result.duration.count() <= 0) result.duration = std::chrono::milliseconds(1);
    {
        std::lock_guard<std::mutex> lock(_mu);
        _results[action_atom] = result;
    }
    updateStatusAtom(action_atom, result.status);
    return result;
}

Handle ActionExecutor::executeSimpleAction(const std::string& action_name,
                                           const std::map<std::string, Handle>& params)
{
    Handle action_atom = _atomspace->add_node(CONCEPT_NODE, "Action:" + action_name);
    Handle pred = _atomspace->add_node(PREDICATE_NODE, "execute");
    HandleSeq args;
    args.push_back(action_atom);
    for (const auto& kv : params) {
        args.push_back(kv.second);
    }
    Handle list = _atomspace->add_link(LIST_LINK, args);
    Handle exec = _atomspace->add_link(EXECUTION_LINK, HandleSeq{pred, list});
    SimpleTruthValue::setTV(exec, 1.0, 0.9);
    executeAction(action_atom, Priority::MEDIUM);
    {
        std::lock_guard<std::mutex> lock(_mu);
        // stash params via result placeholder
        _results[action_atom].message = action_name;
    }
    return action_atom;
}

bool ActionExecutor::registerAction(const std::string& name, ActionCallback callback)
{
    std::lock_guard<std::mutex> lock(_mu);
    _registry[name] = std::move(callback);
    return true;
}

bool ActionExecutor::isActionRegistered(const std::string& name) const
{
    std::lock_guard<std::mutex> lock(_mu);
    return _registry.find(name) != _registry.end();
}

bool ActionExecutor::cancelAction(const Handle& action_atom)
{
    std::lock_guard<std::mutex> lock(_mu);
    auto it = std::find(_pending.begin(), _pending.end(), action_atom);
    if (it != _pending.end()) _pending.erase(it);
    ActionResult r;
    r.status = ActionStatus::CANCELLED;
    r.message = "cancelled";
    _results[action_atom] = r;
    updateStatusAtom(action_atom, ActionStatus::CANCELLED);
    return true;
}

ActionExecutor::ActionStatus
ActionExecutor::getActionStatus(const Handle& action_atom) const
{
    std::lock_guard<std::mutex> lock(_mu);
    auto it = _results.find(action_atom);
    if (it == _results.end()) return ActionStatus::PENDING;
    return it->second.status;
}

ActionExecutor::ActionResult
ActionExecutor::getActionResult(const Handle& action_atom) const
{
    std::lock_guard<std::mutex> lock(_mu);
    auto it = _results.find(action_atom);
    if (it == _results.end()) return ActionResult{};
    return it->second;
}

std::vector<Handle> ActionExecutor::getPendingActions() const
{
    std::lock_guard<std::mutex> lock(_mu);
    return _pending;
}

std::vector<Handle> ActionExecutor::getExecutingActions() const
{
    std::lock_guard<std::mutex> lock(_mu);
    return _executing;
}

int ActionExecutor::processActionQueue()
{
    std::vector<Handle> to_run;
    {
        std::lock_guard<std::mutex> lock(_mu);
        while (!_pending.empty() && _executing.size() + to_run.size() < _max_concurrent) {
            to_run.push_back(_pending.front());
            _pending.erase(_pending.begin());
        }
        for (const auto& h : to_run) {
            _executing.push_back(h);
            _start_times[h] = std::chrono::steady_clock::now();
            ActionResult r;
            r.status = ActionStatus::RUNNING;
            r.message = "running";
            _results[h] = r;
        }
    }
    for (const auto& h : to_run) {
        updateStatusAtom(h, ActionStatus::RUNNING);
        std::string action_name;
        {
            std::lock_guard<std::mutex> lock(_mu);
            action_name = _results[h].message;
        }
        ActionCallback cb;
        {
            std::lock_guard<std::mutex> lock(_mu);
            // Prefer registered callback by name if message holds name
            auto it = _registry.find(action_name);
            if (it != _registry.end()) cb = it->second;
            else if (!_registry.empty()) cb = _registry.begin()->second;
        }
        ActionResult result;
        auto t0 = std::chrono::steady_clock::now();
        if (cb) result = cb(h, {});
        else result = runDefaultAction(h, {});
        auto t1 = std::chrono::steady_clock::now();
        result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0);
        {
            std::lock_guard<std::mutex> lock(_mu);
            _results[h] = result;
            _executing.erase(std::remove(_executing.begin(), _executing.end(), h), _executing.end());
            _start_times.erase(h);
        }
        updateStatusAtom(h, result.status);
    }
    return static_cast<int>(to_run.size());
}

int ActionExecutor::monitorExecutingActions()
{
    int changes = 0;
    auto now = std::chrono::steady_clock::now();
    std::vector<Handle> timed_out;
    {
        std::lock_guard<std::mutex> lock(_mu);
        for (const auto& h : _executing) {
            auto it = _start_times.find(h);
            if (it == _start_times.end()) continue;
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second).count();
            if (ms > _default_timeout_ms) {
                timed_out.push_back(h);
            }
        }
        for (const auto& h : timed_out) {
            ActionResult r;
            r.status = ActionStatus::TIMEOUT;
            r.message = "timeout";
            _results[h] = r;
            _executing.erase(std::remove(_executing.begin(), _executing.end(), h), _executing.end());
            _start_times.erase(h);
            ++changes;
        }
    }
    for (const auto& h : timed_out) updateStatusAtom(h, ActionStatus::TIMEOUT);
    return changes;
}

std::string ActionExecutor::getStatusInfo() const
{
    std::lock_guard<std::mutex> lock(_mu);
    std::ostringstream oss;
    oss << "{\"action_queue_size\":" << _pending.size()
        << ",\"max_concurrent_actions\":" << _max_concurrent
        << ",\"executing_actions\":" << _executing.size()
        << ",\"registered_actions\":" << _registry.size() << "}";
    return oss.str();
}

void ActionExecutor::updateStatusAtom(const Handle& action_atom, ActionStatus status)
{
    if (!_atomspace || !action_atom) return;
    std::string name = "pending";
    switch (status) {
        case ActionStatus::PENDING: name = "pending"; break;
        case ActionStatus::RUNNING: name = "running"; break;
        case ActionStatus::COMPLETED: name = "completed"; break;
        case ActionStatus::FAILED: name = "failed"; break;
        case ActionStatus::CANCELLED: name = "cancelled"; break;
        case ActionStatus::TIMEOUT: name = "timeout"; break;
    }
    Handle sn = _atomspace->add_node(CONCEPT_NODE, name);
    Handle state = _atomspace->add_link(STATE_LINK, HandleSeq{action_atom, sn});
    SimpleTruthValue::setTV(state, 1.0, 1.0);
    Handle pred = _atomspace->add_node(PREDICATE_NODE, "action_status");
    Handle eval = _atomspace->add_link(
        EVALUATION_LINK,
        HandleSeq{pred, _atomspace->add_link(LIST_LINK, HandleSeq{action_atom, sn, _actions_root})});
    SimpleTruthValue::setTV(eval, 1.0, 1.0);
}

ActionExecutor::ActionResult
ActionExecutor::runDefaultAction(const Handle& action_atom,
                                 const std::map<std::string, Handle>& /*params*/)
{
    ActionResult r;
    r.status = ActionStatus::COMPLETED;
    r.message = action_atom && action_atom->is_node()
                    ? ("executed:" + action_atom->get_name())
                    : "executed";
    r.success_probability = 0.9;
    r.result_atom = action_atom;
    if (_atomspace && action_atom) {
        Handle pred = _atomspace->add_node(PREDICATE_NODE, "action_result");
        Handle eval = _atomspace->add_link(
            EVALUATION_LINK,
            HandleSeq{pred, _atomspace->add_link(LIST_LINK, HandleSeq{action_atom})});
        SimpleTruthValue::setTV(eval, r.success_probability, 0.9);
    }
    return r;
}
