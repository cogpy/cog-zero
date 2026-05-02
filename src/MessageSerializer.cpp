/*
 * opencog/agentzero/communication/MessageSerializer.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Message Serialization Component Implementation
 * Part of the AGENT-ZERO-GENESIS project - AZ-COMM-001
 */

#include <sstream>
#include <iomanip>
#include <algorithm>

#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>
#include <opencog/util/Logger.h>

#include "MessageSerializer.h"

using namespace opencog;
using namespace opencog::agentzero::communication;

MessageSerializer::MessageSerializer(AtomSpacePtr atomspace,
                                   bool enable_compression,
                                   bool enable_validation)
    : _atomspace(atomspace),
      _enable_compression(enable_compression),
      _enable_validation(enable_validation),
      _schema_version("1.0"),
      _messages_serialized(0),
      _messages_deserialized(0),
      _compression_ratio_sum(0)
{
    logger().debug("MessageSerializer initialized with compression=%s, validation=%s",
                   enable_compression ? "enabled" : "disabled",
                   enable_validation ? "enabled" : "disabled");
}

MessageSerializer::~MessageSerializer() {
    // Cleanup if needed
}

std::string MessageSerializer::serialize(const CommMessagePtr& message) {
    if (!message) {
        logger().warn("Cannot serialize null message");
        return "";
    }
    
    if (_enable_validation && !validateMessageStructure(message)) {
        logger().warn("Message validation failed for: %s", message->message_id.c_str());
        return "";
    }
    
    try {
        std::string json_data = serializeToJSON(message);
        size_t original_size = json_data.size();
        
        if (_enable_compression && original_size > 1024) {  // Only compress larger messages
            std::string compressed_data = compressData(json_data);
            updateSerializationStats(true, original_size, compressed_data.size());
            return compressed_data;
        } else {
            updateSerializationStats(true, original_size, original_size);
            return json_data;
        }
        
    } catch (const std::exception& e) {
        logger().error("Serialization failed for message %s: %s", 
                      message->message_id.c_str(), e.what());
        return "";
    }
}

CommMessagePtr MessageSerializer::deserialize(const std::string& data) {
    if (data.empty()) {
        return nullptr;
    }
    
    try {
        std::string json_data = data;
        size_t processed_size = data.size();
        
        // Check if data might be compressed (simple heuristic)
        if (_enable_compression && data[0] != '{') {
            json_data = decompressData(data);
            if (json_data.empty()) {
                logger().warn("Decompression failed");
                return nullptr;
            }
        }
        
        CommMessagePtr message = deserializeFromJSON(json_data);
        if (message) {
            updateSerializationStats(false, json_data.size(), processed_size);
            
            if (_enable_validation && !validateMessageStructure(message)) {
                logger().warn("Deserialized message validation failed: %s", 
                             message->message_id.c_str());
                return nullptr;
            }
        }
        
        return message;
        
    } catch (const std::exception& e) {
        logger().error("Deserialization failed: %s", e.what());
        return nullptr;
    }
}

std::vector<uint8_t> MessageSerializer::serializeBinary(const CommMessagePtr& message) {
    // Simple binary serialization (placeholder implementation)
    // In a full implementation, this would use a more efficient binary format
    
    std::string json_data = serialize(message);
    if (json_data.empty()) {
        return {};
    }
    
    std::vector<uint8_t> binary_data(json_data.begin(), json_data.end());
    return binary_data;
}

CommMessagePtr MessageSerializer::deserializeBinary(const std::vector<uint8_t>& data) {
    if (data.empty()) {
        return nullptr;
    }
    
    std::string json_data(data.begin(), data.end());
    return deserialize(json_data);
}

std::string MessageSerializer::serializeAtom(const Handle& handle) {
    if (!_atomspace || handle == Handle::UNDEFINED) {
        return "";
    }
    
    try {
        // Simple atom serialization (placeholder implementation)
        // In a full implementation, this would use AtomSpace's native serialization
        
        AtomPtr atom = handle;
        if (!atom) {
            return "";
        }
        
        std::stringstream ss;
        ss << "{"
           << "\"type\":\"" << atom->get_type() << "\","
           << "\"name\":\"" << atom->to_string() << "\""
           << "}";
        
        return ss.str();
        
    } catch (const std::exception& e) {
        logger().error("Atom serialization failed: %s", e.what());
        return "";
    }
}

Handle MessageSerializer::deserializeAtom(const std::string& atom_data) {
    if (!_atomspace || atom_data.empty()) {
        return Handle::UNDEFINED;
    }
    
    try {
        // Simple atom deserialization (placeholder implementation)
        // In a full implementation, this would parse the serialized atom format
        
        // For now, just create a concept node with the data as name
        return _atomspace->add_node(CONCEPT_NODE, "DeserializedAtom_" + atom_data.substr(0, 20));
        
    } catch (const std::exception& e) {
        logger().error("Atom deserialization failed: %s", e.what());
        return Handle::UNDEFINED;
    }
}

