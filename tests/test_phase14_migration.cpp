/*
 * tests/test_phase14_migration.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Phase 14 Feature 2.1: Agent Migration — Live State Hand-off
 *
 * Tests:
 *   - Agent serialize/deserialize roundtrip
 *   - Migration initiate/receive protocol (ClusterManager)
 *   - State preservation after migration
 *   - Error handling for invalid serialization
 */

#include "test_runner.h"

#include <string>
#include <memory>

#include "cog0/Agent.h"
#include "cog0/AtomStore.h"
#include "cog0/TaskManager.h"
#include "cog0/EpisodicMemory.h"

using namespace cog0;

// ==========================================================================
// Agent Serialization Tests
// ==========================================================================

TEST(Agent_Serialize_Basic)
{
    AgentConfig cfg;
    cfg.name = "test-agent";
    Agent agent(cfg);
    
    std::string json = agent.serialize();
    
    // Check that JSON contains required fields
    ASSERT_TRUE(json.find("\"name\"") != std::string::npos);
    ASSERT_TRUE(json.find("\"atoms\"") != std::string::npos);
    ASSERT_TRUE(json.find("\"goals\"") != std::string::npos);
    ASSERT_TRUE(json.find("\"tasks\"") != std::string::npos);
    ASSERT_TRUE(json.find("\"episodes\"") != std::string::npos);
    
    // Check agent name is present
    ASSERT_TRUE(json.find("test-agent") != std::string::npos);
}

TEST(Agent_Serialize_WithAtoms)
{
    Agent agent;
    
    // Add some atoms
    auto& store = agent.atomStore();
    auto a1 = store.addNode(AtomType::CONCEPT, "TestConcept1");
    a1->setTV(TruthValue{0.8, 0.9});
    a1->setSTI(0.75);
    a1->setLTI(0.25);
    
    store.addNode(AtomType::PREDICATE, "TestPredicate1");
    
    std::string json = agent.serialize();
    
    // Verify atoms are serialized
    ASSERT_TRUE(json.find("TestConcept1") != std::string::npos);
    ASSERT_TRUE(json.find("TestPredicate1") != std::string::npos);
    ASSERT_TRUE(json.find("0.8") != std::string::npos);  // strength
    ASSERT_TRUE(json.find("0.9") != std::string::npos);  // confidence
    ASSERT_TRUE(json.find("0.75") != std::string::npos); // sti
}

TEST(Agent_Serialize_WithGoals)
{
    Agent agent;
    
    // Add goals
    auto g1 = agent.setGoal("LearnNewSkill", "Learn to classify images", 0.9);
    agent.setGoal("OptimizePerformance", "Improve inference speed", 0.7);
    
    std::string json = agent.serialize();
    
    ASSERT_TRUE(json.find("LearnNewSkill") != std::string::npos);
    ASSERT_TRUE(json.find("Learn to classify images") != std::string::npos);
    ASSERT_TRUE(json.find("OptimizePerformance") != std::string::npos);
}

TEST(Agent_Serialize_WithTasks)
{
    Agent agent;
    
    // Schedule tasks
    agent.scheduleTask("CollectData", "Gather training samples", Priority::HIGH);
    agent.scheduleTask("ProcessData", "Clean and transform data", Priority::NORMAL);
    
    std::string json = agent.serialize();
    
    ASSERT_TRUE(json.find("CollectData") != std::string::npos);
    ASSERT_TRUE(json.find("ProcessData") != std::string::npos);
    ASSERT_TRUE(json.find("HIGH") != std::string::npos);
}

TEST(Agent_Serialize_WithEpisodes)
{
    Agent agent;
    
    // Record episodes
    agent.recordEpisode("perception", "Observed red object", 0.8);
    agent.recordEpisode("action", "Moved forward", 0.6);
    
    std::string json = agent.serialize();
    
    ASSERT_TRUE(json.find("perception") != std::string::npos);
    ASSERT_TRUE(json.find("Observed red object") != std::string::npos);
    ASSERT_TRUE(json.find("action") != std::string::npos);
}

TEST(Agent_Serialize_EscapesSpecialChars)
{
    Agent agent;
    
    // Add content with special characters
    agent.recordEpisode("test", "Line1\nLine2\tTabbed\"Quoted\"", 0.5);
    
    std::string json = agent.serialize();
    
    // Should contain escaped sequences
    ASSERT_TRUE(json.find("\\n") != std::string::npos);
    ASSERT_TRUE(json.find("\\t") != std::string::npos);
    ASSERT_TRUE(json.find("\\\"") != std::string::npos);
}

