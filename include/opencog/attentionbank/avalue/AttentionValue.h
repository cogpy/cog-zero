/*
 * Compatibility shim for AttentionValue
 * The attentionbank module is not installed; provide a minimal stub.
 */
#ifndef _COMPAT_ATTENTION_VALUE_H
#define _COMPAT_ATTENTION_VALUE_H

#include <opencog/atoms/value/FloatValue.h>

namespace opencog {

// Minimal AttentionValue stub - STI/LTI as FloatValue
class AttentionValue {
public:
    using AttentionValuePtr = std::shared_ptr<AttentionValue>;
    
    AttentionValue(double sti = 0.0, double lti = 0.0) 
        : _sti(sti), _lti(lti) {}
    
    double getSTI() const { return _sti; }
    double getLTI() const { return _lti; }
    void setSTI(double v) { _sti = v; }
    void setLTI(double v) { _lti = v; }
    
    static AttentionValuePtr createAV(double sti = 0.0, double lti = 0.0) {
        return std::make_shared<AttentionValue>(sti, lti);
    }

private:
    double _sti;
    double _lti;
};

using AttentionValuePtr = AttentionValue::AttentionValuePtr;

} // namespace opencog

#endif // _COMPAT_ATTENTION_VALUE_H
