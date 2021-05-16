#include "starbytes/RT/RTCode.h"

#ifndef STARBYTES_RT_RTSTDLIB_H
#define STARBYTES_RT_RTSTDLIB_H

namespace starbytes {
namespace Runtime::stdlib {


void print(RTObject *object);
void print(RTInternalObject *object);


/// Integer Object

int int_add(RTInternalObject::IntegerParams & lhs,RTInternalObject::IntegerParams & rhs);

int int_sub(RTInternalObject::IntegerParams & lhs,RTInternalObject::IntegerParams & rhs);
    
/// String Object

const char *string_concat();

const char *string_concat();



};
};

#endif