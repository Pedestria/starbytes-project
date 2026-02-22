#include <starbytes/interop.h>
#include "starbytes/base/ADT.h"

#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <cmath>
#include <limits>
#include <sstream>
#include <string>

namespace {

using starbytes::optional;
using rapidjson::Document;
using rapidjson::Value;

struct NativeArgsLayout {
    unsigned argc = 0;
    unsigned index = 0;
    StarbytesObject *argv = nullptr;
};

constexpr int kMaxDepth = 128;

StarbytesObject makeBool(bool value) {
    // Runtime bool consumption currently interprets StarbytesBoolFalse as logical true.
    return StarbytesBoolNew(value ? StarbytesBoolFalse : StarbytesBoolTrue);
}

StarbytesObject makeInt(int value) {
    return StarbytesNumNew(NumTypeInt,value);
}

bool readStringArg(StarbytesFuncArgs args,std::string &outValue) {
    auto arg = StarbytesFuncArgsGetArg(args);
    if(!arg || !StarbytesObjectTypecheck(arg,StarbytesStrType())) {
        return false;
    }
    outValue = StarbytesStrGetBuffer(arg);
    return true;
}

bool readIntArg(StarbytesFuncArgs args,int &outValue) {
    auto arg = StarbytesFuncArgsGetArg(args);
    if(!arg || !StarbytesObjectTypecheck(arg,StarbytesNumType()) || StarbytesNumGetType(arg) != NumTypeInt) {
        return false;
    }
    outValue = StarbytesNumGetIntValue(arg);
    return true;
}

void skipOptionalModuleReceiver(StarbytesFuncArgs args,unsigned expectedUserArgs) {
    auto *raw = reinterpret_cast<NativeArgsLayout *>(args);
    if(!raw || raw->argc < raw->index) {
        return;
    }
    auto remaining = raw->argc - raw->index;
    if(remaining == expectedUserArgs + 1) {
        (void)StarbytesFuncArgsGetArg(args);
    }
}

optional<std::string> numberToString(StarbytesObject object) {
    if(!object || !StarbytesObjectTypecheck(object,StarbytesNumType())) {
        return {};
    }

    if(StarbytesNumGetType(object) == NumTypeInt) {
        return std::to_string(StarbytesNumGetIntValue(object));
    }

    std::ostringstream out;
    out << StarbytesNumGetFloatValue(object);
    return out.str();
}

optional<std::string> dictKeyToString(StarbytesObject key) {
    if(!key) {
        return {};
    }
    if(StarbytesObjectTypecheck(key,StarbytesStrType())) {
        return std::string(StarbytesStrGetBuffer(key));
    }
    return numberToString(key);
}

bool jsonToStarbytes(const Value &value,StarbytesObject &out,int depth) {
    if(depth > kMaxDepth) {
        return false;
    }

    if(value.IsNull()) {
        out = nullptr;
        return true;
    }
    if(value.IsBool()) {
        out = makeBool(value.GetBool());
        return true;
    }
    if(value.IsInt()) {
        out = makeInt(value.GetInt());
        return true;
    }
    if(value.IsUint()) {
        auto uintValue = value.GetUint();
        if(uintValue > static_cast<unsigned>(std::numeric_limits<int>::max())) {
            out = StarbytesNumNew(NumTypeFloat,static_cast<float>(uintValue));
            return true;
        }
        out = makeInt(static_cast<int>(uintValue));
        return true;
    }
    if(value.IsInt64()) {
        auto int64Value = value.GetInt64();
        if(int64Value >= std::numeric_limits<int>::min() && int64Value <= std::numeric_limits<int>::max()) {
            out = makeInt(static_cast<int>(int64Value));
            return true;
        }
        out = StarbytesNumNew(NumTypeFloat,static_cast<float>(int64Value));
        return true;
    }
    if(value.IsUint64()) {
        auto uint64Value = value.GetUint64();
        if(uint64Value <= static_cast<uint64_t>(std::numeric_limits<int>::max())) {
            out = makeInt(static_cast<int>(uint64Value));
            return true;
        }
        out = StarbytesNumNew(NumTypeFloat,static_cast<float>(uint64Value));
        return true;
    }
    if(value.IsNumber()) {
        out = StarbytesNumNew(NumTypeFloat,static_cast<float>(value.GetDouble()));
        return true;
    }
    if(value.IsString()) {
        out = StarbytesStrNewWithData(value.GetString());
        return true;
    }
    if(value.IsArray()) {
        auto array = StarbytesArrayNew();
        for(auto it = value.Begin(); it != value.End(); ++it) {
            StarbytesObject element = nullptr;
            if(!jsonToStarbytes(*it,element,depth + 1)) {
                return false;
            }
            StarbytesArrayPush(array,element);
        }
        out = array;
        return true;
    }
    if(value.IsObject()) {
        auto dict = StarbytesDictNew();
        for(auto it = value.MemberBegin(); it != value.MemberEnd(); ++it) {
            auto key = StarbytesStrNewWithData(it->name.GetString());
            StarbytesObject mapped = nullptr;
            if(!jsonToStarbytes(it->value,mapped,depth + 1)) {
                return false;
            }
            StarbytesDictSet(dict,key,mapped);
        }
        out = dict;
        return true;
    }

    return false;
}

bool starbytesToJson(StarbytesObject object,Value &out,Document::AllocatorType &allocator,int depth) {
    if(depth > kMaxDepth) {
        return false;
    }

    if(!object) {
        out.SetNull();
        return true;
    }

    if(StarbytesObjectTypecheck(object,StarbytesStrType())) {
        auto value = StarbytesStrGetBuffer(object);
        out.SetString(value ? value : "",allocator);
        return true;
    }
    if(StarbytesObjectTypecheck(object,StarbytesNumType())) {
        if(StarbytesNumGetType(object) == NumTypeInt) {
            out.SetInt(StarbytesNumGetIntValue(object));
        }
        else {
            out.SetDouble(StarbytesNumGetFloatValue(object));
        }
        return true;
    }
    if(StarbytesObjectTypecheck(object,StarbytesBoolType())) {
        out.SetBool(StarbytesBoolValue(object) == StarbytesBoolFalse);
        return true;
    }
    if(StarbytesObjectTypecheck(object,StarbytesArrayType())) {
        out.SetArray();
        auto len = StarbytesArrayGetLength(object);
        for(unsigned i = 0; i < len; ++i) {
            auto element = StarbytesArrayIndex(object,i);
            Value elementValue;
            if(!starbytesToJson(element,elementValue,allocator,depth + 1)) {
                return false;
            }
            out.PushBack(elementValue,allocator);
        }
        return true;
    }
    if(StarbytesObjectTypecheck(object,StarbytesDictType())) {
        auto keys = StarbytesObjectGetProperty(object,"keys");
        auto values = StarbytesObjectGetProperty(object,"values");
        if(!keys || !values || !StarbytesObjectTypecheck(keys,StarbytesArrayType()) || !StarbytesObjectTypecheck(values,StarbytesArrayType())) {
            return false;
        }

        auto len = StarbytesArrayGetLength(keys);
        if(len != StarbytesArrayGetLength(values)) {
            return false;
        }

        out.SetObject();
        for(unsigned i = 0; i < len; ++i) {
            auto keyObject = StarbytesArrayIndex(keys,i);
            auto keyText = dictKeyToString(keyObject);
            if(!keyText.has_value()) {
                return false;
            }

            auto valueObject = StarbytesArrayIndex(values,i);
            Value jsonKey;
            jsonKey.SetString(keyText->c_str(),static_cast<rapidjson::SizeType>(keyText->size()),allocator);

            Value jsonValue;
            if(!starbytesToJson(valueObject,jsonValue,allocator,depth + 1)) {
                return false;
            }
            out.AddMember(jsonKey,jsonValue,allocator);
        }
        return true;
    }
    if(StarbytesObjectTypecheck(object,StarbytesRegexType())) {
        out.SetObject();
        auto pattern = StarbytesObjectGetProperty(object,"pattern");
        auto flags = StarbytesObjectGetProperty(object,"flags");
        auto patternText = pattern && StarbytesObjectTypecheck(pattern,StarbytesStrType()) ? StarbytesStrGetBuffer(pattern) : "";
        auto flagsText = flags && StarbytesObjectTypecheck(flags,StarbytesStrType()) ? StarbytesStrGetBuffer(flags) : "";
        Value keyPattern;
        keyPattern.SetString("pattern",allocator);
        Value valPattern;
        valPattern.SetString(patternText ? patternText : "",allocator);
        out.AddMember(keyPattern,valPattern,allocator);

        Value keyFlags;
        keyFlags.SetString("flags",allocator);
        Value valFlags;
        valFlags.SetString(flagsText ? flagsText : "",allocator);
        out.AddMember(keyFlags,valFlags,allocator);
        return true;
    }

    if(!StarbytesObjectIs(object)) {
        out.SetObject();
        auto propCount = StarbytesObjectGetPropertyCount(object);
        for(unsigned i = 0; i < propCount; ++i) {
            auto *prop = StarbytesObjectIndexProperty(object,i);
            if(!prop || prop->name[0] == '\0') {
                continue;
            }
            Value key;
            key.SetString(prop->name,allocator);
            Value mapped;
            if(!starbytesToJson(prop->data,mapped,allocator,depth + 1)) {
                return false;
            }
            out.AddMember(key,mapped,allocator);
        }
        return true;
    }

    return false;
}

STARBYTES_FUNC(json_parse) {
    skipOptionalModuleReceiver(args,1);

    std::string source;
    if(!readStringArg(args,source)) {
        return nullptr;
    }

    Document document;
    document.Parse(source.c_str());
    if(document.HasParseError()) {
        return nullptr;
    }

    StarbytesObject out = nullptr;
    if(!jsonToStarbytes(document,out,0)) {
        return nullptr;
    }
    return out;
}

STARBYTES_FUNC(json_isValid) {
    skipOptionalModuleReceiver(args,1);

    std::string source;
    if(!readStringArg(args,source)) {
        return makeBool(false);
    }

    Document document;
    document.Parse(source.c_str());
    return makeBool(!document.HasParseError());
}

STARBYTES_FUNC(json_stringify) {
    skipOptionalModuleReceiver(args,1);

    auto object = StarbytesFuncArgsGetArg(args);

    Document document;
    auto &allocator = document.GetAllocator();
    Value root;
    if(!starbytesToJson(object,root,allocator,0)) {
        return nullptr;
    }

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    if(!root.Accept(writer)) {
        return nullptr;
    }

    return StarbytesStrNewWithData(buffer.GetString());
}

STARBYTES_FUNC(json_pretty) {
    skipOptionalModuleReceiver(args,2);

    auto object = StarbytesFuncArgsGetArg(args);
    int indent = 2;
    if(!readIntArg(args,indent)) {
        return nullptr;
    }
    if(indent < 0) {
        indent = 0;
    }
    if(indent > 8) {
        indent = 8;
    }

    Document document;
    auto &allocator = document.GetAllocator();
    Value root;
    if(!starbytesToJson(object,root,allocator,0)) {
        return nullptr;
    }

    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    writer.SetIndent(' ',static_cast<unsigned>(indent));
    if(!root.Accept(writer)) {
        return nullptr;
    }

    return StarbytesStrNewWithData(buffer.GetString());
}

void addFunc(StarbytesNativeModule *module,const char *name,unsigned argCount,StarbytesFuncCallback callback) {
    StarbytesFuncDesc desc;
    desc.name = CStringMake(name);
    desc.argCount = argCount;
    desc.callback = callback;
    StarbytesNativeModuleAddDesc(module,&desc);
}

}

STARBYTES_NATIVE_MOD_MAIN() {
    auto module = StarbytesNativeModuleCreate();

    addFunc(module,"json_parse",1,json_parse);
    addFunc(module,"json_isValid",1,json_isValid);
    addFunc(module,"json_stringify",1,json_stringify);
    addFunc(module,"json_pretty",2,json_pretty);

    return module;
}
