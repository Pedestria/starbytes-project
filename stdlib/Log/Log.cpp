#include <starbytes/interop.h>
#include "starbytes/base/ADT.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace {

using starbytes::map;

struct NativeArgsLayout {
    unsigned argc = 0;
    unsigned index = 0;
    StarbytesObject *argv = nullptr;
};

int g_minLevel = 2;
const map<int,std::string> kLevelNames = {
    {0,"TRACE"},
    {1,"DEBUG"},
    {2,"INFO"},
    {3,"WARN"},
    {4,"ERROR"}
};

StarbytesObject makeBool(bool value) {
    // Runtime bool consumption currently interprets StarbytesBoolFalse as logical true.
    return StarbytesBoolNew(value ? StarbytesBoolFalse : StarbytesBoolTrue);
}

StarbytesObject makeInt(int value) {
    return StarbytesNumNew(NumTypeInt,value);
}

bool readIntArg(StarbytesFuncArgs args,int &outValue) {
    auto arg = StarbytesFuncArgsGetArg(args);
    if(!arg || !StarbytesObjectTypecheck(arg,StarbytesNumType()) || StarbytesNumGetType(arg) != NumTypeInt) {
        return false;
    }
    outValue = StarbytesNumGetIntValue(arg);
    return true;
}

bool readStringArg(StarbytesFuncArgs args,std::string &outValue) {
    auto arg = StarbytesFuncArgsGetArg(args);
    if(!arg || !StarbytesObjectTypecheck(arg,StarbytesStrType())) {
        return false;
    }
    outValue = StarbytesStrGetBuffer(arg);
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

std::string levelName(int level) {
    auto it = kLevelNames.find(level);
    if(it == kLevelNames.end()) {
        return "LEVEL";
    }
    return it->second;
}

int clampLevel(int level) {
    if(level < 0) {
        return 0;
    }
    if(level > 4) {
        return 4;
    }
    return level;
}

std::string nowTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto timeValue = std::chrono::system_clock::to_time_t(now);

    std::tm localTime{};
#if defined(_WIN32)
    localtime_s(&localTime,&timeValue);
#else
    localtime_r(&timeValue,&localTime);
#endif

    std::ostringstream out;
    out << std::put_time(&localTime,"%Y-%m-%d %H:%M:%S");
    return out.str();
}

std::string objectToLogValue(StarbytesObject object,int depth = 0);

std::string dictToLogFields(StarbytesObject object,int depth) {
    if(!object || !StarbytesObjectTypecheck(object,StarbytesDictType()) || depth > 6) {
        return "";
    }

    auto keys = StarbytesObjectGetProperty(object,"keys");
    auto values = StarbytesObjectGetProperty(object,"values");
    if(!keys || !values || !StarbytesObjectTypecheck(keys,StarbytesArrayType()) || !StarbytesObjectTypecheck(values,StarbytesArrayType())) {
        return "";
    }

    auto len = StarbytesArrayGetLength(keys);
    if(len == 0 || len != StarbytesArrayGetLength(values)) {
        return "";
    }

    std::ostringstream out;
    out << "{";
    for(unsigned i = 0; i < len; ++i) {
        if(i > 0) {
            out << ", ";
        }
        auto key = StarbytesArrayIndex(keys,i);
        auto value = StarbytesArrayIndex(values,i);
        out << objectToLogValue(key,depth + 1) << ":" << objectToLogValue(value,depth + 1);
    }
    out << "}";
    return out.str();
}

std::string objectToLogValue(StarbytesObject object,int depth) {
    if(depth > 6) {
        return "<depth-limit>";
    }
    if(!object) {
        return "null";
    }

    if(StarbytesObjectTypecheck(object,StarbytesStrType())) {
        auto text = StarbytesStrGetBuffer(object);
        return text ? std::string(text) : std::string();
    }
    if(StarbytesObjectTypecheck(object,StarbytesNumType())) {
        if(StarbytesNumGetType(object) == NumTypeInt) {
            return std::to_string(StarbytesNumGetIntValue(object));
        }
        std::ostringstream out;
        out << StarbytesNumGetFloatValue(object);
        return out.str();
    }
    if(StarbytesObjectTypecheck(object,StarbytesBoolType())) {
        return StarbytesBoolValue(object) == StarbytesBoolFalse ? "true" : "false";
    }
    if(StarbytesObjectTypecheck(object,StarbytesArrayType())) {
        std::ostringstream out;
        out << "[";
        auto len = StarbytesArrayGetLength(object);
        for(unsigned i = 0; i < len; ++i) {
            if(i > 0) {
                out << ", ";
            }
            out << objectToLogValue(StarbytesArrayIndex(object,i),depth + 1);
        }
        out << "]";
        return out.str();
    }
    if(StarbytesObjectTypecheck(object,StarbytesDictType())) {
        return dictToLogFields(object,depth + 1);
    }
    if(StarbytesObjectTypecheck(object,StarbytesRegexType())) {
        auto pattern = StarbytesObjectGetProperty(object,"pattern");
        auto flags = StarbytesObjectGetProperty(object,"flags");
        auto patternText = pattern && StarbytesObjectTypecheck(pattern,StarbytesStrType()) ? StarbytesStrGetBuffer(pattern) : "";
        auto flagsText = flags && StarbytesObjectTypecheck(flags,StarbytesStrType()) ? StarbytesStrGetBuffer(flags) : "";
        return "/" + std::string(patternText ? patternText : "") + "/" + std::string(flagsText ? flagsText : "");
    }
    return "<object>";
}

