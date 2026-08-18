#include "test_runner.h"

#include <opencog/agentzero/ToolRegistry.h>
#include <opencog/agentzero/tools/ToolExecutor.h>
#include <opencog/agentzero/tools/ToolWrapper.h>
#include <opencog/agentzero/tools/RestApiAdapter.h>
#include <opencog/agentzero/tools/RosBehaviorBridge.h>
#include <opencog/agentzero/tools/CapabilityComposer.h>
#include <opencog/agentzero/tools/ResourceManager.h>
#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/atom_types/types.h>

using namespace opencog;
using namespace opencog::agentzero;
using namespace opencog::agentzero::tools;

static AtomSpacePtr make_as() { return createAtomSpace(); }

TEST(ToolsPipeline_RegistryExecutorCommand)
{
    auto as = make_as();
    ToolRegistry registry(as);
    ToolExecutor executor(as);

    ToolRegistry::ToolMetadata meta;
    meta.name = "PipelineEcho";
    meta.description = "pipeline echo";
    meta.category = ToolRegistry::ToolCategory::UTILITY;
    registry.registerTool(meta, [](const HandleSeq& args, AtomSpacePtr space) {
        return space->add_node(CONCEPT_NODE,
            args.empty() ? "none" : ("P:" + args[0]->get_name()));
    });

    Handle in = as->add_node(CONCEPT_NODE, "Goal");
    Handle out = registry.executeTool("PipelineEcho", {in});
    ASSERT_EQ(out->get_name(), std::string("P:Goal"));

    SandboxPolicy policy;
    policy.timeout_ms = 3000.0;
    policy.allowed_paths = {"/bin", "/usr/bin"};
    NormalisedResult cmd = executor.executeCommand("/bin/true", {}, policy);
    ASSERT_TRUE(cmd.success);
}

TEST(ToolsPipeline_RestAndRosIntegration)
{
    auto as = make_as();

    RestApiAdapter rest("http://127.0.0.1:1", as);
    rest.setTimeout(300.0);
    auto rest_tool = rest.createToolWrapper("rest_probe", "/v1/ping", as);
    ASSERT_TRUE(rest_tool != nullptr);

    ToolExecutor executor(as);
    ToolExecutionContext ctx(as);
    ctx.setParameter("hello", "world");
    SandboxPolicy policy;
    policy.timeout_ms = 1000.0;
    policy.allow_network_access = true;
    NormalisedResult nr = executor.execute(*rest_tool, ctx, policy);
    ASSERT_GE(executor.getExecutionCount(), 1);
    ASSERT_FALSE(nr.toJSON().empty());

    RosBehaviorBridge ros("pipeline_ros", as);
    ASSERT_TRUE(ros.connect());
    ASSERT_TRUE(ros.publish("/pipeline", RosMessageType::STD_MSGS_STRING,
                            "{\"data\":\"pipeline\"}"));

    auto ros_tool = ros.createTopicPublisherTool("pipeline_pub", "/pipeline");
    ToolExecutionContext rctx(as);
    rctx.setParameter("data", "from-executor");
    NormalisedResult nros = executor.execute(*ros_tool, rctx, policy);
    ASSERT_GE(executor.getExecutionCount(), 2);
    ASSERT_FALSE(nros.toString().empty());
}

TEST(ToolsPipeline_CapabilityAndResources)
{
    auto as = make_as();

    CapabilityComposer composer(as);
    CapabilityComposer::Capability cap;
    cap.capability_id = "cap.sense";
    cap.name = "Sense";
    cap.description = "sense capability";
    cap.provided_capabilities = {"percept"};
    cap.execute = [](const CapabilityComposer::ExecutionContext&) {
        return true;
    };
    ASSERT_TRUE(composer.registerCapability(cap));
    ASSERT_TRUE(composer.getCapability("cap.sense") != nullptr);

    ResourceManager resources(as);
    // Basic construction and pool registration should succeed
    ASSERT_TRUE(resources.createResourcePool(ResourceType::CPU, "cpu_pool", /*capacity=*/100.0));
    auto pool = resources.getResourcePool("cpu_pool");
    ASSERT_TRUE(pool != nullptr);
    ASSERT_NEAR(pool->getTotalCapacity(), 100.0, 1e-9);

    auto alloc = resources.allocateResource("agent-1", ResourceType::CPU, 10.0);
    ASSERT_TRUE(alloc != nullptr);
    ASSERT_TRUE(alloc->isActive());
    ASSERT_TRUE(resources.deallocateResource(alloc->getAllocationId()));
}
