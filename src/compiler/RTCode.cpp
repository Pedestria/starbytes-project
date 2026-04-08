
#include "starbytes/compiler/RTCode.h"
#include "starbytes/compiler/ASTNodes.def"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <sstream>

namespace starbytes {
namespace Runtime {

    namespace {

        constexpr std::array<char,4> kRTModuleMagic = {'S','B','X','M'};

        void rewindModuleProbe(std::istream &is,
                               const char *bytes,
                               size_t size,
                               std::istream::pos_type startPos){
            is.clear();
            if(startPos != std::istream::pos_type(-1)){
                is.seekg(startPos);
                return;
            }
            for(size_t i = size; i > 0; --i){
                is.putback(bytes[i - 1]);
            }
        }

    }

    void writeRTModuleHeader(std::ostream &os,uint16_t bytecodeVersion){
        os.write(kRTModuleMagic.data(),(std::streamsize)kRTModuleMagic.size());
        os.write((char *)&bytecodeVersion,sizeof(bytecodeVersion));
        uint16_t reservedFlags = 0;
        os.write((char *)&reservedFlags,sizeof(reservedFlags));
    }

    RTModuleHeaderInfo prepareRTModuleStream(std::istream &is){
        RTModuleHeaderInfo info;
        auto startPos = is.tellg();
        std::array<char,4> magic = {};
        if(!is.read(magic.data(),(std::streamsize)magic.size())){
            rewindModuleProbe(is,magic.data(),magic.size(),startPos);
            return info;
        }
        if(magic != kRTModuleMagic){
            rewindModuleProbe(is,magic.data(),magic.size(),startPos);
            return info;
        }

        uint16_t bytecodeVersion = RTBYTECODE_VERSION_LEGACY_V1;
        uint16_t reservedFlags = 0;
        if(!is.read((char *)&bytecodeVersion,sizeof(bytecodeVersion))
           || !is.read((char *)&reservedFlags,sizeof(reservedFlags))){
            rewindModuleProbe(is,magic.data(),magic.size(),startPos);
            return info;
        }
        (void)reservedFlags;
        info.bytecodeVersion = bytecodeVersion;
        info.hasExplicitHeader = true;
        return info;
    }

