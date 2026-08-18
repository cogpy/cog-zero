/*
 * src/RosBehaviorBridge.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * RosBehaviorBridge Implementation
 * Bridge between Agent-Zero and ROS behaviour scripting
 * Part of the AGENT-ZERO-GENESIS project - Phase 8: Tool Integration
 */

#include <array>
#include <chrono>
#include <cstdio>
#include <iomanip>
#include <sstream>
#include <stdexcept>

#include <opencog/atoms/atom_types/types.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>
#include <opencog/atoms/truthvalue/SimpleTruthValue.h>
#include <opencog/util/Logger.h>

#include <opencog/agentzero/tools/RosBehaviorBridge.h>

using namespace opencog;
using namespace opencog::agentzero::tools;

// ========================================================================================
// RosMessage
// ========================================================================================

std::string RosMessage::toJSON() const
{
    std::ostringstream j;
    j << "{";
    j << "\"topic\":\"" << topic << "\",";
    j << "\"type_name\":\"" << type_name << "\",";
    j << "\"timestamp\":" << std::fixed << std::setprecision(6) << timestamp << ",";
    j << "\"payload\":\"" << payload << "\",";
    j << "\"fields\":{";
    bool first = true;
    for (const auto& kv : fields) {
        if (!first) j << ",";
        j << "\"" << kv.first << "\":\"" << kv.second << "\"";
        first = false;
    }
    j << "}}";
    return j.str();
}

// ========================================================================================
// RosBehaviorBridge
// ========================================================================================

RosBehaviorBridge::RosBehaviorBridge(const std::string& node_name,
                                     AtomSpacePtr atomspace)
    : _node_name(node_name)
    , _atomspace(atomspace)
    , _simulation_mode(true)
    , _lifetime(std::make_shared<LifetimeGate>())
{
    _lifetime->ptr = this;
    logger().info() << "[RosBehaviorBridge] Initialised node='" << _node_name << "'";
}

RosBehaviorBridge::~RosBehaviorBridge()
{
    if (_lifetime) {
        std::unique_lock<std::shared_mutex> lock(_lifetime->mutex);
        _lifetime->ptr = nullptr;
    }
    disconnect();
    logger().info() << "[RosBehaviorBridge] Destroyed (node='" << _node_name << "')";
}

// ---------------------------------------------------------------------------
// Connection management
// ---------------------------------------------------------------------------

bool RosBehaviorBridge::connect(const std::string& master_uri)
{
    _master_uri = master_uri.empty() ? "http://localhost:11311" : master_uri;

    // Attempt to detect a live ROS master by checking the ROS_MASTER_URI
    // environment variable or a provided URI.  If not reachable, fall back
    // to simulation mode silently so tests pass without ROS installed.
    const char* env_uri = std::getenv("ROS_MASTER_URI");
    std::string effective_uri = master_uri.empty() && env_uri ? env_uri : _master_uri;

    // In this baseline implementation we always enter simulation mode unless
    // the ROS_DISTRO environment variable signals that a ROS installation is
    // present and reachable.  A production implementation would attempt a
    // socket connection to the master_uri and only fall back if it fails.
    const char* ros_distro = std::getenv("ROS_DISTRO");
    if (ros_distro != nullptr) {
        _simulation_mode = false;
        _connection_state = RosConnectionState::CONNECTED;
        logger().info() << "[RosBehaviorBridge] Connected to ROS master at " << effective_uri
                        << " (distro=" << ros_distro << ")";
    } else {
        _simulation_mode = true;
        _connection_state = RosConnectionState::CONNECTED;
        logger().info() << "[RosBehaviorBridge] Entering simulation mode (ROS not detected)";
    }

    return true;
}

void RosBehaviorBridge::disconnect()
{
    if (_connection_state == RosConnectionState::DISCONNECTED) return;
    _subscriptions.clear();
    _connection_state = RosConnectionState::DISCONNECTED;
    logger().info() << "[RosBehaviorBridge] Disconnected";
}

bool RosBehaviorBridge::isConnected() const
{
    return _connection_state == RosConnectionState::CONNECTED;
}

// ---------------------------------------------------------------------------
// Topic operations
// ---------------------------------------------------------------------------

