#include "starbytes/ByteCode/BCDef.h"


STARBYTES_BYTECODE_NAMESPACE

#define SET_BC_TYPE(bc_class,val) BCType bc_class::static_type = BCType::val

SET_BC_TYPE(BCReference,Reference);
SET_BC_TYPE(BCCodeBegin,CodeBegin);
SET_BC_TYPE(BCCodeEnd,CodeEnd);
SET_BC_TYPE(BCVectorBegin,VectorBegin);
SET_BC_TYPE(BCVectorEnd,VectorEnd);

#undef SET_BC_TYPE


NAMESPACE_END