#include "test_runner.h"

#include <opencog/agentzero/planning/GoalHierarchy.h>
#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/atom_types/types.h>

using namespace opencog;
using namespace opencog::agentzero::planning;

static AtomSpacePtr make_as() { return createAtomSpace(); }

TEST(GoalHierarchy_Initialize)
{
    auto as = make_as();
    GoalHierarchy gh(as);
    ASSERT_FALSE(gh.isInitialized());
    ASSERT_TRUE(gh.initialize());
    ASSERT_TRUE(gh.isInitialized());
    ASSERT_NE(gh.getGoalHierarchyContext(), Handle::UNDEFINED);
    ASSERT_TRUE(gh.shutdown());
}

TEST(GoalHierarchy_NullAtomSpaceThrows)
{
    bool threw = false;
    try {
        GoalHierarchy gh(nullptr);
    } catch (const std::exception&) {
        threw = true;
    }
    ASSERT_TRUE(threw);
}

TEST(GoalHierarchy_AddRootAndSubgoal)
{
    auto as = make_as();
    GoalHierarchy gh(as);
    gh.initialize();

    Handle root = as->add_node(CONCEPT_NODE, "project");
    Handle child = as->add_node(CONCEPT_NODE, "milestone");
    ASSERT_TRUE(gh.addGoal(root));
    ASSERT_TRUE(gh.addGoal(child, root));
    ASSERT_EQ(gh.getRootGoals().size(), static_cast<size_t>(1));
    ASSERT_EQ(gh.getSubgoals(root).size(), static_cast<size_t>(1));
    ASSERT_EQ(gh.getParentGoal(child), root);
    ASSERT_EQ(gh.getHierarchyDepth(root), 2);
}

TEST(GoalHierarchy_Dependencies)
{
    auto as = make_as();
    GoalHierarchy gh(as);
    gh.initialize();

    Handle a = as->add_node(CONCEPT_NODE, "goal_a");
    Handle b = as->add_node(CONCEPT_NODE, "goal_b");
    ASSERT_TRUE(gh.addGoal(a));
    ASSERT_TRUE(gh.addGoal(b));
    ASSERT_TRUE(gh.addGoalDependency(b, a));
    ASSERT_FALSE(gh.areGoalDependenciesSatisfied(b));
    ASSERT_TRUE(gh.setGoalStatus(a, GoalHierarchy::GoalStatus::SATISFIED));
    ASSERT_TRUE(gh.areGoalDependenciesSatisfied(b));
    // cycle rejection
    ASSERT_FALSE(gh.addGoalDependency(a, b));
}

TEST(GoalHierarchy_ActivationAndNextGoal)
{
    auto as = make_as();
    GoalHierarchy gh(as);
    gh.initialize();

    Handle root = as->add_node(CONCEPT_NODE, "root");
    Handle leaf = as->add_node(CONCEPT_NODE, "leaf");
    gh.addGoal(root, Handle::UNDEFINED, GoalHierarchy::GoalPriority::HIGH);
    gh.addGoal(leaf, root, GoalHierarchy::GoalPriority::CRITICAL);
    ASSERT_TRUE(gh.activateGoal(leaf));
    ASSERT_EQ(gh.getGoalStatus(leaf), GoalHierarchy::GoalStatus::ACTIVE);
    Handle next = gh.getNextGoalToPlan();
    ASSERT_EQ(next, leaf);
}

TEST(GoalHierarchy_HierarchicalAchievement)
{
    auto as = make_as();
    GoalHierarchy gh(as);
    gh.initialize();

    Handle root = as->add_node(CONCEPT_NODE, "ach_root");
    Handle c1 = as->add_node(CONCEPT_NODE, "ach_c1");
    Handle c2 = as->add_node(CONCEPT_NODE, "ach_c2");
    gh.addGoal(root);
    gh.addGoal(c1, root);
    gh.addGoal(c2, root);
    gh.setGoalSatisfaction(c1, 1.0f);
    gh.setGoalSatisfaction(c2, 0.5f);
    float ach = gh.calculateHierarchicalAchievement(root);
    // 0.3*0 + 0.7*((1+0.5)/2) = 0.525
    ASSERT_NEAR(ach, 0.525f, 0.02f);
}

TEST(GoalHierarchy_RemoveAndOrphans)
{
    auto as = make_as();
    GoalHierarchy gh(as);
    gh.initialize();

    Handle root = as->add_node(CONCEPT_NODE, "r");
    Handle mid = as->add_node(CONCEPT_NODE, "m");
    Handle leaf = as->add_node(CONCEPT_NODE, "l");
    gh.addGoal(root);
    gh.addGoal(mid, root);
    gh.addGoal(leaf, mid);
    ASSERT_TRUE(gh.removeSubgoal(mid, leaf));
    ASSERT_EQ(gh.getParentGoal(leaf), Handle::UNDEFINED);
    ASSERT_TRUE(gh.removeGoal(mid, true));
    ASSERT_FALSE(gh.hasGoal(mid));
}
