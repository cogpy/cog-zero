/*
 * opencog/agentzero/communication/DialogueManager.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * DialogueManager — multi-turn conversation management (Phase 6 / AZ-NLP-002)
 */

#include <algorithm>
#include <random>
#include <sstream>
#include <stdexcept>

#include <opencog/atoms/atom_types/types.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>
#include <opencog/atoms/truthvalue/SimpleTruthValue.h>
#include <opencog/util/Logger.h>

#include "opencog/agentzero/communication/DialogueManager.h"

using namespace opencog;
using namespace opencog::agentzero::communication;

DialogueManager::DialogueManager(AtomSpacePtr atomspace, const std::string& agent_id)
    : _atomspace(atomspace)
    , _agent_id(agent_id)
{
    if (!_atomspace)
        throw std::runtime_error("DialogueManager requires a valid AtomSpace");

    _language_processor = std::make_unique<LanguageProcessor>(_atomspace);
    initializeAtoms();
    logger().info() << "[DialogueManager] initialized agent_id=" << _agent_id;
}

DialogueManager::~DialogueManager()
{
    std::lock_guard<std::mutex> lock(_mutex);
    for (auto& kv : _conversations)
        kv.second.active = false;
    _conversations.clear();
}

void DialogueManager::initializeAtoms()
{
    _agent_self = _atomspace->add_node(CONCEPT_NODE, "agent:" + _agent_id);
    _agent_self->setTruthValue(SimpleTruthValue::createTV(1.0, 1.0));
    _dialogue_root = _atomspace->add_node(CONCEPT_NODE, "DialogueManagerRoot");
    _atomspace->add_link(MEMBER_LINK, _agent_self, _dialogue_root);
}

DialogueTurn DialogueManager::makeTurn(const std::string& speaker,
                                       const std::string& content,
                                       const std::string& intent)
{
    DialogueTurn turn;
    static thread_local std::mt19937 gen{std::random_device{}()};
    std::uniform_int_distribution<int> dis(1000, 9999);
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  now.time_since_epoch())
                  .count();
    turn.id = "turn_" + std::to_string(ms) + "_" + std::to_string(dis(gen));
    turn.speaker_id = speaker;
    turn.content = content;
    turn.intent = intent;
    turn.timestamp = now;
    return turn;
}

Handle DialogueManager::storeTurnAtom(const std::string& conversation_id,
                                      const DialogueTurn& turn)
{
    Handle conv = _atomspace->add_node(CONCEPT_NODE, "conversation:" + conversation_id);
    Handle utter = _atomspace->add_node(CONCEPT_NODE, "turn:" + turn.id);
    Handle speaker = _atomspace->add_node(CONCEPT_NODE, "speaker:" + turn.speaker_id);
    Handle content = _atomspace->add_node(CONCEPT_NODE, "content:" + turn.content);

    Handle said = _atomspace->add_node(PREDICATE_NODE, "said");
    _atomspace->add_link(EVALUATION_LINK, said,
                         _atomspace->add_link(LIST_LINK, speaker, content));
    _atomspace->add_link(MEMBER_LINK, utter, conv);
    if (!turn.intent.empty()) {
        Handle intent = _atomspace->add_node(CONCEPT_NODE, "intent:" + turn.intent);
        Handle has_intent = _atomspace->add_node(PREDICATE_NODE, "turn-intent");
        _atomspace->add_link(EVALUATION_LINK, has_intent,
                             _atomspace->add_link(LIST_LINK, utter, intent));
    }
    return utter;
}

void DialogueManager::trimHistory(Conversation& conv)
{
    if (conv.history.size() <= _max_history) return;
    conv.history.erase(conv.history.begin(),
                       conv.history.begin() + static_cast<long>(conv.history.size() - _max_history));
}

