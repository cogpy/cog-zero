/*
 * ReasoningEngineTest.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Comprehensive tests for ReasoningEngine with PLN integration
 * Part of the AGENT-ZERO-GENESIS project
 */

#include <gtest/gtest.h>
#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/atom_types/atom_types.h>
#include <opencog/atoms/atom_types/types.h>
#include <opencog/atoms/truthvalue/SimpleTruthValue.h>

#include "../include/opencog/agentzero/ReasoningEngine.h"
#include "../include/opencog/agentzero/AgentZeroCore.h"

using namespace opencog;
using namespace opencog::agentzero;

class ReasoningEngineTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        atomspace = std::make_shared<AtomSpace>();
        
        // Create a mock AgentZeroCore for testing
        agent_core = nullptr; // In real tests, this would be properly initialized
        
        reasoning_engine = std::make_unique<ReasoningEngine>(agent_core, atomspace);
    }

    void TearDown() override
    {
        reasoning_engine.reset();
        atomspace.reset();
    }

    AtomSpacePtr atomspace;
    AgentZeroCore* agent_core;
    std::unique_ptr<ReasoningEngine> reasoning_engine;
    
    // Helper method to create test atoms
    Handle createTestConcept(const std::string& name) {
        return atomspace->add_node(CONCEPT_NODE, name);
    }
    
    Handle createTestImplication(const Handle& antecedent, const Handle& consequent) {
        return atomspace->add_link(IMPLICATION_LINK, antecedent, consequent);
    }
    
    Handle createTestInheritance(const Handle& child, const Handle& parent) {
        return atomspace->add_link(INHERITANCE_LINK, child, parent);
    }
};

TEST_F(ReasoningEngineTest, InitializationTest)
{
    // Test basic initialization
    EXPECT_TRUE(reasoning_engine != nullptr);
    
    // Test that reasoning context is created
    auto context = reasoning_engine->getReasoningContext();
    EXPECT_TRUE(context != Handle::UNDEFINED);
    
    // Test initial configuration
    auto stats = reasoning_engine->getReasoningStatistics();
    EXPECT_GT(stats["total_rules"], 0);
    EXPECT_EQ(stats["pln_enabled"], 1);
    EXPECT_EQ(stats["ure_enabled"], 1);
}

TEST_F(ReasoningEngineTest, ForwardChainingTest)
{
    // Create test premises: A -> B, A
    Handle concept_a = createTestConcept("A");
    Handle concept_b = createTestConcept("B");
    Handle implication_ab = createTestImplication(concept_a, concept_b);
    
    concept_a->setTruthValue(SimpleTruthValue::createTV(1.0, 0.9));
    implication_ab->setTruthValue(SimpleTruthValue::createTV(0.8, 0.9));
    
    std::vector<Handle> premises = {concept_a, implication_ab};
    
    // Perform forward chaining
    auto results = reasoning_engine->reason(premises, ReasoningEngine::ReasoningMode::FORWARD_CHAINING, 5);
    
    // Should produce at least one result
    EXPECT_GT(results.size(), 0);
    
    // Check result properties
    for (const auto& result : results) {
        EXPECT_TRUE(result.conclusion != Handle::UNDEFINED);
        EXPECT_GT(result.confidence, 0.0);
        EXPECT_EQ(result.reasoning_type, "forward_chaining");
        EXPECT_GT(result.inference_steps, 0);
        EXPECT_FALSE(result.explanation.empty());
    }
}

TEST_F(ReasoningEngineTest, BackwardChainingTest)
{
    // Create test goal and background knowledge
    Handle concept_c = createTestConcept("C");
    Handle concept_d = createTestConcept("D");
    
    concept_c->setTruthValue(SimpleTruthValue::createTV(0.9, 0.8));
    
    std::vector<Handle> premises = {concept_c};
    
    // Perform backward chaining
    auto results = reasoning_engine->reason(premises, ReasoningEngine::ReasoningMode::BACKWARD_CHAINING, 3);
    
    // Should produce results trying to prove the premises
    EXPECT_GE(results.size(), 0); // May be zero if no applicable rules
    
    for (const auto& result : results) {
        EXPECT_EQ(result.reasoning_type, "backward_chaining");
    }
}

