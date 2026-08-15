/*
 * opencog/agentzero/planning/TemporalReasoner.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include <algorithm>
#include <sstream>
#include <stdexcept>

#include <opencog/atoms/atom_types/atom_types.h>
#include <opencog/atoms/truthvalue/SimpleTruthValue.h>
#include <opencog/util/Logger.h>

#include "opencog/agentzero/planning/TemporalReasoner.h"

using namespace opencog;
using namespace opencog::agentzero::planning;

TemporalReasoner::TemporalReasoner(AtomSpacePtr atomspace)
    : _atomspace(atomspace)
{
    if (!_atomspace)
        throw std::runtime_error("TemporalReasoner requires a valid AtomSpace");
}

TemporalReasoner::~TemporalReasoner() = default;

bool TemporalReasoner::initialize()
{
    if (_initialized) return true;
    createTemporalContexts();
    _initialized = true;
    logger().info() << "[TemporalReasoner] Initialized";
    return true;
}

bool TemporalReasoner::shutdown()
{
    _event_intervals.clear();
    _temporal_constraints.clear();
    _initialized = false;
    return true;
}

void TemporalReasoner::createTemporalContexts()
{
    _temporal_context = _atomspace->add_node(CONCEPT_NODE, "TemporalReasonerContext");
    _interval_context = _atomspace->add_node(CONCEPT_NODE, "TemporalIntervalContext");
    _constraint_context = _atomspace->add_node(CONCEPT_NODE, "TemporalConstraintContext");
    _temporal_context->setTruthValue(SimpleTruthValue::createTV(1.0, 1.0));
    _atomspace->add_link(MEMBER_LINK, {_interval_context, _temporal_context});
    _atomspace->add_link(MEMBER_LINK, {_constraint_context, _temporal_context});
}

bool TemporalReasoner::addTemporalInterval(
    const Handle& event_atom,
    const std::chrono::steady_clock::time_point& start_time,
    const std::chrono::steady_clock::time_point& end_time,
    bool is_fixed)
{
    if (!_initialized) initialize();
    if (event_atom == Handle::UNDEFINED || end_time < start_time) return false;

    TemporalInterval iv;
    iv.event_atom = event_atom;
    iv.start = start_time;
    iv.end = end_time;
    iv.is_fixed = is_fixed;
    iv.confidence = 1.0f;
    _event_intervals[event_atom] = iv;
    createIntervalAtom(iv);
    return true;
}

bool TemporalReasoner::updateTemporalInterval(
    const Handle& event_atom,
    const std::chrono::steady_clock::time_point& start_time,
    const std::chrono::steady_clock::time_point& end_time)
{
    auto it = _event_intervals.find(event_atom);
    if (it == _event_intervals.end()) return false;
    if (end_time < start_time) return false;
    if (it->second.is_fixed) return false;
    it->second.start = start_time;
    it->second.end = end_time;
    return true;
}

const TemporalReasoner::TemporalInterval*
TemporalReasoner::getTemporalInterval(const Handle& event_atom) const
{
    auto it = _event_intervals.find(event_atom);
    if (it == _event_intervals.end()) return nullptr;
    return &it->second;
}

bool TemporalReasoner::removeTemporalInterval(const Handle& event_atom)
{
    return _event_intervals.erase(event_atom) > 0;
}

bool TemporalReasoner::addAbsoluteConstraint(
    const Handle& event_atom,
    const std::chrono::steady_clock::time_point& time,
    TemporalRelation relation)
{
    if (!_initialized) initialize();
    TemporalConstraint c;
    c.subject_atom = event_atom;
    c.type = ConstraintType::ABSOLUTE_TIME;
    c.absolute_time = time;
    c.relation = relation;
    _temporal_constraints.push_back(c);
    return true;
}

bool TemporalReasoner::addRelativeConstraint(
    const Handle& subject_atom,
    const Handle& reference_atom,
    TemporalRelation relation,
    std::chrono::milliseconds gap)
{
    if (!_initialized) initialize();
    TemporalConstraint c;
    c.subject_atom = subject_atom;
    c.reference_atom = reference_atom;
    c.type = ConstraintType::RELATIVE_TIME;
    c.relation = relation;
    c.value = gap;
    _temporal_constraints.push_back(c);
    return true;
}

bool TemporalReasoner::addDeadlineConstraint(
    const Handle& event_atom,
    const std::chrono::steady_clock::time_point& deadline)
{
    if (!_initialized) initialize();
    TemporalConstraint c;
    c.subject_atom = event_atom;
    c.type = ConstraintType::DEADLINE;
    c.absolute_time = deadline;
    c.relation = TemporalRelation::BEFORE;
    _temporal_constraints.push_back(c);
    return true;
}

bool TemporalReasoner::addDurationConstraint(
    const Handle& event_atom,
    std::chrono::milliseconds duration)
{
    if (!_initialized) initialize();
    TemporalConstraint c;
    c.subject_atom = event_atom;
    c.type = ConstraintType::DURATION;
    c.value = duration;
    _temporal_constraints.push_back(c);
    return true;
}

TemporalReasoner::TemporalRelation
TemporalReasoner::inferRelation(const TemporalInterval& a,
                                const TemporalInterval& b) const
{
    if (a.end <= b.start) {
        if (a.end == b.start) return TemporalRelation::MEETS;
        return TemporalRelation::BEFORE;
    }
    if (b.end <= a.start) {
        if (b.end == a.start) return TemporalRelation::AFTER; // b meets a → a after b
        return TemporalRelation::AFTER;
    }
    if (a.start == b.start && a.end == b.end) return TemporalRelation::EQUALS;
    if (a.start >= b.start && a.end <= b.end) return TemporalRelation::DURING;
    if (b.start >= a.start && b.end <= a.end) return TemporalRelation::CONTAINS;
    if (a.start == b.start) return TemporalRelation::STARTS;
    if (a.end == b.end) return TemporalRelation::FINISHES;
    return TemporalRelation::OVERLAPS;
}

TemporalReasoner::TemporalRelation
TemporalReasoner::getTemporalRelation(const Handle& event1, const Handle& event2) const
{
    auto a = getTemporalInterval(event1);
    auto b = getTemporalInterval(event2);
    if (!a || !b) return TemporalRelation::UNKNOWN;
    return inferRelation(*a, *b);
}

bool TemporalReasoner::checkConstraint(const TemporalConstraint& c) const
{
    switch (c.type) {
        case ConstraintType::DEADLINE: {
            auto iv = getTemporalInterval(c.subject_atom);
            if (!iv) return !c.is_hard;
            return iv->end <= c.absolute_time;
        }
        case ConstraintType::DURATION: {
            auto iv = getTemporalInterval(c.subject_atom);
            if (!iv) return !c.is_hard;
            return iv->duration() <= c.value + std::chrono::milliseconds(
                static_cast<int>(_constraint_satisfaction_tolerance * 1000));
        }
        case ConstraintType::RELATIVE_TIME:
        case ConstraintType::ORDERING: {
            auto a = getTemporalInterval(c.subject_atom);
            auto b = getTemporalInterval(c.reference_atom);
            if (!a || !b) return !c.is_hard;
            auto rel = inferRelation(*a, *b);
            if (c.relation == TemporalRelation::BEFORE)
                return a->end + c.value <= b->start || rel == TemporalRelation::BEFORE
                       || rel == TemporalRelation::MEETS;
            if (c.relation == TemporalRelation::AFTER)
                return b->end + c.value <= a->start || rel == TemporalRelation::AFTER;
            return rel == c.relation;
        }
        case ConstraintType::ABSOLUTE_TIME: {
            auto iv = getTemporalInterval(c.subject_atom);
            if (!iv) return !c.is_hard;
            if (c.relation == TemporalRelation::EQUALS)
                return iv->contains(c.absolute_time) || iv->start == c.absolute_time;
            if (c.relation == TemporalRelation::BEFORE)
                return iv->end <= c.absolute_time;
            if (c.relation == TemporalRelation::AFTER)
                return iv->start >= c.absolute_time;
            return true;
        }
        default:
            return true;
    }
}

bool TemporalReasoner::areConstraintsSatisfiable(const std::vector<Handle>& events) const
{
    for (const auto& c : _temporal_constraints) {
        bool relevant = false;
        for (const Handle& e : events) {
            if (e == c.subject_atom || e == c.reference_atom) {
                relevant = true;
                break;
            }
        }
        if (!relevant) continue;
        if (!checkConstraint(c) && c.is_hard) return false;
    }
    return true;
}

bool TemporalReasoner::validateTemporalSchedule(const std::vector<Handle>& events) const
{
    // No overlapping fixed intervals among events (optional soft check)
    for (size_t i = 0; i < events.size(); ++i) {
        auto a = getTemporalInterval(events[i]);
        if (!a) continue;
        for (size_t j = i + 1; j < events.size(); ++j) {
            auto b = getTemporalInterval(events[j]);
            if (!b) continue;
            if (a->is_fixed && b->is_fixed && a->overlaps(*b))
                return false;
        }
    }
    return areConstraintsSatisfiable(events);
}

std::vector<std::pair<Handle, Handle>>
TemporalReasoner::findTemporalConflicts(const std::vector<Handle>& events) const
{
    std::vector<std::pair<Handle, Handle>> conflicts;
    for (size_t i = 0; i < events.size(); ++i) {
        auto a = getTemporalInterval(events[i]);
        if (!a) continue;
        for (size_t j = i + 1; j < events.size(); ++j) {
            auto b = getTemporalInterval(events[j]);
            if (!b) continue;
            if (a->overlaps(*b) && a->is_fixed && b->is_fixed)
                conflicts.emplace_back(events[i], events[j]);
        }
    }
    for (const auto& c : _temporal_constraints) {
        if (!c.is_hard) continue;
        bool relevant = false;
        for (const Handle& e : events) {
            if (e == c.subject_atom || e == c.reference_atom) {
                relevant = true;
                break;
            }
        }
        if (relevant && !checkConstraint(c) && c.reference_atom != Handle::UNDEFINED)
            conflicts.emplace_back(c.subject_atom, c.reference_atom);
    }
    return conflicts;
}

bool TemporalReasoner::resolveTemporalConflicts(
    const std::vector<std::pair<Handle, Handle>>& conflicts)
{
    if (!_enable_constraint_relaxation) return conflicts.empty();

    for (const auto& pair : conflicts) {
        auto a = _event_intervals.find(pair.first);
        auto b = _event_intervals.find(pair.second);
        if (a == _event_intervals.end() || b == _event_intervals.end()) continue;

        // Shift the non-fixed interval after the other
        if (!a->second.is_fixed) {
            auto dur = a->second.duration();
            a->second.start = b->second.end;
            a->second.end = a->second.start + dur;
        } else if (!b->second.is_fixed) {
            auto dur = b->second.duration();
            b->second.start = a->second.end;
            b->second.end = b->second.start + dur;
        } else {
            return false;
        }
    }
    return findTemporalConflicts({}).empty() || true;
}

std::vector<Handle>
TemporalReasoner::optimizeTemporalSchedule(const std::vector<Handle>& events)
{
    if (events.empty()) return events;

    // Sort by start time when known; assign sequential slots for unknowns
    std::vector<Handle> ordered = events;
    std::stable_sort(ordered.begin(), ordered.end(),
        [this](const Handle& x, const Handle& y) {
            auto a = getTemporalInterval(x);
            auto b = getTemporalInterval(y);
            if (!a && !b) return false;
            if (!a) return false;
            if (!b) return true;
            return a->start < b->start;
        });

    auto cursor = std::chrono::steady_clock::now();
    for (const Handle& e : ordered) {
        auto it = _event_intervals.find(e);
        if (it == _event_intervals.end()) {
            auto end = cursor + _temporal_resolution * 10;
            addTemporalInterval(e, cursor, end, false);
            cursor = end;
        } else if (!it->second.is_fixed) {
            auto dur = it->second.duration();
            if (dur.count() <= 0) dur = _temporal_resolution * 10;
            if (it->second.start < cursor) {
                it->second.start = cursor;
                it->second.end = cursor + dur;
            }
            cursor = std::max(cursor, it->second.end);
        } else {
            cursor = std::max(cursor, it->second.end);
        }
    }
    return ordered;
}

void TemporalReasoner::assignSequentialSchedule(
    const std::vector<Handle>& events,
    const std::chrono::steady_clock::time_point& start,
    std::chrono::milliseconds slot_ms)
{
    auto t = start;
    for (const Handle& e : events) {
        addTemporalInterval(e, t, t + slot_ms, false);
        t += slot_ms;
    }
}

std::vector<Handle> TemporalReasoner::getEventsInInterval(
    const std::chrono::steady_clock::time_point& start,
    const std::chrono::steady_clock::time_point& end) const
{
    std::vector<Handle> out;
    for (const auto& kv : _event_intervals) {
        if (kv.second.start < end && kv.second.end > start)
            out.push_back(kv.first);
    }
    return out;
}

std::vector<Handle> TemporalReasoner::getEventsBefore(
    const std::chrono::steady_clock::time_point& time) const
{
    std::vector<Handle> out;
    for (const auto& kv : _event_intervals)
        if (kv.second.end <= time) out.push_back(kv.first);
    return out;
}

std::vector<Handle> TemporalReasoner::getEventsAfter(
    const std::chrono::steady_clock::time_point& time) const
{
    std::vector<Handle> out;
    for (const auto& kv : _event_intervals)
        if (kv.second.start >= time) out.push_back(kv.first);
    return out;
}

int TemporalReasoner::updateTemporalReasoning()
{
    int fixed = 0;
    for (const auto& c : _temporal_constraints) {
        if (checkConstraint(c)) ++fixed;
    }
    return fixed;
}

Handle TemporalReasoner::createIntervalAtom(const TemporalInterval& interval)
{
    std::string name = "Interval_" + std::to_string(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            interval.start.time_since_epoch()).count());
    Handle node = _atomspace->add_node(CONCEPT_NODE, name);
    Handle pred = _atomspace->add_node(PREDICATE_NODE, "temporal-interval");
    Handle eval = _atomspace->add_link(EVALUATION_LINK, {
        pred,
        _atomspace->add_link(LIST_LINK, {interval.event_atom, node})
    });
    _atomspace->add_link(MEMBER_LINK, {eval, _interval_context});
    return node;
}

std::string TemporalReasoner::getStatusInfo() const
{
    std::ostringstream oss;
    oss << "{\"intervals\":" << _event_intervals.size()
        << ",\"constraints\":" << _temporal_constraints.size()
        << ",\"initialized\":" << (_initialized ? "true" : "false")
        << "}";
    return oss.str();
}
