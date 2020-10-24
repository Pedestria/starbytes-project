#include "starbytes/Base/Base.h"
#include <string>
#include <vector>

#ifndef BYTECODE_BCDEF_H
#define BYTECODE_BCDEF_H



STARBYTES_BYTECODE_NAMESPACE


TYPED_ENUM BCType:int {
    CodeBegin,CodeEnd,VectorBegin,VectorEnd,Reference
};

TYPED_ENUM BCRefType:int {
    Direct,Indirect
};

struct BCUnit {
    BCType type;
};

struct BCReference : BCUnit {
    static BCType static_type;
    BCRefType ref_type;
    std::string var_name;
};

struct BCVectorBegin : BCUnit {
    static BCType static_type;
    std::string name;
};

struct BCVectorEnd : BCUnit {
    static BCType static_type;
    std::string name;
};

struct BCCodeBegin: BCUnit {
    static BCType static_type;
    std::string code_node_name;
};

struct BCCodeEnd : BCUnit {
     static BCType static_type;
     std::string code_node_name;
};


struct BCProgram {
    std::string program_name;
    std::vector<BCUnit> units;
};


NAMESPACE_END

#endif