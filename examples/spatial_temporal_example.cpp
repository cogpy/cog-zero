/*
 * examples/spatial_temporal_example.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Example demonstrating spatial-temporal planning integration
 * Part of AZ-SPATIAL-001: Integrate spacetime for temporal planning
 */

#include <iostream>
#include <chrono>
#include <thread>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Handle.h>
#include <opencog/atoms/atom_types/types.h>
#include <octomap/octomap.h>

#include "opencog/agentzero/SpaceTimeIntegrator.h"

using namespace opencog;
using namespace opencog::agentzero;

int main()
{
    std::cout << "=== Agent-Zero Spatial-Temporal Planning Example ===" << std::endl;
    std::cout << "AZ-SPATIAL-001: Integrate spacetime for temporal planning" << std::endl;
    std::cout << std::endl;

    try {
        // Create AtomSpace
        AtomSpacePtr atomspace = createAtomSpace();
        std::cout << "✓ AtomSpace created" << std::endl;

        // Create SpaceTimeIntegrator
        auto integrator = std::make_unique<SpaceTimeIntegrator>(atomspace);
        std::cout << "✓ SpaceTimeIntegrator created" << std::endl;

        // Configure the integrator
        SpaceTimeIntegrator::Configuration config;
        config.spatial_resolution = 0.1;
        config.time_resolution = std::chrono::milliseconds(100);
        config.time_units = 100;
        config.enable_spatial_constraints = true;
        config.enable_trajectory_planning = true;
        config.enable_timeline_reasoning = true;

        integrator->configure(config);
        std::cout << "✓ SpaceTimeIntegrator configured" << std::endl;

        // Create some test atoms
        Handle robot_atom = atomspace->add_node(CONCEPT_NODE, "Robot");
        Handle goal_atom = atomspace->add_node(CONCEPT_NODE, "Goal");
        Handle obstacle_atom = atomspace->add_node(CONCEPT_NODE, "Obstacle");

        std::cout << "✓ Test atoms created" << std::endl;

        // Define spatial locations
        octomap::point3d robot_start(0.0, 0.0, 0.0);
        octomap::point3d goal_location(5.0, 5.0, 0.0);
        octomap::point3d obstacle_location(2.5, 2.5, 0.0);

        auto current_time = std::chrono::system_clock::now();

        // Insert atoms at their initial locations
        integrator->insertAtomAtLocation(robot_atom, robot_start, current_time);
        integrator->insertAtomAtLocation(goal_atom, goal_location, current_time);
        integrator->insertAtomAtLocation(obstacle_atom, obstacle_location, current_time);

        std::cout << "✓ Atoms placed in spatial-temporal map" << std::endl;

        // Demonstrate spatial relationship queries
        std::cout << std::endl << "=== Spatial Relationship Queries ===" << std::endl;

        double distance = integrator->getDistanceBetween(robot_atom, goal_atom, current_time);
        std::cout << "Distance from robot to goal: " << distance << " meters" << std::endl;

        auto spatial_relations = integrator->getSpatialRelations(robot_atom, goal_atom, obstacle_atom, current_time);
        if (spatial_relations.valid) {
            std::cout << "Spatial relations (robot observing goal relative to obstacle):" << std::endl;
            std::cout << "  X-relation: " << static_cast<int>(spatial_relations.x_relation) << std::endl;
            std::cout << "  Y-relation: " << static_cast<int>(spatial_relations.y_relation) << std::endl;
            std::cout << "  Z-relation: " << static_cast<int>(spatial_relations.z_relation) << std::endl;
        }

        // Demonstrate trajectory planning
        std::cout << std::endl << "=== Trajectory Planning ===" << std::endl;

        auto trajectory_start_time = current_time + std::chrono::seconds(5);
        auto trajectory_end_time = trajectory_start_time + std::chrono::minutes(2);

        SpaceTimeIntegrator::Trajectory planned_trajectory;
        bool trajectory_success = integrator->planTrajectory(
            robot_atom,
            robot_start,
            goal_location,
            trajectory_start_time,
            trajectory_end_time,
            planned_trajectory
        );

        if (trajectory_success) {
            std::cout << "✓ Trajectory planned successfully with " 
                     << planned_trajectory.size() << " waypoints" << std::endl;
            
            // Show first and last waypoints
            if (!planned_trajectory.empty()) {
                const auto& first = planned_trajectory.front();
                const auto& last = planned_trajectory.back();
                
                std::cout << "  Start: (" << first.location.x() << ", " 
                         << first.location.y() << ", " << first.location.z() << ")" << std::endl;
                std::cout << "  End: (" << last.location.x() << ", " 
                         << last.location.y() << ", " << last.location.z() << ")" << std::endl;
            }
        } else {
            std::cout << "✗ Trajectory planning failed" << std::endl;
        }

        // Demonstrate spatial constraint validation
        std::cout << std::endl << "=== Spatial Constraint Validation ===" << std::endl;

        std::vector<SpaceTimeIntegrator::SpatialConstraint> constraints;
        SpaceTimeIntegrator::SpatialConstraint constraint;
        constraint.atom = robot_atom;
        constraint.location = octomap::point3d(1.0, 1.0, 0.0);
        constraint.tolerance = 0.2;
        constraint.start_time = current_time + std::chrono::seconds(10);
        constraint.end_time = constraint.start_time + std::chrono::minutes(1);
        constraints.push_back(constraint);

        bool constraints_valid = integrator->validateSpatialConstraints(constraints);
        std::cout << "Spatial constraints validation: " 
                 << (constraints_valid ? "✓ Valid" : "✗ Invalid") << std::endl;

        // Demonstrate location availability checking
        bool location_available = integrator->checkLocationAvailability(
            octomap::point3d(3.0, 3.0, 0.0),
            current_time + std::chrono::seconds(15),
            current_time + std::chrono::seconds(75)
        );
        std::cout << "Location availability: " 
                 << (location_available ? "✓ Available" : "✗ Occupied") << std::endl;

        // Demonstrate optimal time window finding
        std::cout << std::endl << "=== Optimal Time Window Planning ===" << std::endl;

        Handle planning_action = atomspace->add_node(CONCEPT_NODE, "PlanningAction");
        std::vector<octomap::point3d> preferred_locations = {
            octomap::point3d(4.0, 4.0, 0.0),
            octomap::point3d(3.0, 4.0, 0.0),
            octomap::point3d(4.0, 3.0, 0.0)
        };

        std::vector<SpaceTimeIntegrator::SpatialConstraint> planning_requirements;
        for (const auto& loc : preferred_locations) {
            SpaceTimeIntegrator::SpatialConstraint req;
            req.atom = planning_action;
            req.location = loc;
            req.tolerance = 0.1;
            req.start_time = current_time + std::chrono::minutes(1);
            req.end_time = current_time + std::chrono::minutes(10);
            planning_requirements.push_back(req);
        }

        auto planning_result = integrator->findOptimalTimeWindow(
            planning_action,
            planning_requirements,
            current_time + std::chrono::minutes(1),
            current_time + std::chrono::minutes(10)
        );

        if (planning_result.feasible) {
            std::cout << "✓ Optimal time window found" << std::endl;
            std::cout << "  Confidence: " << planning_result.confidence_score << std::endl;
            std::cout << "  Required constraints: " << planning_result.required_constraints.size() << std::endl;
        } else {
            std::cout << "✗ No feasible time window found" << std::endl;
        }

        // Demonstrate AtomSpace integration
        std::cout << std::endl << "=== AtomSpace Integration ===" << std::endl;

        Handle spatiotemporal_atom = integrator->createSpatialTemporalAtom(
            robot_atom, 
            robot_start, 
            current_time
        );

        if (spatiotemporal_atom != Handle::UNDEFINED) {
            std::cout << "✓ Spatial-temporal atom created: " << spatiotemporal_atom << std::endl;
            auto tv = spatiotemporal_atom->getTruthValue();
            if (tv) {
                std::cout << "  Truth value: strength=" << tv->get_mean() 
                         << ", confidence=" << tv->get_confidence() << std::endl;
            }
        }

        if (trajectory_success && !planned_trajectory.empty()) {
            Handle trajectory_atom = integrator->createTrajectoryAtom(robot_atom, planned_trajectory);
            if (trajectory_atom != Handle::UNDEFINED) {
                std::cout << "✓ Trajectory atom created: " << trajectory_atom << std::endl;
            }
        }

        // Show status information
        std::cout << std::endl << "=== System Status ===" << std::endl;
        std::cout << integrator->getStatusInfo() << std::endl;

        std::cout << std::endl << "Memory usage: " << integrator->getMemoryUsage() << " bytes" << std::endl;

        // Demonstrate timeline queries
        std::cout << std::endl << "=== Timeline Queries ===" << std::endl;

        auto robot_timeline = integrator->getAtomTimeline(robot_atom);
        std::cout << "Robot timeline has " << robot_timeline.size() << " time points" << std::endl;

        auto robot_locations = integrator->getAtomLocationsAtCurrentTime(robot_atom);
        std::cout << "Robot current locations: " << robot_locations.size() << std::endl;

        std::cout << std::endl << "=== Example Complete ===" << std::endl;
        std::cout << "✓ Spatial-temporal planning integration demonstrated successfully" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}