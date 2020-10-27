#include "starbytes/Base/Base.h"
#include "STDObjects.h"

#ifndef INTERPRETER_RTTYPECHECK_H
#define INTERPRETER_RTTYPECHECK_H

STARBYTES_INTERPRETER_NAMESPACE

enum class DynamicTyType:int {
    Class,Function,Interface,Type,StandardLib
};

struct DynType_Args {};

struct DynTy_ClassArgs : DynType_Args {
    std::vector<std::string> props;
    std::vector<std::string> methods;
};
class StarbytesDynamicType {
    public:
        StarbytesDynamicType(std::string & _name,DynamicTyType & _type):name(_name),type(_type){};
        std::string name;
        DynamicTyType type;
        DynType_Args * args;
        ~StarbytesDynamicType(){};
};

StarbytesDynamicType * create_dynamic_type(std::string name,DynamicTyType type);
bool check_typeof_object(StarbytesDynamicType *type,StarbytesObject *object_to_check);

NAMESPACE_END

#endif