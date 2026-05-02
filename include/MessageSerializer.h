/*
 * opencog/agentzero/communication/MessageSerializer.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Message Serialization Component
 * Part of the AGENT-ZERO-GENESIS project - AZ-COMM-001
 */

#ifndef _OPENCOG_AGENTZERO_MESSAGE_SERIALIZER_H
#define _OPENCOG_AGENTZERO_MESSAGE_SERIALIZER_H

#include <memory>
#include <string>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Handle.h>
#include <opencog/util/Logger.h>

#include "CommTypes.h"

namespace opencog {
namespace agentzero {
namespace communication {

/**
 * MessageSerializer - Handles serialization and deserialization of messages
 *
 * This class provides serialization capabilities for Agent-Zero communication
 * messages, supporting both text-based and AtomSpace-based content formats.
 * It enables efficient transmission and storage of complex data structures.
 *
 * Key Features:
 * - JSON serialization for text messages
 * - AtomSpace serialization for complex knowledge structures  
 * - Compression support for large messages
 * - Schema validation and versioning
 * - Binary format support for efficiency
 */
class MessageSerializer {
private:
    // AtomSpace for atom serialization
    AtomSpacePtr _atomspace;
    
    // Serialization configuration
    bool _enable_compression;
    bool _enable_validation;
    std::string _schema_version;
    
    // Statistics
    size_t _messages_serialized;
    size_t _messages_deserialized;
    size_t _compression_ratio_sum;
    mutable std::mutex _stats_mutex;
    
    // Internal methods
    std::string serializeToJSON(const CommMessagePtr& message);
    CommMessagePtr deserializeFromJSON(const std::string& json_data);
    std::string compressData(const std::string& data);
    std::string decompressData(const std::string& compressed_data);
    bool validateMessageStructure(const CommMessagePtr& message);
    std::string formatTimestamp(const std::chrono::system_clock::time_point& time);
    std::chrono::system_clock::time_point parseTimestamp(const std::string& timestamp);
    void updateSerializationStats(bool serializing, size_t original_size, size_t processed_size);

public:
    /**
     * Constructor
     * @param atomspace AtomSpace for atom serialization
     * @param enable_compression Whether to enable compression
     * @param enable_validation Whether to enable validation
     */
    explicit MessageSerializer(AtomSpacePtr atomspace = nullptr,
                              bool enable_compression = false,
                              bool enable_validation = true);
    
    /**
     * Destructor
     */
    virtual ~MessageSerializer();
    
    // Core serialization operations
    /**
     * Serialize message to string format
     * @param message Message to serialize
     * @return Serialized message string
     */
    std::string serialize(const CommMessagePtr& message);
    
    /**
     * Deserialize message from string format
     * @param data Serialized message data
     * @return Deserialized message or null on failure
     */
    CommMessagePtr deserialize(const std::string& data);
    
    /**
     * Serialize message to binary format
     * @param message Message to serialize
     * @return Binary data vector
     */
    std::vector<uint8_t> serializeBinary(const CommMessagePtr& message);
    
    /**
     * Deserialize message from binary format
     * @param data Binary message data
     * @return Deserialized message or null on failure
     */
    CommMessagePtr deserializeBinary(const std::vector<uint8_t>& data);
    
    // AtomSpace serialization
    /**
     * Serialize AtomSpace handle to string
     * @param handle Handle to serialize
     * @return Serialized atom string
     */
    std::string serializeAtom(const Handle& handle);
    
    /**
     * Deserialize string to AtomSpace handle
     * @param atom_data Serialized atom string
     * @return Deserialized handle or UNDEFINED on failure
     */
    Handle deserializeAtom(const std::string& atom_data);
    
    /**
     * Serialize multiple atoms to string
     * @param handles Vector of handles to serialize
     * @return Serialized atoms string
     */
    std::string serializeAtoms(const std::vector<Handle>& handles);
    
