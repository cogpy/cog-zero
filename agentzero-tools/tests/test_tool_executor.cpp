#include "test_runner.h"

#include <opencog/agentzero/tools/ToolExecutor.h>
#include <opencog/agentzero/tools/ToolWrapper.h>
#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/atom_types/types.h>

using namespace opencog;
using namespace opencog::agentzero::tools;

static AtomSpacePtr make_as() { return createAtomSpace(); }

TEST(ToolExecutor_Construct)
{
    auto as = make_as();
    ToolExecutor exec(as);
    ASSERT_EQ(exec.getExecutionCount(), 0);
    ASSERT_EQ(exec.getSuccessCount(), 0);
    ASSERT_EQ(exec.getFailureCount(), 0);
    ASSERT_EQ(exec.getTimeoutCount(), 0);
}

TEST(ToolExecutor_NormaliseCompleted)
{
    ToolExecutor exec(make_as());

    ToolResult raw(ToolStatus::COMPLETED);
    raw.setOutput("hello world");
    raw.setMetadata("key", "value");
    raw.setExecutionTime(12.5);

    NormalisedResult n = exec.normalise(raw);
    ASSERT_TRUE(n.success);
    ASSERT_EQ(n.output, std::string("hello world"));
    ASSERT_EQ(n.metadata.at("key"), std::string("value"));
    ASSERT_EQ(n.metadata.at("raw_status"), std::string("COMPLETED"));
    ASSERT_NEAR(n.execution_time_ms, 12.5, 1e-9);

    auto json = n.toJSON();
    ASSERT_TRUE(json.find("\"success\":true") != std::string::npos);
    ASSERT_FALSE(n.toString().empty());
}

TEST(ToolExecutor_NormaliseFailedAndTimeout)
{
    ToolExecutor exec(make_as());

    ToolResult failed(ToolStatus::FAILED);
    failed.setErrorMessage("boom");
    NormalisedResult nf = exec.normalise(failed);
    ASSERT_FALSE(nf.success);
    ASSERT_EQ(nf.error, std::string("boom"));
    ASSERT_EQ(nf.metadata.at("raw_status"), std::string("FAILED"));

    ToolResult timed(ToolStatus::TIMEOUT);
    timed.setErrorMessage("too slow");
    NormalisedResult nt = exec.normalise(timed);
    ASSERT_FALSE(nt.success);
    ASSERT_EQ(nt.metadata.at("raw_status"), std::string("TIMEOUT"));
}

TEST(ToolExecutor_ExecuteWrapper)
{
    auto as = make_as();
    ToolExecutor exec(as);

    ToolWrapper tool("unit_echo", ToolType::CUSTOM);
    tool.setAtomSpace(as);
    tool.setDescription("echo parameters");
    // CUSTOM tools still produce a structured result via ToolWrapper
    ToolExecutionContext ctx(as);
    ctx.setParameter("msg", "ping");

    SandboxPolicy policy;
    policy.timeout_ms = 5000.0;
    policy.allow_network_access = false;
    policy.allow_filesystem_write = false;

    NormalisedResult result = exec.execute(tool, ctx, policy);
    // Execution path must complete (success depends on tool type support)
    ASSERT_GE(exec.getExecutionCount(), 1);
    ASSERT_FALSE(result.toString().empty());
    ASSERT_EQ(result.metadata.at("tool_name"), std::string("unit_echo"));
}

TEST(ToolExecutor_ExecuteCommandSandbox)
{
    ToolExecutor exec(make_as());

    SandboxPolicy deny;
    deny.timeout_ms = 2000.0;
    deny.allow_filesystem_write = false;
    deny.allowed_paths = {"/bin", "/usr/bin"};

    // Disallowed absolute path outside allowlist should be rejected
    NormalisedResult bad = exec.executeCommand("/tmp/not-allowed-binary", {}, deny);
    ASSERT_FALSE(bad.success);
    ASSERT_FALSE(bad.error.empty());

    // Safe allowlisted command
    NormalisedResult ok = exec.executeCommand("/bin/echo", {"sandbox-ok"}, deny);
    ASSERT_TRUE(ok.success);
    ASSERT_TRUE(ok.output.find("sandbox-ok") != std::string::npos);

    auto stats = exec.getStatistics();
    ASSERT_TRUE(stats.find("execution_count") != std::string::npos ||
                stats.find("success") != std::string::npos);
}
