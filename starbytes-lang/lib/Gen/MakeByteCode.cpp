#include "MakeByteCode.h"

STARBYTES_GEN_NAMESPACE

ByteCode::BCString * make_bc_string(std::string & value){
    ByteCode::BCString * ptr = new ByteCode::BCString();
    ptr->value = value;
    return ptr;
};

ByteCode::BCCodeBegin * make_bc_code_begin(std::string & method_name){
    ByteCode::BCCodeBegin * ptr = new ByteCode::BCCodeBegin();
    ptr->code_node_name = method_name;
    return ptr;
};
ByteCode::BCCodeEnd * make_bc_code_end(std::string & method_name){
    ByteCode::BCCodeEnd * ptr = new ByteCode::BCCodeEnd();
    ptr->code_node_name = method_name;
    return ptr;
};

NAMESPACE_END