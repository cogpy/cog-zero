#include "test_runner.h"

#include <opencog/agentzero/tools/RestApiAdapter.h>
#include <opencog/agentzero/tools/ToolWrapper.h>
#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/atom_types/types.h>

using namespace opencog;
using namespace opencog::agentzero::tools;

static AtomSpacePtr make_as() { return createAtomSpace(); }

TEST(RestApi_ConstructAndConfig)
{
    auto as = make_as();
    RestApiAdapter adapter("http://localhost:9999", as);
    ASSERT_EQ(adapter.getBaseUrl(), std::string("http://localhost:9999"));
    ASSERT_EQ(adapter.getSuccessCount(), 0);
    ASSERT_EQ(adapter.getFailureCount(), 0);

    adapter.setBaseUrl("http://api.example.com:8080");
    ASSERT_EQ(adapter.getBaseUrl(), std::string("http://api.example.com:8080"));

    adapter.setTimeout(5000.0);
    adapter.setDefaultHeader("Accept", "application/json");
    adapter.setBearerToken("token-abc");
    adapter.setBasicAuth("user", "pass");
}

TEST(RestApi_HttpResponseJson)
{
    HttpResponse resp;
    resp.status_code = 200;
    resp.body = "{\"result\":\"ok\"}";
    resp.success = true;
    resp.elapsed_ms = 42.5;
    resp.headers["Content-Type"] = "application/json";

    std::string json = resp.toJSON();
    ASSERT_TRUE(json.find("\"status_code\":200") != std::string::npos);
    ASSERT_TRUE(json.find("\"success\":true") != std::string::npos);
    ASSERT_TRUE(json.find("result") != std::string::npos);
}

TEST(RestApi_ResponseToAtom)
{
    auto as = make_as();
    RestApiAdapter adapter("http://localhost:1", as);

    HttpResponse resp;
    resp.status_code = 201;
    resp.body = "{\"id\":1}";
    resp.success = true;
    resp.elapsed_ms = 1.0;

    Handle h = adapter.responseToAtom(resp, as);
    ASSERT_TRUE(h != Handle::UNDEFINED);
}

TEST(RestApi_CreateToolWrapper)
{
    auto as = make_as();
    RestApiAdapter adapter("http://127.0.0.1:1", as);
    auto tool = adapter.createToolWrapper("status_tool", "/api/v1/status", as);
    ASSERT_TRUE(tool != nullptr);
    ASSERT_EQ(tool->getToolName(), std::string("status_tool"));
    ASSERT_EQ(tool->getToolType(), ToolType::EXTERNAL_REST_API);
}

TEST(RestApi_CallUnreachableEndpoint)
{
    // Connection must fail cleanly without throwing when nothing listens.
    auto as = make_as();
    RestApiAdapter adapter("http://127.0.0.1:1", as);
    adapter.setTimeout(500.0);

    HttpResponse r = adapter.get("/health");
    ASSERT_FALSE(r.success);
    ASSERT_TRUE(r.status_code == 0 || r.status_code >= 400);
    ASSERT_GE(adapter.getFailureCount(), 1);

    ToolExecutionContext ctx(as);
    ctx.setParameter("q", "1");
    ToolResult tr = adapter.callTool("/api/v1/query", ctx);
    ASSERT_FALSE(tr.isSuccess());

    auto stats = adapter.getStatistics();
    ASSERT_FALSE(stats.empty());
}
