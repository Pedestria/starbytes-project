#define __RUNTIME__ 1
#include "starbytes/RT/RTCode.h"
#include <fstream>
#include <iostream>

namespace starbytes {
namespace Runtime {

    #define RTCODE_STREAM_OBJECT_IN_IMPL(object) \
    std::istream & operator >>(std::istream & is,object * obj)

    #define RTCODE_STREAM_OBJECT_OUT_IMPL(object) \
    std::ostream & operator <<(std::ostream & os,object * obj)

    RTCODE_STREAM_OBJECT_IN_IMPL(RTID){
        is.read((char *)&obj->len,sizeof(size_t));
        obj->value = new char[obj->len];
        is.read((char *)obj->value,obj->len);
        return is;
    };

    RTCODE_STREAM_OBJECT_OUT_IMPL(RTID){
        os.write((const char *)&obj->len,sizeof(size_t));
        os.write(obj->value,obj->len);
        return os;
    };

    RTCODE_STREAM_OBJECT_IN_IMPL(RTVar){
        is >> &obj->id;
        is.read((char *)&obj->hasInitValue,sizeof(obj->hasInitValue));
        return is;
    };

    RTCODE_STREAM_OBJECT_OUT_IMPL(RTVar){
        RTCode code = CODE_RTVAR;
        os.write((char *)&code,sizeof(RTCode));
        os << &obj->id;
        os.write((char *)&obj->hasInitValue,sizeof(obj->hasInitValue));
        return os;
    };

    RTCODE_STREAM_OBJECT_IN_IMPL(RTFuncTemplate){
        is >> &obj->name;
        unsigned argCount;
        is.read((char *)&argCount,sizeof(argCount));
        while(argCount > 0){
            RTID id;
            is >> &id;
            obj->argsTemplate.push_back(std::move(id));
            --argCount;
        };
        RTCode code;
        is.read((char *)&code,sizeof(RTCode));
        if(code == CODE_RTBLOCK_BEGIN){
            obj->block_start_pos = is.tellg() - fpos_t(1);
            while(code != CODE_RTBLOCK_END)
                is.read((char *)&code,sizeof(RTCode));
        };
        return is;
    };

    RTCODE_STREAM_OBJECT_OUT_IMPL(RTFuncTemplate){
        /// Just output basic template..
        RTCode code = CODE_RTFUNC;
        os.write((char *)&code,sizeof(RTCode));
        os << &obj->name;
        unsigned argCount = obj->argsTemplate.size();
        os.write((char *)&argCount,sizeof(argCount));
        for(auto & arg : obj->argsTemplate){
            os << &arg;
        };
        return os;
    };

    RTCODE_STREAM_OBJECT_IN_IMPL(RTInternalObject) {
        RTCode code2;
        obj->isInternal = true;
        is.read((char *)&code2,sizeof(RTCode));
        if(code2 == RTINTOBJ_STR){
            obj->type = RTINTOBJ_STR;
            
            RTID strVal;
            is >> &strVal;
            RTInternalObject::StringParams *params = new RTInternalObject::StringParams();
            params->str = std::string(strVal.value,strVal.len);
            obj->data = params;
        }
        else if(code2 == RTINTOBJ_BOOL){
            obj->type = RTINTOBJ_BOOL;
            RTInternalObject::BoolParams *params = new RTInternalObject::BoolParams();
            is.read((char *)&params->value,sizeof(bool));
            obj->data = params;
        }
        else if(code2 == RTINTOBJ_ARRAY){
            obj->type = RTINTOBJ_ARRAY;
            auto *params = new RTInternalObject::ArrayParams();
            unsigned array_len;
            is.read((char *)&array_len,sizeof(array_len));
            for(unsigned i = 0;i < array_len;i++){
                RTCode code3;
                is.read((char *)&code3,sizeof(RTCode));
                if(code3 == CODE_RTINTOBJCREATE){
                    RTInternalObject *child_object = new RTInternalObject();
                    is >> child_object;
                    params->data.push_back(child_object);
                }
//                else if(code3 == CODE_RTOBJCREATE){
//
//                };
            };
            obj->data = params;
        }
        return is;
    };

    void __output_rt_internal_obj(std::ostream & os,RTInternalObject *obj){
        RTCode code2;
        if(obj->type == RTINTOBJ_STR){
            code2 = RTINTOBJ_STR;
            os.write((char *)&code2,sizeof(RTCode));
            RTInternalObject::StringParams *params = (RTInternalObject::StringParams *)obj->data;
            RTID strVal;
            strVal.len = params->str.size();
            strVal.value = params->str.data();
            os << &strVal;
        }
        else if(obj->type == RTINTOBJ_BOOL){
            code2 = RTINTOBJ_BOOL;
            os.write((char *)&code2,sizeof(RTCode));
            RTInternalObject::BoolParams *params = (RTInternalObject::BoolParams *)obj->data;
            os.write((char *)&params->value,sizeof(bool));
        }
        else if(obj->type == RTINTOBJ_ARRAY){
            code2 = RTINTOBJ_ARRAY;
            os.write((char *)&code2,sizeof(RTCode));
            RTInternalObject::ArrayParams *params = (RTInternalObject::ArrayParams *)obj->data;
            /// Write Num of elements in array as a unsigned int;
            unsigned array_len = params->data.size();
            os.write((char *)&array_len,sizeof(array_len));
            for(auto & obj : params->data){
                if(obj->isInternal){
                    RTInternalObject *__obj = (RTInternalObject *)obj;
                    os << __obj;
                }
//                else {
//                    os << obj;
//                };
            };
        };
    };

    RTCODE_STREAM_OBJECT_OUT_IMPL(RTInternalObject) {
        RTCode code = CODE_RTINTOBJCREATE;
        os.write((char *)&code,sizeof(RTCode));
        std::cout << "TYPE:" << obj->type << std::endl;
        
        /// WTF!!
        __output_rt_internal_obj(os,obj);
        return os;
    };

};
}