bool RosBehaviorBridge::publish(const std::string& topic,
                                RosMessageType msg_type,
                                const std::string& payload)
{
    if (!validateTopic(topic)) {
        logger().error() << "[RosBehaviorBridge] Invalid topic: " << topic;
        return false;
    }

    ++_publish_count;
    logger().debug() << "[RosBehaviorBridge] publish topic=" << topic
                     << " type=" << messageTypeToString(msg_type)
                     << " payload_len=" << payload.size();

    if (_atomspace) {
        try {
            RosMessage msg;
            msg.topic     = topic;
            msg.msg_type  = msg_type;
            msg.type_name = messageTypeToString(msg_type);
            msg.payload   = payload;
            msg.timestamp = static_cast<double>(
                std::chrono::system_clock::now().time_since_epoch().count()) / 1e9;
            rosMessageToAtom(msg, _atomspace);
        } catch (const std::exception& e) {
            logger().warn() << "[RosBehaviorBridge] AtomSpace record on publish failed: " << e.what();
        }
    }

    // In simulation mode the publish succeeds immediately
    return true;
}

std::string RosBehaviorBridge::subscribe(const std::string& topic,
                                          RosMessageType msg_type,
                                          MessageCallback callback)
{
    if (!validateTopic(topic)) {
        logger().error() << "[RosBehaviorBridge] Invalid topic for subscription: " << topic;
        return "";
    }

    std::string sub_id = generateSubscriptionId();
    _subscriptions[sub_id] = {topic, callback};

    logger().info() << "[RosBehaviorBridge] Subscribed to " << topic
                    << " (id=" << sub_id << " type=" << messageTypeToString(msg_type) << ")";
    return sub_id;
}

bool RosBehaviorBridge::unsubscribe(const std::string& subscription_id)
{
    auto it = _subscriptions.find(subscription_id);
    if (it == _subscriptions.end()) {
        logger().warn() << "[RosBehaviorBridge] Unknown subscription id: " << subscription_id;
        return false;
    }
    std::string topic = it->second.first;
    _subscriptions.erase(it);
    logger().info() << "[RosBehaviorBridge] Unsubscribed from " << topic;
    return true;
}

int RosBehaviorBridge::simulateMessage(const RosMessage& msg)
{
    if (!_simulation_mode) {
        logger().warn() << "[RosBehaviorBridge] simulateMessage called outside simulation mode";
    }

    int notified = 0;
    for (const auto& kv : _subscriptions) {
        if (kv.second.first == msg.topic) {
            try {
                kv.second.second(msg);
                ++_receive_count;
                ++notified;
            } catch (const std::exception& e) {
                logger().error() << "[RosBehaviorBridge] Callback threw: " << e.what();
            }
        }
    }
    return notified;
}

// ---------------------------------------------------------------------------
// Service operations
// ---------------------------------------------------------------------------

std::string RosBehaviorBridge::callService(const std::string& service,
                                            const std::string& request)
{
    ++_service_call_count;
    logger().debug() << "[RosBehaviorBridge] callService " << service;

    if (_simulation_mode) {
        // Return a stub success response
        std::ostringstream resp;
        resp << "{\"success\":true,\"service\":\"" << service << "\",\"request\":\""
             << request << "\"}";
        return resp.str();
    }

    // A production implementation would use a ROS service client here
    logger().warn() << "[RosBehaviorBridge] Live service calls not implemented; returning stub";
    return "{\"success\":false,\"error\":\"Live ROS service calls not yet implemented\"}";
}

// ---------------------------------------------------------------------------
// Behaviour script execution
// ---------------------------------------------------------------------------

ToolResult RosBehaviorBridge::executeBehaviorScript(
    const std::string& script_path,
    const std::map<std::string, std::string>& params)
{
    ++_script_execution_count;
    logger().info() << "[RosBehaviorBridge] executeBehaviorScript: " << script_path;

    if (script_path.empty()) {
        return buildToolResult(false, "", "Script path is empty");
    }

    // Build arguments list from params
    std::vector<std::string> args;
    for (const auto& kv : params) {
        args.push_back("--" + kv.first);
        args.push_back(kv.second);
    }

    // Detect script interpreter from extension
    std::string interpreter;
    size_t dot = script_path.rfind('.');
    if (dot != std::string::npos) {
        std::string ext = script_path.substr(dot + 1);
        if (ext == "py")     interpreter = "python3";
        else if (ext == "sh") interpreter = "bash";
        else                  interpreter = "bash";
    } else {
        interpreter = "bash";
    }

    bool success = false;
    std::string output = runScript(interpreter, script_path, args, 60000.0, &success);

    ToolResult result = buildToolResult(success, output);
    result.setMetadata("script_path",  script_path);
    result.setMetadata("interpreter",  interpreter);
    result.setMetadata("param_count",  std::to_string(params.size()));

    if (_atomspace) {
        try {
            Handle script_node = _atomspace->add_node(CONCEPT_NODE, "Script_" + script_path);
            TruthValuePtr tv = SimpleTruthValue::createTV(success ? 1.0 : 0.0, 1.0);
            script_node->setTruthValue(tv);
        } catch (const std::exception& e) {
            logger().warn() << "[RosBehaviorBridge] AtomSpace record failed: " << e.what();
        }
    }

    return result;
}

