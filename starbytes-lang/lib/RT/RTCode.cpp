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
        };
        return is;
    };

    void __output_rt_interal_obj(std::ostream & os,RTInternalObject *obj){
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
    };

    RTCODE_STREAM_OBJECT_OUT_IMPL(RTInternalObject) {
        RTCode code = CODE_RTINTOBJCREATE;
        os.write((char *)&code,sizeof(RTCode));
        std::cout << "TYPE:" << obj->type << std::endl;
        
        /// WTF!!
        __output_rt_interal_obj(os,obj);
        return os;
    };

};
}
