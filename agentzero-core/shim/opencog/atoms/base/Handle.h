/*
 * Minimal Handle type for agentzero-core shim builds.
 * Provides Handle::UNDEFINED matching OpenCog conventions.
 */
#ifndef _AGENTZERO_SHIM_HANDLE_H
#define _AGENTZERO_SHIM_HANDLE_H

#include <functional>
#include <iostream>
#include <memory>
#include <vector>

namespace opencog {

class Atom;

class Handle {
public:
    using Ptr = std::shared_ptr<Atom>;

    Handle() = default;
    Handle(std::nullptr_t) : _ptr(nullptr) {}
    Handle(const Ptr& p) : _ptr(p) {}
    Handle(Ptr&& p) : _ptr(std::move(p)) {}
    template <typename T>
    Handle(const std::shared_ptr<T>& p) : _ptr(std::static_pointer_cast<Atom>(p)) {}

    Atom* get() const { return _ptr.get(); }
    Atom& operator*() const { return *_ptr; }
    Atom* operator->() const { return _ptr.get(); }
    explicit operator bool() const { return static_cast<bool>(_ptr); }
    bool operator==(const Handle& o) const { return _ptr == o._ptr; }
    bool operator!=(const Handle& o) const { return _ptr != o._ptr; }
    bool operator<(const Handle& o) const { return _ptr.get() < o._ptr.get(); }
    const Ptr& shared() const { return _ptr; }

    static const Handle UNDEFINED;

private:
    Ptr _ptr;
};

inline const Handle Handle::UNDEFINED{};

using HandleSeq = std::vector<Handle>;

struct HandleHash {
    size_t operator()(const Handle& h) const noexcept {
        return std::hash<const Atom*>{}(h.get());
    }
};

inline std::ostream& operator<<(std::ostream& os, const Handle& h) {
    if (!h) return os << "Handle::UNDEFINED";
    // Atom forward-declared; full to_short_string available when Atom.h included.
    return os << static_cast<const void*>(h.get());
}

} // namespace opencog

namespace std {
template <>
struct hash<opencog::Handle> {
    size_t operator()(const opencog::Handle& h) const noexcept {
        return hash<const opencog::Atom*>{}(h.get());
    }
};
} // namespace std

#endif // _AGENTZERO_SHIM_HANDLE_H
