/*
 * opencog/agentzero/TemporalReasoner.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Temporal Reasoning for Planning Implementation
 * Part of AZ-PLAN-002: Create PlanningEngine with temporal reasoning
 */

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

TemporalReasoner::TemporalReasoner(AtomSpacePtr atomspace)
    : _atomspace(atomspace)
#ifdef HAVE_SPACETIME
    , _time_server(nullptr)
#endif
    , _temporal_resolution(std::chrono::milliseconds(100))
    , _planning_horizon(std::chrono::milliseconds(300000))  // 5 minutes default
    , _enable_temporal_propagation(true)
    , _enable_constraint_relaxation(false)
    , _constraint_satisfaction_tolerance(0.1f)
{
    if (!_atomspace) {
        throw std::invalid_argument("TemporalReasoner requires valid AtomSpace");
    }
    
    initializeTemporalReasoner();
    logger().info("TemporalReasoner: Initialized temporal reasoning system");
}

TemporalReasoner::~TemporalReasoner()
{
    logger().info("TemporalReasoner: Destroyed - %d temporal intervals managed", 
                 (int)_event_intervals.size());
}

void TemporalReasoner::initializeTemporalReasoner()
{
    createTemporalContexts();
    logger().debug("TemporalReasoner: Initialization complete");
}

void TemporalReasoner::createTemporalContexts()
{
    // Create AtomSpace contexts for organizing temporal knowledge
    _temporal_context = _atomspace->add_node(CONCEPT_NODE, "TemporalReasoningContext");
    _interval_context = _atomspace->add_node(CONCEPT_NODE, "TemporalIntervalContext");
    _constraint_context = _atomspace->add_node(CONCEPT_NODE, "TemporalConstraintContext");
    _relation_context = _atomspace->add_node(CONCEPT_NODE, "TemporalRelationContext");
    
    // Set initial truth values
    _temporal_context->setTruthValue(SimpleTruthValue::createTV(1.0, 1.0));
    _interval_context->setTruthValue(SimpleTruthValue::createTV(1.0, 1.0));
    _constraint_context->setTruthValue(SimpleTruthValue::createTV(1.0, 1.0));
    _relation_context->setTruthValue(SimpleTruthValue::createTV(1.0, 1.0));
    
    logger().debug("TemporalReasoner: Created temporal reasoning contexts");
}

bool TemporalReasoner::addTemporalInterval(const Handle& event_atom,
                                         const std::chrono::steady_clock::time_point& start_time,
                                         const std::chrono::steady_clock::time_point& end_time,
                                         bool is_fixed)
{
    if (event_atom == Handle::UNDEFINED || !_atomspace->is_valid_handle(event_atom)) {
        logger().warn("TemporalReasoner: Invalid event atom for temporal interval");
        return false;
    }
    
    if (start_time >= end_time) {
        logger().warn("TemporalReasoner: Invalid temporal interval - start must be before end");
        return false;
    }
    
    TemporalInterval interval;
    interval.start = start_time;
    interval.end = end_time;
    interval.event_atom = event_atom;
    interval.is_fixed = is_fixed;
    interval.confidence = 1.0f;
    
    _event_intervals[event_atom] = interval;
    
    // Create AtomSpace representation
    createTemporalAtom(interval);
    
    logger().debug("TemporalReasoner: Added temporal interval for event (duration: %d ms, fixed: %s)",
                  (int)interval.duration().count(), is_fixed ? "true" : "false");
    
    return true;
}

bool TemporalReasoner::updateTemporalInterval(const Handle& event_atom,
                                            const std::chrono::steady_clock::time_point& start_time,
                                            const std::chrono::steady_clock::time_point& end_time)
{
    auto it = _event_intervals.find(event_atom);
    if (it == _event_intervals.end()) {
        logger().warn("TemporalReasoner: Event not found for interval update");
        return false;
    }
    
    if (it->second.is_fixed) {
        logger().debug("TemporalReasoner: Cannot update fixed temporal interval");
        return false;
    }
    
    if (start_time >= end_time) {
        logger().warn("TemporalReasoner: Invalid temporal interval update");
        return false;
    }
    
    it->second.start = start_time;
    it->second.end = end_time;
    
    // Update AtomSpace representation
    createTemporalAtom(it->second);
    
    logger().debug("TemporalReasoner: Updated temporal interval (new duration: %d ms)",
                  (int)it->second.duration().count());
    
    return true;
}

