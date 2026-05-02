/**
 * ClusterManager.cpp
 * 
 * Implementation of ClusterManager for Agent-Zero
 * Part of the AGENT-ZERO-GENESIS project (AZ-SCALE-001)
 */

#include <opencog/agentzero/distributed/ClusterManager.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>
#include <algorithm>

using namespace opencog;
using namespace opencog::agentzero;

ClusterManager::ClusterManager(AtomSpacePtr atomspace, const std::string& cluster_id)
    : atomspace_(atomspace),
      cluster_id_(cluster_id),
      logger_("ClusterManager")
{
    logger_.info("ClusterManager created with ID: %s", cluster_id.c_str());
}

ClusterManager::~ClusterManager()
{
    shutdown();
}

bool ClusterManager::initialize()
{
    logger_.info("Initializing ClusterManager");
    
    // Store cluster metadata in AtomSpace
    Handle cluster_node = atomspace_->add_node(CONCEPT_NODE, "Cluster:" + cluster_id_);
    Handle type_node = atomspace_->add_node(CONCEPT_NODE, "DistributedCluster");
    atomspace_->add_link(INHERITANCE_LINK, {cluster_node, type_node});
    
    logger_.info("ClusterManager initialized successfully");
    return true;
}

bool ClusterManager::addNode(const std::string& node_id,
                            const std::string& hostname,
                            int port,
                            const NodeCapabilities& capabilities)
{
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    
    // Check if node already exists
    if (nodes_.find(node_id) != nodes_.end()) {
        logger_.warn("Node %s already exists in cluster", node_id.c_str());
        return false;
    }
    
    // Create node info
    NodeInfo info;
    info.node_id = node_id;
    info.hostname = hostname;
    info.port = port;
    info.capabilities = capabilities;
    info.health.is_responsive = true;
    info.health.last_heartbeat = std::chrono::system_clock::now();
    
    nodes_[node_id] = info;
    
    // Store in AtomSpace
    storeNodeInAtomSpace(info);
    
    logger_.info("Added node %s to cluster (CPU: %d, Memory: %zu MB, GPU: %s)",
                node_id.c_str(),
                capabilities.cpu_cores,
                capabilities.memory_mb,
                capabilities.has_gpu ? "yes" : "no");
    
    return true;
}

bool ClusterManager::removeNode(const std::string& node_id)
{
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    
    auto it = nodes_.find(node_id);
    if (it == nodes_.end()) {
        logger_.warn("Node %s not found in cluster", node_id.c_str());
        return false;
    }
    
    // Remove from AtomSpace
    removeNodeFromAtomSpace(node_id);
    
    // Remove from local storage
    nodes_.erase(it);
    
    logger_.info("Removed node %s from cluster", node_id.c_str());
    return true;
}

void ClusterManager::updateNodeHealth(const std::string& node_id, 
                                     const NodeHealth& health)
{
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    
    auto it = nodes_.find(node_id);
    if (it != nodes_.end()) {
        it->second.health = health;
        it->second.health.last_heartbeat = std::chrono::system_clock::now();
    }
}

NodeCapabilities ClusterManager::getNodeCapabilities(const std::string& node_id) const
{
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    
    auto it = nodes_.find(node_id);
    if (it != nodes_.end()) {
        return it->second.capabilities;
    }
    
    return NodeCapabilities();
}

NodeHealth ClusterManager::getNodeHealth(const std::string& node_id) const
{
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    
    auto it = nodes_.find(node_id);
    if (it != nodes_.end()) {
        return it->second.health;
    }
    
    return NodeHealth();
}

std::vector<std::string> ClusterManager::getHealthyNodes() const
{
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    
    std::vector<std::string> healthy_nodes;
    
    for (const auto& pair : nodes_) {
        if (pair.second.health.is_responsive) {
            healthy_nodes.push_back(pair.first);
        }
    }
    
    return healthy_nodes;
}

std::vector<std::string> ClusterManager::getNodesWithCapability(
    const std::string& capability) const
{
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    
    std::vector<std::string> matching_nodes;
    
    for (const auto& pair : nodes_) {
        const auto& caps = pair.second.capabilities.supported_tasks;
        if (std::find(caps.begin(), caps.end(), capability) != caps.end()) {
            matching_nodes.push_back(pair.first);
        }
    }
    
    return matching_nodes;
}

