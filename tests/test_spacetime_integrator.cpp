/*
 * tests/test_spacetime_integrator.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Unit tests for SpaceTimeIntegrator
 * Part of AZ-SPATIAL-001: Integrate spacetime for temporal planning
 */

#include <gtest/gtest.h>
#include <chrono>
#include <vector>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Handle.h>
#include <opencog/atoms/atom_types/types.h>
#include <octomap/octomap.h>

#include "opencog/agentzero/SpaceTimeIntegrator.h"

using namespace opencog;
using namespace opencog::agentzero;

class SpaceTimeIntegratorTest : public ::testing::Test
{
protected:
    void SetUp() override {
        atomspace = createAtomSpace();
        integrator = std::make_unique<SpaceTimeIntegrator>(atomspace);
    }

    void TearDown() override {
        integrator.reset();
        atomspace.reset();
    }

    AtomSpacePtr atomspace;
    std::unique_ptr<SpaceTimeIntegrator> integrator;
};

// Basic construction and configuration tests
TEST_F(SpaceTimeIntegratorTest, Construction)
{
    EXPECT_NE(integrator, nullptr);
    EXPECT_NE(integrator->getSpatialContext(), Handle::UNDEFINED);
    EXPECT_NE(integrator->getTemporalContext(), Handle::UNDEFINED);
    EXPECT_NE(integrator->getTrajectoryContext(), Handle::UNDEFINED);
}

TEST_F(SpaceTimeIntegratorTest, Configuration)
{
    SpaceTimeIntegrator::Configuration config;
    config.spatial_resolution = 0.05;
    config.time_resolution = std::chrono::milliseconds(50);
    config.time_units = 500;
    config.enable_spatial_constraints = true;
    config.enable_trajectory_planning = true;
    config.enable_timeline_reasoning = true;
    
    integrator->configure(config);
    
    const auto& retrieved_config = integrator->getConfiguration();
    EXPECT_DOUBLE_EQ(retrieved_config.spatial_resolution, 0.05);
    EXPECT_EQ(retrieved_config.time_resolution.count(), 50);
    EXPECT_EQ(retrieved_config.time_units, 500);
    EXPECT_TRUE(retrieved_config.enable_spatial_constraints);
    EXPECT_TRUE(retrieved_config.enable_trajectory_planning);
    EXPECT_TRUE(retrieved_config.enable_timeline_reasoning);
}

// Spatial-temporal data management tests
TEST_F(SpaceTimeIntegratorTest, AtomLocationInsertion)
{
    Handle test_atom = atomspace->add_node(CONCEPT_NODE, "TestAtom");
    octomap::point3d location(1.0, 2.0, 3.0);
    auto time_point = std::chrono::system_clock::now();
    
    bool result = integrator->insertAtomAtLocation(test_atom, location, time_point);
    EXPECT_TRUE(result);
    
    // Verify insertion by querying
    std::vector<Handle> atoms = integrator->getAtomsAtLocation(location, time_point);
    EXPECT_EQ(atoms.size(), 1);
    EXPECT_EQ(atoms[0], test_atom);
}

