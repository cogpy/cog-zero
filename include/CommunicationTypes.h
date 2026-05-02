/**
 * CommunicationTypes.h - Type Definitions for Agent-Zero Communication
 * 
 * Part of AZ-HUMAN-001: Create HumanInterface layer
 * Defines all types and structures used in human-agent communication
 * 
 * Copyright (C) 2024 OpenCog Foundation
 */

#ifndef AGENTZERO_COMMUNICATION_TYPES_H
#define AGENTZERO_COMMUNICATION_TYPES_H

#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <memory>
#include <variant>

// OpenCog includes
#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atomspace/Handle.h>
#include <opencog/util/Logger.h>

namespace opencog {
namespace agentzero {
namespace communication {

// Forward declarations
using AtomSpacePtr = std::shared_ptr<AtomSpace>;
using Handle = opencog::Handle;
using TimePoint = std::chrono::system_clock::time_point;
using Duration = std::chrono::milliseconds;

// === Input Types and Structures ===

/**
 * Types of input modalities supported
 */
enum class InputType {
    TEXT,           // Plain text input
    VOICE,          // Voice/audio input
    GESTURE,        // Gesture or touch input
    MULTIMODAL,     // Combined input types
    COMMAND         // System commands
};

/**
 * Raw input data from various sources
 */
struct RawInput {
    InputType type;
    std::string content;              // Primary content (text, audio path, etc.)
    std::map<std::string, std::string> metadata;  // Additional metadata
    TimePoint timestamp;
    std::string source_id;            // Device or source identifier
    
    RawInput() : timestamp(std::chrono::system_clock::now()) {}
};

/**
 * Processed and structured input
 */
struct ProcessedInput {
    std::string normalized_text;      // Normalized text representation
    std::vector<std::string> tokens;  // Tokenized input
    std::map<std::string, std::variant<std::string, double, bool>> entities; // Extracted entities
    double confidence;                // Processing confidence score
    std::vector<std::string> intents; // Detected intents
    std::map<std::string, std::string> parameters; // Extracted parameters
    
    ProcessedInput() : confidence(1.0) {}
};

/**
 * Complete human input structure
 */
struct HumanInput {
    RawInput raw;
    ProcessedInput processed;
    std::string user_id;
    std::map<std::string, std::string> context_hints; // Context from UI/application
    
    HumanInput() = default;
    explicit HumanInput(const std::string& text) {
        raw.type = InputType::TEXT;
        raw.content = text;
        processed.normalized_text = text;
    }
};

// === Output Types and Structures ===

/**
 * Response format types
 */
enum class OutputFormat {
    PLAIN_TEXT,     // Simple text response
    RICH_TEXT,      // Formatted text with markup
    VOICE,          // Text-to-speech output
    MULTIMODAL,     // Combined output formats
    STRUCTURED      // JSON or XML structured data
};

/**
 * Agent's internal response before formatting
 */
struct AgentResponse {
    std::string content;              // Primary response content
    std::vector<std::string> suggestions; // Follow-up suggestions
    std::map<std::string, std::string> metadata; // Response metadata
    double confidence;                // Response confidence
    std::vector<std::string> context_updates; // Context to add/update
    
    AgentResponse() : confidence(1.0) {}
};

/**
 * Formatted response ready for output
 */
struct FormattedResponse {
    OutputFormat format;
    std::string formatted_content;    // Formatted for specific output
    std::map<std::string, std::string> format_metadata;
    
    FormattedResponse() : format(OutputFormat::PLAIN_TEXT) {}
};

/**
 * Complete interaction response
 */
struct InteractionResponse {
    AgentResponse agent_response;
    FormattedResponse formatted_response;
    std::string session_id;
    TimePoint response_timestamp;
    Duration processing_time;
    bool success;
    std::string error_message;        // If success is false
    
    InteractionResponse() : 
        response_timestamp(std::chrono::system_clock::now()),
        processing_time(Duration::zero()),
        success(false) {}
};

// === Session Management ===

/**
 * Session configuration parameters
 */
struct SessionConfig {
    Duration max_idle_time{std::chrono::minutes(30)};
    size_t max_history_size{100};
    bool enable_context_persistence{true};
    bool enable_learning{true};
    std::map<std::string, std::string> custom_settings;
    
    SessionConfig() = default;
};

/**
 * Current session state
 */
struct SessionState {
    std::string session_id;
    std::string user_id;
    TimePoint start_time;
    TimePoint last_activity;
    SessionConfig config;
    size_t interaction_count{0};
    bool active{true};
    
    SessionState() : 
        start_time(std::chrono::system_clock::now()),
        last_activity(std::chrono::system_clock::now()) {}
};

// === Context Management ===

/**
 * Context information for conversations
 */
struct InteractionContext {
    std::map<std::string, std::string> variables;     // Context variables
    std::vector<std::string> active_topics;           // Current conversation topics
    std::string current_domain;                       // Current domain/subject area
    std::map<std::string, Handle> referenced_atoms;   // AtomSpace references
    TimePoint last_update;
    
    InteractionContext() : last_update(std::chrono::system_clock::now()) {}
};

/**
 * Context update structure
 */
struct ContextUpdate {
    enum class UpdateType { SET, ADD, REMOVE, CLEAR };
    
    UpdateType type;
    std::string key;
    std::string value;
    std::map<std::string, std::string> batch_updates;
    