std::map<std::string, size_t> ClusterManager::getClusterCapacity() const
{
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    
    std::map<std::string, size_t> capacity;
    
    size_t total_cpu = 0;
    size_t total_memory = 0;
    size_t gpu_nodes = 0;
    
    for (const auto& pair : nodes_) {
        total_cpu += pair.second.capabilities.cpu_cores;
        total_memory += pair.second.capabilities.memory_mb;
        if (pair.second.capabilities.has_gpu) {
            gpu_nodes++;
        }
    }
    
    capacity["total_cpu_cores"] = total_cpu;
    capacity["total_memory_mb"] = total_memory;
    capacity["gpu_nodes"] = gpu_nodes;
    capacity["total_nodes"] = nodes_.size();
    
    return capacity;
}

std::map<std::string, size_t> ClusterManager::getAvailableResources() const
{
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    
    std::map<std::string, size_t> available;
    
    size_t available_cpu = 0;
    size_t available_memory = 0;
    size_t idle_nodes = 0;
    
    for (const auto& pair : nodes_) {
        if (!pair.second.health.is_responsive) continue;
        
        // Calculate available resources based on usage
        double cpu_available = pair.second.capabilities.cpu_cores * 
                              (1.0 - pair.second.health.cpu_usage);
        double mem_available = pair.second.capabilities.memory_mb * 
                              (1.0 - pair.second.health.memory_usage);
        
        available_cpu += static_cast<size_t>(cpu_available);
        available_memory += static_cast<size_t>(mem_available);
        
        if (pair.second.health.active_tasks == 0) {
            idle_nodes++;
        }
    }
    
    available["available_cpu_cores"] = available_cpu;
    available["available_memory_mb"] = available_memory;
    available["idle_nodes"] = idle_nodes;
    
    return available;
}

int ClusterManager::performHealthCheck()
{
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    
    logger_.info("Performing health check on %zu nodes", nodes_.size());
    
    int responsive_count = 0;
    auto now = std::chrono::system_clock::now();
    
    for (auto& pair : nodes_) {
        // Check if node has sent heartbeat recently (within 60 seconds)
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(
            now - pair.second.health.last_heartbeat);
        
        if (duration.count() > 60) {
            pair.second.health.is_responsive = false;
            logger_.warn("Node %s is unresponsive (last heartbeat: %ld seconds ago)",
                        pair.first.c_str(), duration.count());
        } else {
            pair.second.health.is_responsive = true;
            responsive_count++;
        }
    }
    
    logger_.info("Health check complete: %d/%zu nodes responsive",
                responsive_count, nodes_.size());
    
    return responsive_count;
}

size_t ClusterManager::getNodeCount() const
{
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    return nodes_.size();
}

void ClusterManager::shutdown()
{
    logger_.info("Shutting down ClusterManager");
    
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    nodes_.clear();
}

// Private helper methods

void ClusterManager::storeNodeInAtomSpace(const NodeInfo& node)
{
    // Create node representation in AtomSpace
    Handle node_atom = atomspace_->add_node(CONCEPT_NODE, "ClusterNode:" + node.node_id);
    Handle cluster_atom = atomspace_->add_node(CONCEPT_NODE, "Cluster:" + cluster_id_);
    
    // Link node to cluster
    atomspace_->add_link(MEMBER_LINK, {node_atom, cluster_atom});
    
    // Store capabilities
    Handle cpu_pred = atomspace_->add_node(PREDICATE_NODE, "cpu_cores");
    Handle cpu_val = atomspace_->add_node(NUMBER_NODE, std::to_string(node.capabilities.cpu_cores));
    atomspace_->add_link(EVALUATION_LINK, {cpu_pred, node_atom, cpu_val});
}

void ClusterManager::removeNodeFromAtomSpace(const std::string& node_id)
{
    Handle node_atom = atomspace_->get_node(CONCEPT_NODE, "ClusterNode:" + node_id);
    if (node_atom != Handle::UNDEFINED) {
        atomspace_->remove_atom(node_atom, true);
    }
}

bool ClusterManager::pingNode(const std::string& node_id)
{
    // In real implementation, would send network ping
    // For now, just check if node exists
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    return nodes_.find(node_id) != nodes_.end();
}