const TemporalReasoner::TemporalInterval* TemporalReasoner::getTemporalInterval(const Handle& event_atom) const
{
    auto it = _event_intervals.find(event_atom);
    return (it != _event_intervals.end()) ? &it->second : nullptr;
}

bool TemporalReasoner::removeTemporalInterval(const Handle& event_atom)
{
    auto it = _event_intervals.find(event_atom);
    if (it == _event_intervals.end()) {
        return false;
    }
    
    _event_intervals.erase(it);
    logger().debug("TemporalReasoner: Removed temporal interval for event");
    return true;
}

bool TemporalReasoner::addAbsoluteConstraint(const Handle& event_atom,
                                           const std::chrono::steady_clock::time_point& absolute_time,
                                           float priority,
                                           bool is_hard)
{
    if (event_atom == Handle::UNDEFINED || !_atomspace->is_valid_handle(event_atom)) {
        return false;
    }
    
    TemporalConstraint constraint;
    constraint.subject_atom = event_atom;
    constraint.type = ConstraintType::ABSOLUTE_TIME;
    constraint.absolute_time = absolute_time;
    constraint.priority = std::max(0.0f, std::min(1.0f, priority));
    constraint.is_hard = is_hard;
    
    _temporal_constraints.push_back(constraint);
    
    // Create AtomSpace representation
    createConstraintAtom(constraint);
    
    logger().debug("TemporalReasoner: Added absolute temporal constraint (priority: %.2f, hard: %s)",
                  priority, is_hard ? "true" : "false");
    
    return true;
}

bool TemporalReasoner::addRelativeConstraint(const Handle& subject_atom,
                                           const Handle& reference_atom,
                                           TemporalRelation relation,
                                           std::chrono::milliseconds delay,
                                           float priority,
                                           bool is_hard)
{
    if (subject_atom == Handle::UNDEFINED || reference_atom == Handle::UNDEFINED ||
        !_atomspace->is_valid_handle(subject_atom) || !_atomspace->is_valid_handle(reference_atom)) {
        return false;
    }
    
    TemporalConstraint constraint;
    constraint.subject_atom = subject_atom;
    constraint.reference_atom = reference_atom;
    constraint.type = ConstraintType::RELATIVE_TIME;
    constraint.relation = relation;
    constraint.value = delay;
    constraint.priority = std::max(0.0f, std::min(1.0f, priority));
    constraint.is_hard = is_hard;
    
    _temporal_constraints.push_back(constraint);
    
    // Update temporal relations
    _temporal_relations[subject_atom].push_back(reference_atom);
    
    // Create AtomSpace representation
    createConstraintAtom(constraint);
    
    logger().debug("TemporalReasoner: Added relative temporal constraint (relation: %d, delay: %d ms)",
                  (int)relation, (int)delay.count());
    
    return true;
}

bool TemporalReasoner::addDeadlineConstraint(const Handle& event_atom,
                                           const std::chrono::steady_clock::time_point& deadline,
                                           float priority,
                                           bool is_hard)
{
    if (event_atom == Handle::UNDEFINED || !_atomspace->is_valid_handle(event_atom)) {
        return false;
    }
    
    TemporalConstraint constraint;
    constraint.subject_atom = event_atom;
    constraint.type = ConstraintType::DEADLINE;
    constraint.absolute_time = deadline;
    constraint.priority = std::max(0.0f, std::min(1.0f, priority));
    constraint.is_hard = is_hard;
    
    _temporal_constraints.push_back(constraint);
    
    // Create AtomSpace representation
    createConstraintAtom(constraint);
    
    logger().debug("TemporalReasoner: Added deadline constraint");
    
    return true;
}

