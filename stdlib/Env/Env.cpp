#include <starbytes/interop.h>
#include "starbytes/base/ADT.h"

#include <cstdlib>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <process.h>
extern "C" char **_environ;
#else
#include <unistd.h>
extern "C" char **environ;
#endif

namespace {

using starbytes::string_set;

struct NativeArgsLayout {
    unsigned argc = 0;
    unsigned index = 0;
    StarbytesObject *argv = nullptr;
};

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

StarbytesObject stringListToArray(const std::vector<std::string> &values) {
    auto out = StarbytesArrayNew();
    for(const auto &value : values) {
        StarbytesArrayPush(out,StarbytesStrNewWithData(value.c_str()));
    }
    return out;
}

STARBYTES_FUNC(env_get) {
    skipOptionalModuleReceiver(args,1);
    std::string key;
    if(!readStringArg(args,key) || key.empty()) {
        return nullptr;
    }

    const char *value = std::getenv(key.c_str());
    if(!value) {
        return nullptr;
    }
    return StarbytesStrNewWithData(value);
}

STARBYTES_FUNC(env_set) {
    skipOptionalModuleReceiver(args,2);
    std::string key;
    std::string value;
    if(!readStringArg(args,key) || !readStringArg(args,value) || key.empty()) {
        return nullptr;
    }

#if defined(_WIN32)
    if(_putenv_s(key.c_str(),value.c_str()) != 0) {
        return nullptr;
    }
#else
    if(setenv(key.c_str(),value.c_str(),1) != 0) {
        return nullptr;
    }
#endif
    return makeBool(true);
}

STARBYTES_FUNC(env_unset) {
    skipOptionalModuleReceiver(args,1);
    std::string key;
    if(!readStringArg(args,key) || key.empty()) {
        return nullptr;
    }

#if defined(_WIN32)
    if(_putenv_s(key.c_str(),"") != 0) {
        return nullptr;
    }
#else
    if(unsetenv(key.c_str()) != 0) {
        return nullptr;
    }
#endif
    return makeBool(true);
}

STARBYTES_FUNC(env_has) {
    skipOptionalModuleReceiver(args,1);
    std::string key;
    if(!readStringArg(args,key) || key.empty()) {
        return makeBool(false);
    }
    return makeBool(std::getenv(key.c_str()) != nullptr);
}

STARBYTES_FUNC(env_listKeys) {
    skipOptionalModuleReceiver(args,0);

    string_set uniqueKeys;
#if defined(_WIN32)
    char **envv = _environ;
#else
    char **envv = environ;
#endif

    if(envv) {
        for(char **it = envv; *it; ++it) {
            auto entry = std::string(*it);
            auto pos = entry.find('=');
            if(pos == std::string::npos || pos == 0) {
                continue;
            }
            uniqueKeys.insert(entry.substr(0,pos));
        }
    }

    std::vector<std::string> keys;
    keys.reserve(uniqueKeys.size());
    for(const auto &key : uniqueKeys) {
        keys.push_back(key);
    }
    return stringListToArray(keys);
}

STARBYTES_FUNC(env_osName) {
    skipOptionalModuleReceiver(args,0);
#if defined(_WIN32)
    return StarbytesStrNewWithData("windows");
#elif defined(__APPLE__)
    return StarbytesStrNewWithData("macos");
#elif defined(__linux__)
    return StarbytesStrNewWithData("linux");
#else
    return StarbytesStrNewWithData("unknown");
#endif
}

STARBYTES_FUNC(env_archName) {
    skipOptionalModuleReceiver(args,0);
#if defined(__aarch64__) || defined(_M_ARM64)
    return StarbytesStrNewWithData("arm64");
#elif defined(__x86_64__) || defined(_M_X64)
    return StarbytesStrNewWithData("x64");
#elif defined(__i386__) || defined(_M_IX86)
    return StarbytesStrNewWithData("x86");
#elif defined(__arm__) || defined(_M_ARM)
    return StarbytesStrNewWithData("arm");
#else
    return StarbytesStrNewWithData("unknown");
#endif
}

STARBYTES_FUNC(env_processId) {
    skipOptionalModuleReceiver(args,0);
#if defined(_WIN32)
    return makeInt((int)_getpid());
#else
    return makeInt((int)getpid());
#endif
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

    addFunc(module,"env_get",1,env_get);
    addFunc(module,"env_set",2,env_set);
    addFunc(module,"env_unset",1,env_unset);
    addFunc(module,"env_has",1,env_has);
    addFunc(module,"env_listKeys",0,env_listKeys);
    addFunc(module,"env_osName",0,env_osName);
    addFunc(module,"env_archName",0,env_archName);
    addFunc(module,"env_processId",0,env_processId);

    return module;
}
