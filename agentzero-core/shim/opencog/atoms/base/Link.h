#ifndef _AGENTZERO_SHIM_LINK_H
#define _AGENTZERO_SHIM_LINK_H

#include <opencog/atoms/base/Atom.h>

namespace opencog {

class Link : public Atom {
public:
    Link(Type t, const HandleSeq& outgoing) : Atom(t, outgoing) {}
};

} // namespace opencog

#endif
