/**
 * CommunicationUtils.h - Utility Functions for Agent-Zero Communication
 * 
 * Part of AZ-HUMAN-001: Create HumanInterface layer
 * Provides utility functions and helpers for communication operations
 * 
 * Copyright (C) 2024 OpenCog Foundation
 */

#ifndef AGENTZERO_COMMUNICATION_UTILS_H
#define AGENTZERO_COMMUNICATION_UTILS_H

#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <sstream>

#include "CommunicationTypes.h"

namespace opencog {
namespace agentzero {
namespace communication {
namespace utils {

/**
 * Text Processing Utilities
 */
namespace text {
    /**
     * Normalize text for processing (trim, lowercase, etc.)
     * @param text Input text to normalize
     * @return Normalized text
     */
    std::string normalize(const std::string& text);
    
    /**
     * Tokenize text into words
     * @param text Input text to tokenize
     * @return Vector of tokens
     */
    std::vector<std::string> tokenize(const std::string& text);
    
    /**
     * Extract sentences from text
     * @param text Input text
     * @return Vector of sentences
     */
    std::vector<std::string> extractSentences(const std::string& text);
    
    /**
     * Remove special characters and normalize whitespace
     * @param text Input text
     * @return Cleaned text
     */
    std::string clean(const std::string& text);
    
    /**
     * Calculate similarity between two text strings
     * @param text1 First text
     * @param text2 Second text
     * @return Similarity score between 0.0 and 1.0
     */
    double calculateSimilarity(const std::string& text1, const std::string& text2);
    
    /**
     * Detect language of text (basic implementation)
     * @param text Input text
     * @return ISO language code (e.g., "en", "es", "fr")
     */
    std::string detectLanguage(const std::string& text);
    
    /**
     * Count words in text
     * @param text Input text
     * @return Number of words
     */
    size_t countWords(const std::string& text);
}

/**
 * Session Management Utilities
 */
namespace session {
    /**
     * Generate a unique session identifier
     * @param user_id User identifier
     * @param prefix Optional prefix for the ID
     * @return Unique session ID
     */
    std::string generateSessionId(const std::string& user_id, 
                                 const std::string& prefix = "session");
    
    /**
     * Validate session ID format
     * @param session_id Session ID to validate
     * @return true if valid format, false otherwise
     */
    bool isValidSessionId(const std::string& session_id);
    
    /**
     * Extract user ID from session ID
     * @param session_id Session ID
     * @return User ID if extractable, empty string otherwise
     */
    std::string extractUserId(const std::string& session_id);
    
    /**
     * Calculate session duration
     * @param start_time Session start time
     * @param end_time Session end time (current time if not provided)
     * @return Session duration in milliseconds
     */
    Duration calculateDuration(const TimePoint& start_time, 
                              const TimePoint& end_time = std::chrono::system_clock::now());
}

/**
 * Context Management Utilities
 */
namespace context {
    /**
     * Merge two contexts, with second context taking precedence
     * @param base_context Base context
     * @param update_context Context updates to merge
     * @return Merged context
     */
    InteractionContext mergeContexts(const InteractionContext& base_context,
                                   const InteractionContext& update_context);
    
    /**
     * Extract relevant context from input
     * @param input Human input
     * @return Extracted context information
     */
    InteractionContext extractContextFromInput(const HumanInput& input);
    
    /**
     * Prune old context entries based on age
     * @param context Context to prune
     * @param max_age Maximum age for context entries
     * @return Pruned context
     */
    InteractionContext pruneOldContext(const InteractionContext& context, 
                                     const Duration& max_age);
    
    /**
     * Get context relevance score for a given input
     * @param context Current context
     * @param input Human input
     * @return Relevance score between 0.0 and 1.0
     */
    double calculateContextRelevance(const InteractionContext& context, 
                                   const HumanInput& input);
}

/**
 * Response Formatting Utilities
 */
namespace formatting {
    /**
     * Format response for plain text output
     * @param response Agent response
     * @return Formatted plain text
     */
    std::string formatPlainText(const AgentResponse& response);
    
    /**
     * Format response for rich text output with basic markup
     * @param response Agent response
     * @return Formatted rich text with markup
     */
    std::string formatRichText(const AgentResponse& response);
    
    /**
     * Format response as JSON structure
     * @param response Agent response
     * @return JSON formatted response
     */
    std::string formatJson(const AgentResponse& response);
    
    /**
     * Add suggestions to formatted response
     * @param formatted_text Base formatted text
     * @param suggestions Vector of suggestions
     * @param format Output format type
     * @return Text with appended suggestions
     */
    std::string appendSuggestions(const std::string& formatted_text,
                                const std::vector<std::string>& suggestions,
                                OutputFormat format = OutputFormat::PLAIN_TEXT);
    
    /**
     * Truncate response to maximum length while preserving word boundaries
     * @param text Text to truncate
     * @param max_length Maximum allowed length
     * @return Truncated text
     */
    std::string truncateResponse(const std::string& text, size_t max_length);
}

/**
 * Analytics and Monitoring Utilities
 */
namespace analytics {
    /**
     * Calculate response time percentiles
     * @param response_times Vector of response times
     * @return Map of percentiles (50th, 90th, 95th, 99th)
     */
    std::map<int, Duration> calculatePercentiles(const std::vector<Duration>& response_times);
    
    /**
     * Analyze input patterns and extract metrics
     * @param inputs Vector of processed inputs
     * @return Analytics metrics
     */
    std::map<std::string, double> analyzeInputPatterns(const std::vector<ProcessedInput>& inputs);
    
    /**
     * Generate analytics report as text
     * @param analytics Analytics data
     * @return Formatted analytics report
     */
    std::string generateAnalyticsReport(const InteractionAnalytics& analytics);
    
    /**
     * Calculate trend metrics for time-series data
     * @param values Vector of time-stamped values
     * @return Trend information (slope, correlation, etc.)
     */
    std::map<std::string, double> calculateTrends(
        const std::vector<std::pair<TimePoint, double>>& values);
}

/**
 * Configuration Utilities
 */
namespace config {
    /**
     * Load configuration from string (JSON format)
     * @param config_string JSON configuration string
     * @return Parsed configuration
     */
    CommunicationConfig loadFromString(const std::string& config_string);
    
    /**
     * Save configuration to string (JSON format)
     * @param config Configuration to save
     * @return JSON configuration string
     */
    std::string saveToString(const CommunicationConfig& config);
    
    /**
     * Validate configuration parameters
     * @param config Configuration to validate
     * @return Vector of validation errors (empty if valid)
     */
    std::vector<std::string> validateConfig(const CommunicationConfig& config);
    
    /**
     * Get default configuration
     * @return Default configuration instance
     */
    CommunicationConfig getDefaultConfig();
    
    /**
     * Merge configurations with override precedence
     * @param base_config Base configuration
     * @param override_config Override configuration
     * @return Merged configuration
     */
    CommunicationConfig mergeConfigs(const CommunicationConfig& base_config,
                                   const CommunicationConfig& override_config);
}

/**
 * Error Handling Utilities
 */
namespace error {
    /**
     * Create standardized error response
     * @param error_code Error code identifier
     * @param error_message Human-readable error message
     * @param session_id Session ID where error occurred
     * @return Error response structure
     */
    InteractionResponse createErrorResponse(const std::string& error_code,
                                          const std::string& error_message,
                                          const std::string& session_id);
    
    /**
     * Log error with context information
     * @param error_message Error message
     * @param context Additional context information
     */
    void logError(const std::string& error_message, 
                 const std::map<std::string, std::string>& context = {});
    
    /**
     * Convert exception to user-friendly error message
     * @param exception Exception to convert
     * @return User-friendly error message
     */
    std::string formatExceptionForUser(const std::exception& exception);
    
    /**
     * Validate input and return validation errors
     * @param input Human input to validate
     * @return Vector of validation errors (empty if valid)
     */
    std::vector<std::string> validateInput(const HumanInput& input);
}

/**
 * Performance Utilities
 */
namespace performance {
    /**
     * Simple performance timer class
     */
    class Timer {
    public:
        Timer() : start_time_(std::chrono::high_resolution_clock::now()) {}
        
        /**
         * Get elapsed time since timer creation/reset
         * @return Elapsed time in milliseconds
         */
        Duration elapsed() const {
            auto now = std::chrono::high_resolution_clock::now();
            return std::chrono::duration_cast<Duration>(now - start_time_);
        }
        
        /**
         * Reset timer to current time
         */
        void reset() {
            start_time_ = std::chrono::high_resolution_clock::now();
        }
        
    private:
        std::chrono::high_resolution_clock::time_point start_time_;
    };
    
    /**
     * Create performance metrics for an operation
     * @param operation_name Name of the operation
     * @param duration Time taken for operation
     * @param success Whether operation succeeded
     * @return Performance metrics map
     */
    std::map<std::string, double> createMetrics(const std::string& operation_name,
                                              const Duration& duration,
                                              bool success = true);
    
    /**
     * Profile memory usage of the current process
     * @return Memory usage information
     */
    std::map<std::string, size_t> getMemoryUsage();
}

} // namespace utils
} // namespace communication
} // namespace agentzero
} // namespace opencog

#endif // AGENTZERO_COMMUNICATION_UTILS_H