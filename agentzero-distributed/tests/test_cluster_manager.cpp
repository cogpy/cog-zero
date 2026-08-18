#include "test_runner.h"

#include <opencog/agentzero/distributed/ClusterManager.h>
#include <opencog/atomspace/AtomSpace.h>

using namespace opencog;
using namespace opencog::agentzero;

static AtomSpacePtr make_as() { return createAtomSpace(); }

TEST(ClusterManager_ConstructAndId)
{
    auto as = make_as();
    ClusterManager cluster(as, "test-cluster");
    ASSERT_TRUE(cluster.initialize());
    ASSERT_EQ(cluster.getClusterId(), std::string("test-cluster"));
    ASSERT_EQ(cluster.getNodeCount(), static_cast<size_t>(0));
}

TEST(ClusterManager_AddRemoveNode)
{
    auto as = make_as();
    ClusterManager cluster(as, "c1");
    cluster.initialize();

    NodeCapabilities caps;
    caps.cpu_cores = 4;
    caps.memory_mb = 8192;
    caps.has_gpu = false;

    ASSERT_TRUE(cluster.addNode("node1", "192.168.1.10", 8080, caps));
    ASSERT_EQ(cluster.getNodeCount(), static_cast<size_t>(1));
    ASSERT_FALSE(cluster.addNode("node1", "192.168.1.10", 8080, caps));

    ASSERT_TRUE(cluster.removeNode("node1"));
    ASSERT_EQ(cluster.getNodeCount(), static_cast<size_t>(0));
    ASSERT_FALSE(cluster.removeNode("missing"));
}

TEST(ClusterManager_CapabilitiesHealthAndCapacity)
{
    auto as = make_as();
    ClusterManager cluster(as, "c2");
    cluster.initialize();

    NodeCapabilities caps;
    caps.cpu_cores = 8;
    caps.memory_mb = 16384;
    caps.has_gpu = true;
    caps.supported_tasks = {"reasoning", "learning"};

    ASSERT_TRUE(cluster.addNode("n1", "host-a", 9000, caps));

    auto got = cluster.getNodeCapabilities("n1");
    ASSERT_EQ(got.cpu_cores, 8);
    ASSERT_EQ(got.memory_mb, static_cast<size_t>(16384));
    ASSERT_TRUE(got.has_gpu);
    ASSERT_EQ(got.supported_tasks.size(), static_cast<size_t>(2));

    NodeHealth health;
    health.is_responsive = true;
    health.cpu_usage = 0.5;
    health.memory_usage = 0.25;
    health.active_tasks = 2;
    cluster.updateNodeHealth("n1", health);

    auto h = cluster.getNodeHealth("n1");
    ASSERT_TRUE(h.is_responsive);
    ASSERT_NEAR(h.cpu_usage, 0.5, 1e-9);
    ASSERT_EQ(h.active_tasks, 2);

    auto healthy = cluster.getHealthyNodes();
    ASSERT_EQ(healthy.size(), static_cast<size_t>(1));

    auto with_reason = cluster.getNodesWithCapability("reasoning");
    ASSERT_EQ(with_reason.size(), static_cast<size_t>(1));
    ASSERT_EQ(with_reason[0], std::string("n1"));

    auto capacity = cluster.getClusterCapacity();
    ASSERT_EQ(capacity["total_cpu_cores"], static_cast<size_t>(8));
    ASSERT_EQ(capacity["total_memory_mb"], static_cast<size_t>(16384));
    ASSERT_EQ(capacity["gpu_nodes"], static_cast<size_t>(1));

    auto available = cluster.getAvailableResources();
    ASSERT_EQ(available["available_cpu_cores"], static_cast<size_t>(4));
    ASSERT_EQ(available["available_memory_mb"], static_cast<size_t>(12288));

    ASSERT_EQ(cluster.performHealthCheck(), 1);

    cluster.shutdown();
    ASSERT_EQ(cluster.getNodeCount(), static_cast<size_t>(0));
}

TEST(ClusterManager_MigrationHooks)
{
    auto as = make_as();
    ClusterManager cluster(as, "mig");
    cluster.initialize();

    NodeCapabilities caps;
    caps.cpu_cores = 2;
    caps.memory_mb = 2048;
    cluster.addNode("src", "h1", 1, caps);
    cluster.addNode("dst", "h2", 2, caps);

    ASSERT_EQ(cluster.getMigrationStatus("agent-1"), std::string("none"));
    ASSERT_TRUE(cluster.initiateMigration("src", "dst", "agent-1"));
    auto status = cluster.getMigrationStatus("agent-1");
    ASSERT_TRUE(status == "pending" || status == "in_progress" || status == "completed");

    // receiveMigration expects a JSON payload with name + required agent fields
    const std::string payload =
        "{\"name\":\"agent-1\",\"atoms\":[],\"goals\":[],\"tasks\":[],\"episodes\":[]}";
    ASSERT_TRUE(cluster.receiveMigration(payload, "src"));
    ASSERT_EQ(cluster.getMigrationStatus("agent-1"), std::string("completed"));
}