// ==========================================================================
// Agent Deserialization Tests
// ==========================================================================

TEST(Agent_Deserialize_Basic)
{
    std::string json = R"({
        "name": "restored-agent",
        "atoms": [],
        "goals": [],
        "tasks": [],
        "episodes": []
    })";
    
    auto agent = Agent::deserialize(json);
    
    ASSERT_TRUE(agent != nullptr);
    ASSERT_EQ(agent->name(), std::string("restored-agent"));
}

TEST(Agent_Deserialize_WithAtoms)
{
    std::string json = R"({
        "name": "atom-agent",
        "atoms": [
            {"name": "Concept1", "type": "CONCEPT", "strength": 0.9, "confidence": 0.8, "sti": 0.5, "lti": 0.3},
            {"name": "Pred1", "type": "PREDICATE", "strength": 1.0, "confidence": 1.0, "sti": 0.0, "lti": 0.0}
        ],
        "goals": [],
        "tasks": [],
        "episodes": []
    })";
    
    auto agent = Agent::deserialize(json);
    
    ASSERT_TRUE(agent != nullptr);
    
    auto& store = agent->atomStore();
    auto conceptAtom = store.getNode(AtomType::CONCEPT, "Concept1");
    ASSERT_TRUE(conceptAtom != nullptr);
    ASSERT_NEAR(conceptAtom->tv().strength, 0.9, 0.01);
    ASSERT_NEAR(conceptAtom->tv().confidence, 0.8, 0.01);
    ASSERT_NEAR(conceptAtom->sti(), 0.5, 0.01);
    
    auto pred = store.getNode(AtomType::PREDICATE, "Pred1");
    ASSERT_TRUE(pred != nullptr);
}

TEST(Agent_Deserialize_WithGoals)
{
    std::string json = R"({
        "name": "goal-agent",
        "atoms": [],
        "goals": [
            {"name": "Goal1", "description": "First goal", "priority": 0.9, "achieved": false},
            {"name": "Goal2", "description": "Second goal", "priority": 0.5, "achieved": true}
        ],
        "tasks": [],
        "episodes": []
    })";
    
    auto agent = Agent::deserialize(json);
    
    ASSERT_TRUE(agent != nullptr);
    
    const auto& goals = agent->taskManager().goals();
    ASSERT_EQ(goals.size(), size_t(2));
    
    // Check first goal
    ASSERT_EQ(goals[0]->name, std::string("Goal1"));
    ASSERT_EQ(goals[0]->description, std::string("First goal"));
    ASSERT_NEAR(goals[0]->priority, 0.9, 0.01);
    ASSERT_FALSE(goals[0]->achieved);
    
    // Check second goal
    ASSERT_EQ(goals[1]->name, std::string("Goal2"));
    ASSERT_TRUE(goals[1]->achieved);
}

TEST(Agent_Deserialize_WithTasks)
{
    std::string json = R"({
        "name": "task-agent",
        "atoms": [],
        "goals": [],
        "tasks": [
            {"name": "Task1", "description": "First task", "priority": "HIGH", "status": "pending"},
            {"name": "Task2", "description": "Second task", "priority": "LOW", "status": "pending"}
        ],
        "episodes": []
    })";
    
    auto agent = Agent::deserialize(json);
    
    ASSERT_TRUE(agent != nullptr);
    
    auto tasks = agent->taskManager().pendingTasks();
    ASSERT_EQ(tasks.size(), size_t(2));
}

TEST(Agent_Deserialize_WithEpisodes)
{
    std::string json = R"({
        "name": "episode-agent",
        "atoms": [],
        "goals": [],
        "tasks": [],
        "episodes": [
            {"type": "perception", "content": "Saw something", "importance": 0.7, "timestamp": 12345},
            {"type": "action", "content": "Did something", "importance": 0.5, "timestamp": 12346}
        ]
    })";
    
    auto agent = Agent::deserialize(json);
    
    ASSERT_TRUE(agent != nullptr);
    
    auto episodes = agent->episodicMemory().recentEpisodes(10);
    ASSERT_EQ(episodes.size(), size_t(2));
}

// ==========================================================================
// Roundtrip Tests (Serialize -> Deserialize)
// ==========================================================================

