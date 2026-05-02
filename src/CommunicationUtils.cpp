/**
 * CommunicationUtils.cpp - Implementation of Communication Utilities
 * 
 * Part of AZ-HUMAN-001: Create HumanInterface layer
 * Provides utility functions and helpers for communication operations
 * 
 * Copyright (C) 2024 OpenCog Foundation
 */

#include <agentzero/communication/CommunicationUtils.h>

#include <algorithm>
#include <cctype>
#include <regex>
#include <random>
#include <iomanip>
#include <numeric>
#include <cmath>

#include <opencog/util/Logger.h>

namespace opencog {
namespace agentzero {
namespace communication {
namespace utils {

// === Text Processing Utilities ===

namespace text {

std::string normalize(const std::string& text) {
    std::string normalized = text;
    
    // Trim whitespace from both ends
    normalized.erase(0, normalized.find_first_not_of(" \t\r\n"));
    normalized.erase(normalized.find_last_not_of(" \t\r\n") + 1);
    
    // Convert to lowercase
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    
    // Replace multiple whitespaces with single space
    std::regex multiple_spaces("\\s+");
    normalized = std::regex_replace(normalized, multiple_spaces, " ");
    
    return normalized;
}

std::vector<std::string> tokenize(const std::string& text) {
    std::vector<std::string> tokens;
    std::istringstream iss(text);
    std::string token;
    
    while (iss >> token) {
        // Remove punctuation from token ends
        token.erase(0, token.find_first_not_of(".,!?;:\"'"));
        token.erase(token.find_last_not_of(".,!?;:\"'") + 1);
        
        if (!token.empty()) {
            tokens.push_back(token);
        }
    }
    
    return tokens;
}

std::vector<std::string> extractSentences(const std::string& text) {
    std::vector<std::string> sentences;
    
    // Simple sentence splitting on . ! ?
    std::regex sentence_regex("[.!?]+");
    std::sregex_token_iterator iter(text.begin(), text.end(), sentence_regex, -1);
    std::sregex_token_iterator end;
    
    for (; iter != end; ++iter) {
        std::string sentence = iter->str();
        sentence = normalize(sentence);
        if (!sentence.empty()) {
            sentences.push_back(sentence);
        }
    }
    
    return sentences;
}

std::string clean(const std::string& text) {
    std::string cleaned = text;
    
    // Remove non-printable characters
    cleaned.erase(std::remove_if(cleaned.begin(), cleaned.end(),
                                [](unsigned char c) { return !std::isprint(c) && c != ' ' && c != '\t'; }),
                 cleaned.end());
    
    // Replace tabs with spaces
    std::replace(cleaned.begin(), cleaned.end(), '\t', ' ');
    
    // Normalize whitespace
    return normalize(cleaned);
}

double calculateSimilarity(const std::string& text1, const std::string& text2) {
    auto tokens1 = tokenize(normalize(text1));
    auto tokens2 = tokenize(normalize(text2));
    
    if (tokens1.empty() && tokens2.empty()) {
        return 1.0;
    }
    
    if (tokens1.empty() || tokens2.empty()) {
        return 0.0;
    }
    
    // Simple Jaccard similarity
    std::set<std::string> set1(tokens1.begin(), tokens1.end());
    std::set<std::string> set2(tokens2.begin(), tokens2.end());
    
    std::set<std::string> intersection;
    std::set_intersection(set1.begin(), set1.end(),
                         set2.begin(), set2.end(),
                         std::inserter(intersection, intersection.begin()));
    
    std::set<std::string> union_set;
    std::set_union(set1.begin(), set1.end(),
                  set2.begin(), set2.end(),
                  std::inserter(union_set, union_set.begin()));
    
    return static_cast<double>(intersection.size()) / union_set.size();
}

std::string detectLanguage(const std::string& text) {
    // Very basic language detection - in practice would use proper libraries
    std::string normalized = normalize(text);
    
    // English indicators
    if (normalized.find("the ") != std::string::npos ||
        normalized.find(" and ") != std::string::npos ||
        normalized.find(" is ") != std::string::npos ||
        normalized.find(" are ") != std::string::npos) {
        return "en";
    }
    
    // Spanish indicators
    if (normalized.find(" el ") != std::string::npos ||
        normalized.find(" la ") != std::string::npos ||
        normalized.find(" es ") != std::string::npos ||
        normalized.find(" son ") != std::string::npos) {
        return "es";
    }
    
    // Default to English
    return "en";
}

size_t countWords(const std::string& text) {
    return tokenize(text).size();
}

} // namespace text

// === Session Management Utilities ===

namespace session {

std::string generateSessionId(const std::string& user_id, const std::string& prefix) {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(10000, 99999);
    
    std::ostringstream oss;
    oss << prefix << "_" << user_id << "_" << timestamp << "_" << dis(gen);
    return oss.str();
}

bool isValidSessionId(const std::string& session_id) {
    // Basic validation: must contain underscores and be reasonable length
    return !session_id.empty() && 
           session_id.length() > 10 && 
           session_id.length() < 200 &&
           session_id.find('_') != std::string::npos;
}

std::string extractUserId(const std::string& session_id) {
    // Assuming format: prefix_userid_timestamp_random
    auto parts = tokenize(session_id);  // This won't work as expected with underscores
    
    // Better approach: split on underscores
    std::vector<std::string> parts_underscore;
    std::stringstream ss(session_id);
    std::string part;
    
    while (std::getline(ss, part, '_')) {
        parts_underscore.push_back(part);
    }
    
    if (parts_underscore.size() >= 4) {
        return parts_underscore[1]; // User ID is second part
    }
    
    return "";
}

Duration calculateDuration(const TimePoint& start_time, const TimePoint& end_time) {
    return std::chrono::duration_cast<Duration>(end_time - start_time);
}

} // namespace session

// === Context Management Utilities ===

namespace context {

InteractionContext mergeContexts(const InteractionContext& base_context,
                               const InteractionContext& update_context) {
    InteractionContext merged = base_context;
    
    // Merge variables (update takes precedence)
    for (const auto& pair : update_context.variables) {
        merged.variables[pair.first] = pair.second;
    }
    
    // Merge topics (combine and remove duplicates)
    std::set<std::string> topic_set(merged.active_topics.begin(), merged.active_topics.end());
    for (const auto& topic : update_context.active_topics) {
        topic_set.insert(topic);
    }
    merged.active_topics.assign(topic_set.begin(), topic_set.end());
    
    // Update domain if provided
    if (!update_context.current_domain.empty()) {
        merged.current_domain = update_context.current_domain;
    }
    
    // Merge referenced atoms
    for (const auto& pair : update_context.referenced_atoms) {
        merged.referenced_atoms[pair.first] = pair.second;
    }
    
    merged.last_update = std::chrono::system_clock::now();
    
    return merged;
}

InteractionContext extractContextFromInput(const HumanInput& input) {
    InteractionContext context;
    
    // Extract context from processed input
    for (const auto& token : input.processed.tokens) {
        // Look for context indicators
        if (token.find("topic:") == 0 && token.length() > 6) {
            context.active_topics.push_back(token.substr(6));
        }
    }
    
    // Extract from metadata
    for (const auto& pair : input.raw.metadata) {
        if (pair.first.find("context_") == 0) {
            std::string key = pair.first.substr(8); // Remove "context_" prefix
            context.variables[key] = pair.second;
        }
    }
    
    // Extract from context hints
    for (const auto& pair : input.context_hints) {
        context.variables[pair.first] = pair.second;
    }
    
    context.last_update = std::chrono::system_clock::now();
    
    return context;
}

InteractionContext pruneOldContext(const InteractionContext& context, const Duration& max_age) {
    InteractionContext pruned = context;
    auto cutoff_time = std::chrono::system_clock::now() - max_age;
    
    // If the entire context is too old, clear it
    if (context.last_update < cutoff_time) {
        pruned.variables.clear();
        pruned.active_topics.clear();
        pruned.current_domain.clear();
        pruned.referenced_atoms.clear();
    }
    
    return pruned;
}

double calculateContextRelevance(const InteractionContext& context, const HumanInput& input) {
    double relevance = 0.0;
    int matches = 0;
    int total_checks = 0;
    
    // Check topic overlap
    for (const auto& input_token : input.processed.tokens) {
        total_checks++;
        for (const auto& topic : context.active_topics) {
            if (input_token.find(topic) != std::string::npos) {
                matches++;
                break;
            }
        }
    }
    
    // Check variable matches
    for (const auto& pair : context.variables) {
        total_checks++;
        std::string normalized_input = text::normalize(input.processed.normalized_text);
        if (normalized_input.find(text::normalize(pair.second)) != std::string::npos) {
            matches++;
        }
    }
    
    if (total_checks > 0) {
        relevance = static_cast<double>(matches) / total_checks;
    }
    
    return std::min(1.0, relevance);
}

} // namespace context

// === Response Formatting Utilities ===

namespace formatting {

std::string formatPlainText(const AgentResponse& response) {
    std::ostringstream oss;
    oss << response.content;
    
    if (!response.suggestions.empty()) {
        oss << "\n\nSuggestions:";
        for (size_t i = 0; i < response.suggestions.size(); ++i) {
            oss << "\n" << (i + 1) << ". " << response.suggestions[i];
        }
    }
    
    return oss.str();
}

std::string formatRichText(const AgentResponse& response) {
    std::ostringstream oss;
    oss << "<response>";
    oss << "<content>" << response.content << "</content>";
    
    if (!response.suggestions.empty()) {
        oss << "<suggestions>";
        for (const auto& suggestion : response.suggestions) {
            oss << "<suggestion>" << suggestion << "</suggestion>";
        }
        oss << "</suggestions>";
    }
    
    if (response.confidence < 0.7) {
        oss << "<confidence level=\"low\">" << response.confidence << "</confidence>";
    }
    
    oss << "</response>";
    
    return oss.str();
}

std::string formatJson(const AgentResponse& response) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);
    
