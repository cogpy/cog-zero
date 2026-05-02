/**
 * HumanInterface.cpp - Implementation of Human-Agent Interaction Layer
 * 
 * Part of AZ-HUMAN-001: Create HumanInterface layer
 * Provides comprehensive human-agent interaction capabilities
 * 
 * Copyright (C) 2024 OpenCog Foundation
 */

#include <agentzero/communication/HumanInterface.h>

#include <algorithm>
#include <sstream>
#include <random>
#include <iomanip>
#include <thread>

#include <opencog/util/Logger.h>
#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>
#include <opencog/atoms/truthvalue/SimpleTruthValue.h>

namespace opencog {
namespace agentzero {
namespace communication {

// === Forward Declarations for Helper Classes ===

/**
 * InputProcessor - Processes and normalizes human input
 */
class InputProcessor {
public:
    explicit InputProcessor(const CommunicationConfig& config) : config_(config) {}
    
    ProcessedInput process(const RawInput& raw_input) {
        ProcessedInput processed;
        
        // Basic text normalization
        processed.normalized_text = normalizeText(raw_input.content);
        processed.tokens = tokenize(processed.normalized_text);
        processed.confidence = 0.95; // Default high confidence for text
        
        // Extract basic intents (simple keyword-based for now)
        processed.intents = extractIntents(processed.normalized_text);
        
        return processed;
    }
    
private:
    const CommunicationConfig& config_;
    
    std::string normalizeText(const std::string& text) {
        std::string normalized = text;
        // Basic normalization: trim whitespace, convert to lowercase
        normalized.erase(0, normalized.find_first_not_of(" \t\r\n"));
        normalized.erase(normalized.find_last_not_of(" \t\r\n") + 1);
        std::transform(normalized.begin(), normalized.end(), normalized.begin(), ::tolower);
        return normalized;
    }
    
    std::vector<std::string> tokenize(const std::string& text) {
        std::vector<std::string> tokens;
        std::istringstream iss(text);
        std::string token;
        while (iss >> token) {
            tokens.push_back(token);
        }
        return tokens;
    }
    
    std::vector<std::string> extractIntents(const std::string& text) {
        std::vector<std::string> intents;
        
        // Simple intent detection based on keywords
        if (text.find("hello") != std::string::npos || 
            text.find("hi") != std::string::npos ||
            text.find("greet") != std::string::npos) {
            intents.push_back("greeting");
        }
        
        if (text.find("help") != std::string::npos ||
            text.find("assist") != std::string::npos) {
            intents.push_back("help_request");
        }
        
        if (text.find("?") != std::string::npos ||
            text.find("what") != std::string::npos ||
            text.find("how") != std::string::npos ||
            text.find("when") != std::string::npos ||
            text.find("where") != std::string::npos ||
            text.find("why") != std::string::npos) {
            intents.push_back("question");
        }
        
        if (intents.empty()) {
            intents.push_back("general");
        }
        
        return intents;
    }
};

/**
 * ResponseGenerator - Generates appropriate responses to human input
 */
class ResponseGenerator {
public:
    explicit ResponseGenerator(AtomSpacePtr atomspace, const CommunicationConfig& config) 
        : atomspace_(atomspace), config_(config) {}
    
    AgentResponse generateResponse(const ProcessedInput& input, 
                                 const InteractionContext& context,
                                 const std::string& session_id) {
        AgentResponse response;
        
        // Generate response based on detected intents
        if (!input.intents.empty()) {
            const std::string& primary_intent = input.intents[0];
            
            if (primary_intent == "greeting") {
                response.content = generateGreeting(context);
                response.confidence = 0.9;
            } else if (primary_intent == "help_request") {
                response.content = generateHelp(input, context);
                response.confidence = 0.8;
            } else if (primary_intent == "question") {
                response.content = generateAnswer(input, context);
                response.confidence = 0.7;
            } else {
                response.content = generateGeneral(input, context);
                response.confidence = 0.6;
            }
        } else {
            response.content = "I understand you're trying to communicate with me. Could you please rephrase that?";
            response.confidence = 0.3;
        }
        
        // Add suggestions based on context
        response.suggestions = generateSuggestions(input, context);
        
        return response;
    }
    
private:
    AtomSpacePtr atomspace_;
    const CommunicationConfig& config_;
    
