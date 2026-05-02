/*
 * tests/ToolRegistryTest.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Unit tests for ToolRegistry
 */

#include <gtest/gtest.h>
#include <memory>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Handle.h>
#include <opencog/agentzero/ToolRegistry.h>

using namespace opencog;
using namespace opencog::agentzero;

class ToolRegistryTest : public ::testing::Test {
protected:
    AtomSpacePtr as;
    std::shared_ptr<ToolRegistry> registry;

    void SetUp() override {
        as = createAtomSpace();
        registry = std::make_shared<ToolRegistry>(as);
    }

    void TearDown() override {
        registry.reset();
        as.reset();
    }
};

TEST_F(ToolRegistryTest, ConstructorInitializesCorrectly) {
    ASSERT_NE(registry, nullptr);
    
    // Check that initial tools are discovered
    auto all_tools = registry->getAllTools();
    EXPECT_GT(all_tools.size(), 0) << "Should discover some tools on initialization";
}

TEST_F(ToolRegistryTest, RegisterAndUnregisterTool) {
    // Create tool metadata
    ToolRegistry::ToolMetadata metadata;
    metadata.name = "TestTool";
    metadata.description = "A test tool for unit testing";
    metadata.category = ToolRegistry::ToolCategory::UTILITY;
    metadata.version = "1.0.0";
    metadata.reliability_score = 0.95;
    
    // Create executor
    auto executor = [](const HandleSeq& args, AtomSpacePtr as) -> Handle {
        return as->add_node(CONCEPT_NODE, "TestResult");
    };
    
    // Register tool
    Handle tool_atom = registry->registerTool(metadata, executor);
    EXPECT_NE(tool_atom, Handle::UNDEFINED);
    
    // Check registration
    EXPECT_TRUE(registry->isToolRegistered("TestTool"));
    
    // Get metadata
    auto retrieved_metadata = registry->getToolMetadata("TestTool");
    EXPECT_EQ(retrieved_metadata.name, "TestTool");
    EXPECT_EQ(retrieved_metadata.description, "A test tool for unit testing");
    
    // Unregister tool
    EXPECT_TRUE(registry->unregisterTool("TestTool"));
    EXPECT_FALSE(registry->isToolRegistered("TestTool"));
}

TEST_F(ToolRegistryTest, GetAllTools) {
    auto tools = registry->getAllTools();
    
    // Should have at least the pre-discovered tools
    EXPECT_GT(tools.size(), 0);
    
    // Register a new tool
    ToolRegistry::ToolMetadata metadata;
    metadata.name = "AnotherTestTool";
    metadata.description = "Another test tool";
    metadata.category = ToolRegistry::ToolCategory::UTILITY;
    
    auto executor = [](const HandleSeq& args, AtomSpacePtr as) -> Handle {
        return Handle::UNDEFINED;
    };
    
    registry->registerTool(metadata, executor);
    
    auto updated_tools = registry->getAllTools();
    EXPECT_EQ(updated_tools.size(), tools.size() + 1);
}

TEST_F(ToolRegistryTest, GetToolsByCategory) {
    // Register tools in different categories
    ToolRegistry::ToolMetadata viz_tool;
    viz_tool.name = "VizTool";
    viz_tool.description = "Visualization tool";
    viz_tool.category = ToolRegistry::ToolCategory::VISUALIZATION;
    
    ToolRegistry::ToolMetadata analysis_tool;
    analysis_tool.name = "AnalysisTool";
    analysis_tool.description = "Analysis tool";
    analysis_tool.category = ToolRegistry::ToolCategory::ANALYSIS;
    
    auto dummy_executor = [](const HandleSeq& args, AtomSpacePtr as) -> Handle {
        return Handle::UNDEFINED;
    };
    
    registry->registerTool(viz_tool, dummy_executor);
    registry->registerTool(analysis_tool, dummy_executor);
    
    // Get tools by category
    auto viz_tools = registry->getToolsByCategory(ToolRegistry::ToolCategory::VISUALIZATION);
    EXPECT_GT(viz_tools.size(), 0);
    
    bool found_viz_tool = false;
    for (const auto& name : viz_tools) {
        if (name == "VizTool") {
            found_viz_tool = true;
            break;
        }
    }
    EXPECT_TRUE(found_viz_tool);
}

