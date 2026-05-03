#ifndef STARBYTES_RT_RTNUMERIC_H
#define STARBYTES_RT_RTNUMERIC_H

#include "starbytes/compiler/RTCode.h"
#include "starbytes/interop.h"

namespace starbytes::Runtime {

bool isIntegralNumType(StarbytesNumT numType);
bool isFloatingNumType(StarbytesNumT numType);
int  numericTypeRank(StarbytesNumT numType);
StarbytesNumT promoteNumericType(StarbytesNumT lhs, StarbytesNumT rhs);

bool objectToNumber(StarbytesObject object, long double &value, StarbytesNumT &numType);
StarbytesObject makeNumber(long double value, StarbytesNumT numType);

RTTypedNumericKind typedKindFromNumType(StarbytesNumT numType);
StarbytesNumT      numTypeFromTypedKind(RTTypedNumericKind kind);

bool extractTypedNumericValue(StarbytesObject object, RTTypedNumericKind kind, long double &valueOut);

bool computeTypedBinaryResult(RTTypedNumericKind kind,
                              RTTypedBinaryOp op,
                              long double lhsVal,
                              long double rhsVal,
                              long double &resultOut);

}

#endif
