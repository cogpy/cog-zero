/*
 * Phase 9 — Full system integration tests (all modules together).
 *
 * Exercises a single shared AtomSpace across:
 *   core, perception, knowledge, planning, learning,
 *   communication, memory, and tools.
 */

#include "test_runner.h"

#include <chrono>
#include <string>
#include <vector>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/atom_types/types.h>
#include <opencog/atoms/base/Handle.h>

// Phase 1 — Core
#include <opencog/agentzero/AgentZeroCore.h>

// Phase 2 — Perception
#include <opencog/agentzero/AttentionManager.h>
#include <opencog/agentzero/MultiModalSensor.h>
#include <opencog/agentzero/PerceptualProcessor.h>

// Phase 3 — Knowledge
#include <opencog/agentzero/knowledge/KnowledgeBase.h>

// Phase 4 — Planning
#include <opencog/agentzero/planning/GoalHierarchy.h>
#include <opencog/agentzero/planning/PlanningEngine.h>

// Phase 5 — Learning
#include <opencog/agentzero/ExperienceManager.h>

// Phase 6 — Communication
#include <opencog/agentzero/communication/LanguageProcessor.h>

// Phase 7 — Memory
#include <opencog/agentzero/WorkingMemory.h>
#include <opencog/agentzero/memory/EpisodicMemory.h>

// Phase 8 — Tools
#include <opencog/agentzero/ToolRegistry.h>
#include <opencog/agentzero/tools/ToolExecutor.h>

using namespace opencog;
using namespace opencog::agentzero;
using namespace opencog::agentzero::knowledge;
using namespace opencog::agentzero::planning;
using namespace opencog::agentzero::communication;
using namespace opencog::agentzero::memory;
using namespace opencog::agentzero::tools;

static AtomSpacePtr make_as() { return createAtomSpace(); }

// -----------------------------------------------------------------------
// Integration 1: every module constructs against one AtomSpace

TEST(FullSystem_AllModulesConstruct)
{
    auto as = make_as();
    Handle self = as->add_node(CONCEPT_NODE, "SystemAgent");

    AgentZeroCore core("SystemAgent", as);
    ASSERT_TRUE(core.initialize("SystemAgent", as));

    PerceptualProcessor processor(as, self);
    AttentionManager attention(as);
    KnowledgeBase kb(as);
    ASSERT_TRUE(kb.initialize());

    GoalHierarchy goals(as);
    ASSERT_TRUE(goals.initialize());
    PlanningEngine planner(as);
    ASSERT_TRUE(planner.initialize());

    ExperienceManager experience(as);
    ASSERT_TRUE(experience.isInitialized());

    LanguageProcessor language(as);
    WorkingMemory wm(as);
    EpisodicMemory episodic(as);
    ASSERT_TRUE(episodic.initialize());

    ToolRegistry tools_reg(as);
    ToolExecutor executor(as);

    // Shared AtomSpace still owns the agent self atom.
    ASSERT_TRUE(as->is_valid_handle(self));
    ASSERT_TRUE(core.isInitialized());
    ASSERT_GE(as->get_size(), static_cast<size_t>(1));
}

// -----------------------------------------------------------------------
// Integration 2: perception → attention → working memory → knowledge