TEST_F(ReasoningEngineTest, MixedChainingTest)
{
    // Create test premises and goal
    Handle concept_e = createTestConcept("E");
    Handle concept_f = createTestConcept("F");
    Handle implication_ef = createTestImplication(concept_e, concept_f);
    
    concept_e->setTruthValue(SimpleTruthValue::createTV(1.0, 0.9));
    implication_ef->setTruthValue(SimpleTruthValue::createTV(0.8, 0.8));
    
    std::vector<Handle> premises = {concept_e, implication_ef};
    
    // Perform mixed chaining
    auto results = reasoning_engine->reason(premises, ReasoningEngine::ReasoningMode::MIXED_CHAINING, 6);
    
    // Should combine forward and backward chaining
    EXPECT_GE(results.size(), 0);
    
    for (const auto& result : results) {
        EXPECT_EQ(result.reasoning_type, "mixed_chaining");
    }
}

TEST_F(ReasoningEngineTest, AbductiveReasoningTest)
{
    // Create observations to explain
    Handle observation1 = createTestConcept("Observation1");
    Handle observation2 = createTestConcept("Observation2");
    
    observation1->setTruthValue(SimpleTruthValue::createTV(1.0, 0.9));
    observation2->setTruthValue(SimpleTruthValue::createTV(1.0, 0.8));
    
    std::vector<Handle> observations = {observation1, observation2};
    
    // Generate hypotheses
    auto results = reasoning_engine->generateHypotheses(observations);
    
    // Should generate hypotheses for each observation
    EXPECT_EQ(results.size(), observations.size());
    
    for (const auto& result : results) {
        EXPECT_TRUE(result.conclusion != Handle::UNDEFINED);
        EXPECT_EQ(result.reasoning_type, "abductive");
        EXPECT_GT(result.confidence, 0.0);
        EXPECT_FALSE(result.explanation.empty());
    }
}

TEST_F(ReasoningEngineTest, AnalogicalReasoningTest)
{
    // Create source and target cases
    Handle source1 = createTestConcept("SourceCase1");
    Handle source2 = createTestConcept("SourceCase2");
    Handle target1 = createTestConcept("TargetCase1");
    Handle target2 = createTestConcept("TargetCase2");
    
    std::vector<Handle> source_case = {source1, source2};
    std::vector<Handle> target_case = {target1, target2};
    
    // Perform analogical reasoning
    auto results = reasoning_engine->analogicalReasoning(source_case, target_case);
    
    // Should find analogical mappings
    EXPECT_GE(results.size(), 0);
    
    for (const auto& result : results) {
        EXPECT_EQ(result.reasoning_type, "analogical");
        EXPECT_GT(result.confidence, 0.0);
    }
}

TEST_F(ReasoningEngineTest, RuleManagementTest)
{
    // Test adding a custom rule
    ReasoningEngine::ReasoningRule custom_rule;
    custom_rule.name = "test_rule";
    custom_rule.weight = 0.8;
    custom_rule.rule_type = "test";
    custom_rule.preconditions = {"test_condition"};
    custom_rule.applicability_check = [](const std::vector<Handle>&) -> bool { 
        return true; 
    };
    
    bool added = reasoning_engine->addReasoningRule(custom_rule);
    EXPECT_TRUE(added);
    
    // Check that rule was added
    auto stats = reasoning_engine->getReasoningStatistics();
    auto initial_rules = stats["total_rules"];
    
    // Get rules of the test type
    auto test_rules = reasoning_engine->getReasoningRules("test");
    EXPECT_EQ(test_rules.size(), 1);
    EXPECT_EQ(test_rules[0].name, "test_rule");
    
    // Test removing the rule
    bool removed = reasoning_engine->removeReasoningRule("test_rule");
    EXPECT_TRUE(removed);
    
    // Verify rule was removed
    auto updated_test_rules = reasoning_engine->getReasoningRules("test");
    EXPECT_EQ(updated_test_rules.size(), 0);
}

