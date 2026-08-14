/*
 * Minimal AtomSpace type constants for agentzero-core standalone builds.
 * Used only when real OpenCog atomspace is not available.
 */
#ifndef _AGENTZERO_SHIM_ATOM_TYPES_H
#define _AGENTZERO_SHIM_ATOM_TYPES_H

#include <cstdint>

namespace opencog {

using Type = uint16_t;

// Top-level type used for "match any atom" queries
static constexpr Type ATOM             = 0;
static constexpr Type NOTYPE           = 0;

// Node types
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
static constexpr Type BIND_LINK        = 35;
static constexpr Type LAMBDA_LINK      = 36;

inline bool nameserver_is_a(Type t, Type base) {
    if (t == base) return true;
    // ATOM matches every concrete atom type
    if (base == ATOM) return t != NOTYPE || t == ATOM;
    if (base == NODE) return t >= NODE && t < LIST_LINK;
    if (base == LINK) return t >= LIST_LINK;
    return false;
}

inline const char* nameserver_get_type_name(Type t) {
    switch (t) {
        case ATOM: return "Atom";
        case NODE: return "Node";
        case LINK: return "Link";
        case CONCEPT_NODE: return "ConceptNode";
        case PREDICATE_NODE: return "PredicateNode";
        case SCHEMA_NODE: return "SchemaNode";
        case VARIABLE_NODE: return "VariableNode";
        case NUMBER_NODE: return "NumberNode";
        case TYPE_NODE: return "TypeNode";
        case ANCHOR_NODE: return "AnchorNode";
        case LIST_LINK: return "ListLink";
        case SET_LINK: return "SetLink";
        case EVALUATION_LINK: return "EvaluationLink";
        case INHERITANCE_LINK: return "InheritanceLink";
        case SIMILARITY_LINK: return "SimilarityLink";
        case IMPLICATION_LINK: return "ImplicationLink";
        case EXECUTION_LINK: return "ExecutionLink";
        case STATE_LINK: return "StateLink";
        case MEMBER_LINK: return "MemberLink";
        case CONTEXT_LINK: return "ContextLink";
        case AND_LINK: return "AndLink";
        case OR_LINK: return "OrLink";
        case NOT_LINK: return "NotLink";
        case ORDERED_LINK: return "OrderedLink";
        case UNORDERED_LINK: return "UnorderedLink";
        case BIND_LINK: return "BindLink";
        case LAMBDA_LINK: return "LambdaLink";
        default: return "Unknown";
    }
}

// Minimal NameServer facade matching OpenCog call sites: nameserver().getTypeName(t)
struct NameServer {
    const char* getTypeName(Type t) const { return nameserver_get_type_name(t); }
    bool isA(Type t, Type base) const { return nameserver_is_a(t, base); }
};

inline NameServer& nameserver() {
    static NameServer ns;
    return ns;
}

} // namespace opencog

#endif // _AGENTZERO_SHIM_ATOM_TYPES_H