TEST(FullSystem_PerceptionToKnowledgePipeline)
{
    auto as = make_as();
    Handle self = as->add_node(CONCEPT_NODE, "PerceptAgent");

    PerceptualProcessor processor(as, self);
    AttentionManager attention(as);
    WorkingMemory wm(as, /*max=*/200, /*threshold=*/0.05, std::chrono::seconds(3600));
    KnowledgeBase kb(as);
    ASSERT_TRUE(kb.initialize());

    SensorInfo info("SysCam", "system camera", SensorCapability::VISUAL, 10.0);
    MockSensor sensor(info);

    std::vector<Handle> encoded;
    sensor.registerCallback([&](const SensoryInput& input) {
        Handle h = processor.processInput(input);
        if (h) {
            SalienceScore score = attention.calculateSalience(input);
            attention.allocateAttention(h, score.overall);
            wm.addItem(h, score.overall, "perception");
            encoded.push_back(h);
        }
    });

    ASSERT_TRUE(sensor.initialize());
    ASSERT_TRUE(sensor.start());
    sensor.addTestData({0.2, 0.8, 0.5});
    sensor.addTestData({0.9, 0.1, 0.4});
    ASSERT_TRUE(sensor.generateNextSample());
    ASSERT_TRUE(sensor.generateNextSample());

    ASSERT_EQ(encoded.size(), static_cast<size_t>(2));
    ASSERT_GE(attention.trackedAtomCount(), static_cast<size_t>(2));
    ASSERT_GE(wm.getCurrentSize(), static_cast<size_t>(2));

    // Promote percept concepts into the knowledge base as domain facts.
    auto load = kb.loadFromTriples({
        {"Obstacle", "isa", "Hazard"},
        {"PathClear", "isa", "SafeState"}
    });
    ASSERT_TRUE(load.success);

    auto q = kb.query({QueryTriple{"?x", "isa", "Hazard"}});
    ASSERT_TRUE(q.success);
    ASSERT_GE(q.total_matches, static_cast<size_t>(1));
}

// -----------------------------------------------------------------------
// Integration 3: goals + planning + tools + learning + language

TEST(FullSystem_PlanToolLearnCommunicate)
{
    auto as = make_as();

    // Goals / planning
    auto goals = std::make_shared<GoalHierarchy>(as);
    ASSERT_TRUE(goals->initialize());
    Handle mission = as->add_node(CONCEPT_NODE, "DeliverPackage");
    Handle prep = as->add_node(CONCEPT_NODE, "GatherPackage");
    ASSERT_TRUE(goals->addGoal(mission));
    ASSERT_TRUE(goals->addGoal(prep, mission));

    PlanningEngine planner(as);
    ASSERT_TRUE(planner.initialize());
    planner.setGoalHierarchy(goals);

    PlanningEngine::StripsOperator gather;
    gather.name = "gather-package";
    gather.preconditions = {"at-depot"};
    gather.add_effects = {"holding-package"};
    gather.delete_effects = {};
    planner.registerOperator(gather);

    PlanningEngine::StripsOperator deliver;
    deliver.name = "deliver-package";
    deliver.preconditions = {"holding-package", "at-destination"};
    deliver.add_effects = {"package-delivered"};
    deliver.delete_effects = {"holding-package"};
    planner.registerOperator(deliver);

    planner.setFluent("at-depot", true);
    planner.setFluent("at-destination", true);

    Handle goal = as->add_node(CONCEPT_NODE, "package-delivered");
    ASSERT_EQ(planner.createPlan(goal, PlanningEngine::PlanningStrategy::STRIPS),
              PlanningEngine::PlanResult::SUCCESS);

    // Tools — register a helper invoked during execution bookkeeping
    ToolRegistry registry(as);
    ToolRegistry::ToolMetadata meta;
    meta.name = "log-delivery";
    meta.description = "record delivery event";
    meta.category = ToolRegistry::ToolCategory::UTILITY;
    registry.registerTool(meta, [](const HandleSeq& args, AtomSpacePtr space) {
        std::string label = args.empty() ? "none" : args[0]->get_name();
        return space->add_node(CONCEPT_NODE, "Logged:" + label);
    });
    Handle logged = registry.executeTool("log-delivery", {goal});
    ASSERT_TRUE(static_cast<bool>(logged));
    ASSERT_EQ(logged->get_name(), std::string("Logged:package-delivered"));

    // Learning — record the outcome
    ExperienceManager experience(as);
    Handle ctx = as->add_node(CONCEPT_NODE, "DeliveryContext");
    Handle outcome = as->add_node(CONCEPT_NODE, "Success");
    Handle exp = experience.recordExperience(
        ExperienceType::ACTION_OUTCOME, ctx, goal, outcome, 0.9);
    ASSERT_NE(exp, Handle::UNDEFINED);
    ASSERT_GE(experience.getExperienceCount(), static_cast<size_t>(1));

    // Communication — report status in natural language
    LanguageProcessor language(as);
    auto parsed = language.parseText("Package delivered successfully");
    ASSERT_TRUE(parsed.success);
    ASSERT_FALSE(parsed.tokens.empty());
    std::string reply = language.generateResponse("status of delivery");
    ASSERT_FALSE(reply.empty());
}

