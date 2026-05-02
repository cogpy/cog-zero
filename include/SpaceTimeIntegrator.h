/*
 * opencog/agentzero/SpaceTimeIntegrator.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * SpaceTime Integrator for Temporal Planning
 * Integrates spacetime with temporal planning for Agent-Zero
 * Part of AZ-SPATIAL-001: Integrate spacetime for temporal planning
 */

#ifndef _OPENCOG_AGENTZERO_SPACETIME_INTEGRATOR_H
#define _OPENCOG_AGENTZERO_SPACETIME_INTEGRATOR_H

#include <memory>
#include <vector>
#include <chrono>
#include <map>
#include <string>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Handle.h>
#include <opencog/atoms/truthvalue/TruthValue.h>
#include <opencog/util/Logger.h>
#include <opencog/spacetime/octomap/TimeOctomap.h>

namespace opencog {

// Forward declarations
class ActionScheduler;

namespace agentzero {

/**
 * SpaceTimeIntegrator - Bridge between spacetime and temporal planning
 *
 * This class provides comprehensive integration between OpenCog's spacetime
 * component and Agent-Zero's temporal planning system. It extends action
 * scheduling with spatial awareness and timeline management capabilities.
 *
 * Key features:
 * - Spatial-temporal reasoning for action planning
 * - Timeline-based constraint validation
 * - Location-aware action scheduling
 * - Spatial conflict resolution
 * - Temporal trajectory planning
 */
class SpaceTimeIntegrator
{
public:
    // Construction and initialization
    SpaceTimeIntegrator(AtomSpacePtr atomspace);
    ~SpaceTimeIntegrator();

    // Configuration
    struct Configuration {
        double spatial_resolution = 0.1;           // Spatial resolution in meters
        std::chrono::milliseconds time_resolution{100}; // Time resolution
        unsigned int time_units = 1000;           // Number of time units to track
        bool enable_spatial_constraints = true;   // Enable spatial constraint checking
        bool enable_trajectory_planning = true;   // Enable trajectory planning
        bool enable_timeline_reasoning = true;    // Enable timeline-based reasoning
    };

    void configure(const Configuration& config);
    const Configuration& getConfiguration() const;

    // Spatial-temporal data management
    bool insertAtomAtLocation(const Handle& atom, 
                             const octomap::point3d& location, 
                             const std::chrono::system_clock::time_point& time);
    
    bool insertAtomAtCurrentTime(const Handle& atom, 
                                const octomap::point3d& location);
    
    bool removeAtomFromLocation(const Handle& atom, 
                               const octomap::point3d& location,
                               const std::chrono::system_clock::time_point& time);

    // Spatial query capabilities
    std::vector<Handle> getAtomsAtLocation(const octomap::point3d& location,
                                          const std::chrono::system_clock::time_point& time) const;
    
    std::vector<octomap::point3d> getAtomLocations(const Handle& atom,
                                                  const std::chrono::system_clock::time_point& time) const;
    
    std::vector<octomap::point3d> getAtomLocationsAtCurrentTime(const Handle& atom) const;

    // Timeline query capabilities
    std::vector<std::chrono::system_clock::time_point> getAtomTimeline(const Handle& atom) const;
    
    std::vector<std::chrono::system_clock::time_point> getLocationTimeline(const octomap::point3d& location) const;
    
    bool getAtomLocationAtTime(const Handle& atom, 
                              const std::chrono::system_clock::time_point& time,
                              octomap::point3d& location) const;

    // Spatial relationship queries
    enum class SpatialRelation {
        UNKNOWN = -1,
        ALIGNED = 0,
        LEFT = 1,
        RIGHT = 2,
        BEHIND = 1,
        AHEAD = 2,
        BELOW = 1,
        ABOVE = 2
    };

    struct SpatialRelationResult {
        SpatialRelation x_relation; // ahead/behind/aligned
        SpatialRelation y_relation; // left/right/aligned
        SpatialRelation z_relation; // above/below/aligned
        bool valid;
    };

    SpatialRelationResult getSpatialRelations(const Handle& observer,
                                            const Handle& target,
                                            const Handle& reference,
                                            const std::chrono::system_clock::time_point& time) const;

    double getDistanceBetween(const Handle& atom1,
                             const Handle& atom2,
                             const std::chrono::system_clock::time_point& time) const;

    bool getDirectionVector(const Handle& from_atom,
                           const Handle& to_atom,
                           const std::chrono::system_clock::time_point& time,
                           octomap::point3d& direction) const;

    // Spatial constraint validation for planning
    struct SpatialConstraint {
        Handle atom;
        octomap::point3d location;
        double tolerance = 0.1;  // Distance tolerance in meters
        std::chrono::system_clock::time_point start_time;
        std::chrono::system_clock::time_point end_time;
    };

    bool validateSpatialConstraints(const std::vector<SpatialConstraint>& constraints) const;
    
    bool checkLocationAvailability(const octomap::point3d& location,
                                  const std::chrono::system_clock::time_point& start_time,
                                  const std::chrono::system_clock::time_point& end_time) const;

