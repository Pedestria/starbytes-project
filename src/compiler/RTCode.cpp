
#include "starbytes/compiler/RTCode.h"
#include "starbytes/compiler/ASTNodes.def"
#include <fstream>
#include <iostream>
#include <cmath>
#include <cstring>

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

    static void skipRTIDPayload(std::istream &is){
        size_t len = 0;
        is.read((char *)&len,sizeof(size_t));
        if(len > 0){
            is.seekg((std::streamoff)len,std::ios_base::cur);
        }
    }

    static void skipAttributeListPayload(std::istream &is){
        unsigned attrCount = 0;
        is.read((char *)&attrCount,sizeof(attrCount));
        while(attrCount > 0){
            skipRTIDPayload(is);
            unsigned argCount = 0;
            is.read((char *)&argCount,sizeof(argCount));
            while(argCount > 0){
                bool hasName = false;
                is.read((char *)&hasName,sizeof(hasName));
                if(hasName){
                    skipRTIDPayload(is);
                }
                RTAttrValueType valueType = RTATTR_VALUE_IDENTIFIER;
                is.read((char *)&valueType,sizeof(valueType));
                (void)valueType;
                skipRTIDPayload(is);
                --argCount;
            }
            --attrCount;
        }
    }

    static void skipExpr(std::istream &is);
    static void skipRuntimeStmt(std::istream &is,RTCode code);
    static void skipRuntimeBlock(std::istream &is,RTCode endCode);
    static void skipFuncTemplatePayload(std::istream &is);
    static void skipClassPayload(std::istream &is);

    static void skipRuntimeObjectPayload(std::istream &is){
        RTCode internalCode = CODE_MODULE_END;
        is.read((char *)&internalCode,sizeof(RTCode));
        switch(internalCode){
            case RTINTOBJ_STR:
                skipRTIDPayload(is);
                break;
            case RTINTOBJ_BOOL:
                is.seekg((std::streamoff)sizeof(bool),std::ios_base::cur);
                break;
            case RTINTOBJ_ARRAY: {
                unsigned arrayLen = 0;
                is.read((char *)&arrayLen,sizeof(arrayLen));
                while(arrayLen > 0){
                    RTCode elementCode = CODE_MODULE_END;
                    is.read((char *)&elementCode,sizeof(RTCode));
                    if(elementCode == CODE_RTINTOBJCREATE || elementCode == CODE_RTOBJCREATE){
                        skipRuntimeObjectPayload(is);
                    }
                    --arrayLen;
                }
                break;
            }
            case RTINTOBJ_NUM:
                is.seekg((std::streamoff)sizeof(starbytes_float_t),std::ios_base::cur);
                break;
            case RTINTOBJ_DICTIONARY:
                break;
            default:
                is.seekg((std::streamoff)sizeof(StarbytesClassType),std::ios_base::cur);
                break;
        }
    }

    static void skipExprFromCode(std::istream &is,RTCode code){
        switch(code){
            case CODE_RTINTOBJCREATE:
            case CODE_RTOBJCREATE:
                skipRuntimeObjectPayload(is);
                break;
            case CODE_RTVAR_REF:
            case CODE_RTFUNC_REF:
                skipRTIDPayload(is);
                break;
            case CODE_RTIVKFUNC: {
                skipExpr(is);
                unsigned argCount = 0;
                is.read((char *)&argCount,sizeof(argCount));
                while(argCount > 0){
                    skipExpr(is);
                    --argCount;
                }
                break;
            }
            case CODE_UNARY_OPERATOR: {
                RTCode unaryCode = UNARY_OP_NOT;
                is.read((char *)&unaryCode,sizeof(unaryCode));
                (void)unaryCode;
                skipExpr(is);
                break;
            }
            case CODE_BINARY_OPERATOR: {
                RTCode binaryCode = BINARY_OP_PLUS;
                is.read((char *)&binaryCode,sizeof(binaryCode));
                (void)binaryCode;
                skipExpr(is);
                skipExpr(is);
                break;
            }
            case CODE_RTNEWOBJ: {
                skipRTIDPayload(is);
                unsigned argCount = 0;
                is.read((char *)&argCount,sizeof(argCount));
                while(argCount > 0){
                    skipExpr(is);
                    --argCount;
                }
                break;
            }
            case CODE_RTMEMBER_GET:
                skipExpr(is);
                skipRTIDPayload(is);
                break;
            case CODE_RTMEMBER_SET:
                skipExpr(is);
                skipRTIDPayload(is);
                skipExpr(is);
                break;
            case CODE_RTMEMBER_IVK: {
                skipExpr(is);
                skipRTIDPayload(is);
                unsigned argCount = 0;
                is.read((char *)&argCount,sizeof(argCount));
                while(argCount > 0){
                    skipExpr(is);
                    --argCount;
                }
                break;
            }
            case CODE_RTREGEX_LITERAL:
                skipRTIDPayload(is);
                skipRTIDPayload(is);
                break;
            case CODE_RTVAR_SET:
                skipRTIDPayload(is);
                skipExpr(is);
                break;
            case CODE_RTINDEX_GET:
                skipExpr(is);
                skipExpr(is);
                break;
            case CODE_RTINDEX_SET:
                skipExpr(is);
                skipExpr(is);
                skipExpr(is);
                break;
            case CODE_RTDICT_LITERAL: {
                unsigned pairCount = 0;
                is.read((char *)&pairCount,sizeof(pairCount));
                while(pairCount > 0){
                    skipExpr(is);
                    skipExpr(is);
                    --pairCount;
                }
                break;
            }
            case CODE_RTTYPECHECK:
                skipExpr(is);
                skipRTIDPayload(is);
                break;
            default:
                break;
        }
    }

    static void skipExpr(std::istream &is){
        RTCode code = CODE_MODULE_END;
        is.read((char *)&code,sizeof(RTCode));
        skipExprFromCode(is,code);
    }

    static void skipRuntimeBlock(std::istream &is,RTCode endCode){
        RTCode code = CODE_MODULE_END;
        is.read((char *)&code,sizeof(RTCode));
        while(is.good() && code != endCode){
            skipRuntimeStmt(is,code);
            is.read((char *)&code,sizeof(RTCode));
        }
    }

    static void skipFuncTemplatePayload(std::istream &is){
        skipRTIDPayload(is);
        skipAttributeListPayload(is);
        unsigned argCount = 0;
        is.read((char *)&argCount,sizeof(argCount));
        while(argCount > 0){
            skipRTIDPayload(is);
            --argCount;
        }
        bool isLazy = false;
        is.read((char *)&isLazy,sizeof(isLazy));
        (void)isLazy;
        auto checkpoint = is.tellg();
        RTCode code = CODE_MODULE_END;
        is.read((char *)&code,sizeof(RTCode));
        if(code == CODE_RTFUNCBLOCK_BEGIN){
            skipRuntimeBlock(is,CODE_RTFUNCBLOCK_END);
        }
        else {
            is.seekg(checkpoint);
        }
    }

    static void skipClassPayload(std::istream &is){
        skipRTIDPayload(is);
        skipAttributeListPayload(is);
        bool hasSuperClass = false;
        is.read((char *)&hasSuperClass,sizeof(hasSuperClass));
        if(hasSuperClass){
            skipRTIDPayload(is);
        }

        unsigned fieldCount = 0;
        is.read((char *)&fieldCount,sizeof(fieldCount));
        while(fieldCount > 0){
            RTCode code = CODE_MODULE_END;
            is.read((char *)&code,sizeof(code));
            if(code != CODE_RTVAR){
                break;
            }
            skipRTIDPayload(is);
            bool hasInitValue = false;
            is.read((char *)&hasInitValue,sizeof(hasInitValue));
            skipAttributeListPayload(is);
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
            skipFuncTemplatePayload(is);
            --methodCount;
        }

        unsigned ctorCount = 0;
        is.read((char *)&ctorCount,sizeof(ctorCount));
        while(ctorCount > 0){
            RTCode code = CODE_MODULE_END;
            is.read((char *)&code,sizeof(code));
            if(code != CODE_RTFUNC){
                break;
            }
            skipFuncTemplatePayload(is);
            --ctorCount;
        }

        bool hasFieldInitFunc = false;
        is.read((char *)&hasFieldInitFunc,sizeof(hasFieldInitFunc));
        if(hasFieldInitFunc){
            skipRTIDPayload(is);
        }
    }

    static void skipRuntimeStmt(std::istream &is,RTCode code){
        if(code == CODE_RTVAR){
            skipRTIDPayload(is);
            bool hasInitValue = false;
            is.read((char *)&hasInitValue,sizeof(hasInitValue));
            skipAttributeListPayload(is);
            if(hasInitValue){
                skipExpr(is);
            }
            return;
        }
        if(code == CODE_RTRETURN){
            bool hasValue = false;
            is.read((char *)&hasValue,sizeof(hasValue));
            if(hasValue){
                skipExpr(is);
            }
            return;
        }
        if(code == CODE_RTSECURE_DECL){
            skipRTIDPayload(is);
            bool hasCatchBinding = false;
            is.read((char *)&hasCatchBinding,sizeof(hasCatchBinding));
            if(hasCatchBinding){
                skipRTIDPayload(is);
            }
            bool hasCatchType = false;
            is.read((char *)&hasCatchType,sizeof(hasCatchType));
            if(hasCatchType){
                skipRTIDPayload(is);
            }
            skipExpr(is);
            RTCode blockBegin = CODE_MODULE_END;
            is.read((char *)&blockBegin,sizeof(blockBegin));
            if(blockBegin == CODE_RTBLOCK_BEGIN){
                skipRuntimeBlock(is,CODE_RTBLOCK_END);
            }
            return;
        }
        if(code == CODE_CONDITIONAL){
            unsigned condSpecCount = 0;
            is.read((char *)&condSpecCount,sizeof(condSpecCount));
            while(condSpecCount > 0){
                RTCode specType = COND_TYPE_IF;
                is.read((char *)&specType,sizeof(specType));
                if(specType != COND_TYPE_ELSE){
                    skipExpr(is);
                }
                RTCode blockBegin = CODE_MODULE_END;
                is.read((char *)&blockBegin,sizeof(blockBegin));
                if(blockBegin == CODE_RTBLOCK_BEGIN){
                    skipRuntimeBlock(is,CODE_RTBLOCK_END);
                }
                else if(blockBegin == CODE_RTFUNCBLOCK_BEGIN){
                    skipRuntimeBlock(is,CODE_RTFUNCBLOCK_END);
                }
                --condSpecCount;
            }
            RTCode conditionalEnd = CODE_MODULE_END;
            is.read((char *)&conditionalEnd,sizeof(conditionalEnd));
            (void)conditionalEnd;
            return;
        }
        if(code == CODE_RTFUNC){
            skipFuncTemplatePayload(is);
            return;
        }
        if(code == CODE_RTCLASS_DEF){
            skipClassPayload(is);
            return;
        }
        skipExprFromCode(is,code);
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
        is.read((char *)&obj->isLazy,sizeof(obj->isLazy));
        is.read((char *)&obj->blockByteSize,sizeof(obj->blockByteSize));
        auto checkpoint = is.tellg();
        RTCode code = CODE_MODULE_END;
        is.read((char *)&code,sizeof(RTCode));
        if(code == CODE_RTFUNCBLOCK_BEGIN){
            obj->block_start_pos = is.tellg();
            if(obj->blockByteSize > 0){
                is.seekg((std::streamoff)obj->blockByteSize,std::ios_base::cur);
            }
            RTCode endCode = CODE_MODULE_END;
            is.read((char *)&endCode,sizeof(RTCode));
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
        os.write((char *)&obj->isLazy,sizeof(obj->isLazy));
        os.write((char *)&obj->blockByteSize,sizeof(obj->blockByteSize));
        return os;
    }

    RTCODE_STREAM_OBJECT_IN_IMPL(RTClass){
        is >> &obj->name;
        readAttributeList(is,obj->attributes);
        is.read((char *)&obj->hasSuperClass,sizeof(obj->hasSuperClass));
        if(obj->hasSuperClass){
            is >> &obj->superClassName;
        }
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
        unsigned ctorCount = 0;
        is.read((char *)&ctorCount,sizeof(ctorCount));
        while(ctorCount > 0){
            RTCode code = CODE_MODULE_END;
            is.read((char *)&code,sizeof(code));
            if(code != CODE_RTFUNC){
                break;
            }
            RTFuncTemplate ctorTemplate;
            is >> &ctorTemplate;
            obj->constructors.push_back(std::move(ctorTemplate));
            --ctorCount;
        }
        is.read((char *)&obj->hasFieldInitFunc,sizeof(obj->hasFieldInitFunc));
        if(obj->hasFieldInitFunc){
            is >> &obj->fieldInitFuncName;
        }
        return is;
    }

    RTCODE_STREAM_OBJECT_OUT_IMPL(RTClass){
        RTCode code = CODE_RTCLASS_DEF;
        os.write((char *)&code,sizeof(RTCode));
        os << &obj->name;
        writeAttributeList(os,obj->attributes);
        os.write((char *)&obj->hasSuperClass,sizeof(obj->hasSuperClass));
        if(obj->hasSuperClass){
            os << &obj->superClassName;
        }

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

        unsigned ctorCount = obj->constructors.size();
        os.write((char *)&ctorCount,sizeof(ctorCount));
        for(auto &ctorTemplate : obj->constructors){
            os << &ctorTemplate;
        }
        os.write((char *)&obj->hasFieldInitFunc,sizeof(obj->hasFieldInitFunc));
        if(obj->hasFieldInitFunc){
            os << &obj->fieldInitFuncName;
        }
        return os;
    }

    RTCODE_STREAM_OBJECT_IN_IMPL(StarbytesObject) {
        RTCode code2;
        
        is.read((char *)&code2,sizeof(RTCode));
        if(code2 == RTINTOBJ_STR){
            
            
            RTID strVal;
            is >> &strVal;
            auto *buf = new char[strVal.len + 1];
            std::memcpy(buf,strVal.value,strVal.len);
            buf[strVal.len] = '\0';
            *obj = StarbytesStrNewWithData(buf);
            delete[] buf;
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
