#include "test_runner.h"

#include <opencog/agentzero/ToolRegistry.h>
#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/atom_types/types.h>

using namespace opencog;
using namespace opencog::agentzero;

static AtomSpacePtr make_as() { return createAtomSpace(); }

TEST(ToolRegistry_Construct)
{
    auto as = make_as();
    ToolRegistry registry(as);
    // Built-in discovery should register at least some catalogue entries
    ASSERT_GT(registry.getAllTools().size(), static_cast<size_t>(0));
}

TEST(ToolRegistry_RegisterAndUnregister)
{
    auto as = make_as();
    ToolRegistry registry(as);

    ToolRegistry::ToolMetadata meta;
    meta.name = "UnitTestTool";
    meta.description = "A test tool";
    meta.category = ToolRegistry::ToolCategory::UTILITY;
    meta.version = "1.0.0";
    meta.reliability_score = 0.95;
    meta.capabilities = {ToolRegistry::ToolCapability::READ_ONLY};

    auto executor = [](const HandleSeq&, AtomSpacePtr space) -> Handle {
        return space->add_node(CONCEPT_NODE, "UnitTestResult");
    };

    Handle atom = registry.registerTool(meta, executor);
    ASSERT_TRUE(atom != Handle::UNDEFINED);
    ASSERT_TRUE(registry.isToolRegistered("UnitTestTool"));

    auto got = registry.getToolMetadata("UnitTestTool");
    ASSERT_EQ(got.name, std::string("UnitTestTool"));
    ASSERT_EQ(got.version, std::string("1.0.0"));
    ASSERT_NEAR(got.reliability_score, 0.95, 1e-9);

    ASSERT_TRUE(registry.unregisterTool("UnitTestTool"));
    ASSERT_FALSE(registry.isToolRegistered("UnitTestTool"));
}

TEST(ToolRegistry_CategoryAndCapabilityQuery)
{
    auto as = make_as();
    ToolRegistry registry(as);

    auto dummy = [](const HandleSeq&, AtomSpacePtr) -> Handle {
        return Handle::UNDEFINED;
    };

    ToolRegistry::ToolMetadata viz;
    viz.name = "VizProbe";
    viz.description = "visualization probe";
    viz.category = ToolRegistry::ToolCategory::VISUALIZATION;
    viz.capabilities = {ToolRegistry::ToolCapability::READ_ONLY};
    registry.registerTool(viz, dummy);

    ToolRegistry::ToolMetadata analysis;
    analysis.name = "AnalysisProbe";
    analysis.description = "analysis probe";
    analysis.category = ToolRegistry::ToolCategory::ANALYSIS;
    analysis.capabilities = {
        ToolRegistry::ToolCapability::READ_WRITE,
        ToolRegistry::ToolCapability::BATCH_PROCESSING
    };
    registry.registerTool(analysis, dummy);

    auto viz_tools = registry.getToolsByCategory(ToolRegistry::ToolCategory::VISUALIZATION);
    bool found_viz = false;
    for (const auto& n : viz_tools) {
        if (n == "VizProbe") found_viz = true;
    }
    ASSERT_TRUE(found_viz);

    auto caps = registry.getToolsByCapabilities(
        {ToolRegistry::ToolCapability::BATCH_PROCESSING});
    bool found_analysis = false;
    for (const auto& n : caps) {
        if (n == "AnalysisProbe") found_analysis = true;
    }
    ASSERT_TRUE(found_analysis);

    auto hits = registry.searchTools("visualization");
    ASSERT_GT(hits.size(), static_cast<size_t>(0));
}

TEST(ToolRegistry_ExecuteTool)
{
    auto as = make_as();
    ToolRegistry registry(as);

    ToolRegistry::ToolMetadata meta;
    meta.name = "EchoTool";
    meta.description = "returns a concept atom";
    meta.category = ToolRegistry::ToolCategory::UTILITY;

    auto executor = [](const HandleSeq& args, AtomSpacePtr space) -> Handle {
        std::string name = args.empty() ? "empty" : args[0]->get_name();
        return space->add_node(CONCEPT_NODE, "Echo:" + name);
    };

    registry.registerTool(meta, executor);
    Handle input = as->add_node(CONCEPT_NODE, "Hello");
    Handle result = registry.executeTool("EchoTool", {input});
    ASSERT_TRUE(result != Handle::UNDEFINED);
    ASSERT_EQ(result->get_name(), std::string("Echo:Hello"));

    registry.updateToolReliability("EchoTool", true);
    auto stats = registry.getToolStatistics();
    ASSERT_TRUE(stats.count("EchoTool") > 0);
}

TEST(ToolRegistry_StatusAndConfig)
{
    auto as = make_as();
    ToolRegistry registry(as);

    ToolRegistry::ToolMetadata meta;
    meta.name = "StatusTool";
    meta.description = "status probe";
    meta.category = ToolRegistry::ToolCategory::UTILITY;
    registry.registerTool(meta, [](const HandleSeq&, AtomSpacePtr) {
        return Handle::UNDEFINED;
    });

    ASSERT_EQ(registry.getToolStatus("StatusTool"),
              ToolRegistry::ToolStatus::AVAILABLE);

    registry.setEnableToolComposition(false);
    registry.setEnableCapabilityMatching(true);
    registry.setMinimumReliabilityThreshold(0.5);

    auto cfg = registry.getConfiguration();
    ASSERT_TRUE(cfg.count("enable_tool_composition") > 0);
    ASSERT_EQ(cfg["enable_tool_composition"], std::string("false"));
    ASSERT_TRUE(cfg["minimum_reliability_threshold"].find("0.5") != std::string::npos);
}
