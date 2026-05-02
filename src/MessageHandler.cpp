/*
 * src/MessageHandler.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * MessageHandler Implementation
 * Handles message routing and processing with AtomSpace integration
 * Part of the AGENT-ZERO-GENESIS project Phase 6: Communication & NLP
 */

#include <sstream>
#include <algorithm>

#include <opencog/atoms/atom_types/types.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>
#include <opencog/atoms/truthvalue/SimpleTruthValue.h>
#include <opencog/util/Logger.h>

#include "opencog/agentzero/MessageHandler.h"
#include "opencog/agentzero/DialogueManager.h" // For Message struct

using namespace opencog;
using namespace opencog::agentzero;
using opencog::HandleSeq;

MessageHandler::MessageHandler(AtomSpacePtr atomspace)
    : _atomspace(atomspace)
{
    logger().debug("MessageHandler initialized");
}

MessageHandler::~MessageHandler() {
    logger().debug("MessageHandler destroyed");
}

bool MessageHandler::processMessage(const Message& message) {
    if (!validateMessage(message)) {
        logger().error("Invalid message received: %s", message.id.c_str());
        return false;
    }
    
    try {
        // Create AtomSpace representation
        Handle message_atom = messageToAtom(message);
        
        // Call registered callbacks
        for (const auto& callback : _message_callbacks) {
            try {
                callback(message);
            } catch (const std::exception& e) {
                logger().error("Message callback error: %s", e.what());
            }
        }
        
        logger().debug("Processed message %s successfully", message.id.c_str());
        return true;
        
    } catch (const std::exception& e) {
        logger().error("Error processing message %s: %s", message.id.c_str(), e.what());
        return false;
    }
}

