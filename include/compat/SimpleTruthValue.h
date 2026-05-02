/*
 * Compatibility shim for SimpleTruthValue
 * Modern AtomSpace (5.x) removed SimpleTruthValue in favor of
 * FloatValue-based truth values accessed via truth_key().
 */
#ifndef _COMPAT_SIMPLE_TRUTH_VALUE_H
#define _COMPAT_SIMPLE_TRUTH_VALUE_H

#include <vector>
#include <opencog/atoms/value/FloatValue.h>
#include <opencog/atoms/base/Atom.h>

namespace opencog {

class SimpleTruthValue {
public:
    static ValuePtr createTV(double strength, double confidence) {
        std::vector<double> v = {strength, confidence};
        return createFloatValue(v);
    }
    
    static void setTV(const Handle& h, double strength, double confidence) {
        std::vector<double> v = {strength, confidence};
        h->setValue(truth_key(), createFloatValue(v));
    }
    
    static double getStrength(const Handle& h) {
        auto tv = h->getValue(truth_key());
        if (tv) {
            auto fv = FloatValueCast(tv);
            if (fv && fv->size() > 0) return fv->value()[0];
        }
        return 1.0;
    }
    
    static double getConfidence(const Handle& h) {
        auto tv = h->getValue(truth_key());
        if (tv) {
            auto fv = FloatValueCast(tv);
            if (fv && fv->size() > 1) return fv->value()[1];
        }
        return 1.0;
    }
};

inline ValuePtr createSimpleTruthValue(double s, double c) {
    std::vector<double> v = {s, c};
    return createFloatValue(v);
}

} // namespace opencog

#endif // _COMPAT_SIMPLE_TRUTH_VALUE_H