    std::string generateGreeting(const InteractionContext& context) {
        std::vector<std::string> greetings = {
            "Hello! How can I assist you today?",
            "Hi there! What would you like to talk about?",
            "Greetings! I'm here to help with whatever you need.",
            "Hello! It's nice to meet you. How may I be of service?"
        };
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, greetings.size() - 1);
        
        return greetings[dis(gen)];
    }
    
    std::string generateHelp(const ProcessedInput& input, const InteractionContext& context) {
        return "I'm here to help! I can assist with conversations, answer questions, "
               "and help you explore various topics. What specific area would you like help with?";
    }
    
    std::string generateAnswer(const ProcessedInput& input, const InteractionContext& context) {
        // For now, provide a generic response
        // In a full implementation, this would query knowledge sources
        return "That's an interesting question. Based on what you've asked, let me think about this... "
               "Could you provide a bit more context so I can give you a more specific answer?";
    }
    
    std::string generateGeneral(const ProcessedInput& input, const InteractionContext& context) {
        return "I see what you're saying. Could you tell me more about what you're thinking, "
               "or perhaps ask a specific question I can help with?";
    }
    
    std::vector<std::string> generateSuggestions(const ProcessedInput& input, 
                                               const InteractionContext& context) {
        std::vector<std::string> suggestions;
        
        // Basic suggestions based on input type
        if (!input.intents.empty()) {
            const std::string& intent = input.intents[0];
            
            if (intent == "greeting") {
                suggestions = {
                    "Ask me a question about any topic",
                    "Tell me what you'd like to explore",
                    "Let me know if you need help with something"
                };
            } else if (intent == "question") {
                suggestions = {
                    "Ask a follow-up question",
                    "Request more details on this topic",
                    "Explore a related area"
                };
            }
        }
        
        return suggestions;
    }
};

/**
 * SessionManager - Manages interaction sessions and their lifecycle
 */
class SessionManager {
public:
    explicit SessionManager(const CommunicationConfig& config) : config_(config) {}
    
    std::string createSession(const std::string& user_id, const SessionConfig& session_config) {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        
        std::string session_id = generateSessionId(user_id);
        auto session = std::make_shared<SessionState>();
        session->session_id = session_id;
        session->user_id = user_id;
        session->config = session_config;
        
        sessions_[session_id] = session;
        
        return session_id;
    }
    
    std::shared_ptr<SessionState> getSession(const std::string& session_id) {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        auto it = sessions_.find(session_id);
        return (it != sessions_.end()) ? it->second : nullptr;
    }
    
    bool endSession(const std::string& session_id) {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        auto it = sessions_.find(session_id);
        if (it != sessions_.end()) {
            it->second->active = false;
            sessions_.erase(it);
            return true;
        }
        return false;
    }
    
    void updateSessionActivity(const std::string& session_id) {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        auto it = sessions_.find(session_id);
        if (it != sessions_.end()) {
            it->second->last_activity = std::chrono::system_clock::now();
            it->second->interaction_count++;
        }
    }
    
    size_t getActiveSessionCount() const {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        return sessions_.size();
    }
    
    void cleanupExpiredSessions() {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        auto now = std::chrono::system_clock::now();
        
        auto it = sessions_.begin();
        while (it != sessions_.end()) {
            auto idle_time = now - it->second->last_activity;
            if (idle_time > it->second->config.max_idle_time) {
                it = sessions_.erase(it);
            } else {
                ++it;
            }
        }
    }
    
private:
    const CommunicationConfig& config_;
    std::map<std::string, std::shared_ptr<SessionState>> sessions_;
    mutable std::mutex sessions_mutex_;
    
    std::string generateSessionId(const std::string& user_id) {
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(1000, 9999);
        
        std::ostringstream oss;
        oss << "session_" << user_id << "_" << timestamp << "_" << dis(gen);
        return oss.str();
    }
};

/**
 * ContextManager - Manages conversation context and state
 */
class ContextManager {
public:
    explicit ContextManager(AtomSpacePtr atomspace) : atomspace_(atomspace) {}
    