    bool rtBuiltinMemberIdForName(const std::string &name,RTBuiltinMemberId &idOut){
        idOut = RTBUILTIN_MEMBER_INVALID;
        if(name == "isEmpty"){
            idOut = RTBUILTIN_MEMBER_STRING_IS_EMPTY;
            return true;
        }
        if(name == "at"){
            idOut = RTBUILTIN_MEMBER_STRING_AT;
            return true;
        }
        if(name == "slice"){
            idOut = RTBUILTIN_MEMBER_STRING_SLICE;
            return true;
        }
        if(name == "contains"){
            idOut = RTBUILTIN_MEMBER_STRING_CONTAINS;
            return true;
        }
        if(name == "startsWith"){
            idOut = RTBUILTIN_MEMBER_STRING_STARTS_WITH;
            return true;
        }
        if(name == "endsWith"){
            idOut = RTBUILTIN_MEMBER_STRING_ENDS_WITH;
            return true;
        }
        if(name == "indexOf"){
            idOut = RTBUILTIN_MEMBER_STRING_INDEX_OF;
            return true;
        }
        if(name == "lastIndexOf"){
            idOut = RTBUILTIN_MEMBER_STRING_LAST_INDEX_OF;
            return true;
        }
        if(name == "lower"){
            idOut = RTBUILTIN_MEMBER_STRING_LOWER;
            return true;
        }
        if(name == "upper"){
            idOut = RTBUILTIN_MEMBER_STRING_UPPER;
            return true;
        }
        if(name == "trim"){
            idOut = RTBUILTIN_MEMBER_STRING_TRIM;
            return true;
        }
        if(name == "replace"){
            idOut = RTBUILTIN_MEMBER_STRING_REPLACE;
            return true;
        }
        if(name == "split"){
            idOut = RTBUILTIN_MEMBER_STRING_SPLIT;
            return true;
        }
        if(name == "repeat"){
            idOut = RTBUILTIN_MEMBER_STRING_REPEAT;
            return true;
        }
        if(name == "match"){
            idOut = RTBUILTIN_MEMBER_REGEX_MATCH;
            return true;
        }
        if(name == "findAll"){
            idOut = RTBUILTIN_MEMBER_REGEX_FIND_ALL;
            return true;
        }
        if(name == "push"){
            idOut = RTBUILTIN_MEMBER_ARRAY_PUSH;
            return true;
        }
        if(name == "pop"){
            idOut = RTBUILTIN_MEMBER_ARRAY_POP;
            return true;
        }
        if(name == "set"){
            idOut = RTBUILTIN_MEMBER_ARRAY_SET;
            return true;
        }
        if(name == "insert"){
            idOut = RTBUILTIN_MEMBER_ARRAY_INSERT;
            return true;
        }
        if(name == "removeAt"){
            idOut = RTBUILTIN_MEMBER_ARRAY_REMOVE_AT;
            return true;
        }
        if(name == "clear"){
            idOut = RTBUILTIN_MEMBER_ARRAY_CLEAR;
            return true;
        }
        if(name == "join"){
            idOut = RTBUILTIN_MEMBER_ARRAY_JOIN;
            return true;
        }
        if(name == "copy"){
            idOut = RTBUILTIN_MEMBER_ARRAY_COPY;
            return true;
        }
        if(name == "reverse"){
            idOut = RTBUILTIN_MEMBER_ARRAY_REVERSE;
            return true;
        }
        if(name == "has"){
            idOut = RTBUILTIN_MEMBER_DICT_HAS;
            return true;
        }
        if(name == "get"){
            idOut = RTBUILTIN_MEMBER_DICT_GET;
            return true;
        }
        if(name == "remove"){
            idOut = RTBUILTIN_MEMBER_DICT_REMOVE;
            return true;
        }
        if(name == "keys"){
            idOut = RTBUILTIN_MEMBER_DICT_KEYS;
            return true;
        }
        if(name == "values"){
            idOut = RTBUILTIN_MEMBER_DICT_VALUES;
            return true;
        }
        return false;
    }

    const char *rtBuiltinMemberName(RTBuiltinMemberId id){
        switch(id){
            case RTBUILTIN_MEMBER_STRING_IS_EMPTY:
            case RTBUILTIN_MEMBER_ARRAY_IS_EMPTY:
            case RTBUILTIN_MEMBER_DICT_IS_EMPTY:
                return "isEmpty";
            case RTBUILTIN_MEMBER_STRING_AT:
            case RTBUILTIN_MEMBER_ARRAY_AT:
                return "at";
            case RTBUILTIN_MEMBER_STRING_SLICE:
            case RTBUILTIN_MEMBER_ARRAY_SLICE:
                return "slice";
            case RTBUILTIN_MEMBER_STRING_CONTAINS:
            case RTBUILTIN_MEMBER_ARRAY_CONTAINS:
                return "contains";
            case RTBUILTIN_MEMBER_STRING_STARTS_WITH:
                return "startsWith";
            case RTBUILTIN_MEMBER_STRING_ENDS_WITH:
                return "endsWith";
            case RTBUILTIN_MEMBER_STRING_INDEX_OF:
            case RTBUILTIN_MEMBER_ARRAY_INDEX_OF:
                return "indexOf";
            case RTBUILTIN_MEMBER_STRING_LAST_INDEX_OF:
                return "lastIndexOf";
            case RTBUILTIN_MEMBER_STRING_LOWER:
                return "lower";
            case RTBUILTIN_MEMBER_STRING_UPPER:
                return "upper";
            case RTBUILTIN_MEMBER_STRING_TRIM:
                return "trim";
            case RTBUILTIN_MEMBER_STRING_REPLACE:
            case RTBUILTIN_MEMBER_REGEX_REPLACE:
                return "replace";
            case RTBUILTIN_MEMBER_STRING_SPLIT:
                return "split";
            case RTBUILTIN_MEMBER_STRING_REPEAT:
                return "repeat";
            case RTBUILTIN_MEMBER_REGEX_MATCH:
                return "match";
            case RTBUILTIN_MEMBER_REGEX_FIND_ALL:
                return "findAll";
            case RTBUILTIN_MEMBER_ARRAY_PUSH:
                return "push";
            case RTBUILTIN_MEMBER_ARRAY_POP:
                return "pop";
            case RTBUILTIN_MEMBER_ARRAY_SET:
            case RTBUILTIN_MEMBER_DICT_SET:
                return "set";
            case RTBUILTIN_MEMBER_ARRAY_INSERT:
                return "insert";
            case RTBUILTIN_MEMBER_ARRAY_REMOVE_AT:
                return "removeAt";
            case RTBUILTIN_MEMBER_ARRAY_CLEAR:
            case RTBUILTIN_MEMBER_DICT_CLEAR:
                return "clear";
            case RTBUILTIN_MEMBER_ARRAY_JOIN:
                return "join";
            case RTBUILTIN_MEMBER_ARRAY_COPY:
            case RTBUILTIN_MEMBER_DICT_COPY:
                return "copy";
            case RTBUILTIN_MEMBER_ARRAY_REVERSE:
                return "reverse";
            case RTBUILTIN_MEMBER_DICT_HAS:
                return "has";
            case RTBUILTIN_MEMBER_DICT_GET:
                return "get";
            case RTBUILTIN_MEMBER_DICT_REMOVE:
                return "remove";
            case RTBUILTIN_MEMBER_DICT_KEYS:
                return "keys";
            case RTBUILTIN_MEMBER_DICT_VALUES:
                return "values";
            default:
                return nullptr;
        }
    }

