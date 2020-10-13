#include "starbytes/Base/Base.h"
#include <string>
#include <vector>

#ifndef BYTECODE_BCDEF_H
#define BYTECODE_BCDEF_H



STARBYTES_BYTECODE_NAMESPACE



TYPED_ENUM BCType:int {
    Identifier,Digit
};

struct BCUnit {
    BCType type; 
    BCUnit(BCType _type):type(_type){};
};

struct BCIdentifier : BCUnit {
    static BCType static_type;
    BCIdentifier():BCUnit(BCType::Identifier){};
};

struct BCDigit : BCUnit {
    static BCType static_type;
    BCDigit():BCUnit(BCType::Digit){};
    unsigned value;
};

struct BCProgram {
    std::string program_name;
    std::vector<BCUnit *> units;
};

template<typename _BCTy>
bool bc_unit_is(BCUnit *unit){
    return unit->type == _BCTy::static_type;
};

#define BC_UNIT_IS(ptr,type) bc_unit_is<type>((BCUnit *)ptr)
#define ASSERT_BC_UNIT(ptr,type) ((type *)ptr)

NAMESPACE_END

#endif