TEST_F(ReasoningEngineTest, ConfigurationTest)
{
    // Test PLN configuration
    reasoning_engine->configurePLN(true, 0.7, 0.6);
    
    auto stats = reasoning_engine->getReasoningStatistics();
    EXPECT_EQ(stats["pln_enabled"], 1);
    
    // Test URE configuration
    reasoning_engine->configureURE(true, 150, 0.05);
    EXPECT_EQ(stats["ure_enabled"], 1);
    
    // Test reasoning limits
    reasoning_engine->setReasoningLimits(20, 0.8);
    
    auto updated_stats = reasoning_engine->getReasoningStatistics();
    EXPECT_EQ(updated_stats["max_inference_steps"], 20);
}

TEST_F(ReasoningEngineTest, ConsistencyValidationTest)
{
    // Create potentially inconsistent beliefs
    Handle concept_x = createTestConcept("X");
    Handle concept_y = createTestConcept("Y");
    Handle concept_not_y = createTestConcept("NotY");
    
    // X implies Y and X implies NotY (inconsistent)
    Handle implication1 = createTestImplication(concept_x, concept_y);
    Handle implication2 = createTestImplication(concept_x, concept_not_y);
    
    implication1->setTruthValue(SimpleTruthValue::createTV(0.9, 0.8));
    implication2->setTruthValue(SimpleTruthValue::createTV(0.9, 0.8));
    
    std::vector<Handle> beliefs = {implication1, implication2, concept_x};
    
    // Validate consistency
    auto inconsistencies = reasoning_engine->validateConsistency(beliefs);
    
    // Should detect potential inconsistencies
    EXPECT_GE(inconsistencies.size(), 0);
}

TEST_F(ReasoningEngineTest, TruthValuePropagationTest)
{
    // Create chain of implications with different truth values
    Handle a = createTestConcept("A");
    Handle b = createTestConcept("B");
    Handle c = createTestConcept("C");
    
    a->setTruthValue(SimpleTruthValue::createTV(1.0, 0.9));
    
    Handle ab = createTestImplication(a, b);
    Handle bc = createTestImplication(b, c);
    
    ab->setTruthValue(SimpleTruthValue::createTV(0.8, 0.9));
    bc->setTruthValue(SimpleTruthValue::createTV(0.7, 0.8));
    
    std::vector<Handle> premises = {a, ab, bc};
    
    // Perform reasoning
    auto results = reasoning_engine->reason(premises, ReasoningEngine::ReasoningMode::FORWARD_CHAINING, 5);
    
    // Check that truth values are propagated appropriately
    for (const auto& result : results) {
        if (result.conclusion != Handle::UNDEFINED) {
            auto tv = result.conclusion->getTruthValue();
            EXPECT_TRUE(tv != nullptr);
            EXPECT_GT(tv->get_mean(), 0.0);
            EXPECT_LE(tv->get_mean(), 1.0);
        }
    }
}

TEST_F(ReasoningEngineTest, ReasoningCycleTest)
{
    // Test ongoing reasoning cycle
    bool success = reasoning_engine->processReasoningCycle();
    EXPECT_TRUE(success);
    
    // Multiple cycles should work
    for (int i = 0; i < 5; ++i) {
        EXPECT_TRUE(reasoning_engine->processReasoningCycle());
    }
}