    void addRTInternalAttribute(RTFuncTemplate &func,const std::string &name){
        auto found = std::find_if(func.attributes.begin(),func.attributes.end(),[&](const auto &attr){
            return std::string(attr.name.value,attr.name.len) == name;
        });
        if(found != func.attributes.end()){
            return;
        }
        RTAttribute attr;
        attr.name.len = name.size();
        auto *buf = new char[attr.name.len];
        std::memcpy(buf,name.data(),attr.name.len);
        attr.name.value = buf;
        func.attributes.push_back(std::move(attr));
    }

    bool hasRTInternalAttribute(const RTFuncTemplate &func,const std::string &name){
        return std::any_of(func.attributes.begin(),func.attributes.end(),[&](const auto &attr){
            return attr.name.len == name.size()
                && std::memcmp(attr.name.value,name.data(),name.size()) == 0;
        });
    }

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

    namespace {

        constexpr std::array<char,4> kRTV2FunctionMagic = {'V','2','F','N'};

        bool writeSizedString(std::ostream &os,const std::string &value){
            uint32_t size = (uint32_t)value.size();
            os.write((char *)&size,sizeof(size));
            if(size > 0){
                os.write(value.data(),(std::streamsize)size);
            }
            return os.good();
        }

        bool readSizedString(std::istream &is,std::string &value){
            uint32_t size = 0;
            if(!is.read((char *)&size,sizeof(size))){
                return false;
            }
            value.resize(size);
            if(size > 0 && !is.read(value.data(),(std::streamsize)size)){
                return false;
            }
            return true;
        }

        bool failReadRTV2(std::string *errorOut,const std::string &message){
            if(errorOut){
                *errorOut = message;
            }
            return false;
        }

    }

