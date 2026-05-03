#include "RTNumeric.h"

#include <cmath>
#include <cstdint>

namespace starbytes::Runtime {

bool isIntegralNumType(StarbytesNumT numType){
    return numType == NumTypeInt || numType == NumTypeLong;
}

bool isFloatingNumType(StarbytesNumT numType){
    return numType == NumTypeFloat || numType == NumTypeDouble;
}

int numericTypeRank(StarbytesNumT numType){
    switch(numType){
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

StarbytesNumT promoteNumericType(StarbytesNumT lhs, StarbytesNumT rhs){
    return numericTypeRank(lhs) >= numericTypeRank(rhs) ? lhs : rhs;
}

bool objectToNumber(StarbytesObject object, long double &value, StarbytesNumT &numType){
    if(!object || !StarbytesObjectTypecheck(object, StarbytesNumType())){
        return false;
    }
    numType = StarbytesNumGetType(object);
    if(numType == NumTypeFloat){
        value = StarbytesNumGetFloatValue(object);
    }
    else if(numType == NumTypeDouble){
        value = StarbytesNumGetDoubleValue(object);
    }
    else if(numType == NumTypeLong){
        value = (long double)StarbytesNumGetLongValue(object);
    }
    else {
        value = StarbytesNumGetIntValue(object);
    }
    return true;
}

StarbytesObject makeNumber(long double value, StarbytesNumT numType){
    if(numType == NumTypeDouble){
        return StarbytesNumNew(NumTypeDouble, (double)value);
    }
    if(numType == NumTypeFloat){
        return StarbytesNumNew(NumTypeFloat, (float)value);
    }
    if(numType == NumTypeLong){
        return StarbytesNumNew(NumTypeLong, (int64_t)value);
    }
    return StarbytesNumNew(NumTypeInt, (int)value);
}

RTTypedNumericKind typedKindFromNumType(StarbytesNumT numType){
    switch(numType){
        case NumTypeInt:
            return RTTYPED_NUM_INT;
        case NumTypeLong:
            return RTTYPED_NUM_LONG;
        case NumTypeFloat:
            return RTTYPED_NUM_FLOAT;
        case NumTypeDouble:
            return RTTYPED_NUM_DOUBLE;
        default:
            return RTTYPED_NUM_OBJECT;
    }
}

StarbytesNumT numTypeFromTypedKind(RTTypedNumericKind kind){
    switch(kind){
        case RTTYPED_NUM_INT:
            return NumTypeInt;
        case RTTYPED_NUM_LONG:
            return NumTypeLong;
        case RTTYPED_NUM_FLOAT:
            return NumTypeFloat;
        case RTTYPED_NUM_DOUBLE:
            return NumTypeDouble;
        default:
            return NumTypeInt;
    }
}

bool extractTypedNumericValue(StarbytesObject object, RTTypedNumericKind kind, long double &valueOut){
    StarbytesNumT actualType = NumTypeInt;
    long double rawValue = 0.0;
    if(!objectToNumber(object, rawValue, actualType)){
        return false;
    }
    switch(kind){
        case RTTYPED_NUM_INT:
            valueOut = (long double)((int)rawValue);
            return true;
        case RTTYPED_NUM_LONG:
            valueOut = (long double)((int64_t)rawValue);
            return true;
        case RTTYPED_NUM_FLOAT:
            valueOut = (long double)((float)rawValue);
            return true;
        case RTTYPED_NUM_DOUBLE:
            valueOut = (long double)((double)rawValue);
            return true;
        default:
            break;
    }
    return false;
}

bool computeTypedBinaryResult(RTTypedNumericKind kind,
                              RTTypedBinaryOp op,
                              long double lhsVal,
                              long double rhsVal,
                              long double &resultOut){
    if((op == RTTYPED_BINARY_DIV || op == RTTYPED_BINARY_MOD) && rhsVal == 0.0){
        return false;
    }
    switch(op){
        case RTTYPED_BINARY_ADD:
            resultOut = lhsVal + rhsVal;
            return true;
        case RTTYPED_BINARY_SUB:
            resultOut = lhsVal - rhsVal;
            return true;
        case RTTYPED_BINARY_MUL:
            resultOut = lhsVal * rhsVal;
            return true;
        case RTTYPED_BINARY_DIV:
            resultOut = (kind == RTTYPED_NUM_INT) ? (long double)((int)lhsVal / (int)rhsVal)
                : (kind == RTTYPED_NUM_LONG) ? (long double)((int64_t)lhsVal / (int64_t)rhsVal)
                : lhsVal / rhsVal;
            return true;
        case RTTYPED_BINARY_MOD:
            resultOut = (kind == RTTYPED_NUM_INT) ? (long double)((int)lhsVal % (int)rhsVal)
                : (kind == RTTYPED_NUM_LONG) ? (long double)((int64_t)lhsVal % (int64_t)rhsVal)
                : std::fmod((double)lhsVal, (double)rhsVal);
            return true;
        default:
            break;
    }
    return false;
}

}