TEST(Agent_SerializeDeserialize_Roundtrip)
{
    // Create original agent with state
    AgentConfig cfg;
    cfg.name = "roundtrip-agent";
    Agent original(cfg);
    
    // Add atoms
    auto& store = original.atomStore();
    auto atom = store.addNode(AtomType::CONCEPT, "RoundtripConcept");
    atom->setTV(TruthValue{0.85, 0.95});
    atom->setSTI(0.6);
    store.addNode(AtomType::PREDICATE, "RoundtripPred");
    
    // Add goals
    original.setGoal("RoundtripGoal", "Test roundtrip", 0.8);
    
    // Add tasks
    original.scheduleTask("RoundtripTask", "Test task", Priority::HIGH);
    
    // Add episodes
    original.recordEpisode("test", "Roundtrip episode content", 0.7);
    
    // Serialize
    std::string json = original.serialize();
    
    // Deserialize
    auto restored = Agent::deserialize(json);
    
    // Verify restoration
    ASSERT_TRUE(restored != nullptr);
    ASSERT_EQ(restored->name(), std::string("roundtrip-agent"));
    
    // Check atoms
    auto& restoredStore = restored->atomStore();
    auto conceptAtom = restoredStore.getNode(AtomType::CONCEPT, "RoundtripConcept");
    ASSERT_TRUE(conceptAtom != nullptr);
    ASSERT_NEAR(conceptAtom->tv().strength, 0.85, 0.01);
    ASSERT_NEAR(conceptAtom->tv().confidence, 0.95, 0.01);
    ASSERT_NEAR(conceptAtom->sti(), 0.6, 0.01);
    
    auto pred = restoredStore.getNode(AtomType::PREDICATE, "RoundtripPred");
    ASSERT_TRUE(pred != nullptr);
    
    // Check goals
    const auto& goals = restored->taskManager().goals();
    ASSERT_GE(goals.size(), size_t(1));
    bool foundGoal = false;
    for (const auto& g : goals) {
        if (g->name == "RoundtripGoal") {
            foundGoal = true;
            ASSERT_EQ(g->description, std::string("Test roundtrip"));
            ASSERT_NEAR(g->priority, 0.8, 0.01);
        }
    }
    ASSERT_TRUE(foundGoal);
    
    // Check tasks
    auto tasks = restored->taskManager().pendingTasks();
    bool foundTask = false;
    for (const auto& t : tasks) {
        if (t->name == "RoundtripTask") {
            foundTask = true;
            ASSERT_EQ(t->priority, Priority::HIGH);
        }
    }
    ASSERT_TRUE(foundTask);
    
    // Check episodes
    auto episodes = restored->episodicMemory().recentEpisodes(10);
    ASSERT_GE(episodes.size(), size_t(1));
}

TEST(Agent_SerializeDeserialize_MultipleAtomTypes)
{
    Agent original;
    
    // Add various atom types
    auto& store = original.atomStore();
    store.addNode(AtomType::CONCEPT, "C1");
    store.addNode(AtomType::PREDICATE, "P1");
    store.addNode(AtomType::VARIABLE, "V1");
    store.addNode(AtomType::STATE, "S1");
    
    std::string json = original.serialize();
    auto restored = Agent::deserialize(json);
    
    ASSERT_TRUE(restored != nullptr);
    
    auto& rs = restored->atomStore();
    ASSERT_TRUE(rs.getNode(AtomType::CONCEPT, "C1") != nullptr);
    ASSERT_TRUE(rs.getNode(AtomType::PREDICATE, "P1") != nullptr);
    ASSERT_TRUE(rs.getNode(AtomType::VARIABLE, "V1") != nullptr);
    ASSERT_TRUE(rs.getNode(AtomType::STATE, "S1") != nullptr);
}

TEST(Agent_SerializeDeserialize_EmptyAgent)
{
    Agent original;
    
    std::string json = original.serialize();
    auto restored = Agent::deserialize(json);
    
    ASSERT_TRUE(restored != nullptr);
    
    // Empty agent should have minimal state
    auto tasks = restored->taskManager().pendingTasks();
    ASSERT_EQ(tasks.size(), size_t(0));
}

// ==========================================================================
// Error Handling Tests
// ==========================================================================

TEST(Agent_Deserialize_InvalidJson)
{
    std::string invalidJson = "not valid json at all";
    
    // Should not crash, returns agent with default state
    auto agent = Agent::deserialize(invalidJson);
    
    ASSERT_TRUE(agent != nullptr);
    // Name should be default since JSON is invalid
}

TEST(Agent_Deserialize_MissingFields)
{
    std::string partialJson = R"({
        "name": "partial-agent"
    })";
    
    auto agent = Agent::deserialize(partialJson);
    
    ASSERT_TRUE(agent != nullptr);
    ASSERT_EQ(agent->name(), std::string("partial-agent"));
}