    bool writeRTV2FunctionImage(std::ostream &os,const RTV2FunctionImage &image){
        os.write(kRTV2FunctionMagic.data(),(std::streamsize)kRTV2FunctionMagic.size());
        uint16_t version = 1;
        uint16_t reserved = 0;
        os.write((char *)&version,sizeof(version));
        os.write((char *)&reserved,sizeof(reserved));

        uint32_t instructionCount = (uint32_t)image.instructions.size();
        uint32_t i64ConstCount = (uint32_t)image.i64Consts.size();
        uint32_t f64ConstCount = (uint32_t)image.f64Consts.size();
        uint32_t callSiteCount = (uint32_t)image.callSites.size();
        uint32_t fallbackCount = (uint32_t)image.fallbackStmtBlobs.size();
        os.write((char *)&instructionCount,sizeof(instructionCount));
        os.write((char *)&i64ConstCount,sizeof(i64ConstCount));
        os.write((char *)&f64ConstCount,sizeof(f64ConstCount));
        os.write((char *)&callSiteCount,sizeof(callSiteCount));
        os.write((char *)&fallbackCount,sizeof(fallbackCount));

        for(const auto &instr : image.instructions){
            os.write((char *)&instr.opcode,sizeof(instr.opcode));
            os.write((char *)&instr.kind,sizeof(instr.kind));
            os.write((char *)&instr.aux,sizeof(instr.aux));
            os.write((char *)&instr.flags,sizeof(instr.flags));
            os.write((char *)&instr.a,sizeof(instr.a));
            os.write((char *)&instr.b,sizeof(instr.b));
            os.write((char *)&instr.c,sizeof(instr.c));
            os.write((char *)&instr.d,sizeof(instr.d));
        }
        if(i64ConstCount > 0){
            os.write((char *)image.i64Consts.data(),(std::streamsize)(sizeof(int64_t) * i64ConstCount));
        }
        if(f64ConstCount > 0){
            os.write((char *)image.f64Consts.data(),(std::streamsize)(sizeof(double) * f64ConstCount));
        }
        for(const auto &callSite : image.callSites){
            if(!writeSizedString(os,callSite.targetName)){
                return false;
            }
            uint32_t argCount = (uint32_t)callSite.argSlots.size();
            os.write((char *)&argCount,sizeof(argCount));
            if(argCount > 0){
                os.write((char *)callSite.argSlots.data(),(std::streamsize)(sizeof(uint32_t) * argCount));
            }
        }
        for(const auto &blob : image.fallbackStmtBlobs){
            uint32_t blobSize = (uint32_t)blob.size();
            os.write((char *)&blobSize,sizeof(blobSize));
            if(blobSize > 0){
                os.write(blob.data(),(std::streamsize)blobSize);
            }
        }
        return os.good();
    }

