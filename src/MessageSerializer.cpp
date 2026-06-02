/*
 * opencog/agentzero/communication/MessageSerializer.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Message Serialization Component Implementation
 * Part of the AGENT-ZERO-GENESIS project - AZ-COMM-001
 *
 * Phase 14 Feature 3.1: Enhanced JSON parsing and basic compression
 */

#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>
#include <ctime>

#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>
#include <opencog/util/Logger.h>

#include "MessageSerializer.h"

using namespace opencog;
using namespace opencog::agentzero::communication;

// =============================================================================
// JSON Parsing Utilities (zero-dependency recursive descent parser)
// =============================================================================

namespace {

// Skip whitespace characters
inline void skipWhitespace(const std::string& json, size_t& pos) {
    while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) {
        ++pos;
    }
}

// Parse a JSON string value (handles basic escape sequences)
std::string parseJsonString(const std::string& json, size_t& pos) {
    if (pos >= json.size() || json[pos] != '"') {
        return "";
    }
    ++pos;  // Skip opening quote
    
    std::string result;
    result.reserve(64);  // Pre-allocate for typical string sizes
    
    while (pos < json.size() && json[pos] != '"') {
        if (json[pos] == '\\' && pos + 1 < json.size()) {
            ++pos;
            switch (json[pos]) {
                case 'n': result += '\n'; break;
                case 't': result += '\t'; break;
                case 'r': result += '\r'; break;
                case '"': result += '"'; break;
                case '\\': result += '\\'; break;
                case '/': result += '/'; break;
                default: result += json[pos]; break;
            }
        } else {
            result += json[pos];
        }
        ++pos;
    }
    
    if (pos < json.size()) {
        ++pos;  // Skip closing quote
    }
    
    return result;
}

// Parse a JSON value (string, number, boolean, null) as string
std::string parseJsonValue(const std::string& json, size_t& pos) {
    skipWhitespace(json, pos);
    
    if (pos >= json.size()) {
        return "";
    }
    
    // String value
    if (json[pos] == '"') {
        return parseJsonString(json, pos);
    }
    
    // Number, boolean, or null
    size_t start = pos;
    
    // Handle negative numbers
    if (json[pos] == '-') {
        ++pos;
    }
    
    // Consume alphanumeric characters and decimal points
    while (pos < json.size() && 
           (std::isalnum(static_cast<unsigned char>(json[pos])) || 
            json[pos] == '.' || json[pos] == '+' || json[pos] == '-' || json[pos] == 'e' || json[pos] == 'E')) {
        ++pos;
    }
    
    return json.substr(start, pos - start);
}

// Parse a flat JSON object into a map (does not handle nested objects)
std::map<std::string, std::string> parseJsonObject(const std::string& json, size_t& pos) {
    std::map<std::string, std::string> result;
    
    skipWhitespace(json, pos);
    
    if (pos >= json.size() || json[pos] != '{') {
        return result;
    }
    ++pos;  // Skip opening brace
    
    while (pos < json.size()) {
        skipWhitespace(json, pos);
        
        // End of object
        if (json[pos] == '}') {
            ++pos;
            break;
        }
        
        // Skip comma between entries
        if (json[pos] == ',') {
            ++pos;
            continue;
        }
        
        // Parse key
        std::string key = parseJsonString(json, pos);
        if (key.empty()) {
            break;  // Invalid format
        }
        
        // Skip colon
        skipWhitespace(json, pos);
        if (pos < json.size() && json[pos] == ':') {
            ++pos;
        }
        skipWhitespace(json, pos);
        
        // Handle nested objects/arrays by skipping them
        if (pos < json.size() && (json[pos] == '{' || json[pos] == '[')) {
            char openChar = json[pos];
            char closeChar = (openChar == '{') ? '}' : ']';
            int depth = 1;
            size_t start = pos;
            ++pos;
            
            while (pos < json.size() && depth > 0) {
                if (json[pos] == '"') {
                    // Skip strings to avoid counting braces inside strings
                    ++pos;
                    while (pos < json.size() && json[pos] != '"') {
                        if (json[pos] == '\\' && pos + 1 < json.size()) {
                            ++pos;
                        }
                        ++pos;
                    }
                    if (pos < json.size()) ++pos;
                } else {
                    if (json[pos] == openChar) ++depth;
                    else if (json[pos] == closeChar) --depth;
                    ++pos;
                }
            }
            
            result[key] = json.substr(start, pos - start);
        } else {
            // Parse primitive value
            result[key] = parseJsonValue(json, pos);
        }
    }
    
    return result;
}

// Escape a string for JSON output
std::string escapeJsonString(const std::string& str) {
    std::string result;
    result.reserve(str.size() + 16);
    
    for (char c : str) {
        switch (c) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    // Control character - encode as \uXXXX
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                    result += buf;
                } else {
                    result += c;
                }
                break;
        }
    }
    
    return result;
}

