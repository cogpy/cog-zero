#include "test_runner.h"

#include <opencog/agentzero/distributed/LoadBalancer.h>
#include <opencog/agentzero/distributed/DistributedCoordinator.h>
#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/atom_types/types.h>

using namespace opencog;
using namespace opencog::agentzero;

static AtomSpacePtr make_as() { return createAtomSpace(); }

static std::vector<ComputeNode> sample_nodes()
{
    ComputeNode n1("node1", "localhost", 8080);
    n1.capacity = 100;
    n1.current_load = 10;
    ComputeNode n2("node2", "localhost", 8081);
    n2.capacity = 100;
    n2.current_load = 50;
    ComputeNode n3("node3", "localhost", 8082);
    n3.capacity = 100;
    n3.current_load = 30;
    return {n1, n2, n3};
}

TEST(LoadBalancer_Strategies)
{
    auto as = make_as();
    LoadBalancer balancer(as);
    ASSERT_EQ(balancer.getStrategy(), LoadBalancingStrategy::LEAST_LOADED);

    balancer.setStrategy(LoadBalancingStrategy::ROUND_ROBIN);
    ASSERT_EQ(balancer.getStrategy(), LoadBalancingStrategy::ROUND_ROBIN);
    balancer.setStrategy(LoadBalancingStrategy::WEIGHTED_RANDOM);
    ASSERT_EQ(balancer.getStrategy(), LoadBalancingStrategy::WEIGHTED_RANDOM);
    balancer.setStrategy(LoadBalancingStrategy::LEAST_LOADED);

    auto nodes = sample_nodes();
    Handle atom = as->add_node(CONCEPT_NODE, "TaskAtom");
    DistributedTask task("task1", "reasoning", atom);

    auto assignment = balancer.assignTask(task, nodes);
    ASSERT_TRUE(assignment.success);
    ASSERT_EQ(assignment.task_id, std::string("task1"));
    ASSERT_EQ(assignment.node_id, std::string("node1"));
}

TEST(LoadBalancer_RoundRobinAndEmpty)
{
    auto as = make_as();
    LoadBalancer balancer(as, LoadBalancingStrategy::ROUND_ROBIN);
    auto nodes = sample_nodes();

    Handle a1 = as->add_node(CONCEPT_NODE, "A1");
    Handle a2 = as->add_node(CONCEPT_NODE, "A2");
    Handle a3 = as->add_node(CONCEPT_NODE, "A3");
    DistributedTask t1("t1", "reasoning", a1);
    DistributedTask t2("t2", "reasoning", a2);
    DistributedTask t3("t3", "reasoning", a3);

    auto r1 = balancer.assignTask(t1, nodes);
    auto r2 = balancer.assignTask(t2, nodes);
    auto r3 = balancer.assignTask(t3, nodes);
    ASSERT_TRUE(r1.success && r2.success && r3.success);
    ASSERT_EQ(r1.node_id, std::string("node1"));
    ASSERT_EQ(r2.node_id, std::string("node2"));
    ASSERT_EQ(r3.node_id, std::string("node3"));

    std::vector<ComputeNode> empty;
    auto fail = balancer.assignTask(t1, empty);
    ASSERT_FALSE(fail.success);
    ASSERT_EQ(fail.error_message, std::string("No available nodes"));
}

TEST(LoadBalancer_BatchStatsDistributionAdaptive)
{
    auto as = make_as();
    LoadBalancer balancer(as, LoadBalancingStrategy::LEAST_LOADED);
    auto nodes = sample_nodes();

    std::vector<DistributedTask> tasks;
    for (int i = 0; i < 5; ++i) {
        Handle h = as->add_node(CONCEPT_NODE, "Batch" + std::to_string(i));
        tasks.emplace_back("task" + std::to_string(i), "reasoning", h);
    }
    auto assignments = balancer.assignTasks(tasks, nodes);
    ASSERT_EQ(assignments.size(), static_cast<size_t>(5));
    for (const auto& a : assignments) {
        ASSERT_TRUE(a.success);
    }

    balancer.updateNodeLoad("node1", 25);
    balancer.updateNodeLoad("node2", 75);
    auto stats = balancer.getLoadStats();
    ASSERT_NEAR(stats["average_load"], 50.0, 1e-9);
    ASSERT_NEAR(stats["max_load"], 75.0, 1e-9);
    ASSERT_NEAR(stats["min_load"], 25.0, 1e-9);
    ASSERT_NEAR(stats["load_variance"], 50.0, 1e-9);

    auto dist = balancer.calculateDistribution(10, nodes);
    int total = 0;
    for (const auto& kv : dist) total += kv.second;
    ASSERT_GT(total, 0);
    ASSERT_LE(total, 10);

    auto migrations = balancer.suggestRebalancing(nodes);
    (void)migrations;

    ASSERT_FALSE(balancer.isAdaptiveMode());
    balancer.setAdaptiveMode(true);
    ASSERT_TRUE(balancer.isAdaptiveMode());
}