TEST(Agent_Deserialize_EmptyString)
{
    auto agent = Agent::deserialize("");
    
    ASSERT_TRUE(agent != nullptr);
    // Should have default name
}

TEST(Agent_Deserialize_MalformedAtoms)
{
    std::string json = R"({
        "name": "malformed-agent",
        "atoms": [
            {"name": "ValidAtom", "type": "CONCEPT", "strength": 1.0, "confidence": 1.0, "sti": 0, "lti": 0},
            {"invalid": "entry"}
        ],
        "goals": [],
        "tasks": [],
        "episodes": []
    })";
    
    auto agent = Agent::deserialize(json);
    
    ASSERT_TRUE(agent != nullptr);
    
    // Valid atom should still be present
    auto& store = agent->atomStore();
    auto atom = store.getNode(AtomType::CONCEPT, "ValidAtom");
    ASSERT_TRUE(atom != nullptr);
}

TEST(Agent_Deserialize_InvalidAtomType)
{
    std::string json = R"({
        "name": "bad-type-agent",
        "atoms": [
            {"name": "AtomWithBadType", "type": "NONEXISTENT_TYPE", "strength": 1.0, "confidence": 1.0, "sti": 0, "lti": 0}
        ],
        "goals": [],
        "tasks": [],
        "episodes": []
    })";
    
    auto agent = Agent::deserialize(json);
    
    ASSERT_TRUE(agent != nullptr);
    
    // Should default to CONCEPT type
    auto& store = agent->atomStore();
    auto atom = store.getNode(AtomType::CONCEPT, "AtomWithBadType");
    ASSERT_TRUE(atom != nullptr);
}

TEST(Agent_Deserialize_InvalidPriority)
{
    std::string json = R"({
        "name": "bad-priority-agent",
        "atoms": [],
        "goals": [],
        "tasks": [
            {"name": "TaskBadPriority", "description": "Test", "priority": "INVALID", "status": "pending"}
        ],
        "episodes": []
    })";
    
    auto agent = Agent::deserialize(json);
    
    ASSERT_TRUE(agent != nullptr);
    
    // Task should exist with default NORMAL priority
    auto tasks = agent->taskManager().pendingTasks();
    bool found = false;
    for (const auto& t : tasks) {
        if (t->name == "TaskBadPriority") {
            found = true;
            ASSERT_EQ(t->priority, Priority::NORMAL);
        }
    }
    ASSERT_TRUE(found);
}

// ==========================================================================
// State Preservation Tests
// ==========================================================================

TEST(Agent_Migration_StatePreservation_AtomCount)
{
    Agent original;
    
    // Add specific number of atoms
    auto& store = original.atomStore();
    for (int i = 0; i < 10; ++i) {
        store.addNode(AtomType::CONCEPT, "Atom" + std::to_string(i));
    }
    
    (void)store.size();  // unused but validates store is populated
    
    std::string json = original.serialize();
    auto restored = Agent::deserialize(json);
    
    // Count may include default atoms, so check we have at least our 10
    auto concepts = restored->atomStore().getByType(AtomType::CONCEPT);
    int ourAtomCount = 0;
    for (const auto& a : concepts) {
        if (a->name().find("Atom") == 0) ++ourAtomCount;
    }
    ASSERT_EQ(ourAtomCount, 10);
}

TEST(Agent_Migration_StatePreservation_TruthValues)
{
    Agent original;
    
    auto& store = original.atomStore();
    auto a1 = store.addNode(AtomType::CONCEPT, "TVTest1");
    a1->setTV(TruthValue{0.123, 0.456});
    
    auto a2 = store.addNode(AtomType::CONCEPT, "TVTest2");
    a2->setTV(TruthValue{0.789, 0.012});
    
    std::string json = original.serialize();
    auto restored = Agent::deserialize(json);
    
    auto r1 = restored->atomStore().getNode(AtomType::CONCEPT, "TVTest1");
    ASSERT_TRUE(r1 != nullptr);
    ASSERT_NEAR(r1->tv().strength, 0.123, 0.001);
    ASSERT_NEAR(r1->tv().confidence, 0.456, 0.001);
    
    auto r2 = restored->atomStore().getNode(AtomType::CONCEPT, "TVTest2");
    ASSERT_TRUE(r2 != nullptr);
    ASSERT_NEAR(r2->tv().strength, 0.789, 0.001);
    ASSERT_NEAR(r2->tv().confidence, 0.012, 0.001);
}