    void updateContext(const std::string& session_id, const ContextUpdate& update) {
        std::lock_guard<std::mutex> lock(contexts_mutex_);
        
        auto& context = contexts_[session_id];
        context.last_update = std::chrono::system_clock::now();
        
        switch (update.type) {
            case ContextUpdate::UpdateType::SET:
                context.variables[update.key] = update.value;
                break;
            case ContextUpdate::UpdateType::ADD:
                if (!update.key.empty() && !update.value.empty()) {
                    context.variables[update.key] = update.value;
                }
                for (const auto& pair : update.batch_updates) {
                    context.variables[pair.first] = pair.second;
                }
                break;
            case ContextUpdate::UpdateType::REMOVE:
                context.variables.erase(update.key);
                break;
            case ContextUpdate::UpdateType::CLEAR:
                context.variables.clear();
                context.active_topics.clear();
                break;
        }
    }
    
    InteractionContext getContext(const std::string& session_id) const {
        std::lock_guard<std::mutex> lock(contexts_mutex_);
        auto it = contexts_.find(session_id);
        return (it != contexts_.end()) ? it->second : InteractionContext{};
    }
    
    void clearContext(const std::string& session_id) {
        std::lock_guard<std::mutex> lock(contexts_mutex_);
        contexts_.erase(session_id);
    }
    
private:
    AtomSpacePtr atomspace_;
    std::map<std::string, InteractionContext> contexts_;
    mutable std::mutex contexts_mutex_;
};

/**
 * InteractionKnowledgeStore - Handles AtomSpace integration for interactions
 */
class InteractionKnowledgeStore {
public:
    explicit InteractionKnowledgeStore(AtomSpacePtr atomspace) : atomspace_(atomspace) {}
    
    Handle storeInteraction(const InteractionRecord& record) {
        // Create atoms representing the interaction
        // This is a simplified implementation
        
        Handle user_node = atomspace_->add_node(CONCEPT_NODE, "User:" + record.user_id);
        Handle session_node = atomspace_->add_node(CONCEPT_NODE, "Session:" + record.session_id);
        Handle input_node = atomspace_->add_node(CONCEPT_NODE, 
            "Input:" + record.input.processed.normalized_text);
        Handle response_node = atomspace_->add_node(CONCEPT_NODE,
            "Response:" + record.response.agent_response.content);
        
        // Create interaction link
        HandleSeq interaction_seq = {user_node, input_node, response_node};
        Handle interaction_link = atomspace_->add_link(LIST_LINK, interaction_seq);
        
        // Add timestamp and confidence as truth values
        auto tv = SimpleTruthValue::createTV(record.response.agent_response.confidence, 1.0);
        interaction_link->setTruthValue(tv);
        
        return interaction_link;
    }
    
    std::vector<Handle> retrievePatterns(const PatternQuery& query) {
        std::vector<Handle> results;
        // Simplified pattern retrieval - would use proper pattern matching in full implementation
        return results;
    }
    
    void updateKnowledge(const KnowledgeUpdate& update) {
        // Simplified knowledge update
        // In full implementation, this would handle complex knowledge graphs
    }
    
private:
    AtomSpacePtr atomspace_;
};

/**
 * InteractionAnalyzer - Provides analytics and performance monitoring
 */
class InteractionAnalyzer {
public:
    explicit InteractionAnalyzer() = default;
    
    void recordInteraction(const InteractionRecord& record) {
        std::lock_guard<std::mutex> lock(analytics_mutex_);
        
        analytics_.total_interactions++;
        
        // Update response time
        auto response_time = record.response.processing_time;
        total_response_time_ += response_time;
        analytics_.average_response_time = total_response_time_ / analytics_.total_interactions;
        
        // Update success rate
        if (record.response.success) {
            successful_interactions_++;
        }
        analytics_.success_rate = static_cast<double>(successful_interactions_) / analytics_.total_interactions;
        
        // Update confidence
        total_confidence_ += record.response.agent_response.confidence;
        analytics_.average_confidence = total_confidence_ / analytics_.total_interactions;
        
        // Update input type counts
        analytics_.input_type_counts[record.input.raw.type]++;
        
        // Update intent counts
        for (const auto& intent : record.input.processed.intents) {
            analytics_.intent_counts[intent]++;
        }
    }
    
