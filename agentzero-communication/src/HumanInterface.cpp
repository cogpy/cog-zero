/*
 * opencog/agentzero/communication/HumanInterface.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * HumanInterface — user-facing dialogue layer (Phase 6 / AZ-HUMAN-001)
 */

#include <cctype>
#include <chrono>
#include <random>
#include <sstream>
#include <stdexcept>

#include <opencog/atoms/atom_types/types.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/base/Link.h>
#include <opencog/atoms/truthvalue/SimpleTruthValue.h>
#include <opencog/util/Logger.h>

#include "opencog/agentzero/communication/HumanInterface.h"

using namespace opencog;
using namespace opencog::agentzero::communication;

HumanInterface::HumanInterface(AtomSpacePtr atomspace, const std::string& agent_name)
    : _atomspace(atomspace)
    , _agent_name(agent_name)
{
    if (!_atomspace)
        throw std::runtime_error("HumanInterface requires a valid AtomSpace");

    _language = std::make_unique<LanguageProcessor>(_atomspace);
    _dialogue = std::make_unique<DialogueManager>(_atomspace, agent_name);
    initializeAtoms();
    logger().info() << "[HumanInterface] ready agent_name=" << _agent_name;
}

HumanInterface::~HumanInterface()
{
    std::lock_guard<std::mutex> lock(_mutex);
    for (auto& kv : _sessions) {
        if (kv.second.active)
            _dialogue->endConversation(kv.second.conversation_id);
        kv.second.active = false;
    }
    _sessions.clear();
}

void HumanInterface::initializeAtoms()
{
    _interface_root = _atomspace->add_node(CONCEPT_NODE, "HumanInterfaceRoot");
    _interface_root->setTruthValue(SimpleTruthValue::createTV(1.0, 1.0));
    Handle agent = _atomspace->add_node(CONCEPT_NODE, "agent:" + _agent_name);
    _atomspace->add_link(MEMBER_LINK, agent, _interface_root);
}

std::string HumanInterface::trim(const std::string& s) const
{
    size_t start = 0;
    while (start < s.size() &&
           std::isspace(static_cast<unsigned char>(s[start])))
        ++start;
    size_t end = s.size();
    while (end > start &&
           std::isspace(static_cast<unsigned char>(s[end - 1])))
        --end;
    return s.substr(start, end - start);
}

std::string HumanInterface::formatResponse(const std::string& raw) const
{
    if (raw.empty())
        return _agent_name + ": (no response)";
    // Avoid double-prefixing
    if (raw.rfind(_agent_name + ":", 0) == 0) return raw;
    return _agent_name + ": " + raw;
}

std::string HumanInterface::makeSessionId(const std::string& user_id) const
{
    static thread_local std::mt19937 gen{std::random_device{}()};
    std::uniform_int_distribution<int> dis(1000, 9999);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  std::chrono::system_clock::now().time_since_epoch())
                  .count();
    std::ostringstream ss;
    ss << "session_" << user_id << "_" << ms << "_" << dis(gen);
    return ss.str();
}

std::string HumanInterface::startSession(const std::string& user_id)
{
    std::string uid = user_id.empty() ? "user" : user_id;
    std::string sid = makeSessionId(uid);
    std::string cid = "conv_" + sid;

    HumanSession session;
    session.session_id = sid;
    session.user_id = uid;
    session.conversation_id = cid;
    session.active = true;
    session.turn_count = 0;
    session.started_at = std::chrono::system_clock::now();
    session.last_activity = session.started_at;

    _dialogue->startConversation(cid, {uid, _agent_name});

    {
        std::lock_guard<std::mutex> lock(_mutex);
        _sessions[sid] = session;
        if (_default_session_id.empty())
            _default_session_id = sid;
    }

    Handle sess_node = _atomspace->add_node(CONCEPT_NODE, "session:" + sid);
    Handle user_node = _atomspace->add_node(CONCEPT_NODE, "user:" + uid);
    _atomspace->add_link(MEMBER_LINK, sess_node, _interface_root);
    Handle pred = _atomspace->add_node(PREDICATE_NODE, "session-user");
    _atomspace->add_link(EVALUATION_LINK, pred,
                         _atomspace->add_link(LIST_LINK, sess_node, user_node));

    logger().info() << "[HumanInterface] started session " << sid;
    return sid;
}

bool HumanInterface::endSession(const std::string& session_id)
{
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _sessions.find(session_id);
    if (it == _sessions.end()) return false;
    if (it->second.active)
        _dialogue->endConversation(it->second.conversation_id);
    it->second.active = false;
    if (_default_session_id == session_id)
        _default_session_id.clear();
    return true;
}

HumanSession* HumanInterface::requireSession(const std::string& session_id)
{
    auto it = _sessions.find(session_id);
    if (it == _sessions.end() || !it->second.active) return nullptr;
    return &it->second;
}

std::string HumanInterface::processInput(const std::string& session_id,
                                         const std::string& input)
{
    std::string text = trim(input);
    if (text.empty()) return formatResponse("Please enter a non-empty message.");

    HumanSession* session = nullptr;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        session = requireSession(session_id);
        if (!session) return formatResponse("Session is not active.");
        session->last_activity = std::chrono::system_clock::now();
        ++session->turn_count;
    }

    std::string reply = _dialogue->processMessage(
        session->conversation_id, session->user_id, text);
    return formatResponse(reply);
}

std::string HumanInterface::ensureDefaultSession()
{
    std::lock_guard<std::mutex> lock(_mutex);
    if (!_default_session_id.empty()) {
        auto it = _sessions.find(_default_session_id);
        if (it != _sessions.end() && it->second.active)
            return _default_session_id;
    }
    // Create without re-entering startSession's lock: inline minimal path
    // Release and call startSession
    // (drop lock via scope end)
    return "";
}

std::string HumanInterface::processHumanInput(const std::string& input)
{
    std::string sid;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (!_default_session_id.empty()) {
            auto it = _sessions.find(_default_session_id);
            if (it != _sessions.end() && it->second.active)
                sid = _default_session_id;
        }
    }
    if (sid.empty())
        sid = startSession("user");
    return processInput(sid, input);
}

std::string HumanInterface::generateResponse(const std::string& context)
{
    std::string raw = _language->generateResponse(
        context.empty() ? "status" : context, context);
    return formatResponse(raw);
}

void HumanInterface::resetSession()
{
    std::string old;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        old = _default_session_id;
    }
    if (!old.empty())
        endSession(old);
    startSession("user");
}

bool HumanInterface::isSessionActive(const std::string& session_id) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _sessions.find(session_id);
    return it != _sessions.end() && it->second.active;
}

size_t HumanInterface::getActiveSessionCount() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    size_t n = 0;
    for (const auto& kv : _sessions)
        if (kv.second.active) ++n;
    return n;
}

std::vector<std::string> HumanInterface::listSessions() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    std::vector<std::string> ids;
    for (const auto& kv : _sessions)
        if (kv.second.active) ids.push_back(kv.first);
    return ids;
}