TEST_F(ToolRegistryTest, GetToolsByCapabilities) {
    // Register tool with specific capabilities
    ToolRegistry::ToolMetadata tool;
    tool.name = "ReadOnlyTool";
    tool.description = "A read-only tool";
    tool.category = ToolRegistry::ToolCategory::UTILITY;
    tool.capabilities = {
        ToolRegistry::ToolCapability::READ_ONLY,
        ToolRegistry::ToolCapability::BATCH_PROCESSING
    };
    
    auto executor = [](const HandleSeq& args, AtomSpacePtr as) -> Handle {
        return Handle::UNDEFINED;
    };
    
    registry->registerTool(tool, executor);
    
    // Search for tools with READ_ONLY capability
    std::vector<ToolRegistry::ToolCapability> required_caps = {
        ToolRegistry::ToolCapability::READ_ONLY
    };
    
    auto matching_tools = registry->getToolsByCapabilities(required_caps);
    
    bool found = false;
    for (const auto& name : matching_tools) {
        if (name == "ReadOnlyTool") {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(ToolRegistryTest, SearchTools) {
    // Register a tool with specific keywords
    ToolRegistry::ToolMetadata tool;
    tool.name = "DataAnalyzer";
    tool.description = "Analyzes data and generates reports";
    tool.category = ToolRegistry::ToolCategory::ANALYSIS;
    
    auto executor = [](const HandleSeq& args, AtomSpacePtr as) -> Handle {
        return Handle::UNDEFINED;
    };
    
    registry->registerTool(tool, executor);
    
    // Search by keyword
    auto results = registry->searchTools("analyze");
    
    bool found = false;
    for (const auto& name : results) {
        if (name == "DataAnalyzer") {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
    
    // Search by different keyword
    results = registry->searchTools("report");
    found = false;
    for (const auto& name : results) {
        if (name == "DataAnalyzer") {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(ToolRegistryTest, ExecuteTool) {
    // Register a tool with a simple executor
    ToolRegistry::ToolMetadata tool;
    tool.name = "SimpleExecutor";
    tool.description = "Simple execution test";
    tool.category = ToolRegistry::ToolCategory::UTILITY;
    
    bool executor_called = false;
    auto executor = [&executor_called](const HandleSeq& args, AtomSpacePtr as) -> Handle {
        executor_called = true;
        return as->add_node(CONCEPT_NODE, "ExecutionResult");
    };
    
    registry->registerTool(tool, executor);
    
    // Execute the tool
    HandleSeq args;
    Handle result = registry->executeTool("SimpleExecutor", args);
    
    EXPECT_TRUE(executor_called);
    EXPECT_NE(result, Handle::UNDEFINED);
}

TEST_F(ToolRegistryTest, ExecuteToolChain) {
    // Register first tool
    ToolRegistry::ToolMetadata tool1;
    tool1.name = "Tool1";
    tool1.description = "First tool in chain";
    tool1.category = ToolRegistry::ToolCategory::UTILITY;
    
    auto executor1 = [](const HandleSeq& args, AtomSpacePtr as) -> Handle {
        return as->add_node(CONCEPT_NODE, "Tool1Result");
    };
    
    // Register second tool
    ToolRegistry::ToolMetadata tool2;
    tool2.name = "Tool2";
    tool2.description = "Second tool in chain";
    tool2.category = ToolRegistry::ToolCategory::UTILITY;
    
    auto executor2 = [](const HandleSeq& args, AtomSpacePtr as) -> Handle {
        return as->add_node(CONCEPT_NODE, "Tool2Result");
    };
    
    registry->registerTool(tool1, executor1);
    registry->registerTool(tool2, executor2);
    
    // Execute tool chain
    std::vector<std::string> chain = {"Tool1", "Tool2"};
    HandleSeq initial_input;
    
    Handle result = registry->executeToolChain(chain, initial_input);
    EXPECT_NE(result, Handle::UNDEFINED);
}

TEST_F(ToolRegistryTest, ToolReliabilityTracking) {
    // Register a tool
    ToolRegistry::ToolMetadata tool;
    tool.name = "ReliabilityTestTool";
    tool.description = "Tool for reliability testing";
    tool.category = ToolRegistry::ToolCategory::UTILITY;
    tool.reliability_score = 0.9;
    
    auto executor = [](const HandleSeq& args, AtomSpacePtr as) -> Handle {
        return as->add_node(CONCEPT_NODE, "Result");
    };
    
    registry->registerTool(tool, executor);
    
    double initial_reliability = registry->getToolMetadata("ReliabilityTestTool").reliability_score;
    
    // Simulate successful execution
    registry->updateToolReliability("ReliabilityTestTool", true);
    double after_success = registry->getToolMetadata("ReliabilityTestTool").reliability_score;
    EXPECT_GE(after_success, initial_reliability * 0.9); // Should maintain or improve
    
    // Simulate failed execution
    registry->updateToolReliability("ReliabilityTestTool", false);
    double after_failure = registry->getToolMetadata("ReliabilityTestTool").reliability_score;
    EXPECT_LT(after_failure, after_success); // Should decrease
}

TEST_F(ToolRegistryTest, ToolStatusManagement) {
    // Register a tool
    ToolRegistry::ToolMetadata tool;
    tool.name = "StatusTestTool";
    tool.description = "Tool for status testing";
    tool.category = ToolRegistry::ToolCategory::UTILITY;
    
    auto executor = [](const HandleSeq& args, AtomSpacePtr as) -> Handle {
        return as->add_node(CONCEPT_NODE, "Result");
    };
    
    registry->registerTool(tool, executor);
    
    // Check initial status
    auto status = registry->getToolStatus("StatusTestTool");
    EXPECT_EQ(status, ToolRegistry::ToolStatus::AVAILABLE);
    
    // Execute tool (should change to BUSY then back to AVAILABLE)
    HandleSeq args;
    registry->executeTool("StatusTestTool", args);
    
    // After execution, should be AVAILABLE again
    status = registry->getToolStatus("StatusTestTool");
    EXPECT_EQ(status, ToolRegistry::ToolStatus::AVAILABLE);
}

TEST_F(ToolRegistryTest, ToolStatistics) {
    // Register a tool
    ToolRegistry::ToolMetadata tool;
    tool.name = "StatsTool";
    tool.description = "Tool for statistics testing";
    tool.category = ToolRegistry::ToolCategory::UTILITY;
    
    auto executor = [](const HandleSeq& args, AtomSpacePtr as) -> Handle {
        return as->add_node(CONCEPT_NODE, "Result");
    };
    
    registry->registerTool(tool, executor);
    
    // Execute tool multiple times
    HandleSeq args;
    registry->executeTool("StatsTool", args);
    registry->executeTool("StatsTool", args);
    registry->executeTool("StatsTool", args);
    
    // Check statistics
    auto stats = registry->getToolStatistics();
    EXPECT_GT(stats["StatsTool"].first, 0); // Usage count should be > 0
    
    // Clear statistics
    registry->clearStatistics();
    stats = registry->getToolStatistics();
    EXPECT_EQ(stats["StatsTool"].first, 0); // Usage count should be reset
}

TEST_F(ToolRegistryTest, ConfigurationManagement) {
    // Test configuration settings
    registry->setEnableToolComposition(false);
    auto config = registry->getConfiguration();
    EXPECT_EQ(config["enable_tool_composition"], "false");
    
    registry->setEnableToolComposition(true);
    config = registry->getConfiguration();
    EXPECT_EQ(config["enable_tool_composition"], "true");
    
    // Test reliability threshold
    registry->setMinimumReliabilityThreshold(0.7);
    config = registry->getConfiguration();
    EXPECT_EQ(config["minimum_reliability_threshold"], "0.700000");
}

TEST_F(ToolRegistryTest, AtomSpaceIntegration) {
    // Register a tool
    ToolRegistry::ToolMetadata tool;
    tool.name = "AtomSpaceTestTool";
    tool.description = "Tool for AtomSpace testing";
    tool.category = ToolRegistry::ToolCategory::UTILITY;
    
    auto executor = [](const HandleSeq& args, AtomSpacePtr as) -> Handle {
        return Handle::UNDEFINED;
    };
    
    Handle tool_atom = registry->registerTool(tool, executor);
    
    // Verify atom exists in AtomSpace
    EXPECT_NE(tool_atom, Handle::UNDEFINED);
    EXPECT_TRUE(as->is_valid_handle(tool_atom));
    
    // Check that atom has proper truth value
    TruthValuePtr tv = tool_atom->getTruthValue();
    EXPECT_NE(tv, nullptr);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