    InteractionAnalytics getAnalytics() const {
        std::lock_guard<std::mutex> lock(analytics_mutex_);
        return analytics_;
    }
    
private:
    InteractionAnalytics analytics_;
    Duration total_response_time_{Duration::zero()};
    size_t successful_interactions_{0};
    double total_confidence_{0.0};
    mutable std::mutex analytics_mutex_;
};

// === HumanInterface Implementation ===

HumanInterface::HumanInterface(AtomSpacePtr atomspace, const CommunicationConfig& config)
    : atomspace_(atomspace), config_(config) {
    
    // Initialize components
    input_processor_ = std::make_unique<InputProcessor>(config_);
    response_generator_ = std::make_unique<ResponseGenerator>(atomspace_, config_);
    session_manager_ = std::make_unique<SessionManager>(config_);
    context_manager_ = std::make_unique<ContextManager>(atomspace_);
    knowledge_store_ = std::make_unique<InteractionKnowledgeStore>(atomspace_);
    analyzer_ = std::make_unique<InteractionAnalyzer>();
}

HumanInterface::~HumanInterface() {
    shutdown();
}

bool HumanInterface::initialize() {
    std::lock_guard<std::mutex> lock(interface_mutex_);
    
    if (initialized_) {
        return true;
    }
    
    try {
        // Initialize default processors and formatters
        initializeDefaultProcessors();
        
        // Verify AtomSpace is accessible
        if (!atomspace_) {
            logger().error("HumanInterface: AtomSpace is not available");
            return false;
        }
        
        initialized_ = true;
        shutdown_requested_ = false;
        
        logger().info("HumanInterface: Successfully initialized");
        return true;
    } catch (const std::exception& e) {
        logger().error("HumanInterface: Initialization failed: %s", e.what());
        return false;
    }
}

void HumanInterface::shutdown() {
    std::lock_guard<std::mutex> lock(interface_mutex_);
    
    if (!initialized_ || shutdown_requested_) {
        return;
    }
    
    shutdown_requested_ = true;
    
    // Clean up all sessions
    {
        std::lock_guard<std::mutex> sessions_lock(sessions_mutex_);
        active_sessions_.clear();
    }
    
    logger().info("HumanInterface: Shutdown complete");
}

InteractionResponse HumanInterface::processInput(const HumanInput& input, 
                                                const std::string& session_id) {
    if (!isReady()) {
        InteractionResponse response;
        response.success = false;
        response.error_message = "HumanInterface not ready";
        return response;
    }
    
    return processInputInternal(input, session_id);
}

InteractionResponse HumanInterface::processInputInternal(const HumanInput& input,
                                                        const std::string& session_id) {
    auto start_time = std::chrono::system_clock::now();
    InteractionResponse response;
    response.session_id = session_id;
    
    try {
        // Validate session
        if (!validateSession(session_id)) {
            throw SessionNotFoundException(session_id);
        }
        
        // Update session activity
        session_manager_->updateSessionActivity(session_id);
        
        // Get current context
        auto context = context_manager_->getContext(session_id);
        
        // Process input if not already processed
        ProcessedInput processed_input = input.processed;
        if (processed_input.normalized_text.empty() && !input.raw.content.empty()) {
            processed_input = input_processor_->process(input.raw);
        }
        
        // Generate response
        response.agent_response = response_generator_->generateResponse(
            processed_input, context, session_id);
        
        // Format response
        response.formatted_response.format = config_.default_output_format;
        response.formatted_response.formatted_content = response.agent_response.content;
        
        // Update context based on interaction
        ContextUpdate context_update(ContextUpdate::UpdateType::ADD, 
            "last_input", processed_input.normalized_text);
        context_manager_->updateContext(session_id, context_update);
        
        response.success = true;
        
    } catch (const std::exception& e) {
        response.success = false;
        response.error_message = e.what();
        logger().error("HumanInterface: Processing error: %s", e.what());
    }
    
    // Calculate processing time
    auto end_time = std::chrono::system_clock::now();
    response.response_timestamp = end_time;
    response.processing_time = std::chrono::duration_cast<Duration>(end_time - start_time);
    
    // Log and analyze interaction
    if (response.success) {
        HumanInput complete_input = input;
        complete_input.processed = input.processed;
        if (complete_input.processed.normalized_text.empty()) {
            complete_input.processed = input_processor_->process(input.raw);
        }
        
        InteractionRecord record;
        record.session_id = session_id;
        record.user_id = complete_input.user_id;
        record.input = complete_input;
        record.response = response;
        record.timestamp = start_time;
        
        analyzer_->recordInteraction(record);
        logInteraction(session_id, complete_input, response);
        
        // Store in AtomSpace if enabled
        if (config_.store_interactions_in_atomspace) {
            knowledge_store_->storeInteraction(record);
        }
    }
    
    return response;
}

std::string HumanInterface::startSession(const std::string& user_id,
                                        const SessionConfig& session_config) {
    if (!isReady()) {
        throw CommunicationException("HumanInterface not ready");
    }
    
    return session_manager_->createSession(user_id, session_config);
}

bool HumanInterface::endSession(const std::string& session_id) {
    if (!isReady()) {
        return false;
    }
    
    // Clear context for session
    context_manager_->clearContext(session_id);
    
    // End session in manager
    return session_manager_->endSession(session_id);
}

std::shared_ptr<SessionState> HumanInterface::getSessionState(const std::string& session_id) const {
    return session_manager_->getSession(session_id);
}

void HumanInterface::updateContext(const std::string& session_id, const ContextUpdate& context_update) {
    if (!validateSession(session_id)) {
        throw SessionNotFoundException(session_id);
    }
    
    context_manager_->updateContext(session_id, context_update);
}

InteractionContext HumanInterface::getContext(const std::string& session_id) const {
    return context_manager_->getContext(session_id);
}

void HumanInterface::clearContext(const std::string& session_id) {
    context_manager_->clearContext(session_id);
}

std::vector<InteractionRecord> HumanInterface::getHistory(const std::string& session_id, 
                                                        size_t max_interactions) const {
    // Simplified implementation - in full version would maintain history per session
    return std::vector<InteractionRecord>();
}

InteractionAnalytics HumanInterface::getAnalytics(const std::string& session_id) const {
    return analyzer_->getAnalytics();
}

void HumanInterface::updateConfig(const CommunicationConfig& new_config) {
    std::lock_guard<std::mutex> lock(interface_mutex_);
    config_ = new_config;
}

void HumanInterface::registerInputProcessor(InputType input_type,
                                          std::function<ProcessedInput(const RawInput&)> processor) {
    std::lock_guard<std::mutex> lock(processors_mutex_);
    input_processors_[input_type] = processor;
}

void HumanInterface::registerResponseFormatter(OutputFormat output_format,
                                              std::function<FormattedResponse(const AgentResponse&)> formatter) {
    std::lock_guard<std::mutex> lock(processors_mutex_);
    response_formatters_[output_format] = formatter;
}

Handle HumanInterface::storeInteractionInAtomSpace(const InteractionRecord& interaction_record) {
    return knowledge_store_->storeInteraction(interaction_record);
}

std::vector<Handle> HumanInterface::retrieveInteractionPatterns(const PatternQuery& query_params) const {
    return knowledge_store_->retrievePatterns(query_params);
}

void HumanInterface::updateInteractionKnowledge(const KnowledgeUpdate& knowledge_update) {
    knowledge_store_->updateKnowledge(knowledge_update);
}

SystemStatus HumanInterface::getStatus() const {
    SystemStatus status;
    status.healthy = isReady();
    status.active_sessions = getActiveSessionCount();
    status.status_timestamp = std::chrono::system_clock::now();
    
    auto analytics = analyzer_->getAnalytics();
    status.total_interactions = analytics.total_interactions;
    status.average_response_time_ms = static_cast<double>(analytics.average_response_time.count());
    status.success_rate = analytics.success_rate;
    
    status.component_status["input_processor"] = input_processor_ ? "ready" : "not_ready";
    status.component_status["response_generator"] = response_generator_ ? "ready" : "not_ready";
    status.component_status["session_manager"] = session_manager_ ? "ready" : "not_ready";
    status.component_status["context_manager"] = context_manager_ ? "ready" : "not_ready";
    status.component_status["atomspace"] = atomspace_ ? "ready" : "not_ready";
    
    return status;
}

size_t HumanInterface::getActiveSessionCount() const {
    return session_manager_->getActiveSessionCount();
}

// Private helper methods

std::string HumanInterface::generateSessionId(const std::string& user_id) const {
    // Delegate to session manager
    return session_manager_->createSession(user_id, SessionConfig{});
}

bool HumanInterface::validateSession(const std::string& session_id) const {
    return session_manager_->getSession(session_id) != nullptr;
}

void HumanInterface::cleanupExpiredSessions() {
    session_manager_->cleanupExpiredSessions();
}

void HumanInterface::initializeDefaultProcessors() {
    // Initialize default input processors
    registerInputProcessor(InputType::TEXT, [this](const RawInput& raw) {
        return input_processor_->process(raw);
    });
    
    // Initialize default response formatters
    registerResponseFormatter(OutputFormat::PLAIN_TEXT, [](const AgentResponse& response) {
        FormattedResponse formatted;
        formatted.format = OutputFormat::PLAIN_TEXT;
        formatted.formatted_content = response.content;
        return formatted;
    });
}

void HumanInterface::logInteraction(const std::string& session_id,
                                   const HumanInput& input,
                                   const InteractionResponse& response) const {
    if (config_.enable_detailed_logging) {
        logger().debug("HumanInterface: Session %s - Input: '%s' -> Response: '%s' (%.1fms)",
                      session_id.c_str(),
                      input.processed.normalized_text.c_str(),
                      response.agent_response.content.c_str(),
                      static_cast<double>(response.processing_time.count()));
    }
}

// === Utility Functions Implementation ===

std::string generateUniqueId(const std::string& prefix) {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
        now.time_since_epoch()).count();
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(100000, 999999);
    
