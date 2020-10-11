#include "starbytes/ByteCode/BCDef.h"


STARBYTES_BYTECODE_NAMESPACE

#define SET_BC_TYPE(bc_class,val) BCType bc_class::type = BCType::val

SET_BC_TYPE(BCIdentifier,Identifier);
SET_BC_TYPE(BCDigit,Digit);

#undef SET_BC_TYPE


NAMESPACE_END