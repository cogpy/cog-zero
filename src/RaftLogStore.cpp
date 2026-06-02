/*
 * src/RaftLogStore.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Implementation of RaftLogStore interface and InMemoryLogStore.
 */

#include "cog0/RaftLogStore.h"
#include "cog0/RaftConsensus.h"

#include <algorithm>
#include <stdexcept>

namespace cog0 {

// =========================================================================
// InMemoryLogStore
// =========================================================================

InMemoryLogStore::InMemoryLogStore()
{
    // Initialize with sentinel entry at index 0 (Raft log is 1-indexed).
    // The sentinel has term=0, index=0, and empty command.
    LogEntry sentinel;
    sentinel.term = 0;
    sentinel.index = 0;
    sentinel.command = "";
    _log.push_back(sentinel);
}

uint64_t InMemoryLogStore::append(const LogEntry& entry)
{
    // Create a copy with the correct index
    LogEntry e = entry;
    e.index = static_cast<uint64_t>(_log.size());
    _log.push_back(e);
    return e.index;
}

std::vector<LogEntry> InMemoryLogStore::getRange(uint64_t startIndex,
                                                  uint64_t endIndex) const
{
    std::vector<LogEntry> result;

    if (startIndex > endIndex) {
        return result;  // empty range
    }

    // Clamp indices to valid range
    uint64_t maxIdx = _log.empty() ? 0 : static_cast<uint64_t>(_log.size() - 1);
    if (startIndex > maxIdx) {
        return result;  // start is beyond log
    }

    uint64_t end = std::min(endIndex, maxIdx);
    for (uint64_t i = startIndex; i <= end; ++i) {
        result.push_back(_log[static_cast<size_t>(i)]);
    }

    return result;
}

LogEntry InMemoryLogStore::get(uint64_t index) const
{
    if (index >= _log.size()) {
        // Return empty entry for out-of-bounds access
        return LogEntry{0, 0, ""};
    }
    return _log[static_cast<size_t>(index)];
}

uint64_t InMemoryLogStore::lastIndex() const
{
    // Last valid index (0 if only sentinel exists)
    return _log.empty() ? 0 : static_cast<uint64_t>(_log.size() - 1);
}

uint64_t InMemoryLogStore::lastTerm() const
{
    if (_log.empty()) {
        return 0;
    }
    return _log.back().term;
}

void InMemoryLogStore::truncateAfter(uint64_t index)
{
    // Keep entries [0, index], remove everything after
    if (index + 1 < _log.size()) {
        _log.erase(_log.begin() + static_cast<long>(index + 1), _log.end());
    }
}

size_t InMemoryLogStore::size() const
{
    return _log.size();
}

void InMemoryLogStore::persistState(uint64_t term, const std::string& votedFor)
{
    // In-memory: just store the values (no durable persistence)
    _currentTerm = term;
    _votedFor = votedFor;
}

std::pair<uint64_t, std::string> InMemoryLogStore::loadState() const
{
    return {_currentTerm, _votedFor};
}

void InMemoryLogStore::sync()
{
    // No-op for in-memory store — nothing to sync to disk
}

void InMemoryLogStore::clear()
{
    _log.clear();
    // Re-initialize with sentinel
    LogEntry sentinel;
    sentinel.term = 0;
    sentinel.index = 0;
    sentinel.command = "";
    _log.push_back(sentinel);

    _currentTerm = 0;
    _votedFor.clear();
}

// =========================================================================
// Factory function
// =========================================================================

std::unique_ptr<RaftLogStore> createLogStore(const std::string& type,
                                              const std::string& path)
{
    (void)path;  // Unused for now; will be used by persistent backends

    // Case-insensitive comparison for type
    std::string lowerType = type;
    std::transform(lowerType.begin(), lowerType.end(), lowerType.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    if (lowerType == "memory" || lowerType.empty()) {
        return std::make_unique<InMemoryLogStore>();
    }

    // Future persistent backends:
    //
    // if (lowerType == "rocksdb") {
    //     return std::make_unique<RocksDBLogStore>(path);
    // }
    //
    // if (lowerType == "sqlite") {
    //     return std::make_unique<SQLiteLogStore>(path);
    // }

    // Unknown type: fall back to in-memory with a warning
    // (In production, this could throw or log a warning)
    return std::make_unique<InMemoryLogStore>();
}

} // namespace cog0
