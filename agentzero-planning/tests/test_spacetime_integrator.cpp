#include "test_runner.h"

#include <opencog/agentzero/planning/SpaceTimeIntegrator.h>
#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/atom_types/types.h>

using namespace opencog;
using namespace opencog::agentzero::planning;

static AtomSpacePtr make_as() { return createAtomSpace(); }

TEST(SpaceTime_Initialize)
{
    auto as = make_as();
    SpaceTimeIntegrator sti(as);
    ASSERT_TRUE(sti.initialize());
    ASSERT_TRUE(sti.isInitialized());
    // In CI without OpenCog spacetime package this is the fallback path.
    ASSERT_FALSE(sti.hasNativeSpacetime());
}

TEST(SpaceTime_InsertAndQuery)
{
    auto as = make_as();
    SpaceTimeIntegrator sti(as);
    sti.initialize();

    Handle robot = as->add_node(CONCEPT_NODE, "robot");
    Point3 loc{1.0, 2.0, 0.0};
    auto now = sti.getCurrentTime();
    ASSERT_TRUE(sti.insertAtomAtLocation(robot, loc, now));
    auto atoms = sti.getAtomsAtLocation(loc, now);
    ASSERT_EQ(atoms.size(), static_cast<size_t>(1));
    ASSERT_EQ(atoms[0], robot);
    auto locs = sti.getAtomLocations(robot, now);
    ASSERT_FALSE(locs.empty());
}

TEST(SpaceTime_Trajectory)
{
    auto as = make_as();
    SpaceTimeIntegrator sti(as);
    sti.initialize();

    Handle agent = as->add_node(CONCEPT_NODE, "agent");
    Point3 start{0.0, 0.0, 0.0};
    Point3 goal{1.0, 0.0, 0.0};
    auto t0 = sti.getCurrentTime();
    auto t1 = t0 + std::chrono::seconds(5);
    SpaceTimeIntegrator::Trajectory traj;
    ASSERT_TRUE(sti.planTrajectory(agent, start, goal, t0, t1, traj));
    ASSERT_GE(traj.size(), static_cast<size_t>(2));
    ASSERT_TRUE(sti.validateTrajectory(traj));
}

TEST(SpaceTime_OptimalWindow)
{
    auto as = make_as();
    SpaceTimeIntegrator sti(as);
    sti.initialize();

    Handle action = as->add_node(CONCEPT_NODE, "dock");
    SpaceTimeIntegrator::SpatialConstraint req;
    req.atom = action;
    req.location = Point3{5.0, 5.0, 0.0};
    auto t0 = sti.getCurrentTime();
    auto t1 = t0 + std::chrono::seconds(10);
    auto result = sti.findOptimalTimeWindow(action, {req}, t0, t1);
    ASSERT_TRUE(result.feasible);
    ASSERT_GT(result.confidence_score, 0.0);
}

TEST(SpaceTime_Distance)
{
    auto as = make_as();
    SpaceTimeIntegrator sti(as);
    sti.initialize();
    Handle a = as->add_node(CONCEPT_NODE, "a");
    Handle b = as->add_node(CONCEPT_NODE, "b");
    auto now = sti.getCurrentTime();
    sti.insertAtomAtLocation(a, Point3{0, 0, 0}, now);
    sti.insertAtomAtLocation(b, Point3{3, 4, 0}, now);
    double d = sti.getDistanceBetween(a, b, now);
    ASSERT_NEAR(d, 5.0, 1e-6);
}
