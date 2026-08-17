/*
 * opencog/agentzero/planning/TemporalReasoner.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Temporal constraint reasoning for planning (Phase 4).
 * Works standalone; optionally bridges to spacetime TimeServer when present.
 */

#ifndef _OPENCOG_AGENTZERO_PLANNING_TEMPORAL_REASONER_H
#define _OPENCOG_AGENTZERO_PLANNING_TEMPORAL_REASONER_H

#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Handle.h>
#include <opencog/util/Logger.h>

#ifdef HAVE_SPACETIME
#include <opencog/spacetime/TimeServer.h>
#endif

namespace opencog {
namespace agentzero {
namespace planning {

/**
 * TemporalReasoner — intervals, deadlines, ordering, and schedule validation.
 */
class TemporalReasoner
{
public:
    enum class TemporalRelation {
        BEFORE,
        AFTER,
        DURING,
        OVERLAPS,
        MEETS,
        STARTS,
        FINISHES,
        EQUALS,
        CONTAINS,
        SIMULTANEOUS,
        UNKNOWN
    };

    enum class ConstraintType {
        ABSOLUTE_TIME,
        RELATIVE_TIME,
        DURATION,
        DEADLINE,
        ORDERING,
        PERIODICITY,
        TEMPORAL_GAP
    };

    struct TemporalInterval {
        std::chrono::steady_clock::time_point start;
        std::chrono::steady_clock::time_point end;
        Handle event_atom;
        bool is_fixed{false};
        float confidence{1.0f};

        std::chrono::milliseconds duration() const {
            return std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        }
        bool contains(const std::chrono::steady_clock::time_point& t) const {
            return t >= start && t <= end;
        }
        bool overlaps(const TemporalInterval& o) const {
            return start < o.end && end > o.start;
        }
    };

    struct TemporalConstraint {
        Handle subject_atom;
        Handle reference_atom;
        ConstraintType type{ConstraintType::ORDERING};
        std::chrono::milliseconds value{0};
        std::chrono::steady_clock::time_point absolute_time;
        TemporalRelation relation{TemporalRelation::BEFORE};
        float priority{1.0f};
        bool is_hard{true};
    };

    explicit TemporalReasoner(AtomSpacePtr atomspace);
    ~TemporalReasoner();

    bool initialize();
    bool shutdown();
    bool isInitialized() const { return _initialized; }

    // Intervals
    bool addTemporalInterval(const Handle& event_atom,
                             const std::chrono::steady_clock::time_point& start_time,
                             const std::chrono::steady_clock::time_point& end_time,
                             bool is_fixed = false);
    bool updateTemporalInterval(const Handle& event_atom,
                                const std::chrono::steady_clock::time_point& start_time,
                                const std::chrono::steady_clock::time_point& end_time);
    const TemporalInterval* getTemporalInterval(const Handle& event_atom) const;
    bool removeTemporalInterval(const Handle& event_atom);

    // Constraints
    bool addAbsoluteConstraint(const Handle& event_atom,
                               const std::chrono::steady_clock::time_point& time,
                               TemporalRelation relation = TemporalRelation::EQUALS);
    bool addRelativeConstraint(const Handle& subject_atom,
                               const Handle& reference_atom,
                               TemporalRelation relation,
                               std::chrono::milliseconds gap = std::chrono::milliseconds{0});
    bool addDeadlineConstraint(const Handle& event_atom,
                               const std::chrono::steady_clock::time_point& deadline);
    bool addDurationConstraint(const Handle& event_atom,
                               std::chrono::milliseconds duration);

    // Schedule reasoning
    bool areConstraintsSatisfiable(const std::vector<Handle>& events) const;
    bool validateTemporalSchedule(const std::vector<Handle>& events) const;
    std::vector<Handle> optimizeTemporalSchedule(const std::vector<Handle>& events);
    std::vector<std::pair<Handle, Handle>> findTemporalConflicts(
        const std::vector<Handle>& events) const;
    bool resolveTemporalConflicts(const std::vector<std::pair<Handle, Handle>>& conflicts);

    TemporalRelation getTemporalRelation(const Handle& event1, const Handle& event2) const;

    std::vector<Handle> getEventsInInterval(
        const std::chrono::steady_clock::time_point& start,
        const std::chrono::steady_clock::time_point& end) const;
    std::vector<Handle> getEventsBefore(
        const std::chrono::steady_clock::time_point& time) const;
    std::vector<Handle> getEventsAfter(
        const std::chrono::steady_clock::time_point& time) const;

    // Assign sequential slots of slot_ms starting at start for events lacking intervals
    void assignSequentialSchedule(const std::vector<Handle>& events,
                                  const std::chrono::steady_clock::time_point& start,
                                  std::chrono::milliseconds slot_ms);

    int updateTemporalReasoning();
    size_t constraintCount() const { return _temporal_constraints.size(); }
    size_t intervalCount() const { return _event_intervals.size(); }
    std::string getStatusInfo() const;

    void setPlanningHorizon(std::chrono::milliseconds h) { _planning_horizon = h; }
    void setTemporalResolution(std::chrono::milliseconds r) { _temporal_resolution = r; }

#ifdef HAVE_SPACETIME
    void setTimeServer(spacetime::TimeServer* ts) { _time_server = ts; }
#endif

private:
    AtomSpacePtr _atomspace;
    bool _initialized{false};

#ifdef HAVE_SPACETIME
    spacetime::TimeServer* _time_server{nullptr};
#endif

    std::map<Handle, TemporalInterval> _event_intervals;
    std::vector<TemporalConstraint> _temporal_constraints;

    Handle _temporal_context;
    Handle _interval_context;
    Handle _constraint_context;

    std::chrono::milliseconds _temporal_resolution{100};
    std::chrono::milliseconds _planning_horizon{std::chrono::hours(24)};
    bool _enable_constraint_relaxation{true};
    float _constraint_satisfaction_tolerance{0.05f};

    void createTemporalContexts();
    bool checkConstraint(const TemporalConstraint& c) const;
    TemporalRelation inferRelation(const TemporalInterval& a,
                                   const TemporalInterval& b) const;
    Handle createIntervalAtom(const TemporalInterval& interval);
};

} // namespace planning
} // namespace agentzero
} // namespace opencog

#endif // _OPENCOG_AGENTZERO_PLANNING_TEMPORAL_REASONER_H