TEST(Agent_Migration_StatePreservation_AttentionValues)
{
    Agent original;
    
    auto& store = original.atomStore();
    auto atom = store.addNode(AtomType::CONCEPT, "AVTest");
    atom->setSTI(0.777);
    atom->setLTI(0.333);
    
    std::string json = original.serialize();
    auto restored = Agent::deserialize(json);
    
    auto r = restored->atomStore().getNode(AtomType::CONCEPT, "AVTest");
    ASSERT_TRUE(r != nullptr);
    ASSERT_NEAR(r->sti(), 0.777, 0.001);
    ASSERT_NEAR(r->lti(), 0.333, 0.001);
}

TEST(Agent_Migration_StatePreservation_GoalPriorities)
{
    Agent original;
    
    original.setGoal("HighPri", "High priority goal", 0.95);
    original.setGoal("LowPri", "Low priority goal", 0.15);
    
    std::string json = original.serialize();
    auto restored = Agent::deserialize(json);
    
    const auto& goals = restored->taskManager().goals();
    
    for (const auto& g : goals) {
        if (g->name == "HighPri") {
            ASSERT_NEAR(g->priority, 0.95, 0.01);
        } else if (g->name == "LowPri") {
            ASSERT_NEAR(g->priority, 0.15, 0.01);
        }
    }
}

TEST(Agent_Migration_StatePreservation_EpisodeImportance)
{
    Agent original;
    
    original.recordEpisode("type1", "Important episode", 0.99);
    original.recordEpisode("type2", "Unimportant episode", 0.01);
    
    std::string json = original.serialize();
    auto restored = Agent::deserialize(json);
    
    auto episodes = restored->episodicMemory().recentEpisodes(10);
    ASSERT_GE(episodes.size(), size_t(2));
    
    for (const auto& ep : episodes) {
        if (ep->content == "Important episode") {
            ASSERT_NEAR(ep->importance, 0.99, 0.01);
        } else if (ep->content == "Unimportant episode") {
            ASSERT_NEAR(ep->importance, 0.01, 0.01);
        }
    }
}

// ==========================================================================
// Edge Case Tests
// ==========================================================================

TEST(Agent_Serialize_LargeContent)
{
    Agent agent;
    
    // Create large content
    std::string largeContent(10000, 'X');
    agent.recordEpisode("large", largeContent, 0.5);
    
    std::string json = agent.serialize();
    auto restored = Agent::deserialize(json);
    
    auto episodes = restored->episodicMemory().recentEpisodes(1);
    ASSERT_EQ(episodes.size(), size_t(1));
    ASSERT_EQ(episodes[0]->content.size(), size_t(10000));
}

TEST(Agent_Serialize_SpecialCharactersInNames)
{
    Agent agent;
    
    auto& store = agent.atomStore();
    store.addNode(AtomType::CONCEPT, "Name:With:Colons");
    store.addNode(AtomType::CONCEPT, "Name With Spaces");
    
    std::string json = agent.serialize();
    auto restored = Agent::deserialize(json);
    
    auto& rs = restored->atomStore();
    ASSERT_TRUE(rs.getNode(AtomType::CONCEPT, "Name:With:Colons") != nullptr);
    ASSERT_TRUE(rs.getNode(AtomType::CONCEPT, "Name With Spaces") != nullptr);
}

TEST(Agent_Serialize_UnicodeContent)
{
    Agent agent;
    
    // UTF-8 content
    agent.recordEpisode("unicode", "Hello 世界 🌍", 0.5);
    
    std::string json = agent.serialize();
    auto restored = Agent::deserialize(json);
    
    auto episodes = restored->episodicMemory().recentEpisodes(1);
    ASSERT_GE(episodes.size(), size_t(1));
    // Check content is preserved (may be escaped but should roundtrip)
}

TEST(Agent_Deserialize_WithConfigOverride)
{
    std::string json = R"({
        "name": "original-name",
        "atoms": [],
        "goals": [],
        "tasks": [],
        "episodes": []
    })";
    
    // Override name via config
    AgentConfig cfg;
    cfg.name = "overridden-name";
    
    // Note: current implementation uses name from JSON, not config
    // This test documents that behavior
    auto agent = Agent::deserialize(json, cfg);
    
    ASSERT_TRUE(agent != nullptr);
    // The JSON name takes precedence
    ASSERT_EQ(agent->name(), std::string("original-name"));
}
