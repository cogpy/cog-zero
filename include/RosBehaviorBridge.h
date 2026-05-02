/*
 * opencog/agentzero/tools/RosBehaviorBridge.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * RosBehaviorBridge - ROS behaviour scripting bridge
 * Part of the AGENT-ZERO-GENESIS project - Phase 8: Tool Integration
 */

#ifndef _OPENCOG_AGENTZERO_ROSBEHAVIORBRIDGE_H
#define _OPENCOG_AGENTZERO_ROSBEHAVIORBRIDGE_H

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Handle.h>
#include <opencog/util/Logger.h>

#include <opencog/agentzero/tools/ToolWrapper.h>

namespace opencog {
namespace agentzero {
namespace tools {

/**
 * RosMessageType - Common ROS message types handled by the bridge
 */
enum class RosMessageType {
    STD_MSGS_STRING,      ///< std_msgs/String
    STD_MSGS_BOOL,        ///< std_msgs/Bool
    STD_MSGS_INT32,       ///< std_msgs/Int32
    STD_MSGS_FLOAT64,     ///< std_msgs/Float64
    GEOMETRY_MSGS_POSE,   ///< geometry_msgs/Pose
    GEOMETRY_MSGS_TWIST,  ///< geometry_msgs/Twist
    SENSOR_MSGS_IMAGE,    ///< sensor_msgs/Image
    CUSTOM                ///< User-defined message type
};

/**
 * RosConnectionState - Connection state to the ROS master
 */
enum class RosConnectionState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    ERROR
};

/**
 * RosMessage - Lightweight representation of a ROS message
 */
struct RosMessage {
    std::string topic;
    RosMessageType msg_type{RosMessageType::CUSTOM};
    std::string type_name;                        ///< ROS type string, e.g. "std_msgs/String"
    std::string payload;                          ///< Serialised (JSON) payload
    std::map<std::string, std::string> fields;    ///< Parsed field values
    double timestamp{0.0};                        ///< UNIX timestamp (seconds)

    /** Serialise to a JSON string */
    std::string toJSON() const;
};

/**
 * RosBehaviorBridge - Bridge between Agent-Zero and ROS behaviour scripting
 *
 * Provides an abstraction layer for Robot Operating System (ROS) integration,
 * enabling Agent-Zero to:
 *  - Publish messages to ROS topics
 *  - Subscribe to ROS topics and process message callbacks
 *  - Invoke ROS services synchronously
 *  - Execute ROS behaviour scripts (.py / .launch) as sandboxed processes
 *  - Run inline behaviour-tree descriptions (YAML/JSON)
 *
 * When ROS is not available the bridge operates in **simulation mode**:
 * publish/subscribe calls succeed without network I/O, allowing development
 * and unit testing without a live ROS installation.
 *
 * AtomSpace integration:
 *  - Incoming ROS messages are converted to ConceptNode atoms
 *  - Publish operations record provenance in AtomSpace
 *  - ROS connection state is reflected as TruthValues
 *
 * Usage example:
 * @code
 *   RosBehaviorBridge bridge("my_node", atomspace);
 *   bridge.connect();  // simulation mode if ROS is absent
 *
 *   // Subscribe
 *   bridge.subscribe("/chatter", RosMessageType::STD_MSGS_STRING,
 *       [](const RosMessage& m){ std::cout << m.payload << std::endl; });
 *
 *   // Publish
 *   bridge.publish("/cmd_vel", RosMessageType::GEOMETRY_MSGS_TWIST,
 *       R"({"linear":{"x":0.5},"angular":{"z":0.0}})");
 *
 *   // Run a behaviour script
 *   ToolResult r = bridge.executeBehaviorScript("/path/to/move.py");
 * @endcode
 */
class RosBehaviorBridge
{
public:
    /** Callback invoked when a subscribed message arrives */
    using MessageCallback = std::function<void(const RosMessage&)>;

    /**
     * Constructor
     * @param node_name  ROS node name for this bridge instance
     * @param atomspace  Optional AtomSpace for result integration
     */
    explicit RosBehaviorBridge(
        const std::string& node_name = "agentzero_bridge",
        AtomSpacePtr atomspace = nullptr);

    /** Destructor — unregisters all subscriptions */
    ~RosBehaviorBridge();

    // ----------- Connection management -----------

    /**
     * Connect to a ROS master
     *
     * Falls back to simulation mode if ROS is not available.
     *
     * @param master_uri URI of ROS master (default: env ROS_MASTER_URI or
     *                   "http://localhost:11311")
     * @return true on success or when entering simulation mode
     */
    bool connect(const std::string& master_uri = "");

    /** Disconnect from the ROS master */
    void disconnect();

    /** Check if connected (or in simulation mode) */
    bool isConnected() const;

    /** Get current connection state */
    RosConnectionState getConnectionState() const { return _connection_state; }

    /** Check if operating in simulation mode (no live ROS) */
    bool isSimulationMode() const { return _simulation_mode; }

    // ----------- Topic operations -----------

    /**
     * Publish a message to a ROS topic
     *
     * In simulation mode the call succeeds without network I/O.
     *
     * @param topic    Topic name (e.g. "/cmd_vel")
     * @param msg_type Message type
     * @param payload  Serialised (JSON) message payload
     * @return true on success
     */
    bool publish(const std::string& topic,
                 RosMessageType msg_type,
                 const std::string& payload);

    /**
     * Subscribe to a ROS topic
     *
     * In simulation mode the subscription is registered locally; callbacks
     * can be triggered manually via simulateMessage().
     *
     * @param topic    Topic name (e.g. "/chatter")
     * @param msg_type Expected message type
     * @param callback Invoked on each received message
     * @return Subscription ID used for unsubscribing
     */
    std::string subscribe(const std::string& topic,
                          RosMessageType msg_type,
                          MessageCallback callback);