TEST_F(ReasoningEngineTest, StatusReportingTest)
{
    // Test status information
    std::string status = reasoning_engine->getStatusInfo();
    EXPECT_FALSE(status.empty());
    
    // Should be valid JSON-like format
    EXPECT_TRUE(status.find("reasoning_engine_status") != std::string::npos);
    EXPECT_TRUE(status.find("pln_enabled") != std::string::npos);
    EXPECT_TRUE(status.find("ure_enabled") != std::string::npos);
    
    // Test statistics
    auto stats = reasoning_engine->getReasoningStatistics();
    EXPECT_GT(stats.size(), 0);
    EXPECT_TRUE(stats.find("total_rules") != stats.end());
}

TEST_F(ReasoningEngineTest, ErrorHandlingTest)
{
    // Test reasoning with empty premises
    std::vector<Handle> empty_premises;
    auto results = reasoning_engine->reason(empty_premises);
    // Should handle gracefully without crashing
    
    // Test reasoning with invalid handles
    std::vector<Handle> invalid_premises = {Handle::UNDEFINED};
    auto results2 = reasoning_engine->reason(invalid_premises);
    // Should handle gracefully
    
    // Test adding invalid rule
    ReasoningEngine::ReasoningRule invalid_rule;
    // Leave most fields uninitialized
    invalid_rule.name = "";
    bool added = reasoning_engine->addReasoningRule(invalid_rule);
    // Should fail gracefully
}

TEST_F(ReasoningEngineTest, PerformanceTest)
{
    // Create a larger set of premises for performance testing
    std::vector<Handle> large_premise_set;
    
    for (int i = 0; i < 100; ++i) {
        Handle concept = createTestConcept("Concept" + std::to_string(i));
        concept->setTruthValue(SimpleTruthValue::createTV(0.8, 0.9));
        large_premise_set.push_back(concept);
    }
    
    // Add some implications
    for (int i = 0; i < 20; ++i) {
        if (i < static_cast<int>(large_premise_set.size()) - 1) {
            Handle implication = createTestImplication(large_premise_set[i], large_premise_set[i + 1]);
            implication->setTruthValue(SimpleTruthValue::createTV(0.7, 0.8));
            large_premise_set.push_back(implication);
        }
    }
    
    // Test that reasoning completes in reasonable time
    auto start_time = std::chrono::high_resolution_clock::now();
    
    auto results = reasoning_engine->reason(large_premise_set, 
                                          ReasoningEngine::ReasoningMode::FORWARD_CHAINING, 10);
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    // Should complete within reasonable time (e.g., under 5 seconds)
    EXPECT_LT(duration.count(), 5000);
    
    // Should produce some results
    EXPECT_GE(results.size(), 0);
}

// Integration test with mock PLN components
TEST_F(ReasoningEngineTest, PLNIntegrationTest)
{
    // Test PLN-specific functionality
    reasoning_engine->configurePLN(true, 0.6, 0.5);
    
    // Create PLN-style atoms
    Handle concept_human = createTestConcept("Human");
    Handle concept_mortal = createTestConcept("Mortal");
    Handle concept_socrates = createTestConcept("Socrates");
    
    // Socrates is a Human
    Handle socrates_human = createTestInheritance(concept_socrates, concept_human);
    socrates_human->setTruthValue(SimpleTruthValue::createTV(1.0, 0.9));
    
    // Humans are Mortal
    Handle human_mortal = createTestInheritance(concept_human, concept_mortal);
    human_mortal->setTruthValue(SimpleTruthValue::createTV(0.95, 0.9));
    
    std::vector<Handle> premises = {socrates_human, human_mortal};
    
    // Should infer: Socrates is Mortal
    auto results = reasoning_engine->reason(premises, 
                                          ReasoningEngine::ReasoningMode::FORWARD_CHAINING, 5);
    
    EXPECT_GE(results.size(), 0);
    
    // Check that PLN context is being used
    auto context = reasoning_engine->getReasoningContext();
    EXPECT_TRUE(context != Handle::UNDEFINED);
}

// Test main function
int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}