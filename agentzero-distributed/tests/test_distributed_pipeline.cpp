#include "test_runner.h"

#include <opencog/agentzero/distributed/ClusterManager.h>
#include <opencog/agentzero/distributed/DistributedCoordinator.h>
#include <opencog/agentzero/distributed/LoadBalancer.h>
#include <opencog/agentzero/distributed/CoordinationProtocol.h>
#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/atom_types/types.h>

using namespace opencog;
using namespace opencog::agentzero;

TEST(DistributedPipeline_EndToEnd)
{
    auto as = createAtomSpace();

    ClusterManager cluster(as, "pipeline-cluster");
    ASSERT_TRUE(cluster.initialize());

    NodeCapabilities caps;
    caps.cpu_cores = 4;
    caps.memory_mb = 4096;
    caps.supported_tasks = {"reasoning", "planning"};
    ASSERT_TRUE(cluster.addNode("worker-1", "10.0.0.1", 8001, caps));
    ASSERT_TRUE(cluster.addNode("worker-2", "10.0.0.2", 8002, caps));

    DistributedCoordinator coord(as, "pipeline-coord");
    ASSERT_TRUE(coord.registerNode("worker-1", "10.0.0.1", 8001));
    ASSERT_TRUE(coord.registerNode("worker-2", "10.0.0.2", 8002));

    LoadBalancer balancer(as, LoadBalancingStrategy::LEAST_LOADED);
    auto nodes = coord.getRegisteredNodes();
    // Simulate uneven load
    nodes[0].current_load = 5;
    nodes[1].current_load = 40;

    Handle task_atom = as->add_node(CONCEPT_NODE, "PipelineTask");
    DistributedTask task("pipe-task-1", "reasoning", task_atom);
    auto assignment = balancer.assignTask(task, nodes);
    ASSERT_TRUE(assignment.success);
    ASSERT_EQ(assignment.node_id, std::string("worker-1"));

    auto tid = coord.submitTask("reasoning", task_atom);
    ASSERT_FALSE(tid.empty());

    CoordinationProtocol proto(as, "worker-1");
    ASSERT_TRUE(proto.initialize());
    ASSERT_TRUE(proto.registerPeer("worker-2", "10.0.0.2", 8002));
    auto election = proto.electLeader();
    ASSERT_TRUE(election.success);

    auto handoff = proto.initiateHandoff(tid, "rebalance", "worker-2");
    ASSERT_FALSE(handoff.request_id.empty());
    ASSERT_TRUE(proto.respondToHandoff(handoff.request_id, true));

    auto proposal = proto.createProposal("accept-rebalance", 0.5);
    ASSERT_TRUE(proto.castVote(proposal, "worker-1", true));
    ASSERT_TRUE(proto.castVote(proposal, "worker-2", true));
    ASSERT_TRUE(proto.tallyVotes(proposal).accepted);

    auto stats = coord.getClusterStats();
    ASSERT_EQ(stats["total_nodes"], 2);
    ASSERT_EQ(cluster.getHealthyNodes().size(), static_cast<size_t>(2));

    proto.shutdown();
    coord.shutdown();
    cluster.shutdown();
}
