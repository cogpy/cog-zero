/*
 * standalone/include/cog0/cog0_capi.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * C-compatible API for the cog0 standalone library.
 *
 * This header exposes the core cog0 functionality through a plain-C
 * interface, enabling:
 *   - Python interoperability via ctypes (no build dependency on Cython)
 *   - Cython bindings (see agentzero-python-bridge/)
 *   - FFI access from any language that can call C shared libraries
 *
 * All handles are opaque pointers allocated on the heap.  Callers are
 * responsible for releasing them with the corresponding cog0_*_free()
 * functions.
 *
 * Thread safety:
 *   - cog0_agent_*   functions are NOT thread-safe against each other
 *     unless otherwise noted.  Use one agent per thread or add external
 *     locking.
 *   - cog0_agent_add_percept() IS thread-safe (delegates to the
 *     thread-safe percept queue inside CognitiveLoop).
 *
 * Error handling:
 *   - Functions that can fail return 0 (false) on error or a NULL handle.
 *   - String-returning functions return a NULL-terminated C string
 *     allocated with malloc(); the caller must free() it.
 */

#pragma once

#include <stddef.h>  /* size_t */

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------
 * Opaque handle types
 */

/** Opaque handle for a cog0 Agent instance. */
typedef struct cog0_agent_s* cog0_agent_t;

/* -----------------------------------------------------------------------
 * Agent lifecycle
 */

/**
 * Create a new Agent with the given name.
 *
 * @param name              Agent name (NULL → "cog0-agent").
 * @param cycle_interval_ms Minimum milliseconds between cycles (0 = as fast
 *                          as possible).
 * @param max_tasks_per_cycle Maximum tasks executed per cognitive cycle.
 * @return                  Opaque handle, or NULL on allocation failure.
 */
cog0_agent_t cog0_agent_create(const char* name,
                                int         cycle_interval_ms,
                                size_t      max_tasks_per_cycle);

/**
 * Destroy an Agent and release all associated resources.
 *
 * After this call the handle is invalid and must not be used.
 *
 * @param agent  Handle returned by cog0_agent_create().
 */
void cog0_agent_free(cog0_agent_t agent);

/* -----------------------------------------------------------------------
 * Goal management
 */

/**
 * Set (or replace) a goal on the agent.
 *
 * @param agent     Valid agent handle.
 * @param name      Goal identifier (e.g. "explore-environment").
 * @param desc      Human-readable description (may be NULL or empty).
 * @param priority  Importance weight in [0, 1].  Higher values are more
 *                  important.
 * @return          Non-zero on success, 0 on failure.
 */
int cog0_agent_set_goal(cog0_agent_t agent,
                         const char*  name,
                         const char*  desc,
                         double       priority);

/* -----------------------------------------------------------------------
 * Percept injection
 */

/**
 * Inject a percept into the agent's perception queue.
 *
 * Thread-safe: may be called from any thread while the agent is running.
 *
 * @param agent     Valid agent handle.
 * @param source    Percept source identifier (e.g. "camera-1").
 * @param content   Content string (e.g. "obstacle-detected").
 * @param salience  Attention weight in [0, 1].
 */
void cog0_agent_add_percept(cog0_agent_t agent,
                             const char*  source,
                             const char*  content,
                             double       salience);

/* -----------------------------------------------------------------------
 * Cognitive loop control
 */

/**
 * Run exactly n synchronous cognitive cycles.
 *
 * Blocks until all n cycles complete.
 *
 * @param agent  Valid agent handle.
 * @param n      Number of cycles to run.
 */
void cog0_agent_run_cycles(cog0_agent_t agent, size_t n);

/**
 * Start the background cognitive loop thread.
 *
 * Non-blocking: returns immediately.  Use cog0_agent_is_running() to poll
 * completion, or cog0_agent_stop() to stop early.
 *
 * @param agent  Valid agent handle.
 */
void cog0_agent_start(cog0_agent_t agent);

/**
 * Stop the background cognitive loop thread.
 *
 * Blocks until the loop thread exits.
 *
 * @param agent  Valid agent handle.
 */
void cog0_agent_stop(cog0_agent_t agent);

/**
 * Query whether the background loop is currently running.
 *
 * @param agent  Valid agent handle.
 * @return       Non-zero if running, 0 otherwise.
 */
int cog0_agent_is_running(cog0_agent_t agent);

/* -----------------------------------------------------------------------
 * Introspection
 */

/**
 * Return the number of cognitive cycles completed since the agent was
 * created (or last reset).
 *
 * @param agent  Valid agent handle.
 * @return       Cycle count.
 */
size_t cog0_agent_cycle_count(cog0_agent_t agent);

/**
 * Return the number of atoms currently in the AtomStore.
 *
 * @param agent  Valid agent handle.
 * @return       Atom count.
 */
size_t cog0_agent_atom_count(cog0_agent_t agent);

/**
 * Return a human-readable status report for the agent.
 *
 * The returned string is allocated with malloc() and must be freed by the
 * caller using free().
 *
 * @param agent  Valid agent handle.
 * @return       Null-terminated status string, or NULL on failure.
 */
char* cog0_agent_status_report(cog0_agent_t agent);

/* -----------------------------------------------------------------------
 * AtomStore query helpers
 */

/**
 * Check whether a concept atom with the given name exists in the AtomStore.
 *
 * @param agent  Valid agent handle.
 * @param name   Atom name to look up (e.g. "Goal:explore").
 * @return       Non-zero if the atom exists, 0 otherwise.
 */
int cog0_agent_has_concept(cog0_agent_t agent, const char* name);

/**
 * Add a concept atom to the AtomStore.
 *
 * @param agent  Valid agent handle.
 * @param name   Atom name (e.g. "Fact:sky-is-blue").
 */
void cog0_agent_add_concept(cog0_agent_t agent, const char* name);

/* -----------------------------------------------------------------------
 * Version
 */

/**
 * Return the cog0 library version string (e.g. "0.1.0").
 *
 * The returned pointer is a compile-time constant; do NOT free() it.
 *
 * @return  Version string.
 */
const char* cog0_version(void);

#ifdef __cplusplus
} /* extern "C" */
#endif
