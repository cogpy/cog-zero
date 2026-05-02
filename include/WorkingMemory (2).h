/*
 * opencog/agentzero/memory/WorkingMemory.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * WorkingMemory - Active context and short-term memory
 * Part of Agent-Zero Memory & Context Management module
 * Part of the AGENT-ZERO-GENESIS project - AZ-MEM-002
 *
 * This header forwards to the full WorkingMemory implementation in
 * opencog/agentzero/WorkingMemory.h and exposes it in the
 * opencog::agentzero::memory namespace for API consistency.
 */

#ifndef _OPENCOG_AGENTZERO_MEMORY_WORKING_MEMORY_H
#define _OPENCOG_AGENTZERO_MEMORY_WORKING_MEMORY_H

#include "opencog/agentzero/WorkingMemory.h"

namespace opencog {
namespace agentzero {
namespace memory {

// Expose WorkingMemory and MemoryItem in the memory namespace
using WorkingMemory = ::opencog::agentzero::WorkingMemory;
using MemoryItem = ::opencog::agentzero::MemoryItem;

} // namespace memory
} // namespace agentzero
} // namespace opencog

#endif // _OPENCOG_AGENTZERO_MEMORY_WORKING_MEMORY_H