bool TemporalReasoner::areConstraintsSatisfiable(const std::vector<Handle>& events)
{
    // Check each constraint against the current schedule
    for (const auto& constraint : _temporal_constraints) {
        // Skip constraints not relevant to this event set
        if (std::find(events.begin(), events.end(), constraint.subject_atom) == events.end()) {
            continue;
        }
        
        if (!satisfiesTemporalConstraints(events)) {
            return false;
        }
    }
    
    return true;
}

bool TemporalReasoner::validateTemporalSchedule(const std::vector<Handle>& events)
{
    // Check for temporal conflicts
    auto conflicts = findTemporalConflicts(events);
    if (!conflicts.empty()) {
        logger().debug("TemporalReasoner: Found %d temporal conflicts in schedule", 
                      (int)conflicts.size());
        return false;
    }
    
    // Validate all constraints
    return areConstraintsSatisfiable(events);
}

std::vector<Handle> TemporalReasoner::optimizeTemporalSchedule(const std::vector<Handle>& events)
{
    std::vector<Handle> optimized_events = events;
    
    // Sort events by their temporal intervals
    std::sort(optimized_events.begin(), optimized_events.end(),
              [this](const Handle& a, const Handle& b) {
                  auto interval_a = getTemporalInterval(a);
                  auto interval_b = getTemporalInterval(b);
                  
                  if (interval_a && interval_b) {
                      return interval_a->start < interval_b->start;
                  }
                  return false;
              });
    
    // Apply temporal optimization algorithms
    std::vector<TemporalInterval> intervals;
    for (const Handle& event : optimized_events) {
        auto interval = getTemporalInterval(event);
        if (interval) {
            intervals.push_back(*interval);
        }
    }
    
    auto optimized_intervals = optimizeTemporalSchedule(intervals);
    
    // Update intervals with optimized timing
    for (size_t i = 0; i < optimized_events.size() && i < optimized_intervals.size(); ++i) {
        updateTemporalInterval(optimized_events[i], 
                             optimized_intervals[i].start,
                             optimized_intervals[i].end);
    }
    
    logger().debug("TemporalReasoner: Optimized temporal schedule for %d events", 
                  (int)optimized_events.size());
    
    return optimized_events;
}

std::vector<std::pair<Handle, Handle>> TemporalReasoner::findTemporalConflicts(const std::vector<Handle>& events)
{
    std::vector<std::pair<Handle, Handle>> conflicts;
    
    // Check for overlapping intervals that shouldn't overlap
    for (size_t i = 0; i < events.size(); ++i) {
        for (size_t j = i + 1; j < events.size(); ++j) {
            const Handle& event1 = events[i];
            const Handle& event2 = events[j];
            
            auto interval1 = getTemporalInterval(event1);
            auto interval2 = getTemporalInterval(event2);
            
            if (interval1 && interval2) {
                // Check if intervals overlap when they shouldn't
                if (interval1->overlaps(*interval2)) {
                    // Check if this overlap violates any constraints
                    auto relation = getTemporalRelation(event1, event2);
                    if (relation == TemporalRelation::BEFORE || relation == TemporalRelation::AFTER) {
                        conflicts.emplace_back(event1, event2);
                    }
                }
            }
        }
    }
    
    return conflicts;
}