Handle MessageHandler::messageToAtom(const Message& message) {
    // Create main message atom
    Handle message_atom = _atomspace->add_node(CONCEPT_NODE, std::string("Message:" + message.id));
    message_atom->setTruthValue(SimpleTruthValue::createTV(1.0, 1.0));
    
    // Create component atoms
    Handle sender_atom = _atomspace->add_node(CONCEPT_NODE, std::string(message.sender_id));
    Handle recipient_atom = _atomspace->add_node(CONCEPT_NODE, std::string(message.recipient_id));
    Handle content_atom = _atomspace->add_node(CONCEPT_NODE, std::string("Content:" + message.content));
    
    // Create timestamp atom
    auto timestamp = std::chrono::system_clock::to_time_t(message.timestamp);
    Handle timestamp_atom = _atomspace->add_node(NUMBER_NODE, std::string(std::to_stringtimestamp))
    
    // Create relationships
    _atomspace->add_link(EVALUATION_LINK, HandleSeq{
        _atomspace->add_node(PREDICATE_NODE, std::string("sentBy"),
        _atomspace->add_link(LIST_LINK, HandleSeq{message_atom, sender_atom})
    });
    
    _atomspace->add_link(EVALUATION_LINK, HandleSeq{
        _atomspace->add_node(PREDICATE_NODE, std::string("sentTo"),
        _atomspace->add_link(LIST_LINK, HandleSeq{message_atom, recipient_atom})
    });
    
    _atomspace->add_link(EVALUATION_LINK, HandleSeq{
        _atomspace->add_node(PREDICATE_NODE, std::string("hasContent"),
        _atomspace->add_link(LIST_LINK, HandleSeq{message_atom, content_atom})
    });
    
    _atomspace->add_link(EVALUATION_LINK, HandleSeq{
        _atomspace->add_node(PREDICATE_NODE, std::string("atTime"),
        _atomspace->add_link(LIST_LINK, HandleSeq{message_atom, timestamp_atom})
    });
    
    // Add metadata if present
    for (const auto& [key, value] : message.metadata) {
        Handle key_atom = _atomspace->add_node(CONCEPT_NODE, std::string("MetaKey:" + key));
        Handle value_atom = _atomspace->add_node(CONCEPT_NODE, std::string("MetaValue:" + value));
        
        _atomspace->add_link(EVALUATION_LINK, HandleSeq{
            _atomspace->add_node(PREDICATE_NODE, std::string("hasMetadata"),
            _atomspace->add_link(LIST_LINK, HandleSeq{message_atom, key_atom, value_atom})
        });
    }
    
    return message_atom;
}

Message MessageHandler::atomToMessage(const Handle& message_atom) {
    if (message_atom == Handle::UNDEFINED) {
        throw std::invalid_argument("Invalid message atom handle");
    }
    
    std::string message_name = message_atom->get_name();
    
    // Extract message ID from atom name
    std::string message_id;
    if (message_name.find("Message:") == 0) {
        message_id = message_name.substr(8);
    } else {
        message_id = "unknown_id";
    }
    
    // Initialize message with default values
    std::string sender_id = "unknown";
    std::string recipient_id = "unknown";
    std::string content = "";
    std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::now();
    
    // Query AtomSpace for message properties
    HandleSeq incoming = message_atom->getIncomingSet();
    
    for (const Handle& link : incoming) {
        if (link->get_type() == EVALUATION_LINK) {
            HandleSeq outgoing = link->getOutgoingSet();
            if (outgoing.size() >= 2) {
                Handle predicate = outgoing[0];
                Handle args = outgoing[1];
                
                std::string pred_name = predicate->get_name();
                
                if (pred_name == "sentBy" && args->get_type() == LIST_LINK) {
                    HandleSeq args_out = args->getOutgoingSet();
                    if (args_out.size() >= 2 && args_out[0] == message_atom) {
                        sender_id = args_out[1]->get_name();
                    }
                } else if (pred_name == "sentTo" && args->get_type() == LIST_LINK) {
                    HandleSeq args_out = args->getOutgoingSet();
                    if (args_out.size() >= 2 && args_out[0] == message_atom) {
                        recipient_id = args_out[1]->get_name();
                    }
                } else if (pred_name == "hasContent" && args->get_type() == LIST_LINK) {
                    HandleSeq args_out = args->getOutgoingSet();
                    if (args_out.size() >= 2 && args_out[0] == message_atom) {
                        std::string content_name = args_out[1]->get_name();
                        if (content_name.find("Content:") == 0) {
                            content = content_name.substr(8);
                        }
                    }
                } else if (pred_name == "atTime" && args->get_type() == LIST_LINK) {
                    HandleSeq args_out = args->getOutgoingSet();
                    if (args_out.size() >= 2 && args_out[0] == message_atom) {
                        try {
                            std::time_t time_val = std::stoll(args_out[1]->get_name());
                            timestamp = std::chrono::system_clock::from_time_t(time_val);
                        } catch (...) {
                            // Use current time if parsing fails
                        }
                    }
                }
            }
        }
    }
    
    // Create message object
    Message message(sender_id, recipient_id, content);
    message.id = message_id;
    message.timestamp = timestamp;
    
    return message;
}

void MessageHandler::addMessageCallback(MessageCallback callback) {
    _message_callbacks.push_back(callback);
    logger().debug("Added message callback, total callbacks: %zu", _message_callbacks.size());
}

void MessageHandler::removeAllCallbacks() {
    _message_callbacks.clear();
    logger().debug("Removed all message callbacks");
}

bool MessageHandler::validateMessage(const Message& message) const {
    // Check required fields
    if (message.id.empty()) {
        logger().error("Message validation failed: empty ID");
        return false;
    }
    
    if (message.sender_id.empty()) {
        logger().error("Message validation failed: empty sender ID");
        return false;
    }
    
    if (message.recipient_id.empty()) {
        logger().error("Message validation failed: empty recipient ID");
        return false;
    }
    
    if (message.content.empty()) {
        logger().warn("Message validation warning: empty content for message %s", message.id.c_str());
        // Don't fail validation for empty content, just warn
    }
    
    // Check timestamp is not too far in the future
    auto now = std::chrono::system_clock::now();
    auto future_threshold = now + std::chrono::hours(1);
    
    if (message.timestamp > future_threshold) {
        logger().error("Message validation failed: timestamp too far in future");
        return false;
    }
    
    // Check content length (prevent extremely large messages)
    const size_t MAX_CONTENT_LENGTH = 100000; // 100KB
    if (message.content.length() > MAX_CONTENT_LENGTH) {
        logger().error("Message validation failed: content too long (%zu chars)", 
                      message.content.length());
        return false;
    }
    
    return true;
}

std::vector<Handle> MessageHandler::findMessagesByParticipant(const std::string& participant_id) {
    std::vector<Handle> results;
    
    if (participant_id.empty()) {
        return results;
    }
    
    // Create participant atom to search for
    Handle participant_atom = _atomspace->add_node(CONCEPT_NODE, std::string(participant_id));
    
    // Find all evaluation links involving this participant
    HandleSeq incoming = participant_atom->getIncomingSet();
    
    for (const Handle& link : incoming) {
        if (link->get_type() == LIST_LINK) {
            // Check if this list link is part of an evaluation about sending/receiving
            HandleSeq list_incoming = link->getIncomingSet();
            
            for (const Handle& eval_link : list_incoming) {
                if (eval_link->get_type() == EVALUATION_LINK) {
                    HandleSeq eval_outgoing = eval_link->getOutgoingSet();
                    if (eval_outgoing.size() >= 2) {
                        Handle predicate = eval_outgoing[0];
                        Handle args = eval_outgoing[1];
                        
                        std::string pred_name = predicate->get_name();
                        if ((pred_name == "sentBy" || pred_name == "sentTo") && 
                            args == link && args->get_type() == LIST_LINK) {
                            
                            // Extract message atom
                            HandleSeq args_out = args->getOutgoingSet();
                            if (args_out.size() >= 2) {
                                Handle message_atom = args_out[0];
                                std::string msg_name = message_atom->get_name();
                                
                                if (msg_name.find("Message:") == 0) {
                                    // Avoid duplicates
                                    if (std::find(results.begin(), results.end(), message_atom) == results.end()) {
                                        results.push_back(message_atom);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    return results;
}

std::vector<Handle> MessageHandler::findMessagesByTimeRange(
    const std::chrono::system_clock::time_point& start,
    const std::chrono::system_clock::time_point& end) {
    
    std::vector<Handle> results;
    
    std::time_t start_time = std::chrono::system_clock::to_time_t(start);
    std::time_t end_time = std::chrono::system_clock::to_time_t(end);
    
    // Find all messages and check their timestamps
    // This is a simple implementation - could be optimized with indexing
    
    HandleSeq all_atoms = _atomspace->get_handles_by_type(CONCEPT_NODE);
    
    for (const Handle& atom : all_atoms) {
        std::string atom_name = atom->get_name();
        
        if (atom_name.find("Message:") == 0) {
            // This is a message atom, check its timestamp
            HandleSeq incoming = atom->getIncomingSet();
            
            for (const Handle& link : incoming) {
                if (link->get_type() == EVALUATION_LINK) {
                    HandleSeq outgoing = link->getOutgoingSet();
                    if (outgoing.size() >= 2) {
                        Handle predicate = outgoing[0];
                        Handle args = outgoing[1];
                        
                        if (predicate->get_name() == "atTime" && args->get_type() == LIST_LINK) {
                            HandleSeq args_out = args->getOutgoingSet();
                            if (args_out.size() >= 2 && args_out[0] == atom) {
                                try {
                                    std::time_t msg_time = std::stoll(args_out[1]->get_name());
                                    
                                    if (msg_time >= start_time && msg_time <= end_time) {
                                        results.push_back(atom);
                                    }
                                } catch (...) {
                                    // Skip messages with invalid timestamps
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Sort by timestamp
    std::sort(results.begin(), results.end(), [this](const Handle& a, const Handle& b) {
        try {
            Message msg_a = this->atomToMessage(a);
            Message msg_b = this->atomToMessage(b);
            return msg_a.timestamp < msg_b.timestamp;
        } catch (...) {
            return false;
        }
    });
    
    return results;
}