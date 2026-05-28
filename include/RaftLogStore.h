/*
 * include/RaftLogStore.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Pluggable log store interface for Raft consensus.
 *
 * Design notes
 * ─────────────
 * • RaftLogStore is an abstract interface for persistent Raft log storage.
 *   The default InMemoryLogStore is non-persistent but suitable for testing
 *   and development.
 *
 * • The interface is designed to be future-proof for durable backends like
 *   RocksDB, SQLite, or other persistent stores.
 *
 * • All methods are thread-safe; implementations must provide their own
 *   synchronization as needed.
 *
 * • Log indices are 1-based in Raft semantics. Index 0 is a sentinel entry.
 */

#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace cog0 {

// Forward declaration to avoid circular dependency
struct LogEntry;

/// Abstract interface for persistent Raft log storage.
/// Default implementation is in-memory; can be extended with RocksDB, etc.
class RaftLogStore {
public:
    virtual ~RaftLogStore() = default;

    /// Append an entry to the log. Returns index of appended entry.
    virtual uint64_t append(const LogEntry& entry) = 0;

    /// Get entries in range [startIndex, endIndex] (inclusive).
    virtual std::vector<LogEntry> getRange(uint64_t startIndex,
                                           uint64_t endIndex) const = 0;

    /// Get a single entry by index. Returns empty entry if not found.
    virtual LogEntry get(uint64_t index) const = 0;

    /// Get the last entry's index (0 if empty — i.e., only sentinel).
    virtual uint64_t lastIndex() const = 0;

    /// Get the last entry's term (0 if empty).
    virtual uint64_t lastTerm() const = 0;

    /// Truncate all entries after the given index.
    /// Keeps entries [0, index] and removes everything beyond.
    virtual void truncateAfter(uint64_t index) = 0;

    /// Number of entries in the log (including sentinel at index 0).
    virtual size_t size() const = 0;

    /// Persist the current term and votedFor (Raft persistent state).
    virtual void persistState(uint64_t term, const std::string& votedFor) = 0;

    /// Load persisted state. Returns {term, votedFor}.
    virtual std::pair<uint64_t, std::string> loadState() const = 0;

    /// Sync to durable storage (no-op for in-memory).
    virtual void sync() = 0;

    /// Check if the store is a persistent (durable) implementation.
    virtual bool isPersistent() const { return false; }
};

/// In-memory implementation (default, non-persistent).
/// Suitable for testing and scenarios where durability is not required.
class InMemoryLogStore : public RaftLogStore {
public:
    InMemoryLogStore();

    uint64_t append(const LogEntry& entry) override;
    std::vector<LogEntry> getRange(uint64_t startIndex,
                                   uint64_t endIndex) const override;
    LogEntry get(uint64_t index) const override;
    uint64_t lastIndex() const override;
    uint64_t lastTerm() const override;
    void truncateAfter(uint64_t index) override;
    size_t size() const override;
    void persistState(uint64_t term, const std::string& votedFor) override;
    std::pair<uint64_t, std::string> loadState() const override;
    void sync() override;
    bool isPersistent() const override { return false; }

    /// Clear all log entries and reset state (for testing).
    void clear();

private:
    mutable std::vector<LogEntry> _log;  // index 0 = sentinel
    uint64_t _currentTerm = 0;
    std::string _votedFor;
};

/// Factory function to create log store by type.
/// Supported types: "memory" (default).
/// Future: "rocksdb", "sqlite", etc.
///
/// @param type   Store type identifier (case-insensitive).
/// @param path   Path for persistent stores (ignored for "memory").
/// @return       Unique pointer to created log store.
std::unique_ptr<RaftLogStore> createLogStore(const std::string& type = "memory",
                                              const std::string& path = "");

} // namespace cog0