    // Trajectory planning support
    struct TrajectoryPoint {
        octomap::point3d location;
        std::chrono::system_clock::time_point time;
        Handle associated_atom;
    };

    using Trajectory = std::vector<TrajectoryPoint>;

    bool planTrajectory(const Handle& atom,
                       const octomap::point3d& start_location,
                       const octomap::point3d& goal_location,
                       const std::chrono::system_clock::time_point& start_time,
                       const std::chrono::system_clock::time_point& end_time,
                       Trajectory& result_trajectory);

    bool validateTrajectory(const Trajectory& trajectory) const;

    // Timeline reasoning for action scheduling
    struct TemporalPlanningResult {
        std::chrono::system_clock::time_point optimal_start_time;
        std::chrono::system_clock::time_point optimal_end_time;
        std::vector<SpatialConstraint> required_constraints;
        double confidence_score;
        bool feasible;
    };

    TemporalPlanningResult findOptimalTimeWindow(const Handle& action_atom,
                                               const std::vector<SpatialConstraint>& spatial_requirements,
                                               const std::chrono::system_clock::time_point& earliest_start,
                                               const std::chrono::system_clock::time_point& latest_end);

    // Integration with ActionScheduler
    bool integrateWithActionScheduler(ActionScheduler* scheduler);
    void detachFromActionScheduler();

    // AtomSpace integration
    Handle createSpatialTemporalAtom(const Handle& atom,
                                   const octomap::point3d& location, 
                                   const std::chrono::system_clock::time_point& time);

    Handle createTrajectoryAtom(const Handle& atom, const Trajectory& trajectory);

    std::vector<Handle> querySpatialTemporalAtoms(const octomap::point3d& location,
                                                 const std::chrono::system_clock::time_point& time_start,
                                                 const std::chrono::system_clock::time_point& time_end) const;

    // Time management
    void stepTimeUnit();
    void setCurrentTime(const std::chrono::system_clock::time_point& time);
    std::chrono::system_clock::time_point getCurrentTime() const;

    // Resource and status information
    std::string getStatusInfo() const;
    size_t getMemoryUsage() const;
    void clearTimeline(const std::chrono::system_clock::time_point& before_time);

    // Context atoms for AtomSpace integration
    Handle getSpatialContext() const { return _spatial_context; }
    Handle getTemporalContext() const { return _temporal_context; }
    Handle getTrajectoryContext() const { return _trajectory_context; }

private:
    // Core components
    AtomSpacePtr _atomspace;
    std::unique_ptr<TimeOctomap<Handle>> _spacetime_map;
    ActionScheduler* _action_scheduler;

    // Configuration
    Configuration _config;

    // AtomSpace context atoms
    Handle _spatial_context;
    Handle _temporal_context;
    Handle _trajectory_context;
    Handle _constraint_context;

    // Internal state
    std::chrono::system_clock::time_point _current_time;
    
    // Spatial constraint tracking
    std::map<Handle, std::vector<SpatialConstraint>> _atom_constraints;
    std::map<octomap::point3d, std::vector<Handle>> _location_occupancy;

    // Helper methods
    void initializeSpaceTimeIntegrator();
    void createContextAtoms();
    
    octomap::point3d convertAtomSpaceToOctomap(const Handle& location_atom) const;
    Handle convertOctomapToAtomSpace(const octomap::point3d& point) const;
    
    std::chrono::system_clock::time_point convertAtomSpaceToTimePoint(const Handle& time_atom) const;
    Handle convertTimePointToAtomSpace(const std::chrono::system_clock::time_point& time) const;

    bool isValidLocation(const octomap::point3d& location) const;
    bool isValidTimePoint(const std::chrono::system_clock::time_point& time) const;

    // Constraint validation helpers
    bool checkSpatialConstraint(const SpatialConstraint& constraint) const;
    std::vector<Handle> findConflictingAtoms(const octomap::point3d& location,
                                           const std::chrono::system_clock::time_point& time) const;

    // Trajectory planning helpers
    double calculateTrajectoryLength(const Trajectory& trajectory) const;
    bool isTrajectoryCollisionFree(const Trajectory& trajectory) const;
    
    // Timeline reasoning helpers
    std::vector<std::chrono::system_clock::time_point> findAvailableTimeWindows(
        const octomap::point3d& location,
        const std::chrono::system_clock::time_point& start_search,
        const std::chrono::system_clock::time_point& end_search) const;

    // AtomSpace integration helpers
    void recordSpatialTemporalEvent(const Handle& atom,
                                   const octomap::point3d& location,
                                   const std::chrono::system_clock::time_point& time);
    
    void updateSpatialTemporalIndex(const Handle& atom,
                                   const octomap::point3d& location,
                                   const std::chrono::system_clock::time_point& time,
                                   bool add);
};

} // namespace agentzero
} // namespace opencog

#endif // _OPENCOG_AGENTZERO_SPACETIME_INTEGRATOR_H