    /**
     * Deserialize string to multiple atoms
     * @param atoms_data Serialized atoms string
     * @return Vector of deserialized handles
     */
    std::vector<Handle> deserializeAtoms(const std::string& atoms_data);
    
    // Configuration
    /**
     * Set AtomSpace for atom serialization
     * @param atomspace AtomSpace to use
     */
    void setAtomSpace(AtomSpacePtr atomspace) { _atomspace = atomspace; }
    
    /**
     * Get current AtomSpace
     */
    AtomSpacePtr getAtomSpace() const { return _atomspace; }
    
    /**
     * Enable/disable compression
     * @param enabled Whether to enable compression
     */
    void setCompressionEnabled(bool enabled) { _enable_compression = enabled; }
    
    /**
     * Check if compression is enabled
     */
    bool isCompressionEnabled() const { return _enable_compression; }
    
    /**
     * Enable/disable validation
     * @param enabled Whether to enable validation
     */
    void setValidationEnabled(bool enabled) { _enable_validation = enabled; }
    
    /**
     * Check if validation is enabled
     */
    bool isValidationEnabled() const { return _enable_validation; }
    
    /**
     * Set schema version for serialization
     * @param version Schema version string
     */
    void setSchemaVersion(const std::string& version) { _schema_version = version; }
    
    /**
     * Get current schema version
     */
    const std::string& getSchemaVersion() const { return _schema_version; }
    
    // Utility methods
    /**
     * Get estimated serialized size for message
     * @param message Message to estimate
     * @return Estimated size in bytes
     */
    size_t estimateSerializedSize(const CommMessagePtr& message);
    
    /**
     * Check if message can be serialized
     * @param message Message to check
     * @return true if serializable
     */
    bool isSerializable(const CommMessagePtr& message);
    
    /**
     * Get compression ratio for last operation
     * @return Compression ratio (0.0 = no compression, 1.0 = no change)
     */
    double getCompressionRatio() const;
    
    /**
     * Get average compression ratio
     * @return Average compression ratio across all operations
     */
    double getAverageCompressionRatio() const;
    
    // Statistics and monitoring
    /**
     * Get serialization statistics
     * @return JSON string with serialization statistics
     */
    std::string getStats() const;
    
    /**
     * Reset serialization statistics
     */
    void resetStats();
    
    /**
     * Get number of messages serialized
     */
    size_t getSerializedCount() const;
    
    /**
     * Get number of messages deserialized
     */
    size_t getDeserializedCount() const;
    
    // Format validation and conversion
    /**
     * Validate serialized message format
     * @param data Serialized message data
     * @return true if format is valid
     */
    bool validateFormat(const std::string& data);
    
    /**
     * Convert between serialization formats
     * @param data Input data in one format
     * @param from_format Source format ("json", "binary", "atom")
     * @param to_format Target format ("json", "binary", "atom")
     * @return Converted data or empty string on failure
     */
    std::string convertFormat(const std::string& data,
                             const std::string& from_format,
                             const std::string& to_format);
    
    /**
     * Get supported serialization formats
     * @return Vector of supported format names
     */
    std::vector<std::string> getSupportedFormats() const;
    
    // Schema management
    /**
     * Register custom message schema
     * @param schema_name Name of the schema
     * @param schema_definition JSON schema definition
     * @return true if registration successful
     */
    bool registerSchema(const std::string& schema_name, const std::string& schema_definition);
    
    /**
     * Validate message against schema
     * @param message Message to validate
     * @param schema_name Schema to validate against
     * @return true if validation passes
     */
    bool validateAgainstSchema(const CommMessagePtr& message, const std::string& schema_name);
    
    /**
     * Get registered schemas
     * @return Vector of registered schema names
     */
    std::vector<std::string> getRegisteredSchemas() const;
};

} // namespace communication
} // namespace agentzero
} // namespace opencog

#endif // _OPENCOG_AGENTZERO_MESSAGE_SERIALIZER_H