// -----------------------------------------------------------------------
// Integration 4: full cognitive path with AgentZeroCore orchestration

TEST(FullSystem_CoreOrchestratedCycle)
{
    auto as = make_as();
    AgentZeroCore core("FullCycleAgent", as);
    ASSERT_TRUE(core.initialize("FullCycleAgent", as));

    Handle goal = as->add_node(CONCEPT_NODE, "ExploreRoom");
    ASSERT_TRUE(core.setGoal(goal));

    Handle self = core.getAgentSelfAtom();
    ASSERT_NE(self, Handle::UNDEFINED);

    // Perception + memory side channels on the same AtomSpace
    PerceptualProcessor processor(as, self);
    AttentionManager attention(as);
    WorkingMemory wm(as);
    EpisodicMemory episodic(as);
    ASSERT_TRUE(episodic.initialize());

    SensoryInput si("textual", "operator", {0.9, 0.1}, 0.9);
    Handle percept = processor.processInput(si);
    ASSERT_NE(percept, Handle::UNDEFINED);
    attention.allocateAttention(percept, 0.9);
    ASSERT_TRUE(wm.addItem(percept, 0.9, "perception"));

    std::string eid = episodic.storeEpisodeMulti({percept, goal}, 0.85, "explore");
    ASSERT_FALSE(eid.empty());

    // Drive core cognitive steps
    ASSERT_TRUE(core.processCognitiveStep());
    ASSERT_TRUE(core.processCognitiveStep());
    ASSERT_TRUE(core.isInitialized());

    // Knowledge query still works after cycles
    KnowledgeBase kb(as);
    ASSERT_TRUE(kb.initialize());
    auto by_name = kb.queryByName("Explore");
    ASSERT_FALSE(by_name.empty());

    ASSERT_GE(wm.getCurrentSize(), static_cast<size_t>(1));
    ASSERT_EQ(episodic.getEpisodeCount(), static_cast<size_t>(1));
}

// -----------------------------------------------------------------------
// Integration 5: multi-module stress — many cycles, shared state integrity

TEST(FullSystem_MultiCycleStateIntegrity)
{
    auto as = make_as();
    AgentZeroCore core("StressAgent", as);
    ASSERT_TRUE(core.initialize("StressAgent", as));

    Handle self = core.getAgentSelfAtom();
    PerceptualProcessor processor(as, self);
    AttentionManager attention(as);
    WorkingMemory wm(as, 500, 0.01, std::chrono::seconds(7200));
    ExperienceManager experience(as);
    ToolRegistry registry(as);

    ToolRegistry::ToolMetadata meta;
    meta.name = "tick";
    meta.description = "cycle counter";
    meta.category = ToolRegistry::ToolCategory::UTILITY;
    registry.registerTool(meta, [](const HandleSeq& args, AtomSpacePtr space) {
        return space->add_node(CONCEPT_NODE,
            args.empty() ? "tick" : ("tick:" + args[0]->get_name()));
    });

    for (int i = 0; i < 25; ++i) {
        double conf = 0.5 + (i % 5) * 0.1;
        SensoryInput si("event", "loop", {static_cast<double>(i), conf}, conf);
        Handle h = processor.processInput(si);
        ASSERT_NE(h, Handle::UNDEFINED);
        attention.allocateAttention(h, conf);
        wm.addItem(h, conf, "loop");

        Handle goal = as->add_node(CONCEPT_NODE, "G" + std::to_string(i));
        core.setGoal(goal);
        ASSERT_TRUE(core.processCognitiveStep());

        Handle logged = registry.executeTool("tick", {goal});
        ASSERT_TRUE(static_cast<bool>(logged));

        experience.recordExperience(
            ExperienceType::OBSERVATION, h, goal, logged, conf);
    }

    ASSERT_GE(attention.trackedAtomCount(), static_cast<size_t>(25));
    ASSERT_GE(wm.getCurrentSize(), static_cast<size_t>(25));
    ASSERT_GE(experience.getExperienceCount(), static_cast<size_t>(25));
    ASSERT_TRUE(as->get_size() > 25);
}
