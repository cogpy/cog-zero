/*
 * src/SpaceTimeIntegrator.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * SpaceTime Integrator Implementation
 * Integrates spacetime with temporal planning for Agent-Zero
 * Part of AZ-SPATIAL-001: Integrate spacetime for temporal planning
 */

#include <sstream>
#include <algorithm>
#include <cmath>

#include <opencog/atoms/atom_types/types.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>
#include <opencog/atoms/truthvalue/SimpleTruthValue.h>

#include "opencog/agentzero/SpaceTimeIntegrator.h"
#include "opencog/agentzero/ActionScheduler.h"

using namespace opencog;
using namespace opencog::agentzero;

SpaceTimeIntegrator::SpaceTimeIntegrator(AtomSpacePtr atomspace)
    : _atomspace(atomspace)
    , _action_scheduler(nullptr)
    , _spatial_context(Handle::UNDEFINED)
    , _temporal_context(Handle::UNDEFINED)
    , _trajectory_context(Handle::UNDEFINED)
    , _constraint_context(Handle::UNDEFINED)
    , _current_time(std::chrono::system_clock::now())
{
    logger().info() << "[SpaceTimeIntegrator] Constructor: Initializing spatial-temporal planning system";
    initializeSpaceTimeIntegrator();
}

SpaceTimeIntegrator::~SpaceTimeIntegrator()
{
    logger().info() << "[SpaceTimeIntegrator] Destructor: Cleaning up spatial-temporal resources";
    
    if (_action_scheduler) {
        detachFromActionScheduler();
    }
    
    _spacetime_map.reset();
    _atom_constraints.clear();
    _location_occupancy.clear();
}

void SpaceTimeIntegrator::initializeSpaceTimeIntegrator()
{
    logger().debug() << "[SpaceTimeIntegrator] Initializing spatial-temporal system";
    
    try {
        // Set default configuration
        _config.spatial_resolution = 0.1;
        _config.time_resolution = std::chrono::milliseconds(100);
        _config.time_units = 1000;
        _config.enable_spatial_constraints = true;
        _config.enable_trajectory_planning = true;
        _config.enable_timeline_reasoning = true;

        // Initialize spacetime map
        _spacetime_map = std::make_unique<TimeOctomap<Handle>>(
            _config.time_units,
            _config.spatial_resolution,
            _config.time_resolution
        );

        // Create context atoms in AtomSpace
        createContextAtoms();

        logger().info() << "[SpaceTimeIntegrator] Spatial-temporal system initialized successfully";
        
    } catch (const std::exception& e) {
        logger().error() << "[SpaceTimeIntegrator] Failed to initialize: " << e.what();
        throw;
    }
}

void SpaceTimeIntegrator::createContextAtoms()
{
    _spatial_context = _atomspace->add_node(CONCEPT_NODE, std::string("SpatialContext"));
    _temporal_context = _atomspace->add_node(CONCEPT_NODE, std::string("TemporalContext"));
    _trajectory_context = _atomspace->add_node(CONCEPT_NODE, std::string("TrajectoryContext"));
    _constraint_context = _atomspace->add_node(CONCEPT_NODE, std::string("SpatialConstraintContext"));
    
    // Set truth values
    TruthValuePtr context_tv = SimpleTruthValue::createTV(0.9, 0.9);
    _spatial_context->setTruthValue(context_tv);
    _temporal_context->setTruthValue(context_tv);
    _trajectory_context->setTruthValue(context_tv);
    _constraint_context->setTruthValue(context_tv);
}

void SpaceTimeIntegrator::configure(const Configuration& config)
{
    logger().debug() << "[SpaceTimeIntegrator] Configuring system with new parameters";
    
    _config = config;
    
    // Recreate spacetime map with new configuration
    _spacetime_map = std::make_unique<TimeOctomap<Handle>>(
        _config.time_units,
        _config.spatial_resolution,
        _config.time_resolution
    );
    
    logger().info() << "[SpaceTimeIntegrator] System reconfigured successfully";
}

const SpaceTimeIntegrator::Configuration& SpaceTimeIntegrator::getConfiguration() const
{
    return _config;
}

