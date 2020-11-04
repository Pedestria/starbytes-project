#include "starbytes/Base/Base.h"
#include "starbytes/ByteCode/BCDef.h"

#ifndef GEN_MAKEBYTECODE_H
#define GEN_MAKEBYTECODE_H

STARBYTES_GEN_NAMESPACE


    ByteCode::BCString * make_bc_string(std::string & value);
    ByteCode::BCCodeBegin * make_bc_code_begin(std::string & method_name);
    ByteCode::BCCodeEnd * make_bc_code_end(std::string & method_name);

NAMESPACE_END

#endif