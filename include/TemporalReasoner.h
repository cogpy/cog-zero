/*
 * opencog/agentzero/TemporalReasoner.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Temporal Reasoning for Planning
 * Advanced temporal reasoning and constraint handling for Agent-Zero planning
 * Part of AZ-PLAN-002: Create PlanningEngine with temporal reasoning
 */

#ifndef _OPENCOG_AGENTZERO_TEMPORAL_REASONER_H
#define _OPENCOG_AGENTZERO_TEMPORAL_REASONER_H

#include <memory>
#include <vector>
#include <map>
#include <string>
#include <chrono>
#include <functional>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Handle.h>
#include <opencog/atoms/truthvalue/TruthValue.h>
#include <opencog/util/Logger.h>

#ifdef HAVE_SPACETIME
#include <opencog/spacetime/TimeServer.h>
#include <opencog/spacetime/atom-types/atom_types.h>
#endif

namespace opencog {
namespace agentzero {

/**
 * TemporalReasoner - Advanced temporal reasoning for planning
 *
 * This class provides comprehensive temporal reasoning capabilities
 * for Agent-Zero planning, including temporal constraints, intervals,
 * and temporal pattern recognition using spacetime integration.
 *
 * Key features:
 * - Temporal constraint satisfaction
 * - Temporal interval reasoning
 * - Temporal pattern recognition
 * - Integration with spacetime TimeServer
 * - AtomSpace temporal representation
 * - Performance optimized temporal queries
 */
class TemporalReasoner
{
public:
    // Temporal relation types
    enum class TemporalRelation {
        BEFORE,         // A occurs before B
        AFTER,          // A occurs after B
        DURING,         // A occurs during B
        OVERLAPS,       // A overlaps with B
        MEETS,          // A meets B (end of A = start of B)
        STARTS,         // A starts when B starts
        FINISHES,       // A finishes when B finishes
        EQUALS,         // A and B have same temporal extent
        CONTAINS,       // A contains B temporally
        SIMULTANEOUS    // A and B occur simultaneously
    };
    
    // Temporal constraint types
    enum class ConstraintType {
        ABSOLUTE_TIME,  // Must occur at specific time
        RELATIVE_TIME,  // Must occur relative to another event
        DURATION,       // Must have specific duration
        DEADLINE,       // Must complete before deadline
        ORDERING,       // Must follow specific order
        PERIODICITY,    // Must repeat at intervals
        TEMPORAL_GAP    // Must have gap between events
    };
    
    // Temporal interval representation
    struct TemporalInterval {
        std::chrono::steady_clock::time_point start;
        std::chrono::steady_clock::time_point end;
        Handle event_atom;
        bool is_fixed;      // Whether interval is fixed or flexible
        float confidence;   // Confidence in temporal bounds
        
        TemporalInterval() : is_fixed(false), confidence(1.0f) {}
        
        std::chrono::milliseconds duration() const {
            return std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        }
        
        bool contains(const std::chrono::steady_clock::time_point& time) const {
            return time >= start && time <= end;
        }
        
        bool overlaps(const TemporalInterval& other) const {
            return start < other.end && end > other.start;
        }
    };
    
    // Temporal constraint
    struct TemporalConstraint {
        Handle subject_atom;
        Handle reference_atom;  // For relative constraints
        ConstraintType type;
        std::chrono::milliseconds value;  // Duration, delay, etc.
        std::chrono::steady_clock::time_point absolute_time;
        TemporalRelation relation;
        float priority;         // Constraint priority (1.0 = must satisfy)
        bool is_hard;          // Hard vs soft constraint
        
        TemporalConstraint() : type(ConstraintType::ABSOLUTE_TIME),
                              value(0), relation(TemporalRelation::BEFORE),
                              priority(1.0f), is_hard(true) {}
    };

private:
    // Core references
    AtomSpacePtr _atomspace;
    
#ifdef HAVE_SPACETIME
    spacetime::TimeServer* _time_server;
#endif

    // Temporal reasoning structures
    std::map<Handle, TemporalInterval> _event_intervals;
    std::vector<TemporalConstraint> _temporal_constraints;
    std::map<Handle, std::vector<Handle>> _temporal_relations;
    
    // AtomSpace contexts
    Handle _temporal_context;
    Handle _interval_context;
    Handle _constraint_context;
    Handle _relation_context;
    
    // Configuration
    std::chrono::milliseconds _temporal_resolution;
    std::chrono::milliseconds _planning_horizon;
    bool _enable_temporal_propagation;
    bool _enable_constraint_relaxation;
    float _constraint_satisfaction_tolerance;
    
