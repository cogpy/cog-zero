#include "test_runner.h"

#include <opencog/agentzero/planning/PlanningEngine.h>
#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/atom_types/types.h>

using namespace opencog;
using namespace opencog::agentzero::planning;

static AtomSpacePtr make_as() { return createAtomSpace(); }

static void load_blocks_domain(PlanningEngine& pe)
{
    // Classic STRIPS blocks-world-ish
    PlanningEngine::StripsOperator pickup;
    pickup.name = "pickup-A";
    pickup.preconditions = {"hand-empty", "clear-A"};
    pickup.add_effects = {"holding-A"};
    pickup.delete_effects = {"hand-empty", "clear-A"};
    pe.registerOperator(pickup);

    PlanningEngine::StripsOperator puton;
    puton.name = "put-A-on-B";
    puton.preconditions = {"holding-A", "clear-B"};
    puton.add_effects = {"A-on-B", "hand-empty", "clear-A"};
    puton.delete_effects = {"holding-A", "clear-B"};
    pe.registerOperator(puton);

    pe.setFluent("hand-empty", true);
    pe.setFluent("clear-A", true);
    pe.setFluent("clear-B", true);
}

TEST(PlanningEngine_Initialize)
{
    auto as = make_as();
    PlanningEngine pe(as);
    ASSERT_TRUE(pe.initialize());
    ASSERT_TRUE(pe.isInitialized());
    ASSERT_NE(pe.getPlanningContext(), Handle::UNDEFINED);
}

TEST(PlanningEngine_StripsPlan)
{
    auto as = make_as();
    PlanningEngine pe(as);
    pe.initialize();
    load_blocks_domain(pe);

    Handle goal = as->add_node(CONCEPT_NODE, "A-on-B");
    auto result = pe.createPlan(goal, PlanningEngine::PlanningStrategy::STRIPS);
    ASSERT_EQ(result, PlanningEngine::PlanResult::SUCCESS);
    const auto* plan = pe.getPlan(goal);
    ASSERT_TRUE(plan != nullptr);
    ASSERT_GE(plan->action_names.size(), static_cast<size_t>(2));
    ASSERT_TRUE(pe.getFluent("hand-empty")); // not yet executed
}

TEST(PlanningEngine_StripsExecute)
{
    auto as = make_as();
    PlanningEngine pe(as);
    pe.initialize();
    load_blocks_domain(pe);

    Handle goal = as->add_node(CONCEPT_NODE, "A-on-B");
    ASSERT_EQ(pe.createPlan(goal, PlanningEngine::PlanningStrategy::STRIPS),
              PlanningEngine::PlanResult::SUCCESS);
    ASSERT_TRUE(pe.startExecution(goal));
    while (pe.getPlan(goal)->status == PlanningEngine::ExecutionStatus::EXECUTING) {
        ASSERT_EQ(pe.stepExecution(goal), PlanningEngine::PlanResult::SUCCESS);
    }
    ASSERT_EQ(pe.getPlan(goal)->status, PlanningEngine::ExecutionStatus::COMPLETED);
    ASSERT_TRUE(pe.getFluent("A-on-B"));
}

TEST(PlanningEngine_HtnPlan)
{
    auto as = make_as();
    PlanningEngine pe(as);
    pe.initialize();

    PlanningEngine::StripsOperator step1;
    step1.name = "gather-materials";
    step1.add_effects = {"materials-ready"};
    pe.registerOperator(step1);

    PlanningEngine::StripsOperator step2;
    step2.name = "assemble";
    step2.preconditions = {"materials-ready"};
    step2.add_effects = {"product-built"};
    pe.registerOperator(step2);

    PlanningEngine::HtnMethod m;
    m.name = "build-product-method";
    m.compound_task = "build-product";
    m.subtasks = {"gather-materials", "assemble"};
    pe.registerMethod(m);

    auto result = pe.createPlanForTask("build-product",
                                       PlanningEngine::PlanningStrategy::HIERARCHICAL);
    ASSERT_EQ(result, PlanningEngine::PlanResult::SUCCESS);
    Handle goal = as->get_node(CONCEPT_NODE, "build-product");
    const auto* plan = pe.getPlan(goal);
    ASSERT_TRUE(plan != nullptr);
    ASSERT_EQ(plan->action_names.size(), static_cast<size_t>(2));
    ASSERT_EQ(plan->action_names[0], std::string("gather-materials"));
    ASSERT_EQ(plan->action_names[1], std::string("assemble"));
}

TEST(PlanningEngine_PlanQuality)
{
    auto as = make_as();
    PlanningEngine pe(as);
    pe.initialize();
    load_blocks_domain(pe);

    Handle goal = as->add_node(CONCEPT_NODE, "A-on-B");
    pe.createPlan(goal, PlanningEngine::PlanningStrategy::STRIPS);
    const auto* plan = pe.getPlan(goal);
    ASSERT_TRUE(plan != nullptr);
    auto q = pe.computePlanQuality(*plan);
    ASSERT_GT(q.overallScore(), 0.0f);
    ASSERT_EQ(q.action_count, static_cast<int>(plan->action_sequence.size()));
}

TEST(PlanningEngine_CancelAndAdapt)
{
    auto as = make_as();
    PlanningEngine pe(as);
    pe.initialize();
    load_blocks_domain(pe);
    Handle goal = as->add_node(CONCEPT_NODE, "A-on-B");
    pe.createPlan(goal, PlanningEngine::PlanningStrategy::STRIPS);
    const auto* plan = pe.getPlan(goal);
    Handle plan_atom = plan->plan_atom;
    ASSERT_TRUE(pe.cancelPlan(plan_atom));
    ASSERT_EQ(pe.getPlanStatus(plan_atom), PlanningEngine::ExecutionStatus::CANCELLED);
}