bool SpaceTimeIntegrator::insertAtomAtLocation(const Handle& atom, 
                                             const octomap::point3d& location, 
                                             const std::chrono::system_clock::time_point& time)
{
    if (atom == Handle::UNDEFINED || !isValidLocation(location) || !isValidTimePoint(time)) {
        logger().warn() << "[SpaceTimeIntegrator] Invalid parameters for atom insertion";
        return false;
    }

    logger().debug() << "[SpaceTimeIntegrator] Inserting atom at location: " << atom << " at " 
                     << location.x() << ", " << location.y() << ", " << location.z();

    try {
        // Find the appropriate time slice
        auto* time_slice = _spacetime_map->find(time);
        if (!time_slice) {
            // Step time units until we reach the target time
            while (_spacetime_map->get_current_time() < time) {
                _spacetime_map->step_time_unit();
            }
            time_slice = _spacetime_map->find(time);
        }

        if (!time_slice) {
            logger().warn() << "[SpaceTimeIntegrator] Could not find or create time slice for time point";
            return false;
        }

        // Insert atom at location
        time_slice->insert_atom(location, atom);

        // Update location occupancy tracking
        _location_occupancy[location].push_back(atom);

        // Record in AtomSpace
        recordSpatialTemporalEvent(atom, location, time);
        updateSpatialTemporalIndex(atom, location, time, true);

        logger().debug() << "[SpaceTimeIntegrator] Atom inserted successfully";
        return true;

    } catch (const std::exception& e) {
        logger().error() << "[SpaceTimeIntegrator] Failed to insert atom: " << e.what();
        return false;
    }
}

bool SpaceTimeIntegrator::insertAtomAtCurrentTime(const Handle& atom, 
                                                const octomap::point3d& location)
{
    return insertAtomAtLocation(atom, location, _current_time);
}

bool SpaceTimeIntegrator::removeAtomFromLocation(const Handle& atom, 
                                               const octomap::point3d& location,
                                               const std::chrono::system_clock::time_point& time)
{
    if (atom == Handle::UNDEFINED || !isValidLocation(location) || !isValidTimePoint(time)) {
        logger().warn() << "[SpaceTimeIntegrator] Invalid parameters for atom removal";
        return false;
    }

    logger().debug() << "[SpaceTimeIntegrator] Removing atom from location: " << atom;

    try {
        auto* time_slice = _spacetime_map->find(time);
        if (!time_slice) {
            logger().warn() << "[SpaceTimeIntegrator] Time slice not found for removal";
            return false;
        }

        // Remove atom from location
        time_slice->remove_atoms_at_location(location);

        // Update location occupancy tracking
        auto& occupants = _location_occupancy[location];
        occupants.erase(std::remove(occupants.begin(), occupants.end(), atom), occupants.end());
        if (occupants.empty()) {
            _location_occupancy.erase(location);
        }

        // Update AtomSpace index
        updateSpatialTemporalIndex(atom, location, time, false);

        logger().debug() << "[SpaceTimeIntegrator] Atom removed successfully";
        return true;

    } catch (const std::exception& e) {
        logger().error() << "[SpaceTimeIntegrator] Failed to remove atom: " << e.what();
        return false;
    }
}

std::vector<Handle> SpaceTimeIntegrator::getAtomsAtLocation(const octomap::point3d& location,
                                                          const std::chrono::system_clock::time_point& time) const
{
    std::vector<Handle> atoms;

    if (!isValidLocation(location) || !isValidTimePoint(time)) {
        return atoms;
    }

    try {
        auto* time_slice = _spacetime_map->find(time);
        if (!time_slice) {
            return atoms;
        }

        Handle atom = time_slice->get_atom_at_location(location);
        if (atom != Handle::UNDEFINED) {
            atoms.push_back(atom);
        }

    } catch (const std::exception& e) {
        logger().warn() << "[SpaceTimeIntegrator] Failed to get atoms at location: " << e.what();
    }

    return atoms;
}

std::vector<octomap::point3d> SpaceTimeIntegrator::getAtomLocations(const Handle& atom,
                                                                  const std::chrono::system_clock::time_point& time) const
{
    std::vector<octomap::point3d> locations;

    if (atom == Handle::UNDEFINED || !isValidTimePoint(time)) {
        return locations;
    }

    try {
        auto* time_slice = _spacetime_map->find(time);
        if (!time_slice) {
            return locations;
        }

        locations = time_slice->get_locations(atom);

    } catch (const std::exception& e) {
        logger().warn() << "[SpaceTimeIntegrator] Failed to get atom locations: " << e.what();
    }

    return locations;
}