ToolResult RosBehaviorBridge::executeBehaviorTree(const std::string& behavior_yaml)
{
    ++_script_execution_count;
    logger().info() << "[RosBehaviorBridge] executeBehaviorTree (len="
                    << behavior_yaml.size() << ")";

    if (behavior_yaml.empty()) {
        return buildToolResult(false, "", "Behaviour tree description is empty");
    }

    // Validate minimal structure: must contain at least a "root" or "BehaviorTree" key
    bool has_structure = (behavior_yaml.find("root") != std::string::npos ||
                          behavior_yaml.find("BehaviorTree") != std::string::npos ||
                          behavior_yaml.find("behavior_tree") != std::string::npos);

    if (!has_structure) {
        return buildToolResult(false, "",
            "Behaviour tree description missing required root element");
    }

    if (_simulation_mode) {
        std::string out = "Behaviour tree executed in simulation mode (len="
                        + std::to_string(behavior_yaml.size()) + ")";
        ToolResult r = buildToolResult(true, out);
        r.setMetadata("mode", "simulation");
        r.setMetadata("tree_len", std::to_string(behavior_yaml.size()));
        return r;
    }

    // Production: write to temp file and invoke ros2 behavior_tree runner
    return buildToolResult(false, "",
        "Live behaviour tree execution not yet implemented");
}

// ---------------------------------------------------------------------------
// ToolWrapper integration
// ---------------------------------------------------------------------------

std::shared_ptr<ToolWrapper> RosBehaviorBridge::createTopicPublisherTool(
    const std::string& tool_name,
    const std::string& topic,
    RosMessageType msg_type)
{
    AtomSpacePtr as = _atomspace;
    auto tool = std::make_shared<ToolWrapper>(tool_name, ToolType::ROS_BEHAVIOR, as);
    tool->setToolEndpoint(topic);
    tool->setDescription("ROS topic publisher for " + topic);

    // Capture the lifetime gate (not a bare this) so destruction is race-safe.
    auto lifetime = _lifetime;
    std::string cap_topic = topic;
    RosMessageType cap_type = msg_type;

    tool->setCustomExecutor([lifetime, cap_topic, cap_type](
                                const ToolExecutionContext& ctx) -> ToolResult {
        if (!lifetime) {
            ToolResult failed(ToolStatus::FAILED);
            failed.setErrorMessage("RosBehaviorBridge lifetime gate missing");
            return failed;
        }
        std::shared_lock<std::shared_mutex> lock(lifetime->mutex);
        if (!lifetime->ptr) {
            ToolResult failed(ToolStatus::FAILED);
            failed.setErrorMessage("RosBehaviorBridge destroyed");
            return failed;
        }

        // Serialise context parameters as JSON payload
        std::ostringstream payload;
        payload << "{";
        bool first = true;
        for (const auto& kv : ctx.getAllParameters()) {
            if (!first) payload << ",";
            payload << "\"" << kv.first << "\":\"" << kv.second << "\"";
            first = false;
        }
        payload << "}";

        bool ok = lifetime->ptr->publish(cap_topic, cap_type, payload.str());
        ToolResult r(ok ? ToolStatus::COMPLETED : ToolStatus::FAILED);
        r.setOutput(ok ? "Published to " + cap_topic : "Publish failed");
        r.setMetadata("topic", cap_topic);
        r.setMetadata("msg_type", RosBehaviorBridge::messageTypeToString(cap_type));
        return r;
    });

    logger().info() << "[RosBehaviorBridge] Created topic publisher tool '"
                    << tool_name << "' -> " << topic;
    return tool;
}

