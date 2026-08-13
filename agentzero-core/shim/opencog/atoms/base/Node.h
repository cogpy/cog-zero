#ifndef _AGENTZERO_SHIM_NODE_H
#define _AGENTZERO_SHIM_NODE_H

#include <opencog/atoms/base/Atom.h>

namespace opencog {

class Node : public Atom {
public:
    Node(Type t, const std::string& name) : Atom(t, name) {}
};

} // namespace opencog

#endif