// Simple Run-Length Encoding compression
// Format: for runs of 4+ identical bytes, encode as <marker><count><byte>
// Marker byte is 0xFF (chosen because it's rare in text/JSON)
std::string rleCompress(const std::string& data) {
    if (data.empty()) return data;
    
    const uint8_t RLE_MARKER = 0xFF;
    const size_t MIN_RUN_LENGTH = 4;
    const size_t MAX_RUN_LENGTH = 255;
    
    std::string result;
    result.reserve(data.size());
    
    size_t i = 0;
    while (i < data.size()) {
        // Count consecutive identical bytes
        size_t runLength = 1;
        while (i + runLength < data.size() && 
               runLength < MAX_RUN_LENGTH &&
               data[i + runLength] == data[i]) {
            ++runLength;
        }
        
        if (runLength >= MIN_RUN_LENGTH || 
            static_cast<uint8_t>(data[i]) == RLE_MARKER) {
            // Encode as RLE
            result += static_cast<char>(RLE_MARKER);
            result += static_cast<char>(runLength);
            result += data[i];
            i += runLength;
        } else {
            // Copy literal bytes
            result += data[i];
            ++i;
        }
    }
    
    return result;
}

// RLE decompression
std::string rleDecompress(const std::string& data) {
    if (data.empty()) return data;
    
    const uint8_t RLE_MARKER = 0xFF;
    
    std::string result;
    result.reserve(data.size() * 2);  // Estimate expansion
    
    size_t i = 0;
    while (i < data.size()) {
        if (static_cast<uint8_t>(data[i]) == RLE_MARKER && i + 2 < data.size()) {
            // Decode RLE sequence
            size_t count = static_cast<uint8_t>(data[i + 1]);
            char byte = data[i + 2];
            result.append(count, byte);
            i += 3;
        } else {
            result += data[i];
            ++i;
        }
    }
    
    return result;
}

}  // anonymous namespace

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
    
    if (atoms_data.empty() || atoms_data == "[]") {
        return handles;
    }
    
    // Parse JSON array of atoms
    size_t pos = 0;
    skipWhitespace(atoms_data, pos);
    
    if (pos >= atoms_data.size() || atoms_data[pos] != '[') {
        // Not an array - try to parse as single atom
        Handle atom = deserializeAtom(atoms_data);
        if (atom != Handle::UNDEFINED) {
            handles.push_back(atom);
        }
        return handles;
    }
    
    ++pos;  // Skip opening bracket
    
    while (pos < atoms_data.size()) {
        skipWhitespace(atoms_data, pos);
        
        // End of array
        if (atoms_data[pos] == ']') {
            break;
        }
        
        // Skip comma
        if (atoms_data[pos] == ',') {
            ++pos;
            continue;
        }
        
        // Find the extent of this atom object
        if (atoms_data[pos] == '{') {
            size_t start = pos;
            int depth = 1;
            ++pos;
            
            while (pos < atoms_data.size() && depth > 0) {
                if (atoms_data[pos] == '"') {
                    // Skip strings
                    ++pos;
                    while (pos < atoms_data.size() && atoms_data[pos] != '"') {
                        if (atoms_data[pos] == '\\' && pos + 1 < atoms_data.size()) {
                            ++pos;
                        }
                        ++pos;
                    }
                    if (pos < atoms_data.size()) ++pos;
                } else {
                    if (atoms_data[pos] == '{') ++depth;
                    else if (atoms_data[pos] == '}') --depth;
                    ++pos;
                }
            }
            
            std::string atomStr = atoms_data.substr(start, pos - start);
            Handle atom = deserializeAtom(atomStr);
            if (atom != Handle::UNDEFINED) {
                handles.push_back(atom);
            }
        } else {
            // Skip unexpected content
            ++pos;
        }
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
       << "\"message_id\":\"" << escapeJsonString(message->message_id) << "\","
       << "\"sender\":\"" << escapeJsonString(message->sender.toString()) << "\","
       << "\"recipient\":\"" << escapeJsonString(message->recipient.toString()) << "\","
       << "\"type\":\"" << utils::messageTypeToString(message->type) << "\","
       << "\"priority\":\"" << utils::priorityToString(message->priority) << "\","
       << "\"protocol\":\"" << utils::protocolTypeToString(message->protocol) << "\","
       << "\"content\":\"" << escapeJsonString(message->content) << "\","
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
            ss << "\"" << escapeJsonString(pair.first) << "\":\"" 
               << escapeJsonString(pair.second) << "\"";
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
    
    // Use proper JSON parser
    size_t pos = 0;
    std::map<std::string, std::string> fields = parseJsonObject(json_data, pos);
    
    if (fields.empty()) {
        logger().warn("Failed to parse JSON message data");
        return nullptr;
    }
    
    auto message = std::make_shared<CommMessage>();
    
    // Extract fields using parsed map
    auto it = fields.find("message_id");
    if (it != fields.end()) {
        message->message_id = it->second;
    }
    
    it = fields.find("sender");
    if (it != fields.end()) {
        message->sender = utils::parseAgentId(it->second);
    }
    
    it = fields.find("recipient");
    if (it != fields.end()) {
        message->recipient = utils::parseAgentId(it->second);
    }
    
    it = fields.find("content");
    if (it != fields.end()) {
        message->content = it->second;
    }
    
    it = fields.find("type");
    if (it != fields.end()) {
        message->type = utils::stringToMessageType(it->second);
    } else {
        message->type = MessageType::INFO;
    }
    
    it = fields.find("priority");
    if (it != fields.end()) {
        message->priority = utils::stringToPriority(it->second);
    } else {
        message->priority = MessagePriority::NORMAL;
    }
    
    it = fields.find("protocol");
    if (it != fields.end()) {
        message->protocol = utils::stringToProtocolType(it->second);
    } else {
        message->protocol = ProtocolType::LOCAL;
    }
    
    // Parse timestamp if present
    it = fields.find("timestamp");
    if (it != fields.end() && !it->second.empty()) {
        message->timestamp = parseTimestamp(it->second);
    } else {
        message->timestamp = std::chrono::system_clock::now();
    }
    
    // Parse expiry
    it = fields.find("expires");
    if (it != fields.end() && !it->second.empty()) {
        message->expires = parseTimestamp(it->second);
    } else {
        message->expires = message->timestamp + std::chrono::minutes(30);
    }
    
    // Parse nested metadata object
    it = fields.find("metadata");
    if (it != fields.end() && !it->second.empty()) {
        size_t metaPos = 0;
        std::map<std::string, std::string> metadata = parseJsonObject(it->second, metaPos);
        for (const auto& kv : metadata) {
            message->metadata[kv.first] = kv.second;
        }
    }
    
    // Parse atom_content if present
    it = fields.find("atom_content");
    if (it != fields.end() && _atomspace) {
        message->atom_content = deserializeAtom(it->second);
    }
    
    return message;
}

