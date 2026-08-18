/*
 * include/AgentServiceJson.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Shared JSON helpers for the AgentService JSON-over-TCP transport
 * (GrpcAgentServer / GrpcAgentClient).
 */

#pragma once

#include <cstdint>
#include <cstdio>
#include <string>

namespace cog0 {
namespace agent_json {

inline std::string escape(const std::string& s)
{
    std::string out;
    out.reserve(s.size() + 2);
    out.push_back('"');
    for (unsigned char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out.push_back(static_cast<char>(c));
                }
        }
    }
    out.push_back('"');
    return out;
}

inline std::string extractStringField(const std::string& json, const std::string& key)
{
    const std::string pat = "\"" + key + "\"";
    auto pos = json.find(pat);
    if (pos == std::string::npos) return {};
    pos = json.find(':', pos + pat.size());
    if (pos == std::string::npos) return {};
    pos = json.find('"', pos + 1);
    if (pos == std::string::npos) return {};
    size_t i = pos + 1;
    std::string out;
    while (i < json.size()) {
        char c = json[i++];
        if (c == '\\' && i < json.size()) {
            char n = json[i++];
            switch (n) {
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                default: out.push_back(n); break;
            }
        } else if (c == '"') {
            break;
        } else {
            out.push_back(c);
        }
    }
    return out;
}

inline double extractNumberField(const std::string& json, const std::string& key, double def = 0.0)
{
    const std::string pat = "\"" + key + "\"";
    auto pos = json.find(pat);
    if (pos == std::string::npos) return def;
    pos = json.find(':', pos + pat.size());
    if (pos == std::string::npos) return def;
    pos = json.find_first_of("-0123456789.", pos + 1);
    if (pos == std::string::npos) return def;
    try {
        size_t end = 0;
        return std::stod(json.substr(pos), &end);
    } catch (...) {
        return def;
    }
}

inline uint32_t extractUintField(const std::string& json, const std::string& key, uint32_t def = 0)
{
    double v = extractNumberField(json, key, static_cast<double>(def));
    if (v < 0) return def;
    return static_cast<uint32_t>(v);
}

inline uint64_t extractUint64Field(const std::string& json, const std::string& key, uint64_t def = 0)
{
    const std::string pat = "\"" + key + "\"";
    auto pos = json.find(pat);
    if (pos == std::string::npos) return def;
    pos = json.find(':', pos + pat.size());
    if (pos == std::string::npos) return def;
    pos = json.find_first_of("0123456789", pos + 1);
    if (pos == std::string::npos) return def;
    try {
        size_t end = 0;
        return static_cast<uint64_t>(std::stoull(json.substr(pos), &end));
    } catch (...) {
        return def;
    }
}

inline bool extractBoolField(const std::string& json, const std::string& key, bool def = false)
{
    const std::string pat = "\"" + key + "\"";
    auto pos = json.find(pat);
    if (pos == std::string::npos) return def;
    pos = json.find(':', pos + pat.size());
    if (pos == std::string::npos) return def;
    auto t = json.find("true", pos);
    auto f = json.find("false", pos);
    if (t != std::string::npos && (f == std::string::npos || t < f)) return true;
    if (f != std::string::npos) return false;
    return def;
}

inline std::string extractObjectField(const std::string& json, const std::string& key)
{
    const std::string pat = "\"" + key + "\"";
    auto pos = json.find(pat);
    if (pos == std::string::npos) return "{}";
    pos = json.find('{', pos + pat.size());
    if (pos == std::string::npos) return "{}";
    int depth = 0;
    for (size_t i = pos; i < json.size(); ++i) {
        if (json[i] == '{') ++depth;
        else if (json[i] == '}') {
            --depth;
            if (depth == 0) return json.substr(pos, i - pos + 1);
        }
    }
    return "{}";
}

inline bool responseOk(const std::string& json)
{
    auto pos = json.find("\"ok\"");
    if (pos == std::string::npos) return false;
    pos = json.find(':', pos);
    if (pos == std::string::npos) return false;
    auto t = json.find("true", pos);
    auto f = json.find("false", pos);
    if (t == std::string::npos) return false;
    if (f != std::string::npos && f < t) return false;
    return true;
}

} // namespace agent_json
} // namespace cog0
