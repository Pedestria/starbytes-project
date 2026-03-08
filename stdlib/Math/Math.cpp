#include <starbytes/interop.h>
#include "starbytes/runtime/StdlibMath.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numeric>
#include <string>
#include <vector>

namespace {

using starbytes::ArrayRef;
using starbytes::Runtime::stdlib::invokeMathBuiltinFunction;
using starbytes::string_ref;

struct NativeArgsLayout {
    unsigned argc = 0;
    unsigned index = 0;
    StarbytesObject *argv = nullptr;
};

StarbytesObject makeBool(bool value) {
    return StarbytesBoolNew(value ? StarbytesBoolFalse : StarbytesBoolTrue);
}

StarbytesObject makeDouble(double value) {
    return StarbytesNumNew(NumTypeDouble,value);
}

StarbytesObject makeInt(int value) {
    return StarbytesNumNew(NumTypeInt,value);
}

StarbytesObject makeLong(int64_t value) {
    return StarbytesNumNew(NumTypeLong,value);
}

StarbytesObject failNative(StarbytesFuncArgs args,const std::string &message) {
    StarbytesFuncArgsSetError(args,message.c_str());
    return nullptr;
}

void skipOptionalModuleReceiver(StarbytesFuncArgs args,unsigned expectedUserArgs) {
    auto *raw = reinterpret_cast<NativeArgsLayout *>(args);
    if(!raw || raw->argc < raw->index) {
        return;
    }
    auto remaining = raw->argc - raw->index;
    if(remaining == expectedUserArgs + 1) {
        (void)StarbytesFuncArgsGetArg(args);
    }
}

int numericTypeRank(StarbytesNumT numType) {
    switch(numType) {
        case NumTypeInt:
            return 0;
        case NumTypeLong:
            return 1;
        case NumTypeFloat:
            return 2;
        case NumTypeDouble:
        default:
            return 3;
    }
}

StarbytesNumT promoteNumericType(StarbytesNumT lhs,StarbytesNumT rhs) {
    return numericTypeRank(lhs) >= numericTypeRank(rhs) ? lhs : rhs;
}

StarbytesObject makeNumber(long double value,StarbytesNumT numType) {
    if(numType == NumTypeDouble) {
        return StarbytesNumNew(NumTypeDouble,(double)value);
    }
    if(numType == NumTypeFloat) {
        return StarbytesNumNew(NumTypeFloat,(float)value);
    }
    if(numType == NumTypeLong) {
        return StarbytesNumNew(NumTypeLong,(int64_t)value);
    }
    return StarbytesNumNew(NumTypeInt,(int)value);
}

bool readNumberObject(StarbytesObject object,long double &outValue,StarbytesNumT &outType) {
    if(!object || !StarbytesObjectTypecheck(object,StarbytesNumType())) {
        return false;
    }
    outType = StarbytesNumGetType(object);
    if(outType == NumTypeFloat) {
        outValue = (long double)StarbytesNumGetFloatValue(object);
    }
    else if(outType == NumTypeDouble) {
        outValue = (long double)StarbytesNumGetDoubleValue(object);
    }
    else if(outType == NumTypeLong) {
        outValue = (long double)StarbytesNumGetLongValue(object);
    }
    else {
        outValue = (long double)StarbytesNumGetIntValue(object);
    }
    return true;
}

bool readNumberArg(StarbytesFuncArgs args,long double &outValue,StarbytesNumT &outType) {
    auto arg = StarbytesFuncArgsGetArg(args);
    return readNumberObject(arg,outValue,outType);
}

bool readIntegerObject(StarbytesObject object,int64_t &outValue) {
    if(!object || !StarbytesObjectTypecheck(object,StarbytesNumType())) {
        return false;
    }
    auto numType = StarbytesNumGetType(object);
    if(numType == NumTypeInt) {
        outValue = (int64_t)StarbytesNumGetIntValue(object);
        return true;
    }
    if(numType == NumTypeLong) {
        outValue = StarbytesNumGetLongValue(object);
        return true;
    }
    return false;
}

bool readIntegerArg(StarbytesFuncArgs args,int64_t &outValue) {
    auto arg = StarbytesFuncArgsGetArg(args);
    return readIntegerObject(arg,outValue);
}

bool checkedAddInt64(int64_t lhs,int64_t rhs,int64_t &outValue) {
#if defined(__has_builtin)
#if __has_builtin(__builtin_add_overflow)
    return __builtin_add_overflow(lhs,rhs,&outValue);
#endif
#endif
    if((rhs > 0 && lhs > std::numeric_limits<int64_t>::max() - rhs)
       || (rhs < 0 && lhs < std::numeric_limits<int64_t>::min() - rhs)) {
        return true;
    }
    outValue = lhs + rhs;
    return false;
}

bool checkedMultiplyInt64(int64_t lhs,int64_t rhs,int64_t &outValue) {
#if defined(__has_builtin)
#if __has_builtin(__builtin_mul_overflow)
    return __builtin_mul_overflow(lhs,rhs,&outValue);
#endif
#endif
    if(lhs == 0 || rhs == 0) {
        outValue = 0;
        return false;
    }
    if(lhs == -1 && rhs == std::numeric_limits<int64_t>::min()) {
        return true;
    }
    if(rhs == -1 && lhs == std::numeric_limits<int64_t>::min()) {
        return true;
    }
    if(lhs > 0) {
        if(rhs > 0) {
            if(lhs > std::numeric_limits<int64_t>::max() / rhs) {
                return true;
            }
        }
        else if(rhs < std::numeric_limits<int64_t>::min() / lhs) {
            return true;
        }
    }
    else {
        if(rhs > 0) {
            if(lhs < std::numeric_limits<int64_t>::min() / rhs) {
                return true;
            }
        }
        else if(lhs != 0 && rhs < std::numeric_limits<int64_t>::max() / lhs) {
            return true;
        }
    }
    outValue = lhs * rhs;
    return false;
}

bool fitsInIntRange(int64_t value) {
    return value >= std::numeric_limits<int>::min() && value <= std::numeric_limits<int>::max();
}

bool readNumberArrayObject(StarbytesObject object,StarbytesNumT &widestType) {
    if(!object || !StarbytesObjectTypecheck(object,StarbytesArrayType())) {
        return false;
    }
    widestType = NumTypeInt;
    bool first = true;
    auto len = StarbytesArrayGetLength(object);
    for(unsigned i = 0; i < len; ++i) {
        long double value = 0.0L;
        StarbytesNumT itemType = NumTypeInt;
        if(!readNumberObject(StarbytesArrayIndex(object,i),value,itemType)) {
            return false;
        }
        if(first) {
            widestType = itemType;
            first = false;
        }
        else {
            widestType = promoteNumericType(widestType,itemType);
        }
    }
    return true;
}

bool readNumberArrayArg(StarbytesFuncArgs args,StarbytesObject &outArray,StarbytesNumT &widestType) {
    outArray = StarbytesFuncArgsGetArg(args);
    return readNumberArrayObject(outArray,widestType);
}

struct WelfordStats {
    uint64_t count = 0;
    long double mean = 0.0L;
    long double m2 = 0.0L;
};

bool computeWelfordStats(StarbytesObject valuesObj,WelfordStats &outStats) {
    if(!valuesObj || !StarbytesObjectTypecheck(valuesObj,StarbytesArrayType())) {
        return false;
    }
    outStats = {};
    auto len = StarbytesArrayGetLength(valuesObj);
    for(unsigned i = 0; i < len; ++i) {
        long double value = 0.0L;
        StarbytesNumT valueType = NumTypeInt;
        if(!readNumberObject(StarbytesArrayIndex(valuesObj,i),value,valueType)) {
            return false;
        }
        outStats.count += 1;
        auto delta = value - outStats.mean;
        outStats.mean += delta / (long double)outStats.count;
        auto delta2 = value - outStats.mean;
        outStats.m2 += delta * delta2;
    }
    return true;
}

bool collectNativeArgs(StarbytesFuncArgs args,unsigned expectedArgCount,std::vector<StarbytesObject> &outArgs) {
    outArgs.clear();
    outArgs.reserve(expectedArgCount);
    for(unsigned i = 0; i < expectedArgCount; ++i) {
        auto arg = StarbytesFuncArgsGetArg(args);
        if(!arg) {
            StarbytesFuncArgsSetError(args,("native function expects exactly "
                + std::to_string(expectedArgCount)
                + " argument"
                + (expectedArgCount == 1 ? "" : "s")).c_str());
            return false;
        }
        outArgs.push_back(arg);
    }
    return true;
}

StarbytesObject invokeBuiltinMathWrapper(StarbytesFuncArgs args,const char *name,unsigned expectedArgCount) {
    skipOptionalModuleReceiver(args,expectedArgCount);
    std::vector<StarbytesObject> nativeArgs;
    if(!collectNativeArgs(args,expectedArgCount,nativeArgs)) {
        return nullptr;
    }
    std::string runtimeError;
    auto result = invokeMathBuiltinFunction(string_ref(name),
                                           ArrayRef<StarbytesObject>(nativeArgs.data(),(uint32_t)nativeArgs.size()),
                                           runtimeError);
    if(!result && !runtimeError.empty()) {
        StarbytesFuncArgsSetError(args,runtimeError.c_str());
    }
    return result;
}

STARBYTES_FUNC(math_pi) {
    return makeDouble(3.14159265358979323846264338327950288);
}

STARBYTES_FUNC(math_tau) {
    return makeDouble(6.28318530717958647692528676655900576);
}

STARBYTES_FUNC(math_e) {
    return makeDouble(2.71828182845904523536028747135266249);
}

STARBYTES_FUNC(math_nan) {
    return makeDouble(std::numeric_limits<double>::quiet_NaN());
}

STARBYTES_FUNC(math_infinity) {
    return makeDouble(std::numeric_limits<double>::infinity());
}

STARBYTES_FUNC(math_negativeInfinity) {
    return makeDouble(-std::numeric_limits<double>::infinity());
}

STARBYTES_FUNC(math_sqrt) {
    return invokeBuiltinMathWrapper(args,"sqrt",1);
}

STARBYTES_FUNC(math_abs) {
    return invokeBuiltinMathWrapper(args,"abs",1);
}

STARBYTES_FUNC(math_min) {
    return invokeBuiltinMathWrapper(args,"min",2);
}

STARBYTES_FUNC(math_max) {
    return invokeBuiltinMathWrapper(args,"max",2);
}

STARBYTES_FUNC(math_pow) {
    skipOptionalModuleReceiver(args,2);
    long double base = 0.0;
    long double exponent = 0.0;
    StarbytesNumT baseType = NumTypeInt;
    StarbytesNumT exponentType = NumTypeInt;
    if(!readNumberArg(args,base,baseType) || !readNumberArg(args,exponent,exponentType)) {
        return failNative(args,"pow requires numeric arguments");
    }
    return makeDouble(std::pow((double)base,(double)exponent));
}

STARBYTES_FUNC(math_exp) {
    skipOptionalModuleReceiver(args,1);
    long double value = 0.0;
    StarbytesNumT valueType = NumTypeInt;
    if(!readNumberArg(args,value,valueType)) {
        return failNative(args,"exp requires a numeric argument");
    }
    return makeDouble(std::exp((double)value));
}

STARBYTES_FUNC(math_exp2) {
    skipOptionalModuleReceiver(args,1);
    long double value = 0.0;
    StarbytesNumT valueType = NumTypeInt;
    if(!readNumberArg(args,value,valueType)) {
        return failNative(args,"exp2 requires a numeric argument");
    }
    return makeDouble(std::exp2((double)value));
}

STARBYTES_FUNC(math_log) {
    skipOptionalModuleReceiver(args,1);
    long double value = 0.0;
    StarbytesNumT valueType = NumTypeInt;
    if(!readNumberArg(args,value,valueType)) {
        return failNative(args,"log requires a numeric argument");
    }
    if(value <= 0.0) {
        return failNative(args,"log requires positive numeric input");
    }
    return makeDouble(std::log((double)value));
}

STARBYTES_FUNC(math_log2) {
    skipOptionalModuleReceiver(args,1);
    long double value = 0.0;
    StarbytesNumT valueType = NumTypeInt;
    if(!readNumberArg(args,value,valueType)) {
        return failNative(args,"log2 requires a numeric argument");
    }
    if(value <= 0.0) {
        return failNative(args,"log2 requires positive numeric input");
    }
    return makeDouble(std::log2((double)value));
}

STARBYTES_FUNC(math_log10) {
    skipOptionalModuleReceiver(args,1);
    long double value = 0.0;
    StarbytesNumT valueType = NumTypeInt;
    if(!readNumberArg(args,value,valueType)) {
        return failNative(args,"log10 requires a numeric argument");
    }
    if(value <= 0.0) {
        return failNative(args,"log10 requires positive numeric input");
    }
    return makeDouble(std::log10((double)value));
}

STARBYTES_FUNC(math_cbrt) {
    skipOptionalModuleReceiver(args,1);
    long double value = 0.0;
    StarbytesNumT valueType = NumTypeInt;
    if(!readNumberArg(args,value,valueType)) {
        return failNative(args,"cbrt requires a numeric argument");
    }
    return makeDouble(std::cbrt((double)value));
}

STARBYTES_FUNC(math_hypot) {
    skipOptionalModuleReceiver(args,2);
    long double lhs = 0.0;
    long double rhs = 0.0;
    StarbytesNumT lhsType = NumTypeInt;
    StarbytesNumT rhsType = NumTypeInt;
    if(!readNumberArg(args,lhs,lhsType) || !readNumberArg(args,rhs,rhsType)) {
        return failNative(args,"hypot requires numeric arguments");
    }
    return makeDouble(std::hypot((double)lhs,(double)rhs));
}

STARBYTES_FUNC(math_sin) {
    skipOptionalModuleReceiver(args,1);
    long double value = 0.0;
    StarbytesNumT valueType = NumTypeInt;
    if(!readNumberArg(args,value,valueType)) {
        return failNative(args,"sin requires a numeric argument");
    }
    return makeDouble(std::sin((double)value));
}

STARBYTES_FUNC(math_cos) {
    skipOptionalModuleReceiver(args,1);
    long double value = 0.0;
    StarbytesNumT valueType = NumTypeInt;
    if(!readNumberArg(args,value,valueType)) {
        return failNative(args,"cos requires a numeric argument");
    }
    return makeDouble(std::cos((double)value));
}

STARBYTES_FUNC(math_tan) {
    skipOptionalModuleReceiver(args,1);
    long double value = 0.0;
    StarbytesNumT valueType = NumTypeInt;
    if(!readNumberArg(args,value,valueType)) {
        return failNative(args,"tan requires a numeric argument");
    }
    return makeDouble(std::tan((double)value));
}

STARBYTES_FUNC(math_atan2) {
    skipOptionalModuleReceiver(args,2);
    long double y = 0.0;
    long double x = 0.0;
    StarbytesNumT yType = NumTypeInt;
    StarbytesNumT xType = NumTypeInt;
    if(!readNumberArg(args,y,yType) || !readNumberArg(args,x,xType)) {
        return failNative(args,"atan2 requires numeric arguments");
    }
    return makeDouble(std::atan2((double)y,(double)x));
}

STARBYTES_FUNC(math_floor) {
    skipOptionalModuleReceiver(args,1);
    long double value = 0.0;
    StarbytesNumT valueType = NumTypeInt;
    if(!readNumberArg(args,value,valueType)) {
        return failNative(args,"floor requires a numeric argument");
    }
    return makeDouble(std::floor((double)value));
}

STARBYTES_FUNC(math_ceil) {
    skipOptionalModuleReceiver(args,1);
    long double value = 0.0;
    StarbytesNumT valueType = NumTypeInt;
    if(!readNumberArg(args,value,valueType)) {
        return failNative(args,"ceil requires a numeric argument");
    }
    return makeDouble(std::ceil((double)value));
}

STARBYTES_FUNC(math_round) {
    skipOptionalModuleReceiver(args,1);
    long double value = 0.0;
    StarbytesNumT valueType = NumTypeInt;
    if(!readNumberArg(args,value,valueType)) {
        return failNative(args,"round requires a numeric argument");
    }
    return makeDouble(std::round((double)value));
}

STARBYTES_FUNC(math_trunc) {
    skipOptionalModuleReceiver(args,1);
    long double value = 0.0;
    StarbytesNumT valueType = NumTypeInt;
    if(!readNumberArg(args,value,valueType)) {
        return failNative(args,"trunc requires a numeric argument");
    }
    return makeDouble(std::trunc((double)value));
}

STARBYTES_FUNC(math_isFinite) {
    skipOptionalModuleReceiver(args,1);
    long double value = 0.0;
    StarbytesNumT valueType = NumTypeInt;
    if(!readNumberArg(args,value,valueType)) {
        return failNative(args,"isFinite requires a numeric argument");
    }
    return makeBool(std::isfinite((double)value));
}

STARBYTES_FUNC(math_isInfinite) {
    skipOptionalModuleReceiver(args,1);
    long double value = 0.0;
    StarbytesNumT valueType = NumTypeInt;
    if(!readNumberArg(args,value,valueType)) {
        return failNative(args,"isInfinite requires a numeric argument");
    }
    return makeBool(std::isinf((double)value));
}

STARBYTES_FUNC(math_isNaN) {
    skipOptionalModuleReceiver(args,1);
    long double value = 0.0;
    StarbytesNumT valueType = NumTypeInt;
    if(!readNumberArg(args,value,valueType)) {
        return failNative(args,"isNaN requires a numeric argument");
    }
    return makeBool(std::isnan((double)value));
}

STARBYTES_FUNC(math_degreesToRadians) {
    skipOptionalModuleReceiver(args,1);
    long double angle = 0.0;
    StarbytesNumT angleType = NumTypeInt;
    if(!readNumberArg(args,angle,angleType)) {
        return failNative(args,"degreesToRadians requires a numeric argument");
    }
    return makeDouble((double)(angle * 3.14159265358979323846264338327950288L / 180.0L));
}

STARBYTES_FUNC(math_radiansToDegrees) {
    skipOptionalModuleReceiver(args,1);
    long double angle = 0.0;
    StarbytesNumT angleType = NumTypeInt;
    if(!readNumberArg(args,angle,angleType)) {
        return failNative(args,"radiansToDegrees requires a numeric argument");
    }
    return makeDouble((double)(angle * 180.0L / 3.14159265358979323846264338327950288L));
}

STARBYTES_FUNC(math_approxEqual) {
    skipOptionalModuleReceiver(args,3);
    long double lhs = 0.0;
    long double rhs = 0.0;
    long double epsilon = 0.0;
    StarbytesNumT lhsType = NumTypeInt;
    StarbytesNumT rhsType = NumTypeInt;
    StarbytesNumT epsilonType = NumTypeInt;
    if(!readNumberArg(args,lhs,lhsType)
       || !readNumberArg(args,rhs,rhsType)
       || !readNumberArg(args,epsilon,epsilonType)) {
        return failNative(args,"approxEqual requires numeric arguments");
    }
    if(std::isnan((double)epsilon) || epsilon < 0.0L) {
        return failNative(args,"approxEqual requires a non-negative numeric epsilon");
    }
    auto lhsValue = (double)lhs;
    auto rhsValue = (double)rhs;
    if(lhsValue == rhsValue) {
        return makeBool(true);
    }
    if(std::isnan(lhsValue) || std::isnan(rhsValue)) {
        return makeBool(false);
    }
    return makeBool(std::fabs(lhsValue - rhsValue) <= (double)epsilon);
}

STARBYTES_FUNC(math_clamp) {
    skipOptionalModuleReceiver(args,3);
    auto valueObject = StarbytesFuncArgsGetArg(args);
    auto lowerObject = StarbytesFuncArgsGetArg(args);
    auto upperObject = StarbytesFuncArgsGetArg(args);
    long double value = 0.0;
    long double lower = 0.0;
    long double upper = 0.0;
    StarbytesNumT valueType = NumTypeInt;
    StarbytesNumT lowerType = NumTypeInt;
    StarbytesNumT upperType = NumTypeInt;
    if(!readNumberObject(valueObject,value,valueType)
       || !readNumberObject(lowerObject,lower,lowerType)
       || !readNumberObject(upperObject,upper,upperType)) {
        return failNative(args,"clamp requires numeric arguments");
    }
    if(lower > upper) {
        return failNative(args,"clamp requires lower bound to be <= upper bound");
    }
    auto resultType = promoteNumericType(promoteNumericType(valueType,lowerType),upperType);
    auto clamped = std::max(lower,std::min(value,upper));
    return makeNumber(clamped,resultType);
}

STARBYTES_FUNC(math_lerp) {
    skipOptionalModuleReceiver(args,3);
    long double start = 0.0;
    long double end = 0.0;
    long double t = 0.0;
    StarbytesNumT startType = NumTypeInt;
    StarbytesNumT endType = NumTypeInt;
    StarbytesNumT tType = NumTypeInt;
    if(!readNumberArg(args,start,startType)
       || !readNumberArg(args,end,endType)
       || !readNumberArg(args,t,tType)) {
        return failNative(args,"lerp requires numeric arguments");
    }
    return makeDouble((double)(start + ((end - start) * t)));
}

STARBYTES_FUNC(math_gcd) {
    skipOptionalModuleReceiver(args,2);
    int64_t lhs = 0;
    int64_t rhs = 0;
    if(!readIntegerArg(args,lhs) || !readIntegerArg(args,rhs)) {
        return failNative(args,"gcd requires Int or Long arguments");
    }
    return makeLong(std::gcd(lhs,rhs));
}

STARBYTES_FUNC(math_lcm) {
    skipOptionalModuleReceiver(args,2);
    int64_t lhs = 0;
    int64_t rhs = 0;
    if(!readIntegerArg(args,lhs) || !readIntegerArg(args,rhs)) {
        return failNative(args,"lcm requires Int or Long arguments");
    }
    if(lhs == 0 || rhs == 0) {
        return makeLong(0);
    }
    auto divisor = std::gcd(lhs,rhs);
    int64_t product = 0;
    if(checkedMultiplyInt64(lhs / divisor,rhs,product)) {
        return failNative(args,"lcm overflowed Long range");
    }
    if(product == std::numeric_limits<int64_t>::min()) {
        return failNative(args,"lcm overflowed Long range");
    }
    return makeLong(product < 0 ? -product : product);
}

STARBYTES_FUNC(math_isPowerOfTwo) {
    skipOptionalModuleReceiver(args,1);
    int64_t value = 0;
    if(!readIntegerArg(args,value)) {
        return failNative(args,"isPowerOfTwo requires an Int or Long argument");
    }
    return makeBool(value > 0 && ((value & (value - 1)) == 0));
}

STARBYTES_FUNC(math_nextPowerOfTwo) {
    skipOptionalModuleReceiver(args,1);
    int64_t value = 0;
    if(!readIntegerArg(args,value)) {
        return failNative(args,"nextPowerOfTwo requires an Int or Long argument");
    }
    if(value < 0) {
        return failNative(args,"nextPowerOfTwo requires a non-negative integer");
    }
    if(value == 0) {
        return makeLong(1);
    }
    const int64_t maxRepresentable = (int64_t)1 << 62;
    if(value > maxRepresentable) {
        return failNative(args,"nextPowerOfTwo overflowed Long range");
    }
    uint64_t rounded = (uint64_t)(value - 1);
    rounded |= rounded >> 1;
    rounded |= rounded >> 2;
    rounded |= rounded >> 4;
    rounded |= rounded >> 8;
    rounded |= rounded >> 16;
    rounded |= rounded >> 32;
    rounded += 1;
    if(rounded > (uint64_t)std::numeric_limits<int64_t>::max()) {
        return failNative(args,"nextPowerOfTwo overflowed Long range");
    }
    return makeLong((int64_t)rounded);
}

STARBYTES_FUNC(math_sum) {
    skipOptionalModuleReceiver(args,1);
    StarbytesObject valuesObj = nullptr;
    StarbytesNumT widestType = NumTypeInt;
    if(!readNumberArrayArg(args,valuesObj,widestType)) {
        return failNative(args,"sum requires Array of numeric values");
    }

    auto len = StarbytesArrayGetLength(valuesObj);
    if(len == 0) {
        return makeInt(0);
    }

    if(widestType == NumTypeInt || widestType == NumTypeLong) {
        int64_t total = 0;
        for(unsigned i = 0; i < len; ++i) {
            int64_t itemValue = 0;
            if(!readIntegerObject(StarbytesArrayIndex(valuesObj,i),itemValue)) {
                return failNative(args,"sum requires Array of numeric values");
            }
            if(checkedAddInt64(total,itemValue,total)) {
                return failNative(args,"sum overflowed Long range");
            }
        }
        if(widestType == NumTypeLong || !fitsInIntRange(total)) {
            return makeLong(total);
        }
        return makeInt((int)total);
    }

    long double total = 0.0L;
    for(unsigned i = 0; i < len; ++i) {
        long double itemValue = 0.0L;
        StarbytesNumT itemType = NumTypeInt;
        if(!readNumberObject(StarbytesArrayIndex(valuesObj,i),itemValue,itemType)) {
            return failNative(args,"sum requires Array of numeric values");
        }
        total += itemValue;
    }
    return makeNumber(total,widestType);
}

STARBYTES_FUNC(math_product) {
    skipOptionalModuleReceiver(args,1);
    StarbytesObject valuesObj = nullptr;
    StarbytesNumT widestType = NumTypeInt;
    if(!readNumberArrayArg(args,valuesObj,widestType)) {
        return failNative(args,"product requires Array of numeric values");
    }

    auto len = StarbytesArrayGetLength(valuesObj);
    if(len == 0) {
        return makeInt(1);
    }

    if(widestType == NumTypeInt || widestType == NumTypeLong) {
        int64_t total = 1;
        for(unsigned i = 0; i < len; ++i) {
            int64_t itemValue = 0;
            if(!readIntegerObject(StarbytesArrayIndex(valuesObj,i),itemValue)) {
                return failNative(args,"product requires Array of numeric values");
            }
            if(checkedMultiplyInt64(total,itemValue,total)) {
                return failNative(args,"product overflowed Long range");
            }
        }
        if(widestType == NumTypeLong || !fitsInIntRange(total)) {
            return makeLong(total);
        }
        return makeInt((int)total);
    }

    long double total = 1.0L;
    for(unsigned i = 0; i < len; ++i) {
        long double itemValue = 0.0L;
        StarbytesNumT itemType = NumTypeInt;
        if(!readNumberObject(StarbytesArrayIndex(valuesObj,i),itemValue,itemType)) {
            return failNative(args,"product requires Array of numeric values");
        }
        total *= itemValue;
    }
    return makeNumber(total,widestType);
}

STARBYTES_FUNC(math_mean) {
    skipOptionalModuleReceiver(args,1);
    auto valuesObj = StarbytesFuncArgsGetArg(args);
    WelfordStats stats;
    if(!computeWelfordStats(valuesObj,stats)) {
        return failNative(args,"mean requires Array of numeric values");
    }
    if(stats.count == 0) {
        return failNative(args,"mean requires a non-empty Array of numeric values");
    }
    return makeDouble((double)stats.mean);
}

STARBYTES_FUNC(math_variance) {
    skipOptionalModuleReceiver(args,1);
    auto valuesObj = StarbytesFuncArgsGetArg(args);
    WelfordStats stats;
    if(!computeWelfordStats(valuesObj,stats)) {
        return failNative(args,"variance requires Array of numeric values");
    }
    if(stats.count == 0) {
        return failNative(args,"variance requires a non-empty Array of numeric values");
    }
    return makeDouble((double)(stats.m2 / (long double)stats.count));
}

STARBYTES_FUNC(math_stddev) {
    skipOptionalModuleReceiver(args,1);
    auto valuesObj = StarbytesFuncArgsGetArg(args);
    WelfordStats stats;
    if(!computeWelfordStats(valuesObj,stats)) {
        return failNative(args,"stddev requires Array of numeric values");
    }
    if(stats.count == 0) {
        return failNative(args,"stddev requires a non-empty Array of numeric values");
    }
    auto variance = stats.m2 / (long double)stats.count;
    return makeDouble(std::sqrt((double)variance));
}

void addFunc(StarbytesNativeModule *module,const char *name,unsigned argCount,StarbytesFuncCallback callback) {
    StarbytesFuncDesc desc;
    desc.name = CStringMake(name);
    desc.argCount = argCount;
    desc.callback = callback;
    StarbytesNativeModuleAddDesc(module,&desc);
}

void addValue(StarbytesNativeModule *module,const char *name,StarbytesFuncCallback callback) {
    StarbytesValueDesc desc;
    desc.name = CStringMake(name);
    desc.callback = callback;
    StarbytesNativeModuleAddValueDesc(module,&desc);
}

}