Conversation* DialogueManager::getOrCreate(const std::string& conversation_id,
                                           const std::string& sender_id)
{
    auto it = _conversations.find(conversation_id);
    if (it != _conversations.end() && it->second.active)
        return &it->second;

    Conversation conv;
    conv.id = conversation_id;
    conv.participants = {_agent_id};
    if (!sender_id.empty() && sender_id != _agent_id)
        conv.participants.push_back(sender_id);
    conv.last_activity = std::chrono::system_clock::now();
    conv.active = true;

    Handle conv_node = _atomspace->add_node(CONCEPT_NODE, "conversation:" + conversation_id);
    _atomspace->add_link(MEMBER_LINK, conv_node, _dialogue_root);

    auto [ins, _] = _conversations.emplace(conversation_id, std::move(conv));
    return &ins->second;
}

bool DialogueManager::startConversation(const std::string& conversation_id,
                                        const std::vector<std::string>& participants)
{
    if (conversation_id.empty()) return false;
    std::lock_guard<std::mutex> lock(_mutex);
    if (_conversations.count(conversation_id) && _conversations[conversation_id].active)
        return false;

    Conversation conv;
    conv.id = conversation_id;
    conv.participants = participants;
    if (std::find(conv.participants.begin(), conv.participants.end(), _agent_id) ==
        conv.participants.end()) {
        conv.participants.push_back(_agent_id);
    }
    conv.last_activity = std::chrono::system_clock::now();
    conv.active = true;

    Handle conv_node = _atomspace->add_node(CONCEPT_NODE, "conversation:" + conversation_id);
    _atomspace->add_link(MEMBER_LINK, conv_node, _dialogue_root);

    _conversations[conversation_id] = std::move(conv);
    logger().info() << "[DialogueManager] started conversation " << conversation_id;
    return true;
}

bool DialogueManager::endConversation(const std::string& conversation_id)
{
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _conversations.find(conversation_id);
    if (it == _conversations.end()) return false;
    it->second.active = false;
    logger().info() << "[DialogueManager] ended conversation " << conversation_id;
    return true;
}

bool DialogueManager::isConversationActive(const std::string& conversation_id) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _conversations.find(conversation_id);
    return it != _conversations.end() && it->second.active;
}

std::string DialogueManager::buildContextString(const Conversation& conv) const
{
    std::ostringstream oss;
    if (!conv.topic.empty())
        oss << "topic:" << conv.topic;
    for (const auto& kv : conv.context) {
        if (oss.tellp() > 0) oss << ";";
        oss << kv.first << "=" << kv.second;
    }
    // Include a short recent-history hint
    if (!conv.history.empty()) {
        const auto& last = conv.history.back();
        if (oss.tellp() > 0) oss << ";";
        oss << "last_speaker=" << last.speaker_id;
    }
    return oss.str();
}

std::string DialogueManager::processMessage(const std::string& conversation_id,
                                            const std::string& sender_id,
                                            const std::string& message_content)
{
    if (conversation_id.empty() || message_content.empty())
        return "";

    std::lock_guard<std::mutex> lock(_mutex);
    Conversation* conv = getOrCreate(conversation_id, sender_id);
    if (!conv || !conv->active) return "";

    ParseResult parsed = _language_processor->parseText(message_content);

    DialogueTurn user_turn = makeTurn(sender_id, message_content, parsed.intent);
    user_turn.atom = storeTurnAtom(conversation_id, user_turn);
    conv->history.push_back(user_turn);

    // Promote first entity to topic if none set
    if (conv->topic.empty() && !parsed.detected_entities.empty())
        conv->topic = parsed.detected_entities.front();

    conv->context["last_intent"] = parsed.intent;
    conv->last_activity = std::chrono::system_clock::now();

    std::string ctx = buildContextString(*conv);
    std::string reply = _language_processor->generateResponse(message_content, ctx);

    DialogueTurn agent_turn = makeTurn(_agent_id, reply, "response");
    agent_turn.atom = storeTurnAtom(conversation_id, agent_turn);
    conv->history.push_back(agent_turn);
    trimHistory(*conv);

    return reply;
}