std::string MessageSerializer::compressData(const std::string& data) {
    // Use simple Run-Length Encoding for basic compression
    // In production, this would use zlib or similar for better ratios
    
    if (data.size() < 64) {
        // Don't compress very small data - overhead not worth it
        return data;
    }
    
    logger().debug("Compressing data of size: %zu", data.size());
    
    std::string compressed = rleCompress(data);
    
    // Only use compressed version if it's actually smaller
    if (compressed.size() < data.size()) {
        // Prepend a marker to indicate compression
        logger().debug("Compression ratio: %.2f%%", 
                      100.0 * static_cast<double>(compressed.size()) / static_cast<double>(data.size()));
        return "\x01" + compressed;  // 0x01 marker for RLE compression
    }
    
    // Return original data with no-compression marker
    return "\x00" + data;
}

std::string MessageSerializer::decompressData(const std::string& compressed_data) {
    if (compressed_data.empty()) {
        return compressed_data;
    }
    
    logger().debug("Decompressing data of size: %zu", compressed_data.size());
    
    // Check compression marker
    uint8_t marker = static_cast<uint8_t>(compressed_data[0]);
    std::string payload = compressed_data.substr(1);
    
    switch (marker) {
        case 0x00:
            // No compression - return as-is
            return payload;
            
        case 0x01:
            // RLE compression
            return rleDecompress(payload);
            
        default:
            // Unknown format or legacy uncompressed data
            // Assume it's raw JSON if it starts with '{'
            if (compressed_data[0] == '{') {
                return compressed_data;
            }
            logger().warn("Unknown compression marker: 0x%02x", marker);
            return compressed_data;
    }
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
    if (timestamp.empty()) {
        return std::chrono::system_clock::now();
    }
    
    // Parse ISO 8601 format: YYYY-MM-DDTHH:MM:SSZ
    struct tm tm = {};
    int year, month, day, hour, min, sec;
    
    // Try to parse ISO 8601 format
    if (sscanf(timestamp.c_str(), "%d-%d-%dT%d:%d:%dZ", 
               &year, &month, &day, &hour, &min, &sec) == 6) {
        tm.tm_year = year - 1900;
        tm.tm_mon = month - 1;
        tm.tm_mday = day;
        tm.tm_hour = hour;
        tm.tm_min = min;
        tm.tm_sec = sec;
        tm.tm_isdst = 0;
        
        // Convert to time_t (UTC)
        time_t time_val;
#ifdef _WIN32
        time_val = _mkgmtime(&tm);
#else
        time_val = timegm(&tm);
#endif
        
        if (time_val != static_cast<time_t>(-1)) {
            return std::chrono::system_clock::from_time_t(time_val);
        }
    }
    
    // Fallback: try simpler date format YYYY-MM-DD
    if (sscanf(timestamp.c_str(), "%d-%d-%d", &year, &month, &day) == 3) {
        tm.tm_year = year - 1900;
        tm.tm_mon = month - 1;
        tm.tm_mday = day;
        tm.tm_hour = 0;
        tm.tm_min = 0;
        tm.tm_sec = 0;
        tm.tm_isdst = 0;
        
        time_t time_val;
#ifdef _WIN32
        time_val = _mkgmtime(&tm);
#else
        time_val = timegm(&tm);
#endif
        
        if (time_val != static_cast<time_t>(-1)) {
            return std::chrono::system_clock::from_time_t(time_val);
        }
    }
    
    // Failed to parse - return current time
    logger().debug("Failed to parse timestamp: %s", timestamp.c_str());
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