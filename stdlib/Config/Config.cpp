#include <starbytes/interop.h>
#include "starbytes/base/ADT.h"

#include <rapidjson/document.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <limits>
#include <string>
#include <vector>

#if defined(_WIN32)
extern "C" char **_environ;
#else
extern "C" char **environ;
#endif

namespace {

using starbytes::optional;
using starbytes::string_map;
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

bool readStringArrayArg(StarbytesFuncArgs args,std::vector<std::string> &outValues) {
    auto arg = StarbytesFuncArgsGetArg(args);
    if(!arg || !StarbytesObjectTypecheck(arg,StarbytesArrayType())) {
        return false;
    }

    auto len = StarbytesArrayGetLength(arg);
    outValues.clear();
    outValues.reserve(len);
    for(unsigned i = 0; i < len; ++i) {
        auto item = StarbytesArrayIndex(arg,i);
        if(!item || !StarbytesObjectTypecheck(item,StarbytesStrType())) {
            return false;
        }
        outValues.emplace_back(StarbytesStrGetBuffer(item));
    }
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

optional<std::string> objectToString(StarbytesObject object) {
    if(!object) {
        return {};
    }
    if(StarbytesObjectTypecheck(object,StarbytesStrType())) {
        return std::string(StarbytesStrGetBuffer(object));
    }
    if(StarbytesObjectTypecheck(object,StarbytesNumType())) {
        if(StarbytesNumGetType(object) == NumTypeInt) {
            return std::to_string(StarbytesNumGetIntValue(object));
        }
        return std::to_string(StarbytesNumGetFloatValue(object));
    }
    return {};
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
    if(value.IsInt64()) {
        auto int64Value = value.GetInt64();
        if(int64Value >= std::numeric_limits<int>::min() && int64Value <= std::numeric_limits<int>::max()) {
            out = makeInt((int)int64Value);
        }
        else {
            out = StarbytesNumNew(NumTypeFloat,(float)int64Value);
        }
        return true;
    }
    if(value.IsUint64()) {
        auto uintValue = value.GetUint64();
        if(uintValue <= (uint64_t)std::numeric_limits<int>::max()) {
            out = makeInt((int)uintValue);
        }
        else {
            out = StarbytesNumNew(NumTypeFloat,(float)uintValue);
        }
        return true;
    }
    if(value.IsNumber()) {
        out = StarbytesNumNew(NumTypeFloat,(float)value.GetDouble());
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

bool startsWith(const std::string &value,const std::string &prefix) {
    if(prefix.size() > value.size()) {
        return false;
    }
    return std::equal(prefix.begin(),prefix.end(),value.begin());
}

StarbytesObject mergeObjects(StarbytesObject baseObject,StarbytesObject overlayObject,int depth);

StarbytesObject mergeDicts(StarbytesObject baseDict,StarbytesObject overlayDict,int depth) {
    if(depth > kMaxDepth) {
        return nullptr;
    }

    auto merged = StarbytesDictCopy(baseDict);
    auto overlayKeys = StarbytesObjectGetProperty(overlayDict,"keys");
    auto overlayValues = StarbytesObjectGetProperty(overlayDict,"values");
    if(!overlayKeys || !overlayValues || !StarbytesObjectTypecheck(overlayKeys,StarbytesArrayType())
       || !StarbytesObjectTypecheck(overlayValues,StarbytesArrayType())) {
        return merged;
    }

    auto len = StarbytesArrayGetLength(overlayKeys);
    if(len != StarbytesArrayGetLength(overlayValues)) {
        return merged;
    }

    for(unsigned i = 0; i < len; ++i) {
        auto key = StarbytesArrayIndex(overlayKeys,i);
        auto overlayValue = StarbytesArrayIndex(overlayValues,i);
        auto existing = StarbytesDictGet(merged,key);
        auto mergedValue = mergeObjects(existing,overlayValue,depth + 1);
        StarbytesDictSet(merged,key,mergedValue);
    }
    return merged;
}

StarbytesObject mergeObjects(StarbytesObject baseObject,StarbytesObject overlayObject,int depth) {
    if(depth > kMaxDepth) {
        return overlayObject;
    }
    if(baseObject && overlayObject
       && StarbytesObjectTypecheck(baseObject,StarbytesDictType())
       && StarbytesObjectTypecheck(overlayObject,StarbytesDictType())) {
        return mergeDicts(baseObject,overlayObject,depth + 1);
    }
    return overlayObject;
}

string_map<std::string> dictToStringMap(StarbytesObject dict) {
    string_map<std::string> out;
    if(!dict || !StarbytesObjectTypecheck(dict,StarbytesDictType())) {
        return out;
    }

    auto keys = StarbytesObjectGetProperty(dict,"keys");
    auto values = StarbytesObjectGetProperty(dict,"values");
    if(!keys || !values || !StarbytesObjectTypecheck(keys,StarbytesArrayType()) || !StarbytesObjectTypecheck(values,StarbytesArrayType())) {
        return out;
    }

    auto len = StarbytesArrayGetLength(keys);
    if(len != StarbytesArrayGetLength(values)) {
        return out;
    }

    for(unsigned i = 0; i < len; ++i) {
        auto keyObject = StarbytesArrayIndex(keys,i);
        auto valueObject = StarbytesArrayIndex(values,i);
        auto key = objectToString(keyObject);
        auto value = objectToString(valueObject);
        if(!key.has_value() || !value.has_value()) {
            continue;
        }
        out[*key] = *value;
    }
    return out;
}

STARBYTES_FUNC(config_parseJsonObject) {
    skipOptionalModuleReceiver(args,1);

    std::string source;
    if(!readStringArg(args,source)) {
        return nullptr;
    }

    Document document;
    document.Parse(source.c_str());
    if(document.HasParseError() || !document.IsObject()) {
        return nullptr;
    }

    StarbytesObject out = nullptr;
    if(!jsonToStarbytes(document,out,0) || !out || !StarbytesObjectTypecheck(out,StarbytesDictType())) {
        return nullptr;
    }
    return out;
}

STARBYTES_FUNC(config_merge) {
    skipOptionalModuleReceiver(args,2);

    auto base = StarbytesFuncArgsGetArg(args);
    auto overlay = StarbytesFuncArgsGetArg(args);
    if(!base || !overlay || !StarbytesObjectTypecheck(base,StarbytesDictType()) || !StarbytesObjectTypecheck(overlay,StarbytesDictType())) {
        return nullptr;
    }

    return mergeDicts(base,overlay,0);
}

STARBYTES_FUNC(config_fromEnv) {
    skipOptionalModuleReceiver(args,1);

    std::string prefix;
    if(!readStringArg(args,prefix)) {
        return StarbytesDictNew();
    }

    auto dict = StarbytesDictNew();
#if defined(_WIN32)
    char **envv = _environ;
#else
    char **envv = environ;
#endif

    if(!envv) {
        return dict;
    }

    for(char **it = envv; *it; ++it) {
        std::string entry(*it);
        auto pos = entry.find('=');
        if(pos == std::string::npos || pos == 0) {
            continue;
        }

        auto key = entry.substr(0,pos);
        auto value = entry.substr(pos + 1);
        if(!prefix.empty()) {
            if(!startsWith(key,prefix)) {
                continue;
            }
            key = key.substr(prefix.size());
            if(!key.empty() && key[0] == '_') {
                key.erase(key.begin());
            }
        }

        if(key.empty()) {
            continue;
        }

        StarbytesDictSet(dict,StarbytesStrNewWithData(key.c_str()),StarbytesStrNewWithData(value.c_str()));
    }

    return dict;
}

STARBYTES_FUNC(config_requireKeys) {
    skipOptionalModuleReceiver(args,2);

    auto config = StarbytesFuncArgsGetArg(args);
    std::vector<std::string> keys;
    if(!config || !StarbytesObjectTypecheck(config,StarbytesDictType()) || !readStringArrayArg(args,keys)) {
        return makeBool(false);
    }

    for(const auto &key : keys) {
        auto value = StarbytesDictGet(config,StarbytesStrNewWithData(key.c_str()));
        if(!value) {
            return makeBool(false);
        }
    }
    return makeBool(true);
}

STARBYTES_FUNC(config_getString) {
    skipOptionalModuleReceiver(args,3);

    auto config = StarbytesFuncArgsGetArg(args);
    std::string key;
    std::string defaultValue;
    if(!config || !StarbytesObjectTypecheck(config,StarbytesDictType()) || !readStringArg(args,key) || !readStringArg(args,defaultValue)) {
        return StarbytesStrNewWithData("");
    }

    auto value = StarbytesDictGet(config,StarbytesStrNewWithData(key.c_str()));
    if(value && StarbytesObjectTypecheck(value,StarbytesStrType())) {
        return StarbytesStrNewWithData(StarbytesStrGetBuffer(value));
    }
    return StarbytesStrNewWithData(defaultValue.c_str());
}

STARBYTES_FUNC(config_getInt) {
    skipOptionalModuleReceiver(args,3);

    auto config = StarbytesFuncArgsGetArg(args);
    std::string key;
    int defaultValue = 0;
    if(!config || !StarbytesObjectTypecheck(config,StarbytesDictType()) || !readStringArg(args,key) || !readIntArg(args,defaultValue)) {
        return makeInt(defaultValue);
    }

    auto value = StarbytesDictGet(config,StarbytesStrNewWithData(key.c_str()));
    if(value && StarbytesObjectTypecheck(value,StarbytesNumType())) {
        if(StarbytesNumGetType(value) == NumTypeInt) {
            return makeInt(StarbytesNumGetIntValue(value));
        }
        return makeInt((int)StarbytesNumGetFloatValue(value));
    }

    if(value && StarbytesObjectTypecheck(value,StarbytesStrType())) {
        auto text = std::string(StarbytesStrGetBuffer(value));
        char *end = nullptr;
        auto parsed = std::strtol(text.c_str(),&end,10);
        if(end && *end == '\0' && parsed >= std::numeric_limits<int>::min() && parsed <= std::numeric_limits<int>::max()) {
            return makeInt((int)parsed);
        }
    }

    return makeInt(defaultValue);
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

    addFunc(module,"config_parseJsonObject",1,config_parseJsonObject);
    addFunc(module,"config_merge",2,config_merge);
    addFunc(module,"config_fromEnv",1,config_fromEnv);
    addFunc(module,"config_requireKeys",2,config_requireKeys);
    addFunc(module,"config_getString",3,config_getString);
    addFunc(module,"config_getInt",3,config_getInt);

    return module;
}
