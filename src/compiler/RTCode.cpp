
#include "starbytes/compiler/RTCode.h"
#include "starbytes/compiler/ASTNodes.def"
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
    }

    RTCODE_STREAM_OBJECT_OUT_IMPL(RTID){
        os.write((const char *)&obj->len,sizeof(size_t));
        os.write(obj->value,obj->len);
        return os;
    }

    RTCODE_STREAM_OBJECT_IN_IMPL(RTVar){
        is >> &obj->id;
        is.read((char *)&obj->hasInitValue,sizeof(obj->hasInitValue));
        return is;
    }

    RTCODE_STREAM_OBJECT_OUT_IMPL(RTVar){
        RTCode code = CODE_RTVAR;
        os.write((char *)&code,sizeof(RTCode));
        os << &obj->id;
        os.write((char *)&obj->hasInitValue,sizeof(obj->hasInitValue));
        return os;
    }

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
        if(code == CODE_RTFUNCBLOCK_BEGIN){
            obj->block_start_pos = is.tellg();
            while(code != CODE_RTFUNCBLOCK_END)
                is.read((char *)&code,sizeof(RTCode));
        };
        return is;
    }

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
    }

    RTCODE_STREAM_OBJECT_IN_IMPL(RTClass){
        is >> &obj->name;
        // is.read((char *)&obj->module_id,sizeof(size_t));
        return is;
    }

    RTCODE_STREAM_OBJECT_OUT_IMPL(RTClass){
        RTCode code = CODE_RTCLASS_DEF;
        os.write((char *)&code,sizeof(RTCode));
        os << &obj->name;
        // os.write((char *)&obj->module_id,sizeof(size_t));
        return os;
    }

    RTCODE_STREAM_OBJECT_IN_IMPL(StarbytesObject) {
        RTCode code2;
        
        is.read((char *)&code2,sizeof(RTCode));
        if(code2 == RTINTOBJ_STR){
            
            
            RTID strVal;
            is >> &strVal;
            *obj = StarbytesStrNewWithData(strVal.value);
        }
        else if(code2 == RTINTOBJ_BOOL){
            bool val;
            
            is.read((char *)&val,sizeof(bool));
            *obj = StarbytesBoolNew((StarbytesBoolVal)val);
        }
        else if(code2 == RTINTOBJ_ARRAY){
            
            unsigned array_len;
            is.read((char *)&array_len,sizeof(array_len));
            *obj = StarbytesArrayNew();
            for(unsigned i = 0;i < array_len;i++){
                RTCode code3;
                is.read((char *)&code3,sizeof(RTCode));
                if(code3 == CODE_RTINTOBJCREATE || code3 == CODE_RTOBJCREATE){
                    StarbytesObject _obj;
                    is >> &_obj;
                    StarbytesArrayPush(*obj,_obj);
                }
            };
        }
        else if(code2 == RTINTOBJ_NUM){
            starbytes_float_t val;
            is.read((char *)&val,sizeof(starbytes_float_t));
            *obj = StarbytesNumNew(NumTypeInt,(int)val);
        }
        else if(code2 == RTINTOBJ_DICTIONARY){
            
        }
        else {
            /// Special Process for importing custom class objects.
            StarbytesClassType id;
            is.read((char *)&id,sizeof(id));
        }
        return is;
    }

    void __output_rt_internal_obj(std::ostream & os,StarbytesObject * obj){
        RTCode code2;
        
        if(StarbytesObjectTypecheck(*obj,StarbytesStrType())){
            code2 = RTINTOBJ_STR;
            os.write((char *)&code2,sizeof(RTCode));
            
            RTID strVal;
            strVal.len = StarbytesStrLength(*obj);
            strVal.value = StarbytesStrGetBuffer(*obj);
            os << &strVal;
        }
        else if(StarbytesObjectTypecheck(*obj,StarbytesBoolType())){
            code2 = RTINTOBJ_BOOL;
            os.write((char *)&code2,sizeof(RTCode));
            StarbytesBoolVal val = StarbytesBoolValue(*obj);
            os.write((char *)&val,sizeof(bool));
        }
        else if(StarbytesObjectTypecheck(*obj,StarbytesArrayType())){
            code2 = RTINTOBJ_ARRAY;
            os.write((char *)&code2,sizeof(RTCode));
            
            /// Write Num of elements in array as a unsigned int;
            unsigned array_len = StarbytesArrayGetLength(*obj);
            os.write((char *)&array_len,sizeof(array_len));
            for(unsigned idx = 0;idx < array_len;idx++){
                auto _obj = StarbytesArrayIndex(*obj,idx);
                os << &_obj;
            };
        }
        else if(StarbytesObjectTypecheck(*obj,StarbytesNumType())){
            code2 = RTINTOBJ_NUM;
            os.write((char *)&code2,sizeof(RTCode));
            starbytes_float_t f;
            f = StarbytesNumGetFloatValue(*obj);
            os.write((char *)&f,sizeof(starbytes_float_t));
        }
        else {
        /// Special Process for exporting custom class objects.
            StarbytesClassType id = StarbytesClassObjectGetClass(*obj);
            os.write((char *)&id,sizeof(id));
        }
    }

    RTCODE_STREAM_OBJECT_OUT_IMPL(StarbytesObject) {
        RTCode code = CODE_RTINTOBJCREATE;
        os.write((char *)&code,sizeof(RTCode));
//        std::cout << "TYPE:" << std::endl;
        
        /// WTF!!
        __output_rt_internal_obj(os,obj);
        return os;
    }

//    RTFuncRefObject::RTFuncRefObject(llvm::StringRef name,RTFuncTemplate *temp):name(name),funcTemp(temp){
//
//    };

}
}