std::vector<octomap::point3d> SpaceTimeIntegrator::getAtomLocationsAtCurrentTime(const Handle& atom) const
{
    return getAtomLocations(atom, _current_time);
}

std::vector<std::chrono::system_clock::time_point> SpaceTimeIntegrator::getAtomTimeline(const Handle& atom) const
{
    std::vector<std::chrono::system_clock::time_point> timeline;

    if (atom == Handle::UNDEFINED) {
        return timeline;
    }

    try {
        timeline = _spacetime_map->get_timeline(atom);
    } catch (const std::exception& e) {
        logger().warn() << "[SpaceTimeIntegrator] Failed to get atom timeline: " << e.what();
    }

    return timeline;
}

std::vector<std::chrono::system_clock::time_point> SpaceTimeIntegrator::getLocationTimeline(const octomap::point3d& location) const
{
    std::vector<std::chrono::system_clock::time_point> timeline;

    if (!isValidLocation(location)) {
        return timeline;
    }

    // This would require scanning through all time slices to find when this location was occupied
    // For now, return empty timeline - could be optimized with location indexing
    
    return timeline;
}

bool SpaceTimeIntegrator::getAtomLocationAtTime(const Handle& atom, 
                                              const std::chrono::system_clock::time_point& time,
                                              octomap::point3d& location) const
{
    if (atom == Handle::UNDEFINED || !isValidTimePoint(time)) {
        return false;
    }

    std::vector<octomap::point3d> locations = getAtomLocations(atom, time);
    if (locations.empty()) {
        return false;
    }

    // Return the first location (atoms can have multiple locations)
    location = locations[0];
    return true;
}

SpaceTimeIntegrator::SpatialRelationResult SpaceTimeIntegrator::getSpatialRelations(
    const Handle& observer,
    const Handle& target,
    const Handle& reference,
    const std::chrono::system_clock::time_point& time) const
{
    SpatialRelationResult result;
    result.valid = false;

    if (observer == Handle::UNDEFINED || target == Handle::UNDEFINED || 
        reference == Handle::UNDEFINED || !isValidTimePoint(time)) {
        return result;
    }

    try {
        auto* time_slice = _spacetime_map->find(time);
        if (!time_slice) {
            return result;
        }

        octomap::point3d spatial_relations = time_slice->get_spatial_relations(time, observer, target, reference);
        
        result.x_relation = static_cast<SpatialRelation>(static_cast<int>(spatial_relations.x()));
        result.y_relation = static_cast<SpatialRelation>(static_cast<int>(spatial_relations.y()));
        result.z_relation = static_cast<SpatialRelation>(static_cast<int>(spatial_relations.z()));
        result.valid = (spatial_relations.x() >= 0 && spatial_relations.y() >= 0 && spatial_relations.z() >= 0);

    } catch (const std::exception& e) {
        logger().warn() << "[SpaceTimeIntegrator] Failed to get spatial relations: " << e.what();
    }

    return result;
}

double SpaceTimeIntegrator::getDistanceBetween(const Handle& atom1,
                                             const Handle& atom2,
                                             const std::chrono::system_clock::time_point& time) const
{
    if (atom1 == Handle::UNDEFINED || atom2 == Handle::UNDEFINED || !isValidTimePoint(time)) {
        return -1.0;
    }

    try {
        auto* time_slice = _spacetime_map->find(time);
        if (!time_slice) {
            return -1.0;
        }

        return time_slice->get_distance_between(time, atom1, atom2);

    } catch (const std::exception& e) {
        logger().warn() << "[SpaceTimeIntegrator] Failed to get distance: " << e.what();
        return -1.0;
    }
}

bool SpaceTimeIntegrator::getDirectionVector(const Handle& from_atom,
                                           const Handle& to_atom,
                                           const std::chrono::system_clock::time_point& time,
                                           octomap::point3d& direction) const
{
    if (from_atom == Handle::UNDEFINED || to_atom == Handle::UNDEFINED || !isValidTimePoint(time)) {
        return false;
    }

    try {
        auto* time_slice = _spacetime_map->find(time);
        if (!time_slice) {
            return false;
        }

        return time_slice->get_direction_vector(time, from_atom, to_atom, direction);

    } catch (const std::exception& e) {
        logger().warn() << "[SpaceTimeIntegrator] Failed to get direction vector: " << e.what();
        return false;
    }
}

