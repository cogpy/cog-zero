/*
 * standalone/src/cog0_capi.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Implementation of the cog0 C API (see include/cog0/cog0_capi.h).
 *
 * Each cog0_agent_t handle is a raw pointer to a heap-allocated Agent.
 * All functions guard against NULL handles (returning 0/NULL on bad input).
 */

#include "cog0/cog0_capi.h"
#include "cog0/Agent.h"

#include <cstdlib>   /* malloc, free */
#include <cstring>   /* strdup */
#include <stdexcept>

// -----------------------------------------------------------------------
// Version constant — set by CMake from the project version (CMakeLists.txt)

#ifndef COG0_VERSION
#define COG0_VERSION "0.1.0"
#endif

// -----------------------------------------------------------------------
// Internal helpers

static cog0::Agent* as_agent(cog0_agent_t h) {
    return reinterpret_cast<cog0::Agent*>(h);
}

// -----------------------------------------------------------------------
// Agent lifecycle

extern "C"
cog0_agent_t cog0_agent_create(const char* name,
                                int         cycle_interval_ms,
                                size_t      max_tasks_per_cycle)
{
    try {
        cog0::AgentConfig cfg;
        if (name && name[0] != '\0')
            cfg.name = name;
        if (cycle_interval_ms >= 0)
            cfg.cycleInterval = std::chrono::milliseconds(cycle_interval_ms);
        if (max_tasks_per_cycle > 0)
            cfg.maxTasksPerCycle = max_tasks_per_cycle;

        auto* agent = new cog0::Agent(cfg);
        return reinterpret_cast<cog0_agent_t>(agent);
    } catch (...) {
        return nullptr;
    }
}

extern "C"
void cog0_agent_free(cog0_agent_t agent)
{
    if (!agent) return;
    delete as_agent(agent);
}

// -----------------------------------------------------------------------
// Goal management

extern "C"
int cog0_agent_set_goal(cog0_agent_t agent,
                         const char*  name,
                         const char*  desc,
                         double       priority)
{
    if (!agent || !name) return 0;
    try {
        as_agent(agent)->setGoal(
            name,
            desc ? desc : "",
            priority
        );
        return 1;
    } catch (...) {
        return 0;
    }
}

// -----------------------------------------------------------------------
// Percept injection

extern "C"
void cog0_agent_add_percept(cog0_agent_t agent,
                             const char*  source,
                             const char*  content,
                             double       salience)
{
    if (!agent || !source || !content) return;
    try {
        as_agent(agent)->addPercept(source, content, salience);
    } catch (...) {}
}

// -----------------------------------------------------------------------
// Cognitive loop control

extern "C"
void cog0_agent_run_cycles(cog0_agent_t agent, size_t n)
{
    if (!agent) return;
    try {
        as_agent(agent)->runCycles(n);
    } catch (...) {}
}

extern "C"
void cog0_agent_start(cog0_agent_t agent)
{
    if (!agent) return;
    try {
        as_agent(agent)->start();
    } catch (...) {}
}

extern "C"
void cog0_agent_stop(cog0_agent_t agent)
{
    if (!agent) return;
    try {
        as_agent(agent)->stop();
    } catch (...) {}
}

extern "C"
int cog0_agent_is_running(cog0_agent_t agent)
{
    if (!agent) return 0;
    return as_agent(agent)->isRunning() ? 1 : 0;
}

// -----------------------------------------------------------------------
// Introspection

extern "C"
size_t cog0_agent_cycle_count(cog0_agent_t agent)
{
    if (!agent) return 0;
    return as_agent(agent)->cognitiveLoop().cycleCount();
}

extern "C"
size_t cog0_agent_atom_count(cog0_agent_t agent)
{
    if (!agent) return 0;
    return as_agent(agent)->atomStore().size();
}

extern "C"
char* cog0_agent_status_report(cog0_agent_t agent)
{
    if (!agent) return nullptr;
    try {
        std::string s = as_agent(agent)->statusReport();
        // Return a malloc-allocated copy so the caller can free() it
        char* buf = static_cast<char*>(std::malloc(s.size() + 1));
        if (!buf) return nullptr;
        std::memcpy(buf, s.c_str(), s.size() + 1);
        return buf;
    } catch (...) {
        return nullptr;
    }
}

// -----------------------------------------------------------------------
// AtomStore query helpers

extern "C"
int cog0_agent_has_concept(cog0_agent_t agent, const char* name)
{
    if (!agent || !name) return 0;
    return as_agent(agent)->atomStore()
               .getNode(cog0::AtomType::CONCEPT, name) != nullptr ? 1 : 0;
}

extern "C"
void cog0_agent_add_concept(cog0_agent_t agent, const char* name)
{
    if (!agent || !name) return;
    try {
        as_agent(agent)->atomStore().addNode(cog0::AtomType::CONCEPT, name);
    } catch (...) {}
}

// -----------------------------------------------------------------------
// Version

extern "C"
const char* cog0_version(void)
{
    return COG0_VERSION;
}
