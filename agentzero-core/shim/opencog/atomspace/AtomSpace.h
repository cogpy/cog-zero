/*
 * Minimal in-memory AtomSpace for agentzero-core when real OpenCog is absent.
 *
 * Provides the subset of the AtomSpace API used by Phase 1 orchestration:
 * add_node/add_link, size queries, type queries, and handle validity.
 */
#ifndef _AGENTZERO_SHIM_ATOMSPACE_H
#define _AGENTZERO_SHIM_ATOMSPACE_H

#include <algorithm>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <opencog/atoms/atom_types/types.h>
#include <opencog/atoms/base/Atom.h>
#include <opencog/atoms/base/Handle.h>

namespace opencog {

class AtomSpace : public std::enable_shared_from_this<AtomSpace> {
public:
    AtomSpace() = default;
    ~AtomSpace() = default;

    Handle add_node(Type t, const std::string& name) {
        std::lock_guard<std::mutex> lock(_mu);
        const std::string key = nodeKey(t, name);
        auto it = _nodes.find(key);
        if (it != _nodes.end()) return it->second;
        Handle h(std::make_shared<Atom>(t, name));
        _nodes.emplace(key, h);
        _all.push_back(h);
        return h;
    }

    Handle add_link(Type t, HandleSeq outgoing) {
        std::lock_guard<std::mutex> lock(_mu);
        const std::string key = linkKey(t, outgoing);
        auto it = _links.find(key);
        if (it != _links.end()) return it->second;
        Handle h(std::make_shared<Atom>(t, std::move(outgoing)));
        _links.emplace(key, h);
        _all.push_back(h);
        return h;
    }

    // Convenience overloads matching common OpenCog call patterns
    Handle add_link(Type t, const Handle& a) {
        return add_link(t, HandleSeq{a});
    }
    Handle add_link(Type t, const Handle& a, const Handle& b) {
        return add_link(t, HandleSeq{a, b});
    }
    Handle add_link(Type t, const Handle& a, const Handle& b, const Handle& c) {
        return add_link(t, HandleSeq{a, b, c});
    }

    size_t get_size() const {
        std::lock_guard<std::mutex> lock(_mu);
        return _all.size();
    }

    bool is_valid_handle(const Handle& h) const {
        if (!h) return false;
        std::lock_guard<std::mutex> lock(_mu);
        return std::find(_all.begin(), _all.end(), h) != _all.end();
    }

    HandleSeq get_handles_by_type(Type t, bool subclass = false) const {
        std::lock_guard<std::mutex> lock(_mu);
        HandleSeq out;
        for (const auto& h : _all) {
            if (!h) continue;
            if (h->get_type() == t || (subclass && nameserver_is_a(h->get_type(), t))) {
                out.push_back(h);
            }
        }
        return out;
    }

    Handle get_handle(Type t, const std::string& name) const {
        std::lock_guard<std::mutex> lock(_mu);
        auto it = _nodes.find(nodeKey(t, name));
        if (it == _nodes.end()) return Handle::UNDEFINED;
        return it->second;
    }

    HandleSeq get_all_atoms() const {
        std::lock_guard<std::mutex> lock(_mu);
        return _all;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(_mu);
        _nodes.clear();
        _links.clear();
        _all.clear();
    }

private:
    static std::string nodeKey(Type t, const std::string& name) {
        return "N:" + std::to_string(t) + ":" + name;
    }

    static std::string linkKey(Type t, const HandleSeq& outgoing) {
        std::string k = "L:" + std::to_string(t);
        for (const auto& h : outgoing) {
            k.push_back(':');
            k += std::to_string(reinterpret_cast<uintptr_t>(h.get()));
        }
        return k;
    }

    mutable std::mutex _mu;
    std::unordered_map<std::string, Handle> _nodes;
    std::unordered_map<std::string, Handle> _links;
    HandleSeq _all;
};

using AtomSpacePtr = std::shared_ptr<AtomSpace>;

inline AtomSpacePtr createAtomSpace() {
    return std::make_shared<AtomSpace>();
}

} // namespace opencog

#endif // _AGENTZERO_SHIM_ATOMSPACE_H