bool SpaceTimeIntegrator::validateSpatialConstraints(const std::vector<SpatialConstraint>& constraints) const
{
    if (!_config.enable_spatial_constraints) {
        return true;  // Constraints disabled, always valid
    }

    for (const auto& constraint : constraints) {
        if (!checkSpatialConstraint(constraint)) {
            logger().debug() << "[SpaceTimeIntegrator] Spatial constraint validation failed for atom: " << constraint.atom;
            return false;
        }
    }

    return true;
}

bool SpaceTimeIntegrator::checkLocationAvailability(const octomap::point3d& location,
                                                  const std::chrono::system_clock::time_point& start_time,
                                                  const std::chrono::system_clock::time_point& end_time) const
{
    if (!isValidLocation(location) || !isValidTimePoint(start_time) || !isValidTimePoint(end_time)) {
        return false;
    }

    // Check for conflicts in the time window
    auto current_time = start_time;
    while (current_time <= end_time) {
        std::vector<Handle> conflicting_atoms = findConflictingAtoms(location, current_time);
        if (!conflicting_atoms.empty()) {
            return false;
        }
        current_time += _config.time_resolution;
    }

    return true;
}

bool SpaceTimeIntegrator::planTrajectory(const Handle& atom,
                                       const octomap::point3d& start_location,
                                       const octomap::point3d& goal_location,
                                       const std::chrono::system_clock::time_point& start_time,
                                       const std::chrono::system_clock::time_point& end_time,
                                       Trajectory& result_trajectory)
{
    if (!_config.enable_trajectory_planning) {
        logger().warn() << "[SpaceTimeIntegrator] Trajectory planning is disabled";
        return false;
    }

    if (atom == Handle::UNDEFINED || !isValidLocation(start_location) || 
        !isValidLocation(goal_location) || !isValidTimePoint(start_time) || !isValidTimePoint(end_time)) {
        logger().warn() << "[SpaceTimeIntegrator] Invalid parameters for trajectory planning";
        return false;
    }

    logger().debug() << "[SpaceTimeIntegrator] Planning trajectory for atom: " << atom;

    result_trajectory.clear();

    try {
        // Simple linear trajectory planning (could be enhanced with A* or other algorithms)
        auto duration = end_time - start_time;
        auto time_steps = std::chrono::duration_cast<std::chrono::milliseconds>(duration) / _config.time_resolution;
        
        if (time_steps.count() <= 0) {
            return false;
        }

        octomap::point3d direction = goal_location - start_location;
        double total_distance = direction.norm();
        
        if (total_distance < 1e-6) {
            // Start and goal are the same, create stationary trajectory
            TrajectoryPoint point;
            point.location = start_location;
            point.time = start_time;
            point.associated_atom = atom;
            result_trajectory.push_back(point);
            return true;
        }

        // Create trajectory points
        for (int i = 0; i <= time_steps.count(); ++i) {
            TrajectoryPoint point;
            double t = static_cast<double>(i) / time_steps.count();
            
            point.location = start_location + direction * t;
            point.time = start_time + std::chrono::milliseconds(i * _config.time_resolution.count());
            point.associated_atom = atom;
            
            result_trajectory.push_back(point);
        }

        // Validate trajectory for collisions
        if (!validateTrajectory(result_trajectory)) {
            logger().warn() << "[SpaceTimeIntegrator] Trajectory validation failed";
            result_trajectory.clear();
            return false;
        }

        logger().info() << "[SpaceTimeIntegrator] Trajectory planned successfully with " 
                       << result_trajectory.size() << " points";
        return true;

    } catch (const std::exception& e) {
        logger().error() << "[SpaceTimeIntegrator] Failed to plan trajectory: " << e.what();
        result_trajectory.clear();
        return false;
    }
}

bool SpaceTimeIntegrator::validateTrajectory(const Trajectory& trajectory) const
{
    if (trajectory.empty()) {
        return false;
    }

    return isTrajectoryCollisionFree(trajectory);
}