STARBYTES_NATIVE_MOD_MAIN() {
    auto module = StarbytesNativeModuleCreate();

    addValue(module,"math_pi",math_pi);
    addValue(module,"math_tau",math_tau);
    addValue(module,"math_e",math_e);
    addValue(module,"math_nan",math_nan);
    addValue(module,"math_infinity",math_infinity);
    addValue(module,"math_negativeInfinity",math_negativeInfinity);

    addFunc(module,"math_sqrt",1,math_sqrt);
    addFunc(module,"math_abs",1,math_abs);
    addFunc(module,"math_min",2,math_min);
    addFunc(module,"math_max",2,math_max);

    addFunc(module,"math_pow",2,math_pow);
    addFunc(module,"math_exp",1,math_exp);
    addFunc(module,"math_exp2",1,math_exp2);
    addFunc(module,"math_log",1,math_log);
    addFunc(module,"math_log2",1,math_log2);
    addFunc(module,"math_log10",1,math_log10);
    addFunc(module,"math_cbrt",1,math_cbrt);
    addFunc(module,"math_hypot",2,math_hypot);
    addFunc(module,"math_sin",1,math_sin);
    addFunc(module,"math_cos",1,math_cos);
    addFunc(module,"math_tan",1,math_tan);
    addFunc(module,"math_atan2",2,math_atan2);
    addFunc(module,"math_floor",1,math_floor);
    addFunc(module,"math_ceil",1,math_ceil);
    addFunc(module,"math_round",1,math_round);
    addFunc(module,"math_trunc",1,math_trunc);
    addFunc(module,"math_isFinite",1,math_isFinite);
    addFunc(module,"math_isInfinite",1,math_isInfinite);
    addFunc(module,"math_isNaN",1,math_isNaN);
    addFunc(module,"math_degreesToRadians",1,math_degreesToRadians);
    addFunc(module,"math_radiansToDegrees",1,math_radiansToDegrees);
    addFunc(module,"math_approxEqual",3,math_approxEqual);
    addFunc(module,"math_clamp",3,math_clamp);
    addFunc(module,"math_lerp",3,math_lerp);
    addFunc(module,"math_gcd",2,math_gcd);
    addFunc(module,"math_lcm",2,math_lcm);
    addFunc(module,"math_isPowerOfTwo",1,math_isPowerOfTwo);
    addFunc(module,"math_nextPowerOfTwo",1,math_nextPowerOfTwo);
    addFunc(module,"math_sum",1,math_sum);
    addFunc(module,"math_product",1,math_product);
    addFunc(module,"math_mean",1,math_mean);
    addFunc(module,"math_variance",1,math_variance);
    addFunc(module,"math_stddev",1,math_stddev);

    return module;
}
