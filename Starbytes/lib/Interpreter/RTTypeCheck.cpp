#include "starbytes/Interpreter/RTTypeCheck.h"

STARBYTES_INTERPRETER_NAMESPACE

StarbytesDynamicType * create_dynamic_type(std::string name,DynamicTyType type){
    StarbytesDynamicType *rc = new StarbytesDynamicType(name,type);
    return rc;
}

#define TYPECHECK(string_name,_type)  if(type->name == string_name){ \
            if(object_to_check->isType(_type)){ \
                returncode = true; \
            } \
        }
//IS object of specified type!
bool check_typeof_object(StarbytesDynamicType *type,StarbytesObject *object_to_check){
    bool returncode = false;
    if(type->type == DynamicTyType::StandardLib){
        TYPECHECK("String",SBObjectType::String)
        else TYPECHECK("Number",SBObjectType::Number)
        else TYPECHECK("Dictionary",SBObjectType::Dictionary)
        else TYPECHECK("Array",SBObjectType::Array)
    }
    else if(type->type == DynamicTyType::Class){
        if(object_to_check->isType(SBObjectType::ClassInstance)){
            
        }
    }
    return returncode;
};

NAMESPACE_END