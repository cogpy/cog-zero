/*
 * Minimal in-memory AtomSpace for agentzero-core when real OpenCog is absent.
 *
 * Provides the subset of the AtomSpace API used by Phase 1–3 modules:
 * add_node/add_link, size queries, type queries, handle validity, and
 * incoming-set bookkeeping required by knowledge queries.
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
        Handle h(std::make_shared<Atom>(t, outgoing));
        _links.emplace(key, h);
        _all.push_back(h);
        // Register incoming set on each outgoing atom
        for (const auto& child : outgoing) {
            if (child) child->addIncoming(h);
        }
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
    Handle add_link(Type t, std::initializer_list<Handle> outgoing) {
        return add_link(t, HandleSeq{outgoing});
    }

    // Insert an already-constructed atom (node or link) if absent.
    Handle add_atom(const Handle& atom) {
        if (!atom) return Handle::UNDEFINED;
        if (atom->is_node()) {
            return add_node(atom->get_type(), atom->get_name());
        }
        return add_link(atom->get_type(), atom->getOutgoingSet());
    }

    bool remove_atom(const Handle& h, bool recursive = false) {
        if (!h) return false;
        std::lock_guard<std::mutex> lock(_mu);

        if (recursive) {
            HandleSeq to_remove;
            for (const auto& cand : _all) {
                if (!cand || cand == h || !cand->is_link()) continue;
                const auto& oset = cand->getOutgoingSet();
                if (std::find(oset.begin(), oset.end(), h) != oset.end())
                    to_remove.push_back(cand);
            }
            for (const auto& r : to_remove) {
                eraseAtomUnlocked(r);
            }
        }

        return eraseAtomUnlocked(h);
    }

    size_t get_size() const {
        std::lock_guard<std::mutex> lock(_mu);
        return _all.size();
    }

    size_t size() const { return get_size(); }

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

    // OpenCog-style out-parameter overload used by knowledge sources
    void get_handles_by_type(HandleSeq& out, Type t, bool subclass = false) const {
        out = get_handles_by_type(t, subclass);
    }

    Handle get_handle(Type t, const std::string& name) const {
        std::lock_guard<std::mutex> lock(_mu);
        auto it = _nodes.find(nodeKey(t, name));
        if (it == _nodes.end()) return Handle::UNDEFINED;
        return it->second;
    }

    // Alias used by KnowledgeBase
    Handle get_node(Type t, const std::string& name) const {
        return get_handle(t, name);
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
    bool eraseAtomUnlocked(const Handle& h) {
        if (!h) return false;
        auto it = std::find(_all.begin(), _all.end(), h);
        if (it == _all.end()) return false;

        if (h->is_link()) {
            for (const auto& child : h->getOutgoingSet()) {
                if (child) child->removeIncoming(h);
            }
            for (auto lit = _links.begin(); lit != _links.end(); ) {
                if (lit->second == h) lit = _links.erase(lit);
                else ++lit;
            }
        } else {
            for (auto nit = _nodes.begin(); nit != _nodes.end(); ) {
                if (nit->second == h) nit = _nodes.erase(nit);
                else ++nit;
            }
        }

        _all.erase(it);
        return true;
    }

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
