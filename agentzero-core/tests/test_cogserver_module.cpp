#include "test_runner.h"

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/cogserver/server/CogServer.h>
#include <opencog/agentzero/AgentZeroCore.h>
#include <opencog/atoms/atom_types/types.h>

using namespace opencog;
using namespace opencog::agentzero;

TEST(CogServer_ModuleRegistrationAndCommands)
{
    CogServer server;
    server.loadModules();
    ASSERT_TRUE(server.modulesLoaded());

#ifdef HAVE_COGSERVER
    AgentZeroCore core(server, "ModuleAgent");
    // Module interface
    const char* mid = core.id();
    ASSERT_TRUE(mid != nullptr);
    ASSERT_TRUE(std::string(mid).find("AgentZeroCore") != std::string::npos);

    ASSERT_TRUE(core.config("cognitive_loop=true"));
    core.init();
    ASSERT_TRUE(core.isInitialized());

    // Register module-style commands on the shim CogServer
    server.registerCommand("agent-status", [&](const std::string&) {
        return core.getStatusInfo();
    });
    server.registerCommand("agent-start", [&](const std::string&) {
        return core.start() ? "OK" : "FAIL";
    });
    server.registerCommand("agent-stop", [&](const std::string&) {
        return core.stop() ? "OK" : "FAIL";
    });
    server.registerCommand("agent-goal", [&](const std::string& args) {
        Handle g = core.getAtomSpace()->add_node(CONCEPT_NODE, args.empty() ? "DefaultGoal" : args);
        return core.setGoal(g) ? "OK" : "FAIL";
    });
    server.registerCommand("agent-step", [&](const std::string&) {
        return core.processCognitiveStep() ? "OK" : "FAIL";
    });

    ASSERT_TRUE(server.hasCommand("agent-status"));
    ASSERT_TRUE(server.hasCommand("agent-start"));
    ASSERT_TRUE(server.hasCommand("agent-goal"));

    auto cmds = server.listCommands();
    ASSERT_GE(cmds.size(), 5u);

    ASSERT_EQ(server.runCommand("agent-start"), std::string("OK"));
    ASSERT_TRUE(core.isRunning());
    ASSERT_EQ(server.runCommand("agent-goal", "FindFood"), std::string("OK"));
    ASSERT_EQ(server.runCommand("agent-step"), std::string("OK"));

    std::string status = server.runCommand("agent-status");
    ASSERT_TRUE(status.find("ModuleAgent") != std::string::npos);

    ASSERT_EQ(server.runCommand("agent-stop"), std::string("OK"));
    ASSERT_FALSE(core.isRunning());

    // AtomSpace retained full agent state
    ASSERT_GT(server.getAtomSpace()->get_size(), 0u);
#else
    // Without CogServer, still verify AtomSpace-backed agent responds to the
    // same logical command surface via direct API.
    AgentZeroCore core("ModuleAgent", server.getAtomSpace());
    ASSERT_TRUE(core.initialize("ModuleAgent", server.getAtomSpace()));
    server.registerCommand("agent-status", [&](const std::string&) { return core.getStatusInfo(); });
    ASSERT_TRUE(server.hasCommand("agent-status"));
    ASSERT_TRUE(server.runCommand("agent-status").find("ModuleAgent") != std::string::npos);
#endif
}