    // Internal methods
    void initializeTemporalReasoner();
    void createTemporalContexts();
    bool satisfiesTemporalConstraints(const std::vector<Handle>& events);
    TemporalRelation inferTemporalRelation(const Handle& event1, const Handle& event2);
    std::vector<TemporalInterval> optimizeTemporalSchedule(
        const std::vector<TemporalInterval>& intervals);
    void propagateTemporalConstraints();
    Handle createTemporalAtom(const TemporalInterval& interval);
    Handle createConstraintAtom(const TemporalConstraint& constraint);

public:
    /**
     * Constructor
     * @param atomspace Shared pointer to the AtomSpace
     */
    TemporalReasoner(AtomSpacePtr atomspace);
    
    /**
     * Destructor
     */
    ~TemporalReasoner();
    
    // Temporal interval management
    /**
     * Add temporal interval for an event
     * @param event_atom Handle to the event atom
     * @param start_time Interval start time
     * @param end_time Interval end time
     * @param is_fixed Whether interval is fixed or flexible
     * @return true if interval was added successfully
     */
    bool addTemporalInterval(const Handle& event_atom,
                           const std::chrono::steady_clock::time_point& start_time,
                           const std::chrono::steady_clock::time_point& end_time,
                           bool is_fixed = false);
    
    /**
     * Update temporal interval for an event
     * @param event_atom Handle to the event atom
     * @param start_time New interval start time
     * @param end_time New interval end time
     * @return true if interval was updated successfully
     */
    bool updateTemporalInterval(const Handle& event_atom,
                              const std::chrono::steady_clock::time_point& start_time,
                              const std::chrono::steady_clock::time_point& end_time);
    
    /**
     * Get temporal interval for an event
     * @param event_atom Handle to the event atom
     * @return TemporalInterval or nullptr if not found
     */
    const TemporalInterval* getTemporalInterval(const Handle& event_atom) const;
    
    /**
     * Remove temporal interval for an event
     * @param event_atom Handle to the event atom
     * @return true if interval was removed successfully
     */
    bool removeTemporalInterval(const Handle& event_atom);
    
    // Temporal constraint management
    /**
     * Add absolute temporal constraint
     * @param event_atom Handle to the event atom
     * @param absolute_time When event must occur
     * @param priority Constraint priority (0.0 - 1.0)
     * @param is_hard Whether constraint is hard or soft
     * @return true if constraint was added successfully
     */
    bool addAbsoluteConstraint(const Handle& event_atom,
                             const std::chrono::steady_clock::time_point& absolute_time,
                             float priority = 1.0f,
                             bool is_hard = true);
    
    /**
     * Add relative temporal constraint
     * @param subject_atom Handle to the subject event
     * @param reference_atom Handle to the reference event
     * @param relation Temporal relation between events
     * @param delay Optional delay between events
     * @param priority Constraint priority (0.0 - 1.0)
     * @param is_hard Whether constraint is hard or soft
     * @return true if constraint was added successfully
     */
    bool addRelativeConstraint(const Handle& subject_atom,
                             const Handle& reference_atom,
                             TemporalRelation relation,
                             std::chrono::milliseconds delay = std::chrono::milliseconds(0),
                             float priority = 1.0f,
                             bool is_hard = true);
    
    /**
     * Add duration constraint
     * @param event_atom Handle to the event atom
     * @param min_duration Minimum duration
     * @param max_duration Maximum duration
     * @param priority Constraint priority (0.0 - 1.0)
     * @return true if constraint was added successfully
     */
    bool addDurationConstraint(const Handle& event_atom,
                             std::chrono::milliseconds min_duration,
                             std::chrono::milliseconds max_duration,
                             float priority = 1.0f);
    
    /**
     * Add deadline constraint
     * @param event_atom Handle to the event atom
     * @param deadline When event must complete by
     * @param priority Constraint priority (0.0 - 1.0)
     * @param is_hard Whether constraint is hard or soft
     * @return true if constraint was added successfully
     */
    bool addDeadlineConstraint(const Handle& event_atom,
                             const std::chrono::steady_clock::time_point& deadline,
                             float priority = 1.0f,
                             bool is_hard = true);
    
    /**
     * Remove temporal constraint
     * @param subject_atom Handle to the subject event
     * @param type Constraint type to remove
     * @return true if constraint was removed successfully
     */
    bool removeTemporalConstraint(const Handle& subject_atom, ConstraintType type);
    
    // Temporal reasoning and validation
    /**
     * Check if temporal constraints are satisfiable
     * @param events Vector of event atoms to check
     * @return true if all constraints can be satisfied
     */
    bool areConstraintsSatisfiable(const std::vector<Handle>& events);
    
    /**
     * Validate temporal schedule
     * @param events Vector of scheduled event atoms
     * @return true if schedule satisfies all constraints
     */
    bool validateTemporalSchedule(const std::vector<Handle>& events);
    
