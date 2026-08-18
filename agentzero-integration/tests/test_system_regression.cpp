/*
 * Phase 9 — Regression baseline establishment.
 *
 * Locks in stable behavioural contracts across the full module stack so
 * future changes cannot silently break end-to-end cognitive workflows.
 */

#include "test_runner.h"

#include <string>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/atom_types/types.h>

#include <opencog/agentzero/AgentZeroCore.h>
#include <opencog/agentzero/AttentionManager.h>
#include <opencog/agentzero/MultiModalSensor.h>
#include <opencog/agentzero/PerceptualProcessor.h>
#include <opencog/agentzero/knowledge/KnowledgeBase.h>
#include <opencog/agentzero/planning/GoalHierarchy.h>
#include <opencog/agentzero/planning/PlanningEngine.h>
#include <opencog/agentzero/ExperienceManager.h>
#include <opencog/agentzero/communication/LanguageProcessor.h>
#include <opencog/agentzero/WorkingMemory.h>
#include <opencog/agentzero/memory/EpisodicMemory.h>
#include <opencog/agentzero/ToolRegistry.h>

using namespace opencog;
using namespace opencog::agentzero;
using namespace opencog::agentzero::knowledge;
using namespace opencog::agentzero::planning;
using namespace opencog::agentzero::communication;
using namespace opencog::agentzero::memory;

static AtomSpacePtr make_as() { return createAtomSpace(); }

// -----------------------------------------------------------------------
// Baseline 1: core lifecycle is idempotent and restartable

TEST(Regression_CoreLifecycleBaseline)
{
    auto as = make_as();
    AgentZeroCore core("RegCore", as);
    ASSERT_TRUE(core.initialize("RegCore", as));
    ASSERT_TRUE(core.isInitialized());
    ASSERT_FALSE(core.isRunning());

    Handle g1 = as->add_node(CONCEPT_NODE, "GoalA");
    ASSERT_TRUE(core.setGoal(g1));
    ASSERT_EQ(core.getCurrentGoal(), g1);

    ASSERT_TRUE(core.processCognitiveStep());

    // Re-initialize should succeed (baseline: no crash / false return)
    ASSERT_TRUE(core.initialize("RegCore", as));
    ASSERT_TRUE(core.isInitialized());
}

// -----------------------------------------------------------------------
// Baseline 2: knowledge triple round-trip is stable

TEST(Regression_KnowledgeTripleBaseline)
{
    auto as = make_as();
    KnowledgeBase kb(as);
    ASSERT_TRUE(kb.initialize());

    auto load = kb.loadFromTriples({
        {"Dog", "isa", "Animal"},
        {"Cat", "isa", "Animal"},
        {"Rover", "isa", "Dog"}
    });
    ASSERT_TRUE(load.success);

    auto animals = kb.query({QueryTriple{"?x", "isa", "Animal"}});
    ASSERT_TRUE(animals.success);
    ASSERT_EQ(animals.total_matches, static_cast<size_t>(2));

    auto dogs = kb.query({QueryTriple{"?x", "isa", "Dog"}});
    ASSERT_TRUE(dogs.success);
    ASSERT_EQ(dogs.total_matches, static_cast<size_t>(1));
}

// -----------------------------------------------------------------------
// Baseline 3: STRIPS plan length for classic blocks domain

TEST(Regression_StripsPlanLengthBaseline)
{
    auto as = make_as();
    PlanningEngine pe(as);
    ASSERT_TRUE(pe.initialize());

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

    Handle goal = as->add_node(CONCEPT_NODE, "A-on-B");
    ASSERT_EQ(pe.createPlan(goal, PlanningEngine::PlanningStrategy::STRIPS),
              PlanningEngine::PlanResult::SUCCESS);
    const auto* plan = pe.getPlan(goal);
    ASSERT_TRUE(plan != nullptr);
    // Baseline: exactly two actions for this domain.
    ASSERT_EQ(plan->action_names.size(), static_cast<size_t>(2));
}

// -----------------------------------------------------------------------
// Baseline 4: goal hierarchy parent/child linkage

TEST(Regression_GoalHierarchyBaseline)
{
    auto as = make_as();
    GoalHierarchy gh(as);
    ASSERT_TRUE(gh.initialize());

    Handle root = as->add_node(CONCEPT_NODE, "Mission");
    Handle child = as->add_node(CONCEPT_NODE, "Subtask");
    ASSERT_TRUE(gh.addGoal(root));
    ASSERT_TRUE(gh.addGoal(child, root));
    ASSERT_TRUE(gh.hasGoal(root));
    ASSERT_TRUE(gh.hasGoal(child));
}

// -----------------------------------------------------------------------
// Baseline 5: language processor intent labels remain stable

TEST(Regression_LanguageIntentBaseline)
{
    auto as = make_as();
    LanguageProcessor lp(as);

    auto greeting = lp.parseText("Hello there");
    ASSERT_TRUE(greeting.success);
    ASSERT_FALSE(greeting.intent.empty());

    auto question = lp.parseText("What is the status?");
    ASSERT_TRUE(question.success);
    ASSERT_FALSE(question.tokens.empty());

    std::string reply = lp.generateResponse("hello");
    ASSERT_FALSE(reply.empty());
}

