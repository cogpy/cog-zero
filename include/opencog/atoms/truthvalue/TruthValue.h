/*
 * TruthValue compatibility shim
 * Redirects to the SimpleTruthValue shim which provides the modern API
 */
#ifndef _COMPAT_TRUTH_VALUE_H
#define _COMPAT_TRUTH_VALUE_H

#include "SimpleTruthValue.h"

namespace opencog {

// TruthValuePtr is just a ValuePtr in modern AtomSpace
using TruthValuePtr = ValuePtr;

} // namespace opencog

#endif