    bool readRTV2FunctionImage(const char *data,size_t size,RTV2FunctionImage &imageOut,std::string *errorOut){
        imageOut = RTV2FunctionImage();
        if(!data || size < kRTV2FunctionMagic.size() + sizeof(uint16_t) * 2 + sizeof(uint32_t) * 5){
            return failReadRTV2(errorOut,"V2 function image is truncated");
        }

        std::string buffer(data,size);
        std::istringstream in(buffer,std::ios::in | std::ios::binary);
        std::array<char,4> magic = {};
        if(!in.read(magic.data(),(std::streamsize)magic.size()) || magic != kRTV2FunctionMagic){
            return failReadRTV2(errorOut,"V2 function image magic is invalid");
        }

        uint16_t version = 0;
        uint16_t reserved = 0;
        if(!in.read((char *)&version,sizeof(version)) || !in.read((char *)&reserved,sizeof(reserved))){
            return failReadRTV2(errorOut,"V2 function header is truncated");
        }
        (void)reserved;
        if(version != 1){
            return failReadRTV2(errorOut,"V2 function image version is unsupported");
        }

        uint32_t instructionCount = 0;
        uint32_t i64ConstCount = 0;
        uint32_t f64ConstCount = 0;
        uint32_t callSiteCount = 0;
        uint32_t fallbackCount = 0;
        if(!in.read((char *)&instructionCount,sizeof(instructionCount))
           || !in.read((char *)&i64ConstCount,sizeof(i64ConstCount))
           || !in.read((char *)&f64ConstCount,sizeof(f64ConstCount))
           || !in.read((char *)&callSiteCount,sizeof(callSiteCount))
           || !in.read((char *)&fallbackCount,sizeof(fallbackCount))){
            return failReadRTV2(errorOut,"V2 function table counts are truncated");
        }

        imageOut.instructions.resize(instructionCount);
        for(auto &instr : imageOut.instructions){
            if(!in.read((char *)&instr.opcode,sizeof(instr.opcode))
               || !in.read((char *)&instr.kind,sizeof(instr.kind))
               || !in.read((char *)&instr.aux,sizeof(instr.aux))
               || !in.read((char *)&instr.flags,sizeof(instr.flags))
               || !in.read((char *)&instr.a,sizeof(instr.a))
               || !in.read((char *)&instr.b,sizeof(instr.b))
               || !in.read((char *)&instr.c,sizeof(instr.c))
               || !in.read((char *)&instr.d,sizeof(instr.d))){
                return failReadRTV2(errorOut,"V2 instruction stream is truncated");
            }
        }

        imageOut.i64Consts.resize(i64ConstCount);
        if(i64ConstCount > 0
           && !in.read((char *)imageOut.i64Consts.data(),(std::streamsize)(sizeof(int64_t) * i64ConstCount))){
            return failReadRTV2(errorOut,"V2 i64 constant pool is truncated");
        }

        imageOut.f64Consts.resize(f64ConstCount);
        if(f64ConstCount > 0
           && !in.read((char *)imageOut.f64Consts.data(),(std::streamsize)(sizeof(double) * f64ConstCount))){
            return failReadRTV2(errorOut,"V2 f64 constant pool is truncated");
        }

        imageOut.callSites.resize(callSiteCount);
        for(auto &callSite : imageOut.callSites){
            if(!readSizedString(in,callSite.targetName)){
                return failReadRTV2(errorOut,"V2 call target name table is truncated");
            }
            uint32_t argCount = 0;
            if(!in.read((char *)&argCount,sizeof(argCount))){
                return failReadRTV2(errorOut,"V2 call arg table is truncated");
            }
            callSite.argSlots.resize(argCount);
            if(argCount > 0
               && !in.read((char *)callSite.argSlots.data(),(std::streamsize)(sizeof(uint32_t) * argCount))){
                return failReadRTV2(errorOut,"V2 call arg slots are truncated");
            }
        }

        imageOut.fallbackStmtBlobs.resize(fallbackCount);
        for(auto &blob : imageOut.fallbackStmtBlobs){
            uint32_t blobSize = 0;
            if(!in.read((char *)&blobSize,sizeof(blobSize))){
                return failReadRTV2(errorOut,"V2 fallback blob table is truncated");
            }
            blob.resize(blobSize);
            if(blobSize > 0 && !in.read(blob.data(),(std::streamsize)blobSize)){
                return failReadRTV2(errorOut,"V2 fallback blob payload is truncated");
            }
        }

        return true;
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
            case CODE_RTLOCAL_REF:
                is.seekg((std::streamoff)sizeof(uint32_t),std::ios_base::cur);
                break;
            case CODE_RTTYPED_LOCAL_REF:
                is.seekg((std::streamoff)(sizeof(RTTypedNumericKind) + sizeof(uint32_t)),std::ios_base::cur);
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
            case CODE_RTCALL_DIRECT: {
                skipRTIDPayload(is);
                unsigned argCount = 0;
                is.read((char *)&argCount,sizeof(argCount));
                while(argCount > 0){
                    skipExpr(is);
                    --argCount;
                }
                break;
            }
            case CODE_RTCALL_BUILTIN_MEMBER: {
                skipExpr(is);
                RTBuiltinMemberId memberId = RTBUILTIN_MEMBER_INVALID;
                is.read((char *)&memberId,sizeof(memberId));
                (void)memberId;
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
            case CODE_RTTYPED_BINARY: {
                RTTypedNumericKind kind = RTTYPED_NUM_OBJECT;
                RTTypedBinaryOp op = RTTYPED_BINARY_ADD;
                is.read((char *)&kind,sizeof(kind));
                is.read((char *)&op,sizeof(op));
                (void)kind;
                (void)op;
                skipExpr(is);
                skipExpr(is);
                break;
            }
            case CODE_RTTYPED_NEGATE: {
                RTTypedNumericKind kind = RTTYPED_NUM_OBJECT;
                is.read((char *)&kind,sizeof(kind));
                (void)kind;
                skipExpr(is);
                break;
            }
            case CODE_RTTYPED_COMPARE: {
                RTTypedNumericKind kind = RTTYPED_NUM_OBJECT;
                RTTypedCompareOp op = RTTYPED_COMPARE_EQ;
                is.read((char *)&kind,sizeof(kind));
                is.read((char *)&op,sizeof(op));
                (void)kind;
                (void)op;
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
            case CODE_RTMEMBER_GET_FIELD_SLOT:
                skipExpr(is);
                is.seekg((std::streamoff)sizeof(uint32_t),std::ios_base::cur);
                break;
            case CODE_RTMEMBER_SET:
                skipExpr(is);
                skipRTIDPayload(is);
                skipExpr(is);
                break;
            case CODE_RTMEMBER_SET_FIELD_SLOT:
                skipExpr(is);
                is.seekg((std::streamoff)sizeof(uint32_t),std::ios_base::cur);
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
            case CODE_RTLOCAL_SET:
                is.seekg((std::streamoff)sizeof(uint32_t),std::ios_base::cur);
                skipExpr(is);
                break;
            case CODE_RTLOCAL_DECL: {
                is.seekg((std::streamoff)sizeof(uint32_t),std::ios_base::cur);
                bool hasInitValue = false;
                is.read((char *)&hasInitValue,sizeof(hasInitValue));
                if(hasInitValue){
                    skipExpr(is);
                }
                break;
            }
            case CODE_RTSECURE_LOCAL_DECL: {
                is.seekg((std::streamoff)sizeof(uint32_t),std::ios_base::cur);
                bool hasCatchSlot = false;
                is.read((char *)&hasCatchSlot,sizeof(hasCatchSlot));
                if(hasCatchSlot){
                    is.seekg((std::streamoff)sizeof(uint32_t),std::ios_base::cur);
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
                break;
            }
            case CODE_RTINDEX_GET:
                skipExpr(is);
                skipExpr(is);
                break;
            case CODE_RTTYPED_INDEX_GET: {
                RTTypedNumericKind kind = RTTYPED_NUM_OBJECT;
                is.read((char *)&kind,sizeof(kind));
                (void)kind;
                skipExpr(is);
                skipExpr(is);
                break;
            }
            case CODE_RTINDEX_SET:
                skipExpr(is);
                skipExpr(is);
                skipExpr(is);
                break;
            case CODE_RTTYPED_INDEX_SET: {
                RTTypedNumericKind kind = RTTYPED_NUM_OBJECT;
                is.read((char *)&kind,sizeof(kind));
                (void)kind;
                skipExpr(is);
                skipExpr(is);
                skipExpr(is);
                break;
            }
            case CODE_RTARRAY_LITERAL: {
                unsigned elementCount = 0;
                is.read((char *)&elementCount,sizeof(elementCount));
                while(elementCount > 0){
                    skipExpr(is);
                    --elementCount;
                }
                break;
            }
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
            case CODE_RTTERNARY:
                skipExpr(is);
                skipExpr(is);
                skipExpr(is);
                break;
            case CODE_RTCAST:
                skipExpr(is);
                skipRTIDPayload(is);
                break;
            case CODE_RTTYPED_INTRINSIC: {
                RTTypedNumericKind kind = RTTYPED_NUM_OBJECT;
                RTTypedIntrinsicOp op = RTTYPED_INTRINSIC_SQRT;
                is.read((char *)&kind,sizeof(kind));
                is.read((char *)&op,sizeof(op));
                (void)kind;
                (void)op;
                skipExpr(is);
                break;
            }
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
        unsigned localSlotCount = 0;
        is.read((char *)&localSlotCount,sizeof(localSlotCount));
        while(localSlotCount > 0){
            skipRTIDPayload(is);
            --localSlotCount;
        }
        unsigned slotKindCount = 0;
        is.read((char *)&slotKindCount,sizeof(slotKindCount));
        if(slotKindCount > 0){
            is.seekg((std::streamoff)(sizeof(RTTypedNumericKind) * slotKindCount),std::ios_base::cur);
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
        if(code == CODE_RTLOCAL_DECL){
            uint32_t slot = 0;
            bool hasInitValue = false;
            is.read((char *)&slot,sizeof(slot));
            is.read((char *)&hasInitValue,sizeof(hasInitValue));
            if(hasInitValue){
                skipExpr(is);
            }
            return;
        }
        if(code == CODE_RTSECURE_LOCAL_DECL){
            uint32_t targetSlot = 0;
            bool hasCatchSlot = false;
            bool hasCatchType = false;
            is.read((char *)&targetSlot,sizeof(targetSlot));
            is.read((char *)&hasCatchSlot,sizeof(hasCatchSlot));
            if(hasCatchSlot){
                uint32_t catchSlot = 0;
                is.read((char *)&catchSlot,sizeof(catchSlot));
            }
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
        unsigned localSlotCount = 0;
        is.read((char *)&localSlotCount,sizeof(localSlotCount));
        while(localSlotCount > 0){
            RTID id;
            is >> &id;
            obj->localSlotNames.push_back(std::move(id));
            --localSlotCount;
        }
        unsigned slotKindCount = 0;
        is.read((char *)&slotKindCount,sizeof(slotKindCount));
        while(slotKindCount > 0){
            RTTypedNumericKind kind = RTTYPED_NUM_OBJECT;
            is.read((char *)&kind,sizeof(kind));
            obj->slotKinds.push_back(kind);
            --slotKindCount;
        }
        is.read((char *)&obj->isLazy,sizeof(obj->isLazy));
        is.read((char *)&obj->blockByteSize,sizeof(obj->blockByteSize));
        auto checkpoint = is.tellg();
        RTCode code = CODE_MODULE_END;
        is.read((char *)&code,sizeof(RTCode));
        if(code == CODE_RTFUNCBLOCK_BEGIN){
            obj->block_start_pos = is.tellg();
            if(obj->blockByteSize > 0){
                obj->decodedBody.resize(obj->blockByteSize);
                is.read(obj->decodedBody.data(),(std::streamsize)obj->blockByteSize);
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
        unsigned localSlotCount = obj->localSlotNames.size();
        os.write((char *)&localSlotCount,sizeof(localSlotCount));
        for(auto &slotName : obj->localSlotNames){
            os << &slotName;
        }
        unsigned slotKindCount = obj->slotKinds.size();
        os.write((char *)&slotKindCount,sizeof(slotKindCount));
        if(slotKindCount > 0){
            os.write((char *)obj->slotKinds.data(),(std::streamsize)(sizeof(RTTypedNumericKind) * slotKindCount));
        }
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
            StarbytesNumT numType = NumTypeInt;
            is.read((char *)&numType,sizeof(numType));
            if(numType == NumTypeInt){
                int value = 0;
                is.read((char *)&value,sizeof(value));
                *obj = StarbytesNumNew(NumTypeInt,value);
            }
            else if(numType == NumTypeLong){
                int64_t value = 0;
                is.read((char *)&value,sizeof(value));
                *obj = StarbytesNumNew(NumTypeLong,value);
            }
            else if(numType == NumTypeFloat){
                float value = 0.0f;
                is.read((char *)&value,sizeof(value));
                *obj = StarbytesNumNew(NumTypeFloat,value);
            }
            else {
                double value = 0.0;
                is.read((char *)&value,sizeof(value));
                *obj = StarbytesNumNew(NumTypeDouble,value);
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
            strVal.len = StarbytesStrByteLength(*obj);
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
            auto numType = StarbytesNumGetType(*obj);
            os.write((char *)&numType,sizeof(numType));
            if(numType == NumTypeInt){
                int value = StarbytesNumGetIntValue(*obj);
                os.write((char *)&value,sizeof(value));
            }
            else if(numType == NumTypeLong){
                int64_t value = StarbytesNumGetLongValue(*obj);
                os.write((char *)&value,sizeof(value));
            }
            else if(numType == NumTypeFloat){
                float value = StarbytesNumGetFloatValue(*obj);
                os.write((char *)&value,sizeof(value));
            }
            else {
                double value = StarbytesNumGetDoubleValue(*obj);
                os.write((char *)&value,sizeof(value));
            }
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