StarbytesObject writeLogLine(int level,const std::string &message,StarbytesObject fields) {
    level = clampLevel(level);
    if(level < g_minLevel) {
        return makeBool(false);
    }

    std::ostringstream out;
    out << nowTimestamp() << " [" << levelName(level) << "] " << message;
    auto fieldsText = dictToLogFields(fields,0);
    if(!fieldsText.empty()) {
        out << " " << fieldsText;
    }

    std::cerr << out.str() << std::endl;
    return makeBool(true);
}

STARBYTES_FUNC(log_setMinLevel) {
    skipOptionalModuleReceiver(args,1);

    int level = 2;
    if(!readIntArg(args,level)) {
        return makeBool(false);
    }
    g_minLevel = clampLevel(level);
    return makeBool(true);
}

STARBYTES_FUNC(log_minLevel) {
    skipOptionalModuleReceiver(args,0);
    return makeInt(g_minLevel);
}

STARBYTES_FUNC(log_log) {
    skipOptionalModuleReceiver(args,2);

    int level = 2;
    std::string message;
    if(!readIntArg(args,level) || !readStringArg(args,message)) {
        return makeBool(false);
    }
    return writeLogLine(level,message,nullptr);
}

STARBYTES_FUNC(log_logWithFields) {
    skipOptionalModuleReceiver(args,3);

    int level = 2;
    std::string message;
    if(!readIntArg(args,level) || !readStringArg(args,message)) {
        return makeBool(false);
    }
    auto fields = StarbytesFuncArgsGetArg(args);
    if(!fields || !StarbytesObjectTypecheck(fields,StarbytesDictType())) {
        return makeBool(false);
    }
    return writeLogLine(level,message,fields);
}

STARBYTES_FUNC(log_trace) {
    skipOptionalModuleReceiver(args,1);
    std::string message;
    if(!readStringArg(args,message)) {
        return makeBool(false);
    }
    return writeLogLine(0,message,nullptr);
}

STARBYTES_FUNC(log_debug) {
    skipOptionalModuleReceiver(args,1);
    std::string message;
    if(!readStringArg(args,message)) {
        return makeBool(false);
    }
    return writeLogLine(1,message,nullptr);
}

STARBYTES_FUNC(log_info) {
    skipOptionalModuleReceiver(args,1);
    std::string message;
    if(!readStringArg(args,message)) {
        return makeBool(false);
    }
    return writeLogLine(2,message,nullptr);
}

STARBYTES_FUNC(log_warn) {
    skipOptionalModuleReceiver(args,1);
    std::string message;
    if(!readStringArg(args,message)) {
        return makeBool(false);
    }
    return writeLogLine(3,message,nullptr);
}

STARBYTES_FUNC(log_error) {
    skipOptionalModuleReceiver(args,1);
    std::string message;
    if(!readStringArg(args,message)) {
        return makeBool(false);
    }
    return writeLogLine(4,message,nullptr);
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

    addFunc(module,"log_setMinLevel",1,log_setMinLevel);
    addFunc(module,"log_minLevel",0,log_minLevel);
    addFunc(module,"log_log",2,log_log);
    addFunc(module,"log_logWithFields",3,log_logWithFields);
    addFunc(module,"log_trace",1,log_trace);
    addFunc(module,"log_debug",1,log_debug);
    addFunc(module,"log_info",1,log_info);
    addFunc(module,"log_warn",1,log_warn);
    addFunc(module,"log_error",1,log_error);

    return module;
}