    /**
     * Unsubscribe from a topic
     * @param subscription_id ID returned by subscribe()
     * @return true on success
     */
    bool unsubscribe(const std::string& subscription_id);

    /**
     * Deliver a simulated incoming message to all matching subscribers
     *
     * Only available in simulation mode. Useful for unit testing.
     *
     * @param msg Message to deliver
     * @return Number of subscribers notified
     */
    int simulateMessage(const RosMessage& msg);

    // ----------- Service operations -----------

    /**
     * Call a ROS service synchronously
     *
     * In simulation mode returns a stub response.
     *
     * @param service  Service name (e.g. "/move_base/make_plan")
     * @param request  Serialised (JSON) service request
     * @return Serialised (JSON) service response; empty string on failure
     */
    std::string callService(const std::string& service,
                            const std::string& request = "");

    // ----------- Behaviour script execution -----------

    /**
     * Execute a ROS behaviour script file
     *
     * The script is run as a sandboxed subprocess. stdout/stderr are captured
     * and returned in the ToolResult.
     *
     * @param script_path  Absolute path to the behaviour script
     * @param params       Key-value parameters passed as env vars or CLI flags
     * @return ToolResult containing execution outcome
     */
    ToolResult executeBehaviorScript(
        const std::string& script_path,
        const std::map<std::string, std::string>& params = {});

    /**
     * Execute an inline behaviour-tree description
     *
     * Validates and interprets a YAML/JSON behaviour-tree description.
     * In simulation mode the execution is synthesised without launching ROS.
     *
     * @param behavior_yaml Behaviour tree YAML or JSON description string
     * @return ToolResult containing execution outcome
     */
    ToolResult executeBehaviorTree(const std::string& behavior_yaml);

    // ----------- ToolWrapper integration -----------

    /**
     * Create a ToolWrapper that publishes to a ROS topic
     *
     * When the ToolWrapper is executed, the context parameters are
     * serialised to JSON and published to @p topic.
     *
     * @param tool_name  Name for the ToolWrapper
     * @param topic      ROS topic to publish to
     * @param msg_type   Message type (default: STD_MSGS_STRING)
     * @return Configured ToolWrapper
     */
    std::shared_ptr<ToolWrapper> createTopicPublisherTool(
        const std::string& tool_name,
        const std::string& topic,
        RosMessageType msg_type = RosMessageType::STD_MSGS_STRING);

    /**
     * Create a ToolWrapper that calls a ROS service
     *
     * When the ToolWrapper is executed, the context parameters are
     * serialised to JSON and sent as the service request.
     *
     * @param tool_name  Name for the ToolWrapper
     * @param service    ROS service to call
     * @return Configured ToolWrapper
     */
    std::shared_ptr<ToolWrapper> createServiceCallerTool(
        const std::string& tool_name,
        const std::string& service);

    // ----------- AtomSpace integration -----------

    /**
     * Convert a ROS message to AtomSpace atoms
     *
     * Creates a ConceptNode for the topic and EvaluationLinks for
     * each message field.
     *
     * @param msg       ROS message to convert
     * @param atomspace Target AtomSpace (uses bridge's if nullptr)
     * @return Handle to root ConceptNode; Handle::UNDEFINED on failure
     */
    Handle rosMessageToAtom(const RosMessage& msg,
                            AtomSpacePtr atomspace = nullptr) const;

    /**
     * Set the AtomSpace for provenance and result integration
     * @param atomspace AtomSpace instance
     */
    void setAtomSpace(AtomSpacePtr atomspace) { _atomspace = atomspace; }

    /** Get the current AtomSpace */
    AtomSpacePtr getAtomSpace() const { return _atomspace; }

    // ----------- Statistics -----------

    /** Number of messages published */
    int getPublishCount() const { return _publish_count; }

    /** Number of messages received via subscriptions */
    int getReceiveCount() const { return _receive_count; }

    /** Number of service calls made */
    int getServiceCallCount() const { return _service_call_count; }

    /** Number of behaviour scripts executed */
    int getScriptExecutionCount() const { return _script_execution_count; }

    /**
     * Return aggregated statistics as a JSON string
     */
    std::string getStatistics() const;

    // ----------- Static utilities -----------

    /** Convert RosMessageType to its ROS type string */
    static std::string messageTypeToString(RosMessageType type);

    /** Parse a ROS type string to RosMessageType */
    static RosMessageType messageTypeFromString(const std::string& type_str);

private:
    std::string _node_name;
    AtomSpacePtr _atomspace;

    RosConnectionState _connection_state{RosConnectionState::DISCONNECTED};
    bool _simulation_mode{true};
    std::string _master_uri;

    // subscription_id -> {topic, callback}
    std::map<std::string, std::pair<std::string, MessageCallback>> _subscriptions;

    // Statistics
    int _publish_count{0};
    int _receive_count{0};
    int _service_call_count{0};
    int _script_execution_count{0};
    int _sub_id_counter{0};

    // Internal helpers
    std::string generateSubscriptionId();
    bool validateTopic(const std::string& topic) const;
    ToolResult buildToolResult(bool success,
                               const std::string& output,
                               const std::string& error = "") const;
    std::string paramsToJson(const std::map<std::string, std::string>& params) const;
    std::string runScript(const std::string& interpreter,
                          const std::string& script_path,
                          const std::vector<std::string>& args,
                          double timeout_ms = 30000.0,
                          bool* success_out = nullptr);
};

} // namespace tools
} // namespace agentzero
} // namespace opencog

#endif // _OPENCOG_AGENTZERO_ROSBEHAVIORBRIDGE_H
