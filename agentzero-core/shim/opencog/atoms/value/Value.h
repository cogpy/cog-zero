/*
 * Minimal Value hierarchy for agentzero-core shim builds.
 */
#ifndef _AGENTZERO_SHIM_VALUE_H
#define _AGENTZERO_SHIM_VALUE_H

#include <memory>
#include <string>
#include <vector>
#include <cstdint>

namespace opencog {

using Type = uint16_t;

class Value;
using ValuePtr = std::shared_ptr<Value>;

class Value {
public:
    explicit Value(Type t = 0) : _type(t) {}
    virtual ~Value() = default;
    Type get_type() const { return _type; }
    virtual std::string to_string(const std::string& = "") const { return "Value"; }
protected:
    Type _type;
};

class FloatValue : public Value {
public:
    explicit FloatValue(const std::vector<double>& v)
        : Value(100), _value(v) {}
    const std::vector<double>& value() const { return _value; }
    size_t size() const { return _value.size(); }
    std::string to_string(const std::string& = "") const override {
        return "FloatValue";
    }
private:
    std::vector<double> _value;
};

inline ValuePtr createFloatValue(const std::vector<double>& v) {
    return std::make_shared<FloatValue>(v);
}

inline std::shared_ptr<FloatValue> FloatValueCast(const ValuePtr& v) {
    return std::dynamic_pointer_cast<FloatValue>(v);
}

// Truth-value key identity used by SimpleTruthValue helpers
inline constexpr intptr_t truth_key() { return 1; }

} // namespace opencog

#endif // _AGENTZERO_SHIM_VALUE_H