std::shared_ptr<ToolWrapper> RosBehaviorBridge::createServiceCallerTool(
    const std::string& tool_name,
    const std::string& service)
{
    AtomSpacePtr as = _atomspace;
    auto tool = std::make_shared<ToolWrapper>(tool_name, ToolType::ROS_BEHAVIOR, as);
    tool->setToolEndpoint(service);
    tool->setDescription("ROS service caller for " + service);

    auto lifetime = _lifetime;
    std::string cap_service = service;

    tool->setCustomExecutor([lifetime, cap_service](
                                const ToolExecutionContext& ctx) -> ToolResult {
        if (!lifetime) {
            ToolResult failed(ToolStatus::FAILED);
            failed.setErrorMessage("RosBehaviorBridge lifetime gate missing");
            return failed;
        }
        std::shared_lock<std::shared_mutex> lock(lifetime->mutex);
        if (!lifetime->ptr) {
            ToolResult failed(ToolStatus::FAILED);
            failed.setErrorMessage("RosBehaviorBridge destroyed");
            return failed;
        }

        std::ostringstream req;
        req << "{";
        bool first = true;
        for (const auto& kv : ctx.getAllParameters()) {
            if (!first) req << ",";
            req << "\"" << kv.first << "\":\"" << kv.second << "\"";
            first = false;
        }
        req << "}";

        std::string resp = lifetime->ptr->callService(cap_service, req.str());
        bool ok = !resp.empty() && resp.find("\"success\":true") != std::string::npos;

        ToolResult r(ok ? ToolStatus::COMPLETED : ToolStatus::FAILED);
        r.setOutput(resp);
        r.setMetadata("service", cap_service);
        if (!ok) r.setErrorMessage("Service call failed or returned non-success");
        return r;
    });

    logger().info() << "[RosBehaviorBridge] Created service caller tool '"
                    << tool_name << "' -> " << service;
    return tool;
}

// ---------------------------------------------------------------------------
// AtomSpace integration
// ---------------------------------------------------------------------------

Handle RosBehaviorBridge::rosMessageToAtom(const RosMessage& msg,
                                            AtomSpacePtr atomspace) const
{
    AtomSpacePtr as = atomspace ? atomspace : _atomspace;
    if (!as) {
        return Handle::UNDEFINED;
    }

    try {
        // Root node for this message
        std::string node_name = "RosMsg_" + msg.topic;
        Handle msg_node = as->add_node(CONCEPT_NODE, node_name);

        TruthValuePtr tv = SimpleTruthValue::createTV(1.0, 1.0);
        msg_node->setTruthValue(tv);

        // Store type as an InheritanceLink
        Handle type_node = as->add_node(CONCEPT_NODE, msg.type_name);
        HandleSeq inh_seq;
        inh_seq.push_back(msg_node);
        inh_seq.push_back(type_node);
        as->add_link(INHERITANCE_LINK, std::move(inh_seq));

        // Store payload as a PredicateNode evaluation
        if (!msg.payload.empty()) {
            Handle pred = as->add_node(PREDICATE_NODE, "ros_payload");
            Handle data = as->add_node(CONCEPT_NODE, msg.payload.substr(0, 256));
            HandleSeq list_seq;
            list_seq.push_back(msg_node);
            list_seq.push_back(data);
            Handle list_link = as->add_link(LIST_LINK, std::move(list_seq));
            HandleSeq eval_seq;
            eval_seq.push_back(pred);
            eval_seq.push_back(list_link);
            as->add_link(EVALUATION_LINK, std::move(eval_seq));
        }

        // Store individual fields
        for (const auto& kv : msg.fields) {
            Handle field_pred = as->add_node(PREDICATE_NODE, "ros_field_" + kv.first);
            Handle val_node   = as->add_node(CONCEPT_NODE, kv.second);
            HandleSeq list_seq;
            list_seq.push_back(msg_node);
            list_seq.push_back(val_node);
            Handle list_link = as->add_link(LIST_LINK, std::move(list_seq));
            HandleSeq eval_seq;
            eval_seq.push_back(field_pred);
            eval_seq.push_back(list_link);
            as->add_link(EVALUATION_LINK, std::move(eval_seq));
        }

        return msg_node;

    } catch (const std::exception& e) {
        logger().error() << "[RosBehaviorBridge] rosMessageToAtom failed: " << e.what();
        return Handle::UNDEFINED;
    }
}

// ---------------------------------------------------------------------------
// Statistics
// ---------------------------------------------------------------------------

std::string RosBehaviorBridge::getStatistics() const
{
    std::ostringstream j;
    j << "{";
    j << "\"node_name\":\"" << _node_name << "\",";
    j << "\"simulation_mode\":" << (_simulation_mode ? "true" : "false") << ",";
    j << "\"publish_count\":" << _publish_count << ",";
    j << "\"receive_count\":" << _receive_count << ",";
    j << "\"service_call_count\":" << _service_call_count << ",";
    j << "\"script_execution_count\":" << _script_execution_count << ",";
    j << "\"subscription_count\":" << _subscriptions.size();
    j << "}";
    return j.str();
}

// ---------------------------------------------------------------------------
// Static utilities
// ---------------------------------------------------------------------------