bool TemporalReasoner::resolveTemporalConflicts(const std::vector<std::pair<Handle, Handle>>& conflicts)
{
    bool all_resolved = true;
    
    for (const auto& [event1, event2] : conflicts) {
        auto interval1 = getTemporalInterval(event1);
        auto interval2 = getTemporalInterval(event2);
        
        if (!interval1 || !interval2) {
            continue;
        }
        
        // Simple conflict resolution: adjust non-fixed intervals
        if (!interval1->is_fixed && interval2->is_fixed) {
            // Move event1 to after event2
            auto new_start = interval2->end + _temporal_resolution;
            auto duration = interval1->duration();
            updateTemporalInterval(event1, new_start, new_start + duration);
            
            logger().debug("TemporalReasoner: Resolved conflict by moving event1 after event2");
        } else if (interval1->is_fixed && !interval2->is_fixed) {
            // Move event2 to after event1
            auto new_start = interval1->end + _temporal_resolution;
            auto duration = interval2->duration();
            updateTemporalInterval(event2, new_start, new_start + duration);
            
            logger().debug("TemporalReasoner: Resolved conflict by moving event2 after event1");
        } else if (!interval1->is_fixed && !interval2->is_fixed) {
            // Both are flexible - apply constraint-based resolution
            auto relation = getTemporalRelation(event1, event2);
            if (relation == TemporalRelation::BEFORE) {
                // Ensure event1 is before event2
                auto duration1 = interval1->duration();
                auto duration2 = interval2->duration();
                auto total_duration = duration1 + duration2 + _temporal_resolution;
                
                auto mid_point = interval1->start + (interval2->end - interval1->start) / 2;
                updateTemporalInterval(event1, mid_point - total_duration / 2, 
                                     mid_point - total_duration / 2 + duration1);
                updateTemporalInterval(event2, mid_point + total_duration / 2 - duration2,
                                     mid_point + total_duration / 2);
                
                logger().debug("TemporalReasoner: Resolved flexible conflict with BEFORE relation");
            }
        } else {
            // Both are fixed - cannot resolve
            logger().warn("TemporalReasoner: Cannot resolve conflict between fixed intervals");
            all_resolved = false;
        }
    }
    
    return all_resolved;
}

TemporalReasoner::TemporalRelation TemporalReasoner::getTemporalRelation(const Handle& event1, const Handle& event2)
{
    // Check explicit relations first
    auto it = _temporal_relations.find(event1);
    if (it != _temporal_relations.end()) {
        for (const Handle& related : it->second) {
            if (related == event2) {
                // Find the specific relation from constraints
                for (const auto& constraint : _temporal_constraints) {
                    if (constraint.subject_atom == event1 && constraint.reference_atom == event2) {
                        return constraint.relation;
                    }
                }
            }
        }
    }
    
    // Infer relation from temporal intervals
    return inferTemporalRelation(event1, event2);
}

TemporalReasoner::TemporalRelation TemporalReasoner::inferTemporalRelation(const Handle& event1, const Handle& event2)
{
    auto interval1 = getTemporalInterval(event1);
    auto interval2 = getTemporalInterval(event2);
    
    if (!interval1 || !interval2) {
        return TemporalRelation::BEFORE;  // Default assumption
    }
    
    // Apply Allen's interval algebra
    if (interval1->end <= interval2->start) {
        return TemporalRelation::BEFORE;
    } else if (interval2->end <= interval1->start) {
        return TemporalRelation::AFTER;
    } else if (interval1->start == interval2->start && interval1->end == interval2->end) {
        return TemporalRelation::EQUALS;
    } else if (interval1->start <= interval2->start && interval1->end >= interval2->end) {
        return TemporalRelation::CONTAINS;
    } else if (interval2->start <= interval1->start && interval2->end >= interval1->end) {
        return TemporalRelation::DURING;
    } else if (interval1->end == interval2->start) {
        return TemporalRelation::MEETS;
    } else if (interval1->start == interval2->start) {
        return TemporalRelation::STARTS;
    } else if (interval1->end == interval2->end) {
        return TemporalRelation::FINISHES;
    } else if (interval1->overlaps(*interval2)) {
        return TemporalRelation::OVERLAPS;
    }
    
    return TemporalRelation::BEFORE;
}

std::vector<Handle> TemporalReasoner::getEventsInInterval(
    const std::chrono::steady_clock::time_point& start_time,
    const std::chrono::steady_clock::time_point& end_time)
{
    std::vector<Handle> events;
    
    for (const auto& [event_atom, interval] : _event_intervals) {
        if (interval.start >= start_time && interval.end <= end_time) {
            events.push_back(event_atom);
        }
    }
    
    return events;
}