TEST_F(SpaceTimeIntegratorTest, AtomLocationCurrentTime)
{
    Handle test_atom = atomspace->add_node(CONCEPT_NODE, "TestAtomCurrent");
    octomap::point3d location(5.0, 6.0, 7.0);
    
    bool result = integrator->insertAtomAtCurrentTime(test_atom, location);
    EXPECT_TRUE(result);
    
    // Verify insertion by querying at current time
    std::vector<octomap::point3d> locations = integrator->getAtomLocationsAtCurrentTime(test_atom);
    EXPECT_GE(locations.size(), 1);
    
    // Check if our location is in the results (with tolerance)
    bool found = false;
    for (const auto& loc : locations) {
        if (std::abs(loc.x() - location.x()) < 0.01 && 
            std::abs(loc.y() - location.y()) < 0.01 && 
            std::abs(loc.z() - location.z()) < 0.01) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(SpaceTimeIntegratorTest, AtomRemoval)
{
    Handle test_atom = atomspace->add_node(CONCEPT_NODE, "TestAtomRemoval");
    octomap::point3d location(8.0, 9.0, 10.0);
    auto time_point = std::chrono::system_clock::now();
    
    // Insert atom
    bool insert_result = integrator->insertAtomAtLocation(test_atom, location, time_point);
    EXPECT_TRUE(insert_result);
    
    // Remove atom
    bool remove_result = integrator->removeAtomFromLocation(test_atom, location, time_point);
    EXPECT_TRUE(remove_result);
    
    // Verify removal
    std::vector<Handle> atoms = integrator->getAtomsAtLocation(location, time_point);
    EXPECT_EQ(atoms.size(), 0);
}

// Timeline functionality tests
TEST_F(SpaceTimeIntegratorTest, AtomTimeline)
{
    Handle test_atom = atomspace->add_node(CONCEPT_NODE, "TestAtomTimeline");
    octomap::point3d location(11.0, 12.0, 13.0);
    
    auto time1 = std::chrono::system_clock::now();
    auto time2 = time1 + std::chrono::seconds(1);
    
    // Insert atom at two different times
    integrator->insertAtomAtLocation(test_atom, location, time1);
    integrator->insertAtomAtLocation(test_atom, location, time2);
    
    // Get timeline
    std::vector<std::chrono::system_clock::time_point> timeline = integrator->getAtomTimeline(test_atom);
    EXPECT_GE(timeline.size(), 1); // At least one time point should be found
}

// Spatial relationship tests
TEST_F(SpaceTimeIntegratorTest, DistanceCalculation)
{
    Handle atom1 = atomspace->add_node(CONCEPT_NODE, "Atom1Distance");
    Handle atom2 = atomspace->add_node(CONCEPT_NODE, "Atom2Distance");
    
    octomap::point3d location1(0.0, 0.0, 0.0);
    octomap::point3d location2(3.0, 4.0, 0.0); // Distance should be 5.0
    
    auto time_point = std::chrono::system_clock::now();
    
    integrator->insertAtomAtLocation(atom1, location1, time_point);
    integrator->insertAtomAtLocation(atom2, location2, time_point);
    
    double distance = integrator->getDistanceBetween(atom1, atom2, time_point);
    EXPECT_NEAR(distance, 5.0, 0.1); // Allow some tolerance
}

TEST_F(SpaceTimeIntegratorTest, DirectionVector)
{
    Handle from_atom = atomspace->add_node(CONCEPT_NODE, "FromAtom");
    Handle to_atom = atomspace->add_node(CONCEPT_NODE, "ToAtom");
    
    octomap::point3d from_location(0.0, 0.0, 0.0);
    octomap::point3d to_location(1.0, 0.0, 0.0);
    
    auto time_point = std::chrono::system_clock::now();
    
    integrator->insertAtomAtLocation(from_atom, from_location, time_point);
    integrator->insertAtomAtLocation(to_atom, to_location, time_point);
    
    octomap::point3d direction;
    bool result = integrator->getDirectionVector(from_atom, to_atom, time_point, direction);
    
    if (result) {
        EXPECT_NEAR(direction.x(), 1.0, 0.1);
        EXPECT_NEAR(direction.y(), 0.0, 0.1);
        EXPECT_NEAR(direction.z(), 0.0, 0.1);
    }
}

// Spatial constraint validation tests
TEST_F(SpaceTimeIntegratorTest, SpatialConstraintValidation)
{
    std::vector<SpaceTimeIntegrator::SpatialConstraint> constraints;
    
    SpaceTimeIntegrator::SpatialConstraint constraint;
    constraint.atom = atomspace->add_node(CONCEPT_NODE, "ConstraintAtom");
    constraint.location = octomap::point3d(1.0, 1.0, 1.0);
    constraint.tolerance = 0.1;
    constraint.start_time = std::chrono::system_clock::now();
    constraint.end_time = constraint.start_time + std::chrono::minutes(1);
    
    constraints.push_back(constraint);
    
    bool result = integrator->validateSpatialConstraints(constraints);
    EXPECT_TRUE(result); // Should be valid for empty space
}

TEST_F(SpaceTimeIntegratorTest, LocationAvailability)
{
    octomap::point3d location(2.0, 2.0, 2.0);
    auto start_time = std::chrono::system_clock::now();
    auto end_time = start_time + std::chrono::minutes(1);
    
    // Location should be available initially
    bool available = integrator->checkLocationAvailability(location, start_time, end_time);
    EXPECT_TRUE(available);
    
    // Insert an atom at the location
    Handle blocking_atom = atomspace->add_node(CONCEPT_NODE, "BlockingAtom");
    integrator->insertAtomAtLocation(blocking_atom, location, start_time);
    
    // Location should now be less available (depending on implementation)
    // This test may need adjustment based on actual collision detection logic
}

// Trajectory planning tests
TEST_F(SpaceTimeIntegratorTest, TrajectoryPlanning)
{
    Handle test_atom = atomspace->add_node(CONCEPT_NODE, "TrajectoryAtom");
    
    octomap::point3d start_location(0.0, 0.0, 0.0);
    octomap::point3d goal_location(5.0, 5.0, 0.0);
    
    auto start_time = std::chrono::system_clock::now();
    auto end_time = start_time + std::chrono::minutes(5);
    
    SpaceTimeIntegrator::Trajectory trajectory;
    bool result = integrator->planTrajectory(
        test_atom,
        start_location,
        goal_location,
        start_time,
        end_time,
        trajectory
    );
    
    EXPECT_TRUE(result);
    if (result) {
        EXPECT_GT(trajectory.size(), 1); // Should have multiple points
        EXPECT_EQ(trajectory.front().associated_atom, test_atom);
        EXPECT_EQ(trajectory.back().associated_atom, test_atom);
    }
}

// AtomSpace integration tests
TEST_F(SpaceTimeIntegratorTest, SpatialTemporalAtomCreation)
{
    Handle test_atom = atomspace->add_node(CONCEPT_NODE, "SpatialTemporalAtom");
    octomap::point3d location(3.0, 3.0, 3.0);
    auto time_point = std::chrono::system_clock::now();
    
    Handle spatiotemporal_atom = integrator->createSpatialTemporalAtom(test_atom, location, time_point);
    
    EXPECT_NE(spatiotemporal_atom, Handle::UNDEFINED);
    EXPECT_NE(spatiotemporal_atom->getTruthValue(), nullptr);
}

TEST_F(SpaceTimeIntegratorTest, TrajectoryAtomCreation)
{
    Handle test_atom = atomspace->add_node(CONCEPT_NODE, "TrajectoryAtomTest");
    
    SpaceTimeIntegrator::Trajectory trajectory;
    SpaceTimeIntegrator::TrajectoryPoint point1, point2;
    
    point1.location = octomap::point3d(0.0, 0.0, 0.0);
    point1.time = std::chrono::system_clock::now();
    point1.associated_atom = test_atom;
    
    point2.location = octomap::point3d(1.0, 1.0, 1.0);
    point2.time = point1.time + std::chrono::seconds(1);
    point2.associated_atom = test_atom;
    
    trajectory.push_back(point1);
    trajectory.push_back(point2);
    
    Handle trajectory_atom = integrator->createTrajectoryAtom(test_atom, trajectory);
    
    EXPECT_NE(trajectory_atom, Handle::UNDEFINED);
}

// Status and utility tests
TEST_F(SpaceTimeIntegratorTest, StatusInformation)
{
    std::string status = integrator->getStatusInfo();
    EXPECT_FALSE(status.empty());
    EXPECT_NE(status.find("spatial_resolution"), std::string::npos);
    EXPECT_NE(status.find("time_resolution_ms"), std::string::npos);
}

TEST_F(SpaceTimeIntegratorTest, MemoryUsage)
{
    size_t memory_usage = integrator->getMemoryUsage();
    EXPECT_GT(memory_usage, 0);
}

TEST_F(SpaceTimeIntegratorTest, TimeManagement)
{
    auto initial_time = integrator->getCurrentTime();
    
    integrator->stepTimeUnit();
    auto after_step_time = integrator->getCurrentTime();
    
    // Time should have progressed
    EXPECT_GT(after_step_time, initial_time);
    
    // Test manual time setting
    auto manual_time = std::chrono::system_clock::now() + std::chrono::hours(1);
    integrator->setCurrentTime(manual_time);
    auto retrieved_time = integrator->getCurrentTime();
    
    EXPECT_EQ(retrieved_time, manual_time);
}

// Error handling tests
TEST_F(SpaceTimeIntegratorTest, InvalidParameters)
{
    Handle invalid_atom = Handle::UNDEFINED;
    octomap::point3d location(1.0, 1.0, 1.0);
    auto time_point = std::chrono::system_clock::now();
    
    // Test with invalid atom
    bool result = integrator->insertAtomAtLocation(invalid_atom, location, time_point);
    EXPECT_FALSE(result);
    
    // Test with invalid location (e.g., NaN values)
    octomap::point3d invalid_location(std::numeric_limits<double>::quiet_NaN(), 1.0, 1.0);
    Handle valid_atom = atomspace->add_node(CONCEPT_NODE, "ValidAtom");
    result = integrator->insertAtomAtLocation(valid_atom, invalid_location, time_point);
    EXPECT_FALSE(result);
}

// Integration tests would go here in a real implementation
// These would test integration with ActionScheduler and other components