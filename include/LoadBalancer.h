/**
 * LoadBalancer.h
 * 
 * Load Balancing for Distributed Agent-Zero
 * Part of the AGENT-ZERO-GENESIS project (AZ-SCALE-001)
 * 
 * Implements intelligent load balancing strategies for distributing
 * computational tasks across cluster nodes with OpenCog integration.
 */

#ifndef _OPENCOG_AGENTZERO_LOAD_BALANCER_H
#define _OPENCOG_AGENTZERO_LOAD_BALANCER_H

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <queue>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/util/Logger.h>

namespace opencog { namespace agentzero {

// Forward declarations
struct ComputeNode;
struct DistributedTask;

/**
 * Load balancing strategy enumeration
 */
enum class LoadBalancingStrategy {
    ROUND_ROBIN,        // Simple round-robin distribution
    LEAST_LOADED,       // Assign to node with least current load
    WEIGHTED_RANDOM,    // Random weighted by available capacity
    TASK_AFFINITY,      // Consider task type and node capabilities
    LOCALITY_AWARE      // Consider data locality in AtomSpace
};

/**
 * Task assignment result
 */
struct TaskAssignment {
    std::string task_id;
    std::string node_id;
    bool success;
    std::string error_message;
    
    TaskAssignment() : success(false) {}
    TaskAssignment(const std::string& tid, const std::string& nid)
        : task_id(tid), node_id(nid), success(true) {}
};

/**
 * LoadBalancer
 * 
 * Intelligent load balancing for distributed Agent-Zero tasks.
 * Implements multiple strategies for optimal task distribution
 * across compute nodes.
 * 
 * Key Features:
 * - Multiple load balancing strategies
 * - Task type-aware assignment
 * - Dynamic load monitoring
 * - AtomSpace locality optimization
 * - Adaptive strategy selection
 */
class LoadBalancer {
public:
    /**
     * Constructor
     * @param atomspace Shared AtomSpace for distributed operations
     * @param strategy Initial load balancing strategy
     */
    LoadBalancer(AtomSpacePtr atomspace, 
                LoadBalancingStrategy strategy = LoadBalancingStrategy::LEAST_LOADED);
    
    /**
     * Destructor
     */
    ~LoadBalancer();
    
    /**
     * Assign a task to an optimal node
     * @param task Task to be assigned
     * @param available_nodes List of available compute nodes
     * @return Task assignment result with selected node
     */
    TaskAssignment assignTask(const DistributedTask& task,
                            const std::vector<ComputeNode>& available_nodes);
    
    /**
     * Assign multiple tasks in batch
     * @param tasks Vector of tasks to assign
     * @param available_nodes List of available compute nodes
     * @return Vector of task assignments
     */
    std::vector<TaskAssignment> assignTasks(
        const std::vector<DistributedTask>& tasks,
        const std::vector<ComputeNode>& available_nodes);
    
    /**
     * Change load balancing strategy
     * @param strategy New strategy to use
     */
    void setStrategy(LoadBalancingStrategy strategy);
    
    /**
     * Get current strategy
     * @return Current load balancing strategy
     */
    LoadBalancingStrategy getStrategy() const { return strategy_; }
    
    /**
     * Update node load information
     * @param node_id Node identifier
     * @param current_load Current load value (0-100)
     */
    void updateNodeLoad(const std::string& node_id, int current_load);
    
    /**
     * Get load statistics
     * @return Map of load distribution statistics
     */
    std::map<std::string, double> getLoadStats() const;
    
    /**
     * Suggest task migration for rebalancing
     * @param nodes Current cluster nodes with their loads
     * @return Suggested task migrations (task_id -> new_node_id)
     */
    std::map<std::string, std::string> suggestRebalancing(
        const std::vector<ComputeNode>& nodes);
    
    /**
     * Calculate optimal task distribution
     * @param num_tasks Number of tasks to distribute
     * @param nodes Available compute nodes
     * @return Map of node_id -> number of tasks to assign
     */
    std::map<std::string, int> calculateDistribution(
        int num_tasks,
        const std::vector<ComputeNode>& nodes);
    
    /**
     * Enable/disable adaptive strategy selection
     * When enabled, LoadBalancer automatically switches strategies
     * based on cluster conditions
     * @param enabled True to enable adaptive mode
     */
    void setAdaptiveMode(bool enabled);
    
    /**
     * Check if adaptive mode is enabled
     * @return True if adaptive mode is enabled
     */
    bool isAdaptiveMode() const { return adaptive_mode_; }

private:
    AtomSpacePtr atomspace_;
    LoadBalancingStrategy strategy_;
    bool adaptive_mode_;
    
    // Node load tracking
    std::map<std::string, int> node_loads_;
    mutable std::mutex load_mutex_;
    
    // Round-robin state
    size_t round_robin_index_;
    
    // Statistics
    std::map<std::string, int> assignments_per_node_;
    
    // Strategy implementations
    TaskAssignment assignRoundRobin(const DistributedTask& task,
                                   const std::vector<ComputeNode>& nodes);
    TaskAssignment assignLeastLoaded(const DistributedTask& task,
                                    const std::vector<ComputeNode>& nodes);
    TaskAssignment assignWeightedRandom(const DistributedTask& task,
                                       const std::vector<ComputeNode>& nodes);
    TaskAssignment assignTaskAffinity(const DistributedTask& task,
                                     const std::vector<ComputeNode>& nodes);
    TaskAssignment assignLocalityAware(const DistributedTask& task,
                                      const std::vector<ComputeNode>& nodes);
    
    // Helper methods
    bool nodeSupportsTask(const ComputeNode& node, const std::string& task_type);
    double calculateTaskAffinity(const ComputeNode& node, const DistributedTask& task);
    int calculateNodeScore(const ComputeNode& node);
    void updateStrategy();  // For adaptive mode
    
    // Logging
    Logger logger_;
};

}} // namespace opencog::agentzero

#endif // _OPENCOG_AGENTZERO_LOAD_BALANCER_H