std::vector<Handle> TemporalReasoner::getEventsBefore(
    const std::chrono::steady_clock::time_point& time_point)
{
    std::vector<Handle> events;
    
    for (const auto& [event_atom, interval] : _event_intervals) {
        if (interval.end <= time_point) {
            events.push_back(event_atom);
        }
    }
    
    return events;
}

std::vector<Handle> TemporalReasoner::getEventsAfter(
    const std::chrono::steady_clock::time_point& time_point)
{
    std::vector<Handle> events;
    
    for (const auto& [event_atom, interval] : _event_intervals) {
        if (interval.start >= time_point) {
            events.push_back(event_atom);
        }
    }
    
    return events;
}

bool TemporalReasoner::satisfiesTemporalConstraints(const std::vector<Handle>& events)
{
    for (const auto& constraint : _temporal_constraints) {
        if (std::find(events.begin(), events.end(), constraint.subject_atom) == events.end()) {
            continue;  // Constraint not relevant to this event set
        }
        
        switch (constraint.type) {
            case ConstraintType::ABSOLUTE_TIME: {
                auto interval = getTemporalInterval(constraint.subject_atom);
                if (interval) {
                    auto tolerance = std::chrono::milliseconds(
                        (int)(_constraint_satisfaction_tolerance * 1000));
                    if (std::abs(std::chrono::duration_cast<std::chrono::milliseconds>(
                        interval->start - constraint.absolute_time).count()) > tolerance.count()) {
                        return false;
                    }
                }
                break;
            }
            
            case ConstraintType::DEADLINE: {
                auto interval = getTemporalInterval(constraint.subject_atom);
                if (interval && interval->end > constraint.absolute_time) {
                    return false;
                }
                break;
            }
            
            case ConstraintType::RELATIVE_TIME: {
                if (constraint.reference_atom != Handle::UNDEFINED) {
                    auto subject_interval = getTemporalInterval(constraint.subject_atom);
                    auto reference_interval = getTemporalInterval(constraint.reference_atom);
                    
                    if (subject_interval && reference_interval) {
                        auto actual_relation = inferTemporalRelation(
                            constraint.subject_atom, constraint.reference_atom);
                        if (actual_relation != constraint.relation) {
                            return false;
                        }
                    }
                }
                break;
            }
            
            default:
                break;
        }
    }
    
    return true;
}

std::vector<TemporalReasoner::TemporalInterval> TemporalReasoner::optimizeTemporalSchedule(
    const std::vector<TemporalInterval>& intervals)
{
    std::vector<TemporalInterval> optimized = intervals;
    
    // Simple optimization: compact intervals to minimize total time
    std::sort(optimized.begin(), optimized.end(),
              [](const TemporalInterval& a, const TemporalInterval& b) {
                  return a.start < b.start;
              });
    
    // Adjust flexible intervals to reduce gaps
    for (size_t i = 1; i < optimized.size(); ++i) {
        if (!optimized[i].is_fixed && !optimized[i-1].is_fixed) {
            // Move current interval to start right after previous one
            auto duration = optimized[i].duration();
            optimized[i].start = optimized[i-1].end + _temporal_resolution;
            optimized[i].end = optimized[i].start + duration;
        }
    }
    
    return optimized;
}

Handle TemporalReasoner::createTemporalAtom(const TemporalInterval& interval)
{
    // Create AtomSpace representation of temporal interval
    Handle time_interval = _atomspace->add_node(CONCEPT_NODE, "temporal-interval");
    
    // Link to temporal context
    _atomspace->add_link(MEMBER_LINK, {interval.event_atom, _temporal_context});
    
    // Create start time representation
    auto start_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        interval.start.time_since_epoch()).count();
    Handle start_time = _atomspace->add_node(NUMBER_NODE, std::to_string(start_ms));
    
    // Create end time representation  
    auto end_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        interval.end.time_since_epoch()).count();
    Handle end_time = _atomspace->add_node(NUMBER_NODE, std::to_string(end_ms));
    
    // Create temporal interval link
    Handle interval_link = _atomspace->add_link(EVALUATION_LINK, {
        _atomspace->add_node(PREDICATE_NODE, "temporal-interval"),
        _atomspace->add_link(LIST_LINK, {interval.event_atom, start_time, end_time})
    });
    
    // Set confidence as truth value
    interval_link->setTruthValue(SimpleTruthValue::createTV(interval.confidence, 1.0));
    
    return interval_link;
}

