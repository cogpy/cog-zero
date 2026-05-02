/**
 * DistributedCoordinator.h
 * 
 * Distributed Computing Coordinator for Agent-Zero
 * Part of the AGENT-ZERO-GENESIS project (AZ-SCALE-001)
 * 
 * Manages coordination of Agent-Zero instances across distributed computing nodes,
 * enabling scalable multi-agent systems with OpenCog integration.
 */

#ifndef _OPENCOG_AGENTZERO_DISTRIBUTED_COORDINATOR_H
#define _OPENCOG_AGENTZERO_DISTRIBUTED_COORDINATOR_H

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <mutex>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/util/Logger.h>

namespace opencog { namespace agentzero {

/**
 * Represents a compute node in the distributed system
 */
struct ComputeNode {
    std::string id;
    std::string hostname;
    int port;
    bool is_active;
    int capacity;
    int current_load;
    
    ComputeNode(const std::string& node_id, const std::string& host, int p)
        : id(node_id), hostname(host), port(p), 
          is_active(true), capacity(100), current_load(0) {}
};

/**
 * Task to be distributed across compute nodes
 */
struct DistributedTask {
    std::string task_id;
    std::string task_type;
    Handle task_atom;  // AtomSpace representation of the task
    std::string assigned_node;
    bool completed;
    
    DistributedTask(const std::string& id, const std::string& type, Handle atom)
        : task_id(id), task_type(type), task_atom(atom), 
          assigned_node(""), completed(false) {}
};

/**
 * DistributedCoordinator
 * 
 * Central coordinator for distributed Agent-Zero operations.
 * Manages cluster of compute nodes, task distribution, and result aggregation.
 * 
 * Key Features:
 * - Node registration and health monitoring
 * - Task distribution with load balancing
 * - Result aggregation from distributed workers
 * - Integration with AtomSpace for distributed knowledge
 */
class DistributedCoordinator {
public:
    /**
     * Constructor
     * @param atomspace Shared AtomSpace for distributed operations
     * @param coordinator_id Unique identifier for this coordinator
     */
    DistributedCoordinator(AtomSpacePtr atomspace, const std::string& coordinator_id);
    
    /**
     * Destructor - ensures graceful shutdown of all nodes
     */
    ~DistributedCoordinator();
    
    /**
     * Register a compute node with the coordinator
     * @param node_id Unique identifier for the node
     * @param hostname Network hostname or IP address
     * @param port Port number for communication
     * @return true if registration successful
     */
    bool registerNode(const std::string& node_id, 
                     const std::string& hostname, 
                     int port);
    
    /**
     * Unregister a compute node
     * @param node_id Node identifier to remove
     * @return true if unregistration successful
     */
    bool unregisterNode(const std::string& node_id);
    
    /**
     * Submit a task for distributed execution
     * @param task_type Type of task (e.g., "reasoning", "learning", "planning")
     * @param task_atom AtomSpace Handle representing the task
     * @return Task ID for tracking
     */
    std::string submitTask(const std::string& task_type, Handle task_atom);
    
    /**
     * Get status of a submitted task
     * @param task_id Task identifier
     * @return true if task is completed
     */
    bool isTaskCompleted(const std::string& task_id) const;
    
    /**
     * Get result of a completed task
     * @param task_id Task identifier
     * @return Handle to result atom, or Handle::UNDEFINED if not ready
     */
    Handle getTaskResult(const std::string& task_id) const;
    
    /**
     * Get list of all registered nodes
     * @return Vector of node information
     */
    std::vector<ComputeNode> getRegisteredNodes() const;
    
    /**
     * Get cluster statistics
     * @return Map of statistics (total_nodes, active_nodes, total_capacity, etc.)
     */
    std::map<std::string, int> getClusterStats() const;
    
    /**
     * Perform health check on all nodes
     * Updates node status and removes unresponsive nodes
     */
    void healthCheck();
    
    /**
     * Set callback for task completion notification
     * @param callback Function to call when task completes
     */
    void setTaskCompletionCallback(std::function<void(const std::string&)> callback);
    
    /**
     * Get coordinator ID
     * @return This coordinator's unique identifier
     */
    std::string getCoordinatorId() const { return coordinator_id_; }
    
    /**
     * Shutdown coordinator and all managed nodes
     */
    void shutdown();

private:
    AtomSpacePtr atomspace_;
    std::string coordinator_id_;
    
    // Node management
    std::map<std::string, ComputeNode> nodes_;
    mutable std::mutex nodes_mutex_;
    
    // Task management
    std::map<std::string, DistributedTask> tasks_;
    mutable std::mutex tasks_mutex_;
    
    // Callbacks
    std::function<void(const std::string&)> task_completion_callback_;
    
    // Helper methods
    std::string selectNodeForTask(const std::string& task_type);
    void updateNodeLoad(const std::string& node_id, int delta);
    std::string generateTaskId();
    
    // Logging
    Logger logger_;
};

}} // namespace opencog::agentzero

#endif // _OPENCOG_AGENTZERO_DISTRIBUTED_COORDINATOR_H
