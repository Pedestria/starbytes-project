
#include "starbytes/compiler/RTCode.h"
#include "starbytes/compiler/ASTNodes.def"
#include <fstream>
#include <iostream>
#include <cmath>

namespace starbytes {
namespace Runtime {

    #define RTCODE_STREAM_OBJECT_IN_IMPL(object) \
    std::istream & operator >>(std::istream & is,object * obj)

    #define RTCODE_STREAM_OBJECT_OUT_IMPL(object) \
    std::ostream & operator <<(std::ostream & os,object * obj)

    RTCODE_STREAM_OBJECT_IN_IMPL(RTID){
        is.read((char *)&obj->len,sizeof(size_t));
        auto *buf = new char[obj->len];
        is.read((char *)buf,obj->len);
        obj->value = buf;
        return is;
    }

    RTCODE_STREAM_OBJECT_OUT_IMPL(RTID){
        os.write((const char *)&obj->len,sizeof(size_t));
        os.write(obj->value,obj->len);
        return os;
    }

    static void writeAttributeList(std::ostream &os, std::vector<RTAttribute> &attributes){
        unsigned attrCount = attributes.size();
        os.write((char *)&attrCount,sizeof(attrCount));
        for(auto &attr : attributes){
            os << &attr;
        }
    }

    static void readAttributeList(std::istream &is, std::vector<RTAttribute> &attributes){
        unsigned attrCount = 0;
        is.read((char *)&attrCount,sizeof(attrCount));
        while(attrCount > 0){
            RTAttribute attr;
            is >> &attr;
            attributes.push_back(std::move(attr));
            --attrCount;
        }
    }

    RTCODE_STREAM_OBJECT_IN_IMPL(RTAttributeArg){
        is.read((char *)&obj->hasName,sizeof(obj->hasName));
        if(obj->hasName){
            is >> &obj->name;
        }
        is.read((char *)&obj->valueType,sizeof(obj->valueType));
        is >> &obj->value;
        return is;
    }

    RTCODE_STREAM_OBJECT_OUT_IMPL(RTAttributeArg){
        os.write((char *)&obj->hasName,sizeof(obj->hasName));
        if(obj->hasName){
            os << &obj->name;
        }
        os.write((char *)&obj->valueType,sizeof(obj->valueType));
        os << &obj->value;
        return os;
    }

    RTCODE_STREAM_OBJECT_IN_IMPL(RTAttribute){
        is >> &obj->name;
        unsigned argCount = 0;
        is.read((char *)&argCount,sizeof(argCount));
        while(argCount > 0){
            RTAttributeArg arg;
            is >> &arg;
            obj->args.push_back(std::move(arg));
            --argCount;
        }
        return is;
    }

    RTCODE_STREAM_OBJECT_OUT_IMPL(RTAttribute){
        os << &obj->name;
        unsigned argCount = obj->args.size();
        os.write((char *)&argCount,sizeof(argCount));
        for(auto &arg : obj->args){
            os << &arg;
        }
        return os;
    }

    RTCODE_STREAM_OBJECT_IN_IMPL(RTVar){
        is >> &obj->id;
        is.read((char *)&obj->hasInitValue,sizeof(obj->hasInitValue));
        readAttributeList(is,obj->attributes);
        return is;
    }

    RTCODE_STREAM_OBJECT_OUT_IMPL(RTVar){
        RTCode code = CODE_RTVAR;
        os.write((char *)&code,sizeof(RTCode));
        os << &obj->id;
        os.write((char *)&obj->hasInitValue,sizeof(obj->hasInitValue));
        writeAttributeList(os,obj->attributes);
        return os;
    }

    RTCODE_STREAM_OBJECT_IN_IMPL(RTFuncTemplate){
        is >> &obj->name;
        readAttributeList(is,obj->attributes);
        unsigned argCount;
        is.read((char *)&argCount,sizeof(argCount));
        while(argCount > 0){
            RTID id;
            is >> &id;
            obj->argsTemplate.push_back(std::move(id));
            --argCount;
        };
        auto checkpoint = is.tellg();
        RTCode code = CODE_MODULE_END;
        is.read((char *)&code,sizeof(RTCode));
        if(code == CODE_RTFUNCBLOCK_BEGIN){
            obj->block_start_pos = is.tellg();
            while(code != CODE_RTFUNCBLOCK_END)
                is.read((char *)&code,sizeof(RTCode));
        }
        else {
            is.seekg(checkpoint);
        }
        return is;
    }

    RTCODE_STREAM_OBJECT_OUT_IMPL(RTFuncTemplate){
        /// Just output basic template..
        RTCode code = CODE_RTFUNC;
        os.write((char *)&code,sizeof(RTCode));
        os << &obj->name;
        writeAttributeList(os,obj->attributes);
        unsigned argCount = obj->argsTemplate.size();
        os.write((char *)&argCount,sizeof(argCount));
        for(auto & arg : obj->argsTemplate){
            os << &arg;
        };
        return os;
    }

    RTCODE_STREAM_OBJECT_IN_IMPL(RTClass){
        is >> &obj->name;
        readAttributeList(is,obj->attributes);
        unsigned fieldCount = 0;
        is.read((char *)&fieldCount,sizeof(fieldCount));
        while(fieldCount > 0){
            RTCode code = CODE_MODULE_END;
            is.read((char *)&code,sizeof(code));
            if(code != CODE_RTVAR){
                break;
            }
            RTVar field;
            is >> &field;
            obj->fields.push_back(std::move(field));
            --fieldCount;
        }
        unsigned methodCount = 0;
        is.read((char *)&methodCount,sizeof(methodCount));
        while(methodCount > 0){
            RTCode code = CODE_MODULE_END;
            is.read((char *)&code,sizeof(code));
            if(code != CODE_RTFUNC){
                break;
            }
            RTFuncTemplate method;
            is >> &method;
            obj->methods.push_back(std::move(method));
            --methodCount;
        }
        return is;
    }

    RTCODE_STREAM_OBJECT_OUT_IMPL(RTClass){
        RTCode code = CODE_RTCLASS_DEF;
        os.write((char *)&code,sizeof(RTCode));
        os << &obj->name;
        writeAttributeList(os,obj->attributes);

        unsigned fieldCount = obj->fields.size();
        os.write((char *)&fieldCount,sizeof(fieldCount));
        for(auto &field : obj->fields){
            os << &field;
        }

        unsigned methodCount = obj->methods.size();
        os.write((char *)&methodCount,sizeof(methodCount));
        for(auto &method : obj->methods){
            os << &method;
        }
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
            double intPart = 0.0;
            if(std::modf(val,&intPart) == 0.0){
                *obj = StarbytesNumNew(NumTypeInt,(int)intPart);
            }
            else {
                *obj = StarbytesNumNew(NumTypeFloat,val);
            }
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
            auto floatObj = StarbytesNumConvertTo(*obj,NumTypeFloat);
            starbytes_float_t f = StarbytesNumGetFloatValue(floatObj);
            StarbytesObjectRelease(floatObj);
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
