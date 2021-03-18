#include "starbytes/ByteCode/BCDef.h"

STARBYTES_BYTECODE_NAMESPACE

void bcid_to_cpp_str(BCId & id_ref,std::string & str_ref){
    str_ref = id_ref.data();
};

NAMESPACE_END