SpaceTimeIntegrator::TemporalPlanningResult SpaceTimeIntegrator::findOptimalTimeWindow(
    const Handle& action_atom,
    const std::vector<SpatialConstraint>& spatial_requirements,
    const std::chrono::system_clock::time_point& earliest_start,
    const std::chrono::system_clock::time_point& latest_end)
{
    TemporalPlanningResult result;
    result.feasible = false;
    result.confidence_score = 0.0;

    if (!_config.enable_timeline_reasoning) {
        logger().warn() << "[SpaceTimeIntegrator] Timeline reasoning is disabled";
        return result;
    }

    if (action_atom == Handle::UNDEFINED || !isValidTimePoint(earliest_start) || !isValidTimePoint(latest_end)) {
        logger().warn() << "[SpaceTimeIntegrator] Invalid parameters for temporal planning";
        return result;
    }

    logger().debug() << "[SpaceTimeIntegrator] Finding optimal time window for action: " << action_atom;

    try {
        // Find available time windows for each spatial constraint
        std::vector<std::vector<std::chrono::system_clock::time_point>> available_windows;
        
        for (const auto& constraint : spatial_requirements) {
            auto windows = findAvailableTimeWindows(constraint.location, earliest_start, latest_end);
            available_windows.push_back(windows);
        }

        // Find overlapping time windows (simplified approach)
        if (!available_windows.empty()) {
            // Use the first constraint's windows as starting point
            for (const auto& window_start : available_windows[0]) {
                auto window_end = window_start + std::chrono::minutes(10); // Assume 10-minute windows
                
                if (window_end > latest_end) {
                    window_end = latest_end;
                }

                bool window_feasible = true;
                for (const auto& constraint : spatial_requirements) {
                    if (!checkLocationAvailability(constraint.location, window_start, window_end)) {
                        window_feasible = false;
                        break;
                    }
                }

                if (window_feasible) {
                    result.optimal_start_time = window_start;
                    result.optimal_end_time = window_end;
                    result.required_constraints = spatial_requirements;
                    result.confidence_score = 0.8; // High confidence for first valid window
                    result.feasible = true;
                    break;
                }
            }
        }

        if (!result.feasible) {
            // Fallback: use earliest possible time
            result.optimal_start_time = earliest_start;
            result.optimal_end_time = earliest_start + std::chrono::minutes(5);
            result.confidence_score = 0.1; // Low confidence
        }

        logger().info() << "[SpaceTimeIntegrator] Temporal planning result - Feasible: " << result.feasible 
                       << ", Confidence: " << result.confidence_score;

    } catch (const std::exception& e) {
        logger().error() << "[SpaceTimeIntegrator] Failed to find optimal time window: " << e.what();
    }

    return result;
}

bool SpaceTimeIntegrator::integrateWithActionScheduler(ActionScheduler* scheduler)
{
    if (!scheduler) {
        logger().warn() << "[SpaceTimeIntegrator] Cannot integrate with null ActionScheduler";
        return false;
    }

    _action_scheduler = scheduler;
    
    logger().info() << "[SpaceTimeIntegrator] Successfully integrated with ActionScheduler";
    return true;
}

void SpaceTimeIntegrator::detachFromActionScheduler()
{
    _action_scheduler = nullptr;
    logger().info() << "[SpaceTimeIntegrator] Detached from ActionScheduler";
}

Handle SpaceTimeIntegrator::createSpatialTemporalAtom(const Handle& atom,
                                                    const octomap::point3d& location, 
                                                    const std::chrono::system_clock::time_point& time)
{
    try {
        Handle location_atom = convertOctomapToAtomSpace(location);
        Handle time_atom = convertTimePointToAtomSpace(time);
        
        HandleSeq spatiotemporal_content;
        spatiotemporal_content.push_back(atom);
        spatiotemporal_content.push_back(location_atom);
        spatiotemporal_content.push_back(time_atom);
        spatiotemporal_content.push_back(_spatial_context);
        spatiotemporal_content.push_back(_temporal_context);
        
        Handle spatiotemporal_atom = _atomspace->add_link(EVALUATION_LINK, std::move(spatiotemporal_content));
        
        TruthValuePtr tv = SimpleTruthValue::createTV(0.9, 0.9);
        spatiotemporal_atom->setTruthValue(tv);
        
        return spatiotemporal_atom;
        
    } catch (const std::exception& e) {
        logger().warn() << "[SpaceTimeIntegrator] Failed to create spatial-temporal atom: " << e.what();
        return Handle::UNDEFINED;
    }
}

