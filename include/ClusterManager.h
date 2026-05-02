/**
 * ClusterManager.h
 * 
 * Cluster Management for Distributed Agent-Zero
 * Part of the AGENT-ZERO-GENESIS project (AZ-SCALE-001)
 * 
 * Manages compute cluster resources, node discovery, and cluster topology
 * for distributed OpenCog operations.
 */

#ifndef _OPENCOG_AGENTZERO_CLUSTER_MANAGER_H
#define _OPENCOG_AGENTZERO_CLUSTER_MANAGER_H

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <chrono>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/util/Logger.h>

namespace opencog { namespace agentzero {

/**
 * Node capabilities and resources
 */
struct NodeCapabilities {
    int cpu_cores;
    size_t memory_mb;
    bool has_gpu;
    std::vector<std::string> supported_tasks;
    
    NodeCapabilities() 
        : cpu_cores(1), memory_mb(1024), has_gpu(false) {}
};

/**
 * Node health status
 */
struct NodeHealth {
    bool is_responsive;
    std::chrono::system_clock::time_point last_heartbeat;
    double cpu_usage;
    double memory_usage;
    int active_tasks;
    
    NodeHealth()
        : is_responsive(true), 
          last_heartbeat(std::chrono::system_clock::now()),
          cpu_usage(0.0), memory_usage(0.0), active_tasks(0) {}
};

/**
 * ClusterManager
 * 
 * Manages the distributed compute cluster for Agent-Zero operations.
 * Handles node discovery, health monitoring, resource tracking, and
 * cluster topology management.
 * 
 * Key Features:
 * - Automatic node discovery and registration
 * - Health monitoring and failure detection
 * - Resource capacity tracking
 * - Cluster topology management
 * - Integration with AtomSpace for cluster state representation
 */
class ClusterManager {
public:
    /**
     * Constructor
     * @param atomspace Shared AtomSpace for cluster state
     * @param cluster_id Unique identifier for this cluster
     */
    ClusterManager(AtomSpacePtr atomspace, const std::string& cluster_id);
    
    /**
     * Destructor
     */
    ~ClusterManager();
    
    /**
     * Initialize cluster manager and start discovery
     * @return true if initialization successful
     */
    bool initialize();
    
    /**
     * Add a node to the cluster
     * @param node_id Unique node identifier
     * @param hostname Node hostname/IP
     * @param port Communication port
     * @param capabilities Node capabilities
     * @return true if node added successfully
     */
    bool addNode(const std::string& node_id,
                const std::string& hostname,
                int port,
                const NodeCapabilities& capabilities);
    
    /**
     * Remove a node from the cluster
     * @param node_id Node identifier
     * @return true if node removed successfully
     */
    bool removeNode(const std::string& node_id);
    
    /**
     * Update node health status
     * @param node_id Node identifier
     * @param health Current health status
     */
    void updateNodeHealth(const std::string& node_id, const NodeHealth& health);
    
    /**
     * Get node capabilities
     * @param node_id Node identifier
     * @return Node capabilities, or empty if node not found
     */
    NodeCapabilities getNodeCapabilities(const std::string& node_id) const;
    
    /**
     * Get node health status
     * @param node_id Node identifier
     * @return Node health status
     */
    NodeHealth getNodeHealth(const std::string& node_id) const;
    
    /**
     * Get all healthy nodes
     * @return Vector of node IDs that are healthy and responsive
     */
    std::vector<std::string> getHealthyNodes() const;
    
    /**
     * Get all nodes with specific capability
     * @param capability Required capability (e.g., "reasoning", "gpu")
     * @return Vector of node IDs with that capability
     */
    std::vector<std::string> getNodesWithCapability(const std::string& capability) const;
    
    /**
     * Get total cluster capacity
     * @return Map of resource totals (cpu_cores, memory_mb, etc.)
     */
    std::map<std::string, size_t> getClusterCapacity() const;
    
    /**
     * Get available cluster resources
     * @return Map of currently available resources
     */
    std::map<std::string, size_t> getAvailableResources() const;
    
    /**
     * Perform cluster-wide health check
     * Pings all nodes and updates their status
     * @return Number of responsive nodes
     */
    int performHealthCheck();
    
    /**
     * Get cluster identifier
     * @return Cluster ID
     */
    std::string getClusterId() const { return cluster_id_; }
    
    /**
     * Get number of nodes in cluster
     * @return Total node count
     */
    size_t getNodeCount() const;
    
    /**
     * Shutdown cluster manager
     */
    void shutdown();

private:
    AtomSpacePtr atomspace_;
    std::string cluster_id_;
    
    // Node data structures
    struct NodeInfo {
        std::string node_id;
        std::string hostname;
        int port;
        NodeCapabilities capabilities;
        NodeHealth health;
    };
    
    std::map<std::string, NodeInfo> nodes_;
    mutable std::mutex nodes_mutex_;
    
    // Helper methods
    void storeNodeInAtomSpace(const NodeInfo& node);
    void removeNodeFromAtomSpace(const std::string& node_id);
    bool pingNode(const std::string& node_id);
    
    // Logging
    Logger logger_;
};

}} // namespace opencog::agentzero

#endif // _OPENCOG_AGENTZERO_CLUSTER_MANAGER_H
