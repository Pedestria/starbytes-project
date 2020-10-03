#include "starbytes/Interpreter/STDObjects.h"

STARBYTES_INTERPRETER_NAMESPACE

bool StarbytesString::__is_equal(StarbytesObject *_obj) {
        bool return_code;
        if(_obj->isType(SBObjectType::String)){
            StarbytesString *s = (StarbytesString *)_obj;
            if(s->INTERAL_STRING == INTERAL_STRING){
                return_code = true;
            }
            else{
                return_code = false;
            }
        }
        else{
            return_code = false;
        }
        return return_code;
    }

    void StarbytesString::append(StarbytesString *obj) {
        INTERAL_STRING.append(obj->__get_interal());
    };

    bool StarbytesBoolean::__is_equal(StarbytesObject *_obj){
        bool return_code;
        if(_obj->isType(SBObjectType::Boolean)){
            StarbytesBoolean *s = (StarbytesBoolean *)_obj;
            if(s->INTERNAL_BOOLEAN == INTERNAL_BOOLEAN){
                return_code = true;
            }
            else{
                return_code = false;
            }
        }
        else{
            return_code = false;
        }
        return return_code;
    };

NAMESPACE_END