Handle SpaceTimeIntegrator::createTrajectoryAtom(const Handle& atom, const Trajectory& trajectory)
{
    try {
        HandleSeq trajectory_content;
        trajectory_content.push_back(atom);
        
        // Create sequence of trajectory points
        for (const auto& point : trajectory) {
            Handle location_atom = convertOctomapToAtomSpace(point.location);
            Handle time_atom = convertTimePointToAtomSpace(point.time);
            
            HandleSeq point_content;
            point_content.push_back(location_atom);
            point_content.push_back(time_atom);
            Handle trajectory_point = _atomspace->add_link(LIST_LINK, std::move(point_content));
            
            trajectory_content.push_back(trajectory_point);
        }
        
        trajectory_content.push_back(_trajectory_context);
        Handle trajectory_atom = _atomspace->add_link(EVALUATION_LINK, std::move(trajectory_content));
        
        TruthValuePtr tv = SimpleTruthValue::createTV(0.8, 0.9);
        trajectory_atom->setTruthValue(tv);
        
        return trajectory_atom;
        
    } catch (const std::exception& e) {
        logger().warn() << "[SpaceTimeIntegrator] Failed to create trajectory atom: " << e.what();
        return Handle::UNDEFINED;
    }
}

std::vector<Handle> SpaceTimeIntegrator::querySpatialTemporalAtoms(
    const octomap::point3d& location,
    const std::chrono::system_clock::time_point& time_start,
    const std::chrono::system_clock::time_point& time_end) const
{
    std::vector<Handle> result;

    if (!isValidLocation(location) || !isValidTimePoint(time_start) || !isValidTimePoint(time_end)) {
        return result;
    }

    // Query AtomSpace for spatial-temporal atoms in the given range
    // This is a simplified implementation - a real system would use more sophisticated AtomSpace queries
    
    return result;
}

void SpaceTimeIntegrator::stepTimeUnit()
{
    _spacetime_map->step_time_unit();
    _current_time = _spacetime_map->get_current_time();
}

void SpaceTimeIntegrator::setCurrentTime(const std::chrono::system_clock::time_point& time)
{
    _current_time = time;
}

std::chrono::system_clock::time_point SpaceTimeIntegrator::getCurrentTime() const
{
    return _current_time;
}

std::string SpaceTimeIntegrator::getStatusInfo() const
{
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"spatial_resolution\": " << _config.spatial_resolution << ",\n";
    oss << "  \"time_resolution_ms\": " << _config.time_resolution.count() << ",\n";
    oss << "  \"time_units\": " << _config.time_units << ",\n";
    oss << "  \"spatial_constraints_enabled\": " << (_config.enable_spatial_constraints ? "true" : "false") << ",\n";
    oss << "  \"trajectory_planning_enabled\": " << (_config.enable_trajectory_planning ? "true" : "false") << ",\n";
    oss << "  \"timeline_reasoning_enabled\": " << (_config.enable_timeline_reasoning ? "true" : "false") << ",\n";
    oss << "  \"tracked_atoms\": " << _atom_constraints.size() << ",\n";
    oss << "  \"occupied_locations\": " << _location_occupancy.size() << ",\n";
    oss << "  \"action_scheduler_integrated\": " << (_action_scheduler ? "true" : "false") << "\n";
    oss << "}";
    return oss.str();
}

size_t SpaceTimeIntegrator::getMemoryUsage() const
{
    size_t usage = sizeof(*this);
    usage += _atom_constraints.size() * (sizeof(Handle) + sizeof(std::vector<SpatialConstraint>));
    usage += _location_occupancy.size() * (sizeof(octomap::point3d) + sizeof(std::vector<Handle>));
    return usage;
}

void SpaceTimeIntegrator::clearTimeline(const std::chrono::system_clock::time_point& before_time)
{
    // Clear old timeline data to manage memory usage
    // This is a placeholder - the actual implementation would need to carefully remove old data
    logger().info() << "[SpaceTimeIntegrator] Timeline cleanup requested for data before specified time";
}

// Private helper methods

bool SpaceTimeIntegrator::isValidLocation(const octomap::point3d& location) const
{
    // Basic validation - could be enhanced with bounds checking
    return std::isfinite(location.x()) && std::isfinite(location.y()) && std::isfinite(location.z());
}