// -----------------------------------------------------------------------
// Baseline 6: memory episode store/retrieve contract

TEST(Regression_EpisodicMemoryBaseline)
{
    auto as = make_as();
    EpisodicMemory em(as, 100);
    ASSERT_TRUE(em.initialize());

    Handle a = as->add_node(CONCEPT_NODE, "EventA");
    Handle b = as->add_node(CONCEPT_NODE, "EventB");
    std::string id = em.storeEpisodeMulti({a, b}, 0.7, "baseline-ctx");
    ASSERT_FALSE(id.empty());
    ASSERT_EQ(em.getEpisodeCount(), static_cast<size_t>(1));

    auto eps = em.getEpisodesByContext("baseline-ctx");
    ASSERT_EQ(eps.size(), static_cast<size_t>(1));
}

// -----------------------------------------------------------------------
// Baseline 7: working memory capacity / importance retrieval

TEST(Regression_WorkingMemoryBaseline)
{
    auto as = make_as();
    WorkingMemory wm(as, 10, 0.1, std::chrono::seconds(60));
    Handle hi = as->add_node(CONCEPT_NODE, "HighImp");
    Handle lo = as->add_node(CONCEPT_NODE, "LowImp");
    ASSERT_TRUE(wm.addItem(hi, 0.95, "focus"));
    ASSERT_TRUE(wm.addItem(lo, 0.2, "background"));
    ASSERT_TRUE(wm.hasItem(hi));
    auto important = wm.getImportantItems(0.5);
    ASSERT_GE(important.size(), static_cast<size_t>(1));
    ASSERT_EQ(important[0]->atom, hi);
}

// -----------------------------------------------------------------------
// Baseline 8: tool registration + execute is deterministic

TEST(Regression_ToolRegistryBaseline)
{
    auto as = make_as();
    ToolRegistry registry(as);
    ToolRegistry::ToolMetadata meta;
    meta.name = "baseline-echo";
    meta.description = "deterministic echo";
    meta.category = ToolRegistry::ToolCategory::UTILITY;
    registry.registerTool(meta, [](const HandleSeq& args, AtomSpacePtr space) {
        return space->add_node(CONCEPT_NODE,
            args.empty() ? "none" : ("B:" + args[0]->get_name()));
    });

    Handle in = as->add_node(CONCEPT_NODE, "X");
    Handle out1 = registry.executeTool("baseline-echo", {in});
    Handle out2 = registry.executeTool("baseline-echo", {in});
    ASSERT_TRUE(static_cast<bool>(out1));
    ASSERT_TRUE(static_cast<bool>(out2));
    ASSERT_EQ(out1->get_name(), std::string("B:X"));
    ASSERT_EQ(out2->get_name(), std::string("B:X"));
}

// -----------------------------------------------------------------------
// Baseline 9: experience recording count is monotonic

TEST(Regression_ExperienceCountBaseline)
{
    auto as = make_as();
    ExperienceManager em(as);
    ASSERT_TRUE(em.isInitialized());
    ASSERT_EQ(em.getExperienceCount(), static_cast<size_t>(0));

    Handle c = as->add_node(CONCEPT_NODE, "Ctx");
    Handle t = as->add_node(CONCEPT_NODE, "Task");
    Handle o = as->add_node(CONCEPT_NODE, "Out");
    ASSERT_NE(em.recordExperience(ExperienceType::OBSERVATION, c, t, o, 0.5),
              Handle::UNDEFINED);
    ASSERT_EQ(em.getExperienceCount(), static_cast<size_t>(1));
    ASSERT_NE(em.recordExperience(ExperienceType::ACTION_OUTCOME, c, t, o, 0.6),
              Handle::UNDEFINED);
    ASSERT_EQ(em.getExperienceCount(), static_cast<size_t>(2));
}

// -----------------------------------------------------------------------
// Baseline 10: full stack smoke — modules still cooperate after baseline ops

TEST(Regression_FullStackSmokeBaseline)
{
    auto as = make_as();
    AgentZeroCore core("Smoke", as);
    ASSERT_TRUE(core.initialize("Smoke", as));
    Handle self = core.getAgentSelfAtom();

    PerceptualProcessor processor(as, self);
    AttentionManager attention(as);
    KnowledgeBase kb(as);
    ASSERT_TRUE(kb.initialize());
    WorkingMemory wm(as);
    LanguageProcessor lp(as);

    SensoryInput si("textual", "smoke", {0.7, 0.3}, 0.7);
    Handle p = processor.processInput(si);
    ASSERT_NE(p, Handle::UNDEFINED);
    attention.allocateAttention(p, 0.7);
    wm.addItem(p, 0.7, "smoke");

    kb.loadFromTriples({{"baseline-ok", "isa", "Status"}});
    auto q = kb.query({QueryTriple{"?x", "isa", "Status"}});
    ASSERT_TRUE(q.success);

    Handle goal = as->add_node(CONCEPT_NODE, "StayHealthy");
    core.setGoal(goal);
    ASSERT_TRUE(core.processCognitiveStep());

    auto parsed = lp.parseText("all systems nominal");
    ASSERT_TRUE(parsed.success);
    ASSERT_GE(wm.getCurrentSize(), static_cast<size_t>(1));
}