    std::ostringstream oss;
    if (!prefix.empty()) {
        oss << prefix << "_";
    }
    oss << timestamp << "_" << dis(gen);
    return oss.str();
}

} // namespace communication
} // namespace agentzero
} // namespace opencog

/*
 * HumanInterface.cpp (agentzero::communication)
 *
 * Copyright (C) 2024 Agent-Zero-Genesis Project
 * 
 * Human-agent interaction layer for Agent-Zero
 */

#include "agentzero/communication/HumanInterface.h"
#include "agentzero/communication/DialogueManager.h"

#include <algorithm>
#include <cctype>

namespace agentzero {
namespace communication {

// -----------------------------------------------------------------------
// Construction / destruction
// -----------------------------------------------------------------------

HumanInterface::HumanInterface(opencog::AtomSpacePtr atomspace,
                                const std::string& agent_name)
    : _atomspace(atomspace)
    , _agent_name(agent_name)
    , _dialogue(std::make_unique<DialogueManager>(atomspace))
{
}

HumanInterface::~HumanInterface() = default;

// -----------------------------------------------------------------------
// Public interface
// -----------------------------------------------------------------------

std::string HumanInterface::process_human_input(const std::string& input)
{
    std::string raw = _dialogue->process_dialogue(trim_string(input));
    if (raw.empty()) {
        return "[" + _agent_name + "]: Please provide some input.";
    }
    return format_response(raw);
}

std::string HumanInterface::generate_response(const std::string& context)
{
    if (context.empty()) {
        return "[" + _agent_name + "]: Ready to assist you.";
    }
    // Use the dialogue manager to generate a context-aware reply but
    // without permanently adding it to the conversation history.
    std::string raw = _dialogue->process_dialogue(context);
    return format_response(raw);
}

void HumanInterface::reset_session()
{
    _dialogue->reset_context();
}

// -----------------------------------------------------------------------
// Private helpers
// -----------------------------------------------------------------------

std::string HumanInterface::trim_string(const std::string& s) const
{
    auto begin = std::find_if(s.begin(), s.end(),
                              [](unsigned char c){ return !std::isspace(c); });
    if (begin == s.end()) return "";
    auto end = std::find_if(s.rbegin(), s.rend(),
                            [](unsigned char c){ return !std::isspace(c); }).base();
    return std::string(begin, end);
}

std::string HumanInterface::format_response(const std::string& raw_response) const
{
    return "[" + _agent_name + "]: " + raw_response;
}

} // namespace communication
} // namespace agentzero