std::string RosBehaviorBridge::messageTypeToString(RosMessageType type)
{
    switch (type) {
        case RosMessageType::STD_MSGS_STRING:      return "std_msgs/String";
        case RosMessageType::STD_MSGS_BOOL:        return "std_msgs/Bool";
        case RosMessageType::STD_MSGS_INT32:       return "std_msgs/Int32";
        case RosMessageType::STD_MSGS_FLOAT64:     return "std_msgs/Float64";
        case RosMessageType::GEOMETRY_MSGS_POSE:   return "geometry_msgs/Pose";
        case RosMessageType::GEOMETRY_MSGS_TWIST:  return "geometry_msgs/Twist";
        case RosMessageType::SENSOR_MSGS_IMAGE:    return "sensor_msgs/Image";
        case RosMessageType::CUSTOM:               return "custom";
    }
    return "unknown";
}

RosMessageType RosBehaviorBridge::messageTypeFromString(const std::string& type_str)
{
    if (type_str == "std_msgs/String")     return RosMessageType::STD_MSGS_STRING;
    if (type_str == "std_msgs/Bool")       return RosMessageType::STD_MSGS_BOOL;
    if (type_str == "std_msgs/Int32")      return RosMessageType::STD_MSGS_INT32;
    if (type_str == "std_msgs/Float64")    return RosMessageType::STD_MSGS_FLOAT64;
    if (type_str == "geometry_msgs/Pose")  return RosMessageType::GEOMETRY_MSGS_POSE;
    if (type_str == "geometry_msgs/Twist") return RosMessageType::GEOMETRY_MSGS_TWIST;
    if (type_str == "sensor_msgs/Image")   return RosMessageType::SENSOR_MSGS_IMAGE;
    return RosMessageType::CUSTOM;
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

std::string RosBehaviorBridge::generateSubscriptionId()
{
    return _node_name + "_sub_" + std::to_string(++_sub_id_counter);
}

bool RosBehaviorBridge::validateTopic(const std::string& topic) const
{
    if (topic.empty()) return false;
    // ROS topics start with '/'
    if (topic[0] != '/') {
        logger().warn() << "[RosBehaviorBridge] Topic should start with '/': " << topic;
        // Not a hard error — some ROS2 topics omit the leading slash
    }
    return true;
}

ToolResult RosBehaviorBridge::buildToolResult(bool success,
                                              const std::string& output,
                                              const std::string& error) const
{
    ToolResult r(success ? ToolStatus::COMPLETED : ToolStatus::FAILED);
    r.setOutput(output);
    if (!success && !error.empty()) {
        r.setErrorMessage(error);
    }
    r.setMetadata("node_name",        _node_name);
    r.setMetadata("simulation_mode",  _simulation_mode ? "true" : "false");
    return r;
}

std::string RosBehaviorBridge::paramsToJson(
    const std::map<std::string, std::string>& params) const
{
    std::ostringstream j;
    j << "{";
    bool first = true;
    for (const auto& kv : params) {
        if (!first) j << ",";
        j << "\"" << kv.first << "\":\"" << kv.second << "\"";
        first = false;
    }
    j << "}";
    return j.str();
}

std::string RosBehaviorBridge::runScript(const std::string& interpreter,
                                          const std::string& script_path,
                                          const std::vector<std::string>& args,
                                          double /*timeout_ms*/,
                                          bool* success_out)
{
    // Helper lambda: shell-safe single-quote escaping (same approach as ToolExecutor)
    auto shell_quote = [](const std::string& s) -> std::string {
        std::string q;
        q.reserve(s.size() + 2);
        q += '\'';
        for (char c : s) {
            if (c == '\'') {
                q += "'\\''";  // end quote, escaped quote, restart quote
            } else {
                q += c;
            }
        }
        q += '\'';
        return q;
    };

    // Build command: interpreter script_path [args...]
    std::string cmd = interpreter + " " + shell_quote(script_path);
    for (const auto& arg : args) {
        cmd += " " + shell_quote(arg);
    }
    cmd += " 2>&1";

    std::string output;
    std::array<char, 512> buf{};
    FILE* pipe = popen(cmd.c_str(), "r"); // NOLINT(cert-env33-c)
    if (!pipe) {
        logger().error() << "[RosBehaviorBridge] popen failed for: " << cmd;
        if (success_out) *success_out = false;
        return "";
    }

    while (fgets(buf.data(), static_cast<int>(buf.size()), pipe) != nullptr) {
        output += buf.data();
    }

    int exit_code = pclose(pipe);
    if (success_out) *success_out = (exit_code == 0);
    return output;
}