bool DialogueManager::sendMessage(const std::string& conversation_id,
                                  const std::string& /*recipient_id*/,
                                  const std::string& message_content)
{
    if (conversation_id.empty() || message_content.empty()) return false;
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _conversations.find(conversation_id);
    if (it == _conversations.end() || !it->second.active) return false;

    DialogueTurn turn = makeTurn(_agent_id, message_content, "outbound");
    turn.atom = storeTurnAtom(conversation_id, turn);
    it->second.history.push_back(turn);
    it->second.last_activity = std::chrono::system_clock::now();
    trimHistory(it->second);
    return true;
}

void DialogueManager::setConversationContext(const std::string& conversation_id,
                                             const std::string& key,
                                             const std::string& value)
{
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _conversations.find(conversation_id);
    if (it == _conversations.end()) return;
    it->second.context[key] = value;
    it->second.last_activity = std::chrono::system_clock::now();
}

std::string DialogueManager::getConversationContext(const std::string& conversation_id,
                                                    const std::string& key) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _conversations.find(conversation_id);
    if (it == _conversations.end()) return "";
    auto cit = it->second.context.find(key);
    if (cit == it->second.context.end()) return "";
    return cit->second;
}

void DialogueManager::setConversationTopic(const std::string& conversation_id,
                                           const std::string& topic)
{
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _conversations.find(conversation_id);
    if (it == _conversations.end()) return;
    it->second.topic = topic;
    it->second.last_activity = std::chrono::system_clock::now();
}

std::string DialogueManager::getConversationTopic(const std::string& conversation_id) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _conversations.find(conversation_id);
    if (it == _conversations.end()) return "";
    return it->second.topic;
}

void DialogueManager::addConversationGoal(const std::string& conversation_id,
                                          const Handle& goal_atom)
{
    if (goal_atom == Handle::UNDEFINED) return;
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _conversations.find(conversation_id);
    if (it == _conversations.end()) return;
    it->second.goals.push_back(goal_atom);

    Handle conv = _atomspace->add_node(CONCEPT_NODE, "conversation:" + conversation_id);
    Handle pred = _atomspace->add_node(PREDICATE_NODE, "conversation-goal");
    _atomspace->add_link(EVALUATION_LINK, pred,
                         _atomspace->add_link(LIST_LINK, conv, goal_atom));
}

std::vector<Handle> DialogueManager::getConversationGoals(const std::string& conversation_id) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _conversations.find(conversation_id);
    if (it == _conversations.end()) return {};
    return it->second.goals;
}

std::vector<DialogueTurn> DialogueManager::getHistory(const std::string& conversation_id,
                                                      size_t limit) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _conversations.find(conversation_id);
    if (it == _conversations.end()) return {};
    const auto& hist = it->second.history;
    if (limit == 0 || limit >= hist.size()) return hist;
    return std::vector<DialogueTurn>(hist.end() - static_cast<long>(limit), hist.end());
}

std::vector<std::string> DialogueManager::getActiveConversations() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    std::vector<std::string> ids;
    for (const auto& kv : _conversations) {
        if (kv.second.active) ids.push_back(kv.first);
    }
    return ids;
}

std::vector<std::string> DialogueManager::getParticipants(const std::string& conversation_id) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _conversations.find(conversation_id);
    if (it == _conversations.end()) return {};
    return it->second.participants;
}

void DialogueManager::setMaxHistory(size_t max_history)
{
    std::lock_guard<std::mutex> lock(_mutex);
    _max_history = std::max<size_t>(1, max_history);
}

void DialogueManager::setTimeout(std::chrono::minutes timeout)
{
    std::lock_guard<std::mutex> lock(_mutex);
    _timeout = timeout;
}

size_t DialogueManager::getActiveCount() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    size_t n = 0;
    for (const auto& kv : _conversations)
        if (kv.second.active) ++n;
    return n;
}

size_t DialogueManager::cleanupInactiveConversations()
{
    std::lock_guard<std::mutex> lock(_mutex);
    auto now = std::chrono::system_clock::now();
    size_t closed = 0;
    for (auto& kv : _conversations) {
        if (!kv.second.active) continue;
        if (now - kv.second.last_activity > _timeout) {
            kv.second.active = false;
            ++closed;
        }
    }
    return closed;
}
