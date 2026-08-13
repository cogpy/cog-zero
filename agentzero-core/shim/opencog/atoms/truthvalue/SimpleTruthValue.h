/*
 * SimpleTruthValue shim for agentzero-core builds without real OpenCog.
 */
#ifndef _AGENTZERO_SHIM_SIMPLE_TRUTH_VALUE_H
#define _AGENTZERO_SHIM_SIMPLE_TRUTH_VALUE_H

#include <opencog/atoms/value/Value.h>
#include <opencog/atoms/base/Handle.h>
#include <opencog/atoms/base/Atom.h>

namespace opencog {

using TruthValuePtr = ValuePtr;

class SimpleTruthValue {
public:
    static ValuePtr createTV(double strength, double confidence) {
        return createFloatValue(std::vector<double>{strength, confidence});
    }

    static void setTV(const Handle& h, double strength, double confidence) {
        if (!h) return;
        h->setValue(truth_key(), createTV(strength, confidence));
    }

    static double getStrength(const Handle& h) {
        if (!h) return 0.0;
        auto tv = h->getValue(truth_key());
        auto fv = FloatValueCast(tv);
        if (fv && fv->size() > 0) return fv->value()[0];
        return 1.0;
    }

    static double getConfidence(const Handle& h) {
        if (!h) return 0.0;
        auto tv = h->getValue(truth_key());
        auto fv = FloatValueCast(tv);
        if (fv && fv->size() > 1) return fv->value()[1];
        return 1.0;
    }
};

inline ValuePtr createSimpleTruthValue(double s, double c) {
    return SimpleTruthValue::createTV(s, c);
}

} // namespace opencog

#endif
