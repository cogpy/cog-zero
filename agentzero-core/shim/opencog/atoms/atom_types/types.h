/*
 * Minimal AtomSpace type constants for agentzero-core standalone builds.
 * Used only when real OpenCog atomspace is not available.
 */
#ifndef _AGENTZERO_SHIM_ATOM_TYPES_H
#define _AGENTZERO_SHIM_ATOM_TYPES_H

#include <cstdint>

namespace opencog {

using Type = uint16_t;

// Node types
static constexpr Type NOTYPE           = 0;
static constexpr Type NODE             = 1;
static constexpr Type LINK             = 2;
static constexpr Type CONCEPT_NODE     = 3;
static constexpr Type PREDICATE_NODE   = 4;
static constexpr Type SCHEMA_NODE      = 5;
static constexpr Type VARIABLE_NODE    = 6;
static constexpr Type NUMBER_NODE      = 7;
static constexpr Type TYPE_NODE        = 8;
static constexpr Type ANCHOR_NODE      = 9;

// Link types
static constexpr Type LIST_LINK        = 20;
static constexpr Type SET_LINK         = 21;
static constexpr Type EVALUATION_LINK  = 22;
static constexpr Type INHERITANCE_LINK = 23;
static constexpr Type SIMILARITY_LINK  = 24;
static constexpr Type IMPLICATION_LINK = 25;
static constexpr Type EXECUTION_LINK   = 26;
static constexpr Type STATE_LINK       = 27;
static constexpr Type MEMBER_LINK      = 28;
static constexpr Type CONTEXT_LINK     = 29;
static constexpr Type AND_LINK         = 30;
static constexpr Type OR_LINK          = 31;
static constexpr Type NOT_LINK         = 32;
static constexpr Type ORDERED_LINK     = 33;
static constexpr Type UNORDERED_LINK   = 34;

inline bool nameserver_is_a(Type t, Type base) {
    if (t == base) return true;
    if (base == NODE) return t >= NODE && t < LIST_LINK;
    if (base == LINK) return t >= LIST_LINK;
    return false;
}

} // namespace opencog

#endif // _AGENTZERO_SHIM_ATOM_TYPES_H