bool SpaceTimeIntegrator::isValidTimePoint(const std::chrono::system_clock::time_point& time) const
{
    // Basic validation
    return time != std::chrono::system_clock::time_point{};
}

bool SpaceTimeIntegrator::checkSpatialConstraint(const SpatialConstraint& constraint) const
{
    if (constraint.atom == Handle::UNDEFINED || !isValidLocation(constraint.location)) {
        return false;
    }

    // Check if the constraint can be satisfied
    return checkLocationAvailability(constraint.location, constraint.start_time, constraint.end_time);
}

std::vector<Handle> SpaceTimeIntegrator::findConflictingAtoms(const octomap::point3d& location,
                                                           const std::chrono::system_clock::time_point& time) const
{
    return getAtomsAtLocation(location, time);
}

double SpaceTimeIntegrator::calculateTrajectoryLength(const Trajectory& trajectory) const
{
    if (trajectory.size() < 2) {
        return 0.0;
    }

    double total_length = 0.0;
    for (size_t i = 1; i < trajectory.size(); ++i) {
        octomap::point3d delta = trajectory[i].location - trajectory[i-1].location;
        total_length += delta.norm();
    }

    return total_length;
}

bool SpaceTimeIntegrator::isTrajectoryCollisionFree(const Trajectory& trajectory) const
{
    for (const auto& point : trajectory) {
        std::vector<Handle> conflicts = findConflictingAtoms(point.location, point.time);
        if (!conflicts.empty()) {
            // Check if any conflict is not the trajectory atom itself
            for (const Handle& conflict : conflicts) {
                if (conflict != point.associated_atom) {
                    return false;
                }
            }
        }
    }
    return true;
}

std::vector<std::chrono::system_clock::time_point> SpaceTimeIntegrator::findAvailableTimeWindows(
    const octomap::point3d& location,
    const std::chrono::system_clock::time_point& start_search,
    const std::chrono::system_clock::time_point& end_search) const
{
    std::vector<std::chrono::system_clock::time_point> windows;

    auto current_time = start_search;
    while (current_time < end_search) {
        if (getAtomsAtLocation(location, current_time).empty()) {
            windows.push_back(current_time);
        }
        current_time += _config.time_resolution;
    }

    return windows;
}

void SpaceTimeIntegrator::recordSpatialTemporalEvent(const Handle& atom,
                                                   const octomap::point3d& location,
                                                   const std::chrono::system_clock::time_point& time)
{
    createSpatialTemporalAtom(atom, location, time);
}

void SpaceTimeIntegrator::updateSpatialTemporalIndex(const Handle& atom,
                                                   const octomap::point3d& location,
                                                   const std::chrono::system_clock::time_point& time,
                                                   bool add)
{
    // Update internal indexing structures
    if (add) {
        // Add to index
    } else {
        // Remove from index
    }
}

octomap::point3d SpaceTimeIntegrator::convertAtomSpaceToOctomap(const Handle& location_atom) const
{
    // Simplified conversion - would need proper implementation based on AtomSpace representation
    return octomap::point3d(0.0, 0.0, 0.0);
}

Handle SpaceTimeIntegrator::convertOctomapToAtomSpace(const octomap::point3d& point) const
{
    try {
        std::ostringstream oss;
        oss << "Location_" << point.x() << "_" << point.y() << "_" << point.z();
        return _atomspace->add_node(CONCEPT_NODE, oss.str());
    } catch (const std::exception& e) {
        logger().warn() << "[SpaceTimeIntegrator] Failed to convert point to AtomSpace: " << e.what();
        return Handle::UNDEFINED;
    }
}

std::chrono::system_clock::time_point SpaceTimeIntegrator::convertAtomSpaceToTimePoint(const Handle& time_atom) const
{
    // Simplified conversion - would need proper implementation
    return std::chrono::system_clock::now();
}

Handle SpaceTimeIntegrator::convertTimePointToAtomSpace(const std::chrono::system_clock::time_point& time) const
{
    try {
        auto time_t = std::chrono::system_clock::to_time_t(time);
        std::ostringstream oss;
        oss << "Time_" << time_t;
        return _atomspace->add_node(CONCEPT_NODE, oss.str());
    } catch (const std::exception& e) {
        logger().warn() << "[SpaceTimeIntegrator] Failed to convert time to AtomSpace: " << e.what();
        return Handle::UNDEFINED;
    }
}