    /**
     * Optimize temporal schedule to satisfy constraints
     * @param events Vector of event atoms to optimize
     * @return optimized vector of event atoms with updated intervals
     */
    std::vector<Handle> optimizeTemporalSchedule(const std::vector<Handle>& events);
    
    /**
     * Find temporal conflicts in schedule
     * @param events Vector of event atoms to check
     * @return vector of conflicting event pairs
     */
    std::vector<std::pair<Handle, Handle>> findTemporalConflicts(
        const std::vector<Handle>& events);
    
    /**
     * Resolve temporal conflicts
     * @param conflicts Vector of conflicting event pairs
     * @return true if conflicts were resolved successfully
     */
    bool resolveTemporalConflicts(
        const std::vector<std::pair<Handle, Handle>>& conflicts);
    
    // Temporal relation inference
    /**
     * Infer temporal relation between two events
     * @param event1 Handle to first event
     * @param event2 Handle to second event
     * @return TemporalRelation between the events
     */
    TemporalRelation getTemporalRelation(const Handle& event1, const Handle& event2);
    
    /**
     * Set explicit temporal relation between events
     * @param event1 Handle to first event
     * @param event2 Handle to second event
     * @param relation Temporal relation
     * @return true if relation was set successfully
     */
    bool setTemporalRelation(const Handle& event1, const Handle& event2,
                           TemporalRelation relation);
    
    /**
     * Get events occurring during time interval
     * @param start_time Interval start time
     * @param end_time Interval end time
     * @return vector of event atoms occurring during interval
     */
    std::vector<Handle> getEventsInInterval(
        const std::chrono::steady_clock::time_point& start_time,
        const std::chrono::steady_clock::time_point& end_time);
    
    /**
     * Get events occurring before specified time
     * @param time_point Reference time point
     * @return vector of event atoms occurring before time
     */
    std::vector<Handle> getEventsBefore(
        const std::chrono::steady_clock::time_point& time_point);
    
    /**
     * Get events occurring after specified time
     * @param time_point Reference time point
     * @return vector of event atoms occurring after time
     */
    std::vector<Handle> getEventsAfter(
        const std::chrono::steady_clock::time_point& time_point);
    
#ifdef HAVE_SPACETIME
    // Spacetime integration
    /**
     * Set TimeServer for spacetime integration
     * @param time_server Pointer to TimeServer instance
     */
    void setTimeServer(spacetime::TimeServer* time_server) {
        _time_server = time_server;
    }
    
    /**
     * Sync with TimeServer temporal information
     * @return number of events synced
     */
    int syncWithTimeServer();
    
    /**
     * Create TimeNode for event
     * @param event_atom Handle to event atom
     * @param time_point Time point for the event
     * @return Handle to created TimeNode
     */
    Handle createTimeNode(const Handle& event_atom,
                         const std::chrono::steady_clock::time_point& time_point);
    
    /**
     * Create AtTimeLink for temporal association
     * @param event_atom Handle to event atom
     * @param time_node Handle to TimeNode
     * @return Handle to created AtTimeLink
     */
    Handle createAtTimeLink(const Handle& event_atom, const Handle& time_node);
#endif
    
    // Configuration
    /**
     * Set temporal resolution
     * @param resolution_ms Temporal resolution in milliseconds
     */
    void setTemporalResolution(int resolution_ms) {
        _temporal_resolution = std::chrono::milliseconds(resolution_ms);
    }
    
    /**
     * Set planning horizon
     * @param horizon_ms Planning horizon in milliseconds
     */
    void setPlanningHorizon(int horizon_ms) {
        _planning_horizon = std::chrono::milliseconds(horizon_ms);
    }
    
    /**
     * Configure temporal reasoning features
     * @param propagation Enable temporal constraint propagation
     * @param relaxation Enable constraint relaxation
     * @param tolerance Constraint satisfaction tolerance (0.0 - 1.0)
     */
    void configureFeatures(bool propagation, bool relaxation, float tolerance) {
        _enable_temporal_propagation = propagation;
        _enable_constraint_relaxation = relaxation;
        _constraint_satisfaction_tolerance = tolerance;
    }
    
    // AtomSpace integration
    /**
     * Get the temporal context atom
     * @return Handle to temporal context
     */
    Handle getTemporalContext() const { return _temporal_context; }
    
    /**
     * Get status information for debugging
     * @return JSON string with temporal reasoning status
     */
    std::string getStatusInfo() const;
    
    /**
     * Update temporal reasoning with current time
     * @return number of constraints updated
     */
    int updateTemporalReasoning();
};

} // namespace agentzero
} // namespace opencog

#endif // _OPENCOG_AGENTZERO_TEMPORAL_REASONER_H