/*
 * Minimal Atom base for agentzero-core shim builds.
 */
#ifndef _AGENTZERO_SHIM_ATOM_H
#define _AGENTZERO_SHIM_ATOM_H

#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

#include <opencog/atoms/atom_types/types.h>
#include <opencog/atoms/base/Handle.h>
#include <opencog/atoms/value/Value.h>

namespace opencog {

class Atom : public std::enable_shared_from_this<Atom> {
public:
    // Node
    Atom(Type t, std::string name)
        : _type(t), _name(std::move(name)) {}

    // Link
    Atom(Type t, HandleSeq outgoing)
        : _type(t), _outgoing(std::move(outgoing)) {}

    virtual ~Atom() = default;

    Type get_type() const { return _type; }
    Type getType() const { return _type; }
    const std::string& get_name() const { return _name; }
    const std::string& getName() const { return _name; }
    const HandleSeq& getOutgoingSet() const { return _outgoing; }
    HandleSeq getOutgoingSetCopy() const { return _outgoing; }
    bool is_node() const { return _outgoing.empty(); }
    bool is_link() const { return !_outgoing.empty(); }
    size_t get_arity() const { return _outgoing.size(); }

    void setValue(intptr_t key, const ValuePtr& v) {
        std::lock_guard<std::mutex> lock(_mu);
        _values[key] = v;
    }

    ValuePtr getValue(intptr_t key) const {
        std::lock_guard<std::mutex> lock(_mu);
        auto it = _values.find(key);
        if (it == _values.end()) return nullptr;
        return it->second;
    }

    // Compatibility with older TruthValue API used in some sources
    void setTruthValue(const ValuePtr& tv) { setValue(truth_key(), tv); }
    ValuePtr getTruthValue() const { return getValue(truth_key()); }

    std::string to_string(const std::string& indent = "") const {
        std::ostringstream oss;
        oss << indent;
        if (is_node()) {
            oss << "(Node type=" << _type << " name=\"" << _name << "\")";
        } else {
            oss << "(Link type=" << _type << " arity=" << _outgoing.size() << ")";
        }
        return oss.str();
    }

    std::string to_short_string(const std::string& = "") const {
        if (is_node()) return _name.empty() ? ("type:" + std::to_string(_type)) : _name;
        return "link:" + std::to_string(_type);
    }

    // Attention-value placeholders (STI/LTI)
    void setSTI(short s) { _sti = s; }
    void setLTI(short l) { _lti = l; }
    short getSTI() const { return _sti; }
    short getLTI() const { return _lti; }

protected:
    Type _type = NOTYPE;
    std::string _name;
    HandleSeq _outgoing;
    mutable std::mutex _mu;
    std::map<intptr_t, ValuePtr> _values;
    short _sti = 0;
    short _lti = 0;
};

using AtomPtr = std::shared_ptr<Atom>;

} // namespace opencog

#endif // _AGENTZERO_SHIM_ATOM_H