    ContextUpdate(UpdateType t, const std::string& k, const std::string& v) 
        : type(t), key(k), value(v) {}
    ContextUpdate() : type(UpdateType::SET) {}
};

// === History and Analytics ===

/**
 * Complete interaction record for history
 */
struct InteractionRecord {
    std::string session_id;
    std::string user_id;
    HumanInput input;
    InteractionResponse response;
    InteractionContext context_before;
    InteractionContext context_after;
    TimePoint timestamp;
    std::map<std::string, double> metrics;  // Custom metrics
    
    InteractionRecord() : timestamp(std::chrono::system_clock::now()) {}
};

/**
 * Analytics and performance metrics
 */
struct InteractionAnalytics {
    size_t total_interactions{0};
    Duration average_response_time{Duration::zero()};
    double success_rate{0.0};
    double average_confidence{0.0};
    std::map<InputType, size_t> input_type_counts;
    std::map<std::string, size_t> intent_counts;
    std::map<std::string, double> custom_metrics;
    TimePoint period_start;
    TimePoint period_end;
    
    InteractionAnalytics() : 
        period_start(std::chrono::system_clock::now()),
        period_end(std::chrono::system_clock::now()) {}
};

// === Configuration ===

/**
 * Main communication configuration
 */
struct CommunicationConfig {
    // Processing settings
    bool enable_input_preprocessing{true};
    bool enable_context_awareness{true};
    bool enable_learning_from_interactions{true};
    
    // Response settings
    OutputFormat default_output_format{OutputFormat::PLAIN_TEXT};
    double min_confidence_threshold{0.3};
    size_t max_response_length{1000};
    
    // Session settings
    Duration default_session_timeout{std::chrono::hours(1)};
    size_t max_concurrent_sessions{100};
    size_t max_history_per_session{50};
    
    // AtomSpace integration
    bool store_interactions_in_atomspace{true};
    bool enable_pattern_learning{true};
    std::string atomspace_namespace{"human-interface"};
    
    // Logging and monitoring
    bool enable_detailed_logging{false};
    bool enable_performance_monitoring{true};
    
    // Custom settings
    std::map<std::string, std::string> custom_config;
    
    CommunicationConfig() = default;
};

// === AtomSpace Integration ===

/**
 * Pattern query for AtomSpace interaction retrieval
 */
struct PatternQuery {
    std::vector<std::string> required_patterns;
    std::map<std::string, std::string> variable_bindings;
    double min_confidence{0.0};
    size_t max_results{50};
    TimePoint since_timestamp;
    
    PatternQuery() : since_timestamp(std::chrono::system_clock::now() - std::chrono::hours(24)) {}
};

/**
 * Knowledge update for AtomSpace
 */
struct KnowledgeUpdate {
    enum class UpdateType { CREATE, UPDATE, DELETE, MERGE };
    
    UpdateType type;
    std::vector<Handle> atoms_to_update;
    std::map<std::string, std::string> new_knowledge;
    double confidence{1.0};
    std::string source{"human-interface"};
    
    KnowledgeUpdate(UpdateType t) : type(t) {}
    KnowledgeUpdate() : type(UpdateType::CREATE) {}
};

// === System Status and Monitoring ===

/**
 * Overall system status
 */
struct SystemStatus {
    bool healthy{true};
    size_t active_sessions{0};
    size_t total_interactions{0};
    Duration uptime{Duration::zero()};
    double average_response_time_ms{0.0};
    double success_rate{0.0};
    std::map<std::string, std::string> component_status;
    std::vector<std::string> warnings;
    std::vector<std::string> errors;
    TimePoint status_timestamp;
    
    SystemStatus() : status_timestamp(std::chrono::system_clock::now()) {}
};

// === Error Handling ===

/**
 * Communication-specific exception types
 */
class CommunicationException : public std::exception {
public:
    explicit CommunicationException(const std::string& message) : message_(message) {}
    const char* what() const noexcept override { return message_.c_str(); }
private:
    std::string message_;
};

class SessionNotFoundException : public CommunicationException {
public:
    explicit SessionNotFoundException(const std::string& session_id) 
        : CommunicationException("Session not found: " + session_id) {}
};

class ProcessingException : public CommunicationException {
public:
    explicit ProcessingException(const std::string& message) 
        : CommunicationException("Processing error: " + message) {}
};

// === Utility Functions ===

/**
 * Convert InputType enum to string
 */
inline std::string inputTypeToString(InputType type) {
    switch (type) {
        case InputType::TEXT: return "text";
        case InputType::VOICE: return "voice";
        case InputType::GESTURE: return "gesture";
        case InputType::MULTIMODAL: return "multimodal";
        case InputType::COMMAND: return "command";
        default: return "unknown";
    }
}

/**
 * Convert OutputFormat enum to string
 */
inline std::string outputFormatToString(OutputFormat format) {
    switch (format) {
        case OutputFormat::PLAIN_TEXT: return "plain_text";
        case OutputFormat::RICH_TEXT: return "rich_text";
        case OutputFormat::VOICE: return "voice";
        case OutputFormat::MULTIMODAL: return "multimodal";
        case OutputFormat::STRUCTURED: return "structured";
        default: return "unknown";
    }
}

/**
 * Generate unique identifier
 */
std::string generateUniqueId(const std::string& prefix = "");

/**
 * Calculate time difference in milliseconds
 */
inline long long timeDiffMs(const TimePoint& start, const TimePoint& end) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
}

} // namespace communication
} // namespace agentzero
} // namespace opencog

#endif // AGENTZERO_COMMUNICATION_TYPES_H