std::string MessageSerializer::serializeAtoms(const std::vector<Handle>& handles) {
    std::stringstream ss;
    ss << "[";
    
    for (size_t i = 0; i < handles.size(); ++i) {
        if (i > 0) ss << ",";
        ss << serializeAtom(handles[i]);
    }
    
    ss << "]";
    return ss.str();
}

std::vector<Handle> MessageSerializer::deserializeAtoms(const std::string& atoms_data) {
    std::vector<Handle> handles;
    
    // Simple parsing (placeholder implementation)
    // In a full implementation, this would properly parse the JSON array
    
    if (atoms_data.empty() || atoms_data == "[]") {
        return handles;
    }
    
    // For now, just create one atom from the data
    Handle atom = deserializeAtom(atoms_data);
    if (atom != Handle::UNDEFINED) {
        handles.push_back(atom);
    }
    
    return handles;
}

size_t MessageSerializer::estimateSerializedSize(const CommMessagePtr& message) {
    if (!message) {
        return 0;
    }
    
    // Estimate based on message content
    size_t base_size = 200; // JSON overhead
    size_t content_size = message->content.size();
    size_t metadata_size = message->metadata.size() * 50; // Rough estimate
    
    return base_size + content_size + metadata_size;
}

bool MessageSerializer::isSerializable(const CommMessagePtr& message) {
    return message != nullptr && !message->message_id.empty();
}

double MessageSerializer::getCompressionRatio() const {
    // This would return the ratio for the last operation
    // Placeholder implementation
    return _enable_compression ? 0.7 : 1.0;
}

double MessageSerializer::getAverageCompressionRatio() const {
    std::lock_guard<std::mutex> lock(_stats_mutex);
    
    if (_messages_serialized == 0) {
        return 1.0;
    }
    
    return static_cast<double>(_compression_ratio_sum) / static_cast<double>(_messages_serialized);
}

std::string MessageSerializer::getStats() const {
    std::lock_guard<std::mutex> lock(_stats_mutex);
    
    std::stringstream ss;
    ss << "{"
       << "\"messages_serialized\":" << _messages_serialized << ","
       << "\"messages_deserialized\":" << _messages_deserialized << ","
       << "\"compression_enabled\":" << (_enable_compression ? "true" : "false") << ","
       << "\"validation_enabled\":" << (_enable_validation ? "true" : "false") << ","
       << "\"schema_version\":\"" << _schema_version << "\","
       << "\"average_compression_ratio\":" << getAverageCompressionRatio()
       << "}";
    
    return ss.str();
}

void MessageSerializer::resetStats() {
    std::lock_guard<std::mutex> lock(_stats_mutex);
    _messages_serialized = 0;
    _messages_deserialized = 0;
    _compression_ratio_sum = 0;
}

size_t MessageSerializer::getSerializedCount() const {
    std::lock_guard<std::mutex> lock(_stats_mutex);
    return _messages_serialized;
}

size_t MessageSerializer::getDeserializedCount() const {
    std::lock_guard<std::mutex> lock(_stats_mutex);
    return _messages_deserialized;
}

bool MessageSerializer::validateFormat(const std::string& data) {
    if (data.empty()) {
        return false;
    }
    
    // Simple format validation - check if it's valid JSON
    return (data[0] == '{' && data[data.size()-1] == '}') ||
           (data[0] == '[' && data[data.size()-1] == ']');
}

std::string MessageSerializer::convertFormat(const std::string& data,
                                           const std::string& from_format,
                                           const std::string& to_format) {
    // Placeholder implementation for format conversion
    logger().debug("Format conversion: %s -> %s", from_format.c_str(), to_format.c_str());
    
    if (from_format == to_format) {
        return data;
    }
    
    // Simple pass-through for now
    return data;
}

std::vector<std::string> MessageSerializer::getSupportedFormats() const {
    return {"json", "binary", "atom"};
}

bool MessageSerializer::registerSchema(const std::string& schema_name, 
                                      const std::string& schema_definition) {
    // Placeholder for schema registration
    logger().debug("Registering schema: %s", schema_name.c_str());
    return true;
}

bool MessageSerializer::validateAgainstSchema(const CommMessagePtr& message, 
                                             const std::string& schema_name) {
    // Placeholder for schema validation
    logger().debug("Validating message against schema: %s", schema_name.c_str());
    return validateMessageStructure(message);
}

std::vector<std::string> MessageSerializer::getRegisteredSchemas() const {
    return {"default", "agent-zero-v1"};
}

// Private helper methods

std::string MessageSerializer::serializeToJSON(const CommMessagePtr& message) {
    if (!message) {
        return "";
    }
    
    std::stringstream ss;
    ss << "{"
       << "\"message_id\":\"" << message->message_id << "\","
       << "\"sender\":\"" << message->sender.toString() << "\","
       << "\"recipient\":\"" << message->recipient.toString() << "\","
       << "\"type\":\"" << utils::messageTypeToString(message->type) << "\","
       << "\"priority\":\"" << utils::priorityToString(message->priority) << "\","
       << "\"protocol\":\"" << utils::protocolTypeToString(message->protocol) << "\","
       << "\"content\":\"" << message->content << "\","
       << "\"timestamp\":\"" << formatTimestamp(message->timestamp) << "\","
       << "\"expires\":\"" << formatTimestamp(message->expires) << "\","
       << "\"schema_version\":\"" << _schema_version << "\"";
    
    // Add metadata
    if (!message->metadata.empty()) {
        ss << ",\"metadata\":{";
        bool first = true;
        for (const auto& pair : message->metadata) {
            if (!first) ss << ",";
            first = false;
            ss << "\"" << pair.first << "\":\"" << pair.second << "\"";
        }
        ss << "}";
    }
    
    // Add atom content if present
    if (message->atom_content != Handle::UNDEFINED) {
        std::string atom_data = serializeAtom(message->atom_content);
        if (!atom_data.empty()) {
            ss << ",\"atom_content\":" << atom_data;
        }
    }
    
    ss << "}";
    return ss.str();
}

CommMessagePtr MessageSerializer::deserializeFromJSON(const std::string& json_data) {
    if (json_data.empty()) {
        return nullptr;
    }
    
    // Simple JSON parsing (placeholder implementation)
    // In a full implementation, this would use a proper JSON parser
    
    auto message = std::make_shared<CommMessage>();
    
    // Extract basic fields (very basic parsing for demonstration)
    size_t id_pos = json_data.find("\"message_id\":\"");
    if (id_pos != std::string::npos) {
        id_pos += 14;  // Length of "\"message_id\":\""
        size_t end_pos = json_data.find("\"", id_pos);
        if (end_pos != std::string::npos) {
            message->message_id = json_data.substr(id_pos, end_pos - id_pos);
        }
    }
    
    size_t sender_pos = json_data.find("\"sender\":\"");
    if (sender_pos != std::string::npos) {
        sender_pos += 10;  // Length of "\"sender\":\""
        size_t end_pos = json_data.find("\"", sender_pos);
        if (end_pos != std::string::npos) {
            message->sender = utils::parseAgentId(json_data.substr(sender_pos, end_pos - sender_pos));
        }
    }
    
    size_t recipient_pos = json_data.find("\"recipient\":\"");
    if (recipient_pos != std::string::npos) {
        recipient_pos += 13;  // Length of "\"recipient\":\""
        size_t end_pos = json_data.find("\"", recipient_pos);
        if (end_pos != std::string::npos) {
            message->recipient = utils::parseAgentId(json_data.substr(recipient_pos, end_pos - recipient_pos));
        }
    }
    
    size_t content_pos = json_data.find("\"content\":\"");
    if (content_pos != std::string::npos) {
        content_pos += 11;  // Length of "\"content\":\""
        size_t end_pos = json_data.find("\"", content_pos);
        if (end_pos != std::string::npos) {
            message->content = json_data.substr(content_pos, end_pos - content_pos);
        }
    }
    
    // Set default values for fields not parsed
    message->type = MessageType::INFO;
    message->priority = MessagePriority::NORMAL;
    message->protocol = ProtocolType::LOCAL;
    message->timestamp = std::chrono::system_clock::now();
    message->expires = message->timestamp + std::chrono::minutes(30);
    
    return message;
}

std::string MessageSerializer::compressData(const std::string& data) {
    // Placeholder for compression implementation
    // In a full implementation, this would use a compression library like zlib
    
    logger().debug("Compressing data of size: %zu", data.size());
    
    // Simple "compression" - just return original data for now
    return data;
}

std::string MessageSerializer::decompressData(const std::string& compressed_data) {
    // Placeholder for decompression implementation
    // In a full implementation, this would decompress the data
    
    logger().debug("Decompressing data of size: %zu", compressed_data.size());
    
    // Simple "decompression" - just return original data for now
    return compressed_data;
}

bool MessageSerializer::validateMessageStructure(const CommMessagePtr& message) {
    if (!message) {
        return false;
    }
    
    // Basic validation checks
    if (message->message_id.empty()) {
        logger().warn("Message validation failed: empty message_id");
        return false;
    }
    
    if (message->sender.name.empty()) {
        logger().warn("Message validation failed: empty sender name");
        return false;
    }
    
    if (message->recipient.name.empty()) {
        logger().warn("Message validation failed: empty recipient name");
        return false;
    }
    
    return true;
}

std::string MessageSerializer::formatTimestamp(const std::chrono::system_clock::time_point& time) {
    auto time_t = std::chrono::system_clock::to_time_t(time);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

std::chrono::system_clock::time_point MessageSerializer::parseTimestamp(const std::string& timestamp) {
    // Placeholder for timestamp parsing
    // In a full implementation, this would properly parse ISO 8601 timestamps
    
    return std::chrono::system_clock::now();
}

void MessageSerializer::updateSerializationStats(bool serializing, size_t original_size, size_t processed_size) {
    std::lock_guard<std::mutex> lock(_stats_mutex);
    
    if (serializing) {
        _messages_serialized++;
        if (original_size > 0) {
            double ratio = static_cast<double>(processed_size) / static_cast<double>(original_size);
            _compression_ratio_sum += static_cast<size_t>(ratio * 100); // Store as percentage
        }
    } else {
        _messages_deserialized++;
    }
}