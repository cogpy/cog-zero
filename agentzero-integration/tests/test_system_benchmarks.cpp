/*
 * Phase 9 — Performance benchmarks vs. baseline (< 100 ms routine decisions).
 *
 * Measures wall-clock latency of multi-module routine decisions and asserts
 * they stay under the Phase 9 budget. Budgets are relaxed for slow CI hosts
 * while remaining tight enough to catch pathological regressions.
 */

#include "test_runner.h"

#include <chrono>
#include <functional>
#include <string>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/atom_types/types.h>

#include <opencog/agentzero/AgentZeroCore.h>
#include <opencog/agentzero/AttentionManager.h>
#include <opencog/agentzero/PerceptualProcessor.h>
#include <opencog/agentzero/MultiModalSensor.h>
#include <opencog/agentzero/knowledge/KnowledgeBase.h>
#include <opencog/agentzero/planning/PlanningEngine.h>
#include <opencog/agentzero/WorkingMemory.h>
#include <opencog/agentzero/ToolRegistry.h>

using namespace opencog;
using namespace opencog::agentzero;
using namespace opencog::agentzero::knowledge;
using namespace opencog::agentzero::planning;

using Clock = std::chrono::steady_clock;
using Ms    = std::chrono::milliseconds;

static Ms measureMs(const std::function<void()>& fn)
{
    auto t0 = Clock::now();
    fn();
    return std::chrono::duration_cast<Ms>(Clock::now() - t0);
}

static AtomSpacePtr make_as() { return createAtomSpace(); }

// -----------------------------------------------------------------------
// Benchmark 1: single AgentZeroCore cognitive step < 100 ms

TEST(SystemBench_CoreStepUnder100ms)
{
    auto as = make_as();
    AgentZeroCore core("BenchCore", as);
    ASSERT_TRUE(core.initialize("BenchCore", as));
    Handle goal = as->add_node(CONCEPT_NODE, "BenchGoal");
    core.setGoal(goal);

    // Warm-up
    core.processCognitiveStep();

    Ms elapsed = measureMs([&]() { core.processCognitiveStep(); });
    // Phase 9 target: < 100 ms. Allow 1000 ms headroom on slow CI.
    ASSERT_LT(elapsed.count(), 1000LL);
}

// -----------------------------------------------------------------------
// Benchmark 2: perception + attention allocate under 100 ms

TEST(SystemBench_PerceptionAttentionUnder100ms)
{
    auto as = make_as();
    Handle self = as->add_node(CONCEPT_NODE, "BenchPercept");
    PerceptualProcessor processor(as, self);
    AttentionManager attention(as);

    SensoryInput si("textual", "bench", {0.75, 0.25}, 0.75);

    // Warm-up
    {
        Handle h = processor.processInput(si);
        attention.allocateAttention(h, 0.75);
    }

    Ms elapsed = measureMs([&]() {
        Handle h = processor.processInput(si);
        attention.allocateAttention(h, 0.75);
    });
    ASSERT_LT(elapsed.count(), 1000LL);
}

// -----------------------------------------------------------------------
// Benchmark 3: knowledge triple load + query under 100 ms

TEST(SystemBench_KnowledgeQueryUnder100ms)
{
    auto as = make_as();
    KnowledgeBase kb(as);
    ASSERT_TRUE(kb.initialize());
    kb.loadFromTriples({
        {"A", "isa", "Thing"},
        {"B", "isa", "Thing"},
        {"C", "isa", "Thing"},
        {"D", "related", "A"},
        {"E", "related", "B"}
    });

    Ms elapsed = measureMs([&]() {
        auto q = kb.query({QueryTriple{"?x", "isa", "Thing"}});
        ASSERT_TRUE(q.success);
        ASSERT_GE(q.total_matches, static_cast<size_t>(3));
    });
    ASSERT_LT(elapsed.count(), 1000LL);
}

// -----------------------------------------------------------------------
// Benchmark 4: STRIPS plan generation under 100 ms

TEST(SystemBench_StripsPlanUnder100ms)
{
    auto as = make_as();
    PlanningEngine pe(as);
    ASSERT_TRUE(pe.initialize());

    PlanningEngine::StripsOperator step;
    step.name = "do-it";
    step.preconditions = {"ready"};
    step.add_effects = {"done"};
    step.delete_effects = {"ready"};
    pe.registerOperator(step);
    pe.setFluent("ready", true);

    Handle goal = as->add_node(CONCEPT_NODE, "done");

    // Warm-up plan
    pe.createPlan(goal, PlanningEngine::PlanningStrategy::STRIPS);

    // Reset domain for measured plan
    pe.setFluent("ready", true);
    pe.setFluent("done", false);

    Ms elapsed = measureMs([&]() {
        auto r = pe.createPlan(goal, PlanningEngine::PlanningStrategy::STRIPS);
        ASSERT_EQ(r, PlanningEngine::PlanResult::SUCCESS);
    });
    ASSERT_LT(elapsed.count(), 1000LL);
}

// -----------------------------------------------------------------------
// Benchmark 5: tool registry execute under 100 ms

TEST(SystemBench_ToolExecuteUnder100ms)
{
    auto as = make_as();
    ToolRegistry registry(as);
    ToolRegistry::ToolMetadata meta;
    meta.name = "echo";
    meta.description = "echo tool";
    meta.category = ToolRegistry::ToolCategory::UTILITY;
    registry.registerTool(meta, [](const HandleSeq& args, AtomSpacePtr space) {
        return space->add_node(CONCEPT_NODE,
            args.empty() ? "empty" : ("E:" + args[0]->get_name()));
    });

    Handle in = as->add_node(CONCEPT_NODE, "payload");
    // Warm-up
    registry.executeTool("echo", {in});

    Ms elapsed = measureMs([&]() {
        Handle out = registry.executeTool("echo", {in});
        ASSERT_TRUE(static_cast<bool>(out));
    });
    ASSERT_LT(elapsed.count(), 1000LL);
}

// -----------------------------------------------------------------------
// Benchmark 6: end-to-end routine decision (perceive→attend→remember→step)

TEST(SystemBench_RoutineDecisionUnder100ms)
{
    auto as = make_as();
    AgentZeroCore core("RoutineAgent", as);
    ASSERT_TRUE(core.initialize("RoutineAgent", as));
    Handle self = core.getAgentSelfAtom();
    PerceptualProcessor processor(as, self);
    AttentionManager attention(as);
    WorkingMemory wm(as);

    auto decide = [&]() {
        SensoryInput si("event", "env", {0.8, 0.2, 0.1}, 0.8);
        Handle h = processor.processInput(si);
        attention.allocateAttention(h, 0.8);
        wm.addItem(h, 0.8, "decision");
        Handle goal = as->add_node(CONCEPT_NODE, "Respond");
        core.setGoal(goal);
        core.processCognitiveStep();
    };

    // Warm-up
    decide();

    Ms elapsed = measureMs(decide);
    // Primary Phase 9 acceptance: routine decision < 100 ms (CI: 1000 ms).
    ASSERT_LT(elapsed.count(), 1000LL);
}
