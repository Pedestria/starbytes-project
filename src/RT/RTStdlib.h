#include "starbytes/AST/ASTNodes.def"
#include "starbytes/RT/RTCode.h"

#ifndef STARBYTES_RT_RTSTDLIB_H
#define STARBYTES_RT_RTSTDLIB_H

namespace starbytes {
namespace Runtime::stdlib {


void print(RTObject *object);

/// Array Object

void array_push(RTInternalObject::ArrayParams &array,RTObject *any);
RTObject *array_index(RTInternalObject::ArrayParams &array,unsigned idx);
unsigned array_length(RTInternalObject::ArrayParams &array);


/// Number Object

starbytes_float_t num_add(RTInternalObject::NumberParams & lhs,RTInternalObject::NumberParams & rhs);

starbytes_float_t num_sub(RTInternalObject::NumberParams & lhs,RTInternalObject::NumberParams & rhs);
    
/// String Object

void string_concat(RTInternalObject::StringParams & in1,RTInternalObject::StringParams & in2,std::string & out);

void string_substring(RTInternalObject::StringParams & in,unsigned idx,unsigned len,std::string & out);

unsigned string_length(RTInternalObject::StringParams & in);

}
}

#endif