Handle TemporalReasoner::createConstraintAtom(const TemporalConstraint& constraint)
{
    // Create AtomSpace representation of temporal constraint
    std::string constraint_type_name;
    switch (constraint.type) {
        case ConstraintType::ABSOLUTE_TIME: constraint_type_name = "absolute-time"; break;
        case ConstraintType::RELATIVE_TIME: constraint_type_name = "relative-time"; break;
        case ConstraintType::DEADLINE: constraint_type_name = "deadline"; break;
        case ConstraintType::DURATION: constraint_type_name = "duration"; break;
        case ConstraintType::ORDERING: constraint_type_name = "ordering"; break;
        case ConstraintType::PERIODICITY: constraint_type_name = "periodicity"; break;
        case ConstraintType::TEMPORAL_GAP: constraint_type_name = "temporal-gap"; break;
    }
    
    Handle constraint_predicate = _atomspace->add_node(PREDICATE_NODE, std::move(constraint_type_name));
    Handle constraint_link;
    
    if (constraint.reference_atom != Handle::UNDEFINED) {
        // Relative constraint
        constraint_link = _atomspace->add_link(EVALUATION_LINK, {
            constraint_predicate,
            _atomspace->add_link(LIST_LINK, {constraint.subject_atom, constraint.reference_atom})
        });
    } else {
        // Absolute constraint
        constraint_link = _atomspace->add_link(EVALUATION_LINK, {
            constraint_predicate,
            _atomspace->add_link(LIST_LINK, {constraint.subject_atom})
        });
    }
    
    // Set priority as truth value
    constraint_link->setTruthValue(SimpleTruthValue::createTV(constraint.priority, 1.0));
    
    // Link to constraint context
    _atomspace->add_link(MEMBER_LINK, {constraint_link, _constraint_context});
    
    return constraint_link;
}

int TemporalReasoner::updateTemporalReasoning()
{
    int updated_count = 0;
    
    // Propagate temporal constraints if enabled
    if (_enable_temporal_propagation) {
        propagateTemporalConstraints();
        updated_count++;
    }
    
#ifdef HAVE_SPACETIME
    // Sync with TimeServer if available
    if (_time_server) {
        updated_count += syncWithTimeServer();
    }
#endif
    
    return updated_count;
}

void TemporalReasoner::propagateTemporalConstraints()
{
    // Propagate temporal constraints through the relation network
    for (const auto& [event, related_events] : _temporal_relations) {
        for (const Handle& related : related_events) {
            // Find and apply transitive temporal relations
            auto relation = getTemporalRelation(event, related);
            
            // Simple transitivity: if A before B and B before C, then A before C
            for (const auto& [other_event, other_related] : _temporal_relations) {
                if (other_event == related) {
                    for (const Handle& transitively_related : other_related) {
                        if (transitively_related != event) {
                            // Add transitive relation
                            _temporal_relations[event].push_back(transitively_related);
                        }
                    }
                }
            }
        }
    }
}

std::string TemporalReasoner::getStatusInfo() const
{
    std::ostringstream info;
    info << std::fixed << std::setprecision(2);
    info << "{\n";
    info << "  \"temporal_intervals\": " << _event_intervals.size() << ",\n";
    info << "  \"temporal_constraints\": " << _temporal_constraints.size() << ",\n";
    info << "  \"temporal_relations\": " << _temporal_relations.size() << ",\n";
    info << "  \"temporal_resolution_ms\": " << _temporal_resolution.count() << ",\n";
    info << "  \"planning_horizon_ms\": " << _planning_horizon.count() << ",\n";
    info << "  \"constraint_tolerance\": " << _constraint_satisfaction_tolerance << "\n";
    info << "}";
    return info.str();
}