    oss << "{";
    oss << "\"content\":\"" << response.content << "\",";
    oss << "\"confidence\":" << response.confidence << ",";
    oss << "\"suggestions\":[";
    
    for (size_t i = 0; i < response.suggestions.size(); ++i) {
        if (i > 0) oss << ",";
        oss << "\"" << response.suggestions[i] << "\"";
    }
    
    oss << "],";
    oss << "\"metadata\":{";
    
    bool first = true;
    for (const auto& pair : response.metadata) {
        if (!first) oss << ",";
        oss << "\"" << pair.first << "\":\"" << pair.second << "\"";
        first = false;
    }
    
    oss << "}";
    oss << "}";
    
    return oss.str();
}

std::string appendSuggestions(const std::string& formatted_text,
                            const std::vector<std::string>& suggestions,
                            OutputFormat format) {
    if (suggestions.empty()) {
        return formatted_text;
    }
    
    std::ostringstream oss;
    oss << formatted_text;
    
    switch (format) {
        case OutputFormat::PLAIN_TEXT:
            oss << "\n\nSuggestions:";
            for (size_t i = 0; i < suggestions.size(); ++i) {
                oss << "\n• " << suggestions[i];
            }
            break;
            
        case OutputFormat::RICH_TEXT:
            oss << "<suggestions>";
            for (const auto& suggestion : suggestions) {
                oss << "<item>" << suggestion << "</item>";
            }
            oss << "</suggestions>";
            break;
            
        default:
            // For other formats, use plain text format
            oss << "\nSuggestions: ";
            for (size_t i = 0; i < suggestions.size(); ++i) {
                if (i > 0) oss << "; ";
                oss << suggestions[i];
            }
            break;
    }
    
    return oss.str();
}

std::string truncateResponse(const std::string& text, size_t max_length) {
    if (text.length() <= max_length) {
        return text;
    }
    
    // Find the last space before max_length
    size_t truncate_pos = text.find_last_of(' ', max_length - 3);
    
    if (truncate_pos == std::string::npos || truncate_pos < max_length / 2) {
        // No suitable break point found, truncate at max_length - 3
        return text.substr(0, max_length - 3) + "...";
    }
    
    return text.substr(0, truncate_pos) + "...";
}

} // namespace formatting

// === Analytics and Monitoring Utilities ===

namespace analytics {

std::map<int, Duration> calculatePercentiles(const std::vector<Duration>& response_times) {
    std::map<int, Duration> percentiles;
    
    if (response_times.empty()) {
        return percentiles;
    }
    
    std::vector<Duration> sorted_times = response_times;
    std::sort(sorted_times.begin(), sorted_times.end());
    
    std::vector<int> percentile_marks = {50, 90, 95, 99};
    
    for (int p : percentile_marks) {
        size_t index = (p * sorted_times.size()) / 100;
        if (index >= sorted_times.size()) {
            index = sorted_times.size() - 1;
        }
        percentiles[p] = sorted_times[index];
    }
    
    return percentiles;
}

std::map<std::string, double> analyzeInputPatterns(const std::vector<ProcessedInput>& inputs) {
    std::map<std::string, double> metrics;
    
    if (inputs.empty()) {
        return metrics;
    }
    
    // Calculate average confidence
    double total_confidence = 0.0;
    for (const auto& input : inputs) {
        total_confidence += input.confidence;
    }
    metrics["average_confidence"] = total_confidence / inputs.size();
    
    // Calculate average token count
    size_t total_tokens = 0;
    for (const auto& input : inputs) {
        total_tokens += input.tokens.size();
    }
    metrics["average_tokens"] = static_cast<double>(total_tokens) / inputs.size();
    
    // Count intent frequencies
    std::map<std::string, int> intent_counts;
    for (const auto& input : inputs) {
        for (const auto& intent : input.intents) {
            intent_counts[intent]++;
        }
    }
    
    // Find most common intent
    if (!intent_counts.empty()) {
        auto most_common = std::max_element(intent_counts.begin(), intent_counts.end(),
            [](const auto& a, const auto& b) { return a.second < b.second; });
        
        metrics["most_common_intent_frequency"] = 
            static_cast<double>(most_common->second) / inputs.size();
    }
    
    return metrics;
}

std::string generateAnalyticsReport(const InteractionAnalytics& analytics) {
    std::ostringstream oss;
    
    oss << "=== Interaction Analytics Report ===\n";
    oss << "Total Interactions: " << analytics.total_interactions << "\n";
    oss << "Average Response Time: " << analytics.average_response_time.count() << "ms\n";
    oss << "Success Rate: " << std::fixed << std::setprecision(1) 
        << (analytics.success_rate * 100) << "%\n";
    oss << "Average Confidence: " << std::fixed << std::setprecision(2) 
        << analytics.average_confidence << "\n\n";
    
    oss << "Input Type Distribution:\n";
    for (const auto& pair : analytics.input_type_counts) {
        oss << "  " << inputTypeToString(pair.first) << ": " << pair.second << "\n";
    }
    
    oss << "\nTop Intents:\n";
    // Sort intents by frequency
    std::vector<std::pair<std::string, size_t>> sorted_intents(
        analytics.intent_counts.begin(), analytics.intent_counts.end());
    std::sort(sorted_intents.begin(), sorted_intents.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });
    
    for (size_t i = 0; i < std::min(size_t(5), sorted_intents.size()); ++i) {
        oss << "  " << sorted_intents[i].first << ": " << sorted_intents[i].second << "\n";
    }
    
    return oss.str();
}

std::map<std::string, double> calculateTrends(
    const std::vector<std::pair<TimePoint, double>>& values) {
    std::map<std::string, double> trends;
    
    if (values.size() < 2) {
        return trends;
    }
    
    // Calculate simple linear regression slope
    size_t n = values.size();
    double sum_x = 0, sum_y = 0, sum_xy = 0, sum_x2 = 0;
    
    for (size_t i = 0; i < n; ++i) {
        double x = static_cast<double>(i);  // Use index as x
        double y = values[i].second;
        
        sum_x += x;
        sum_y += y;
        sum_xy += x * y;
        sum_x2 += x * x;
    }
    
    double slope = (n * sum_xy - sum_x * sum_y) / (n * sum_x2 - sum_x * sum_x);
    trends["slope"] = slope;
    
    // Calculate correlation coefficient
    double mean_x = sum_x / n;
    double mean_y = sum_y / n;
    
    double num = 0, den_x = 0, den_y = 0;
    for (size_t i = 0; i < n; ++i) {
        double x = static_cast<double>(i);
        double y = values[i].second;
        
        num += (x - mean_x) * (y - mean_y);
        den_x += (x - mean_x) * (x - mean_x);
        den_y += (y - mean_y) * (y - mean_y);
    }
    
    if (den_x > 0 && den_y > 0) {
        trends["correlation"] = num / std::sqrt(den_x * den_y);
    }
    
    return trends;
}

} // namespace analytics

// === Configuration Utilities ===

namespace config {

CommunicationConfig loadFromString(const std::string& config_string) {
    // Simplified JSON parsing - in practice would use proper JSON library
    CommunicationConfig config;
    
    // For now, return default config
    // TODO: Implement proper JSON parsing
    return getDefaultConfig();
}

std::string saveToString(const CommunicationConfig& config) {
    std::ostringstream oss;
    
    oss << "{\n";
    oss << "  \"enable_input_preprocessing\": " 
        << (config.enable_input_preprocessing ? "true" : "false") << ",\n";
    oss << "  \"enable_context_awareness\": "
        << (config.enable_context_awareness ? "true" : "false") << ",\n";
    oss << "  \"min_confidence_threshold\": " << config.min_confidence_threshold << ",\n";
    oss << "  \"max_response_length\": " << config.max_response_length << ",\n";
    oss << "  \"default_session_timeout_hours\": " 
        << std::chrono::duration_cast<std::chrono::hours>(config.default_session_timeout).count() << ",\n";
    oss << "  \"max_concurrent_sessions\": " << config.max_concurrent_sessions << "\n";
    oss << "}";
    
    return oss.str();
}

std::vector<std::string> validateConfig(const CommunicationConfig& config) {
    std::vector<std::string> errors;
    
    if (config.min_confidence_threshold < 0.0 || config.min_confidence_threshold > 1.0) {
        errors.push_back("min_confidence_threshold must be between 0.0 and 1.0");
    }
    
    if (config.max_response_length < 10) {
        errors.push_back("max_response_length must be at least 10");
    }
    
    if (config.max_concurrent_sessions < 1) {
        errors.push_back("max_concurrent_sessions must be at least 1");
    }
    
    if (config.default_session_timeout < std::chrono::minutes(1)) {
        errors.push_back("default_session_timeout must be at least 1 minute");
    }
    
    return errors;
}

CommunicationConfig getDefaultConfig() {
    return CommunicationConfig{};  // Uses default values from struct
}

CommunicationConfig mergeConfigs(const CommunicationConfig& base_config,
                               const CommunicationConfig& override_config) {
    CommunicationConfig merged = base_config;
    
    // Simple field-by-field merge - override takes precedence for non-default values
    // This is simplified - in practice would need more sophisticated merging logic
    
    merged.enable_input_preprocessing = override_config.enable_input_preprocessing;
    merged.enable_context_awareness = override_config.enable_context_awareness;
    merged.min_confidence_threshold = override_config.min_confidence_threshold;
    merged.max_response_length = override_config.max_response_length;
    merged.default_session_timeout = override_config.default_session_timeout;
    merged.max_concurrent_sessions = override_config.max_concurrent_sessions;
    
    // Merge custom config
    for (const auto& pair : override_config.custom_config) {
        merged.custom_config[pair.first] = pair.second;
    }
    
    return merged;
}

} // namespace config

// === Error Handling Utilities ===

namespace error {

InteractionResponse createErrorResponse(const std::string& error_code,
                                      const std::string& error_message,
                                      const std::string& session_id) {
    InteractionResponse response;
    response.session_id = session_id;
    response.success = false;
    response.error_message = error_message;
    response.response_timestamp = std::chrono::system_clock::now();
    
    // Create a user-friendly agent response
    response.agent_response.content = "I apologize, but I encountered an issue processing your request. " +
                                     error_message;
    response.agent_response.confidence = 0.0;
    response.agent_response.metadata["error_code"] = error_code;
    
    // Format for plain text output
    response.formatted_response.format = OutputFormat::PLAIN_TEXT;
    response.formatted_response.formatted_content = response.agent_response.content;
    
    return response;
}

void logError(const std::string& error_message, 
             const std::map<std::string, std::string>& context) {
    std::ostringstream oss;
    oss << "HumanInterface Error: " << error_message;
    
    if (!context.empty()) {
        oss << " [Context: ";
        bool first = true;
        for (const auto& pair : context) {
            if (!first) oss << ", ";
            oss << pair.first << "=" << pair.second;
            first = false;
        }
        oss << "]";
    }
    
    logger().error(oss.str());
}

std::string formatExceptionForUser(const std::exception& exception) {
    std::string message = exception.what();
    
    // Convert technical error messages to user-friendly ones
    if (message.find("AtomSpace") != std::string::npos) {
        return "I'm having trouble accessing my knowledge base. Please try again in a moment.";
    }
    
    if (message.find("timeout") != std::string::npos) {
        return "Your request is taking longer than expected. Please try again.";
    }
    
    if (message.find("not found") != std::string::npos) {
        return "I couldn't find the information you're looking for.";
    }
    
    // Generic fallback
    return "I encountered an unexpected issue. Please rephrase your request or try again.";
}

std::vector<std::string> validateInput(const HumanInput& input) {
    std::vector<std::string> errors;
    
    if (input.raw.content.empty()) {
        errors.push_back("Input content cannot be empty");
    }
    
    if (input.raw.content.length() > 10000) {
        errors.push_back("Input content is too long (maximum 10000 characters)");
    }
    
    if (input.user_id.empty()) {
        errors.push_back("User ID is required");
    }
    
    // Check for potentially harmful content (basic check)
    std::string normalized = text::normalize(input.raw.content);
    if (normalized.find("<script") != std::string::npos ||
        normalized.find("javascript:") != std::string::npos) {
        errors.push_back("Input contains potentially harmful content");
    }
    
    return errors;
}

} // namespace error

// === Performance Utilities ===

namespace performance {

std::map<std::string, double> createMetrics(const std::string& operation_name,
                                          const Duration& duration,
                                          bool success) {
    std::map<std::string, double> metrics;
    
    metrics[operation_name + "_duration_ms"] = static_cast<double>(duration.count());
    metrics[operation_name + "_success"] = success ? 1.0 : 0.0;
    
    return metrics;
}

std::map<std::string, size_t> getMemoryUsage() {
    std::map<std::string, size_t> usage;
    
    // This is platform-specific and simplified
    // In a real implementation, would use proper system calls
    usage["virtual_memory_kb"] = 0;  // Would get actual values
    usage["resident_memory_kb"] = 0;
    usage["peak_memory_kb"] = 0;
    
    return usage;
}

} // namespace performance

} // namespace utils
} // namespace communication
} // namespace agentzero
} // namespace opencog