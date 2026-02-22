#include <starbytes/interop.h>
#include "starbytes/base/ADT.h"

#include <array>
#include <cstdio>
#include <string>
#include <vector>

#if defined(HAS_SYS_WAIT_H)
#include <sys/wait.h>
#endif

namespace {

using starbytes::Twine;

struct NativeArgsLayout {
    unsigned argc = 0;
    unsigned index = 0;
    StarbytesObject *argv = nullptr;
};

struct ProcessRunOutput {
    int exitCode = -1;
    std::string output;
    bool started = false;
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

std::string quoteShellArg(const std::string &value) {
#if defined(_WIN32)
    std::string out;
    out.reserve(value.size() + 2);
    out.push_back('"');
    for(char ch : value) {
        if(ch == '"') {
            out.push_back('\\');
        }
        out.push_back(ch);
    }
    out.push_back('"');
    return out;
#else
    std::string out;
    out.reserve(value.size() + 2);
    out.push_back('\'');
    for(char ch : value) {
        if(ch == '\'') {
            out += "'\\''";
            continue;
        }
        out.push_back(ch);
    }
    out.push_back('\'');
    return out;
#endif
}

std::string buildCommandFromArgs(const std::string &program,const std::vector<std::string> &args) {
    Twine command;
    command + quoteShellArg(program);
    for(const auto &arg : args) {
        command + " ";
        command + quoteShellArg(arg);
    }
    return command.str();
}

ProcessRunOutput runCommandCapture(const std::string &command) {
    ProcessRunOutput out;
    std::string commandWithRedirect = command + " 2>&1";

#if defined(_WIN32)
    FILE *pipe = _popen(commandWithRedirect.c_str(),"r");
#else
    FILE *pipe = popen(commandWithRedirect.c_str(),"r");
#endif
    if(!pipe) {
        return out;
    }

    out.started = true;
    std::array<char,4096> buffer{};
    while(std::fgets(buffer.data(),(int)buffer.size(),pipe) != nullptr) {
        out.output += buffer.data();
    }

#if defined(_WIN32)
    auto status = _pclose(pipe);
    out.exitCode = status;
#else
    auto status = pclose(pipe);
#if defined(HAS_SYS_WAIT_H)
    if(WIFEXITED(status)) {
        out.exitCode = WEXITSTATUS(status);
    }
    else {
        out.exitCode = status;
    }
#else
    out.exitCode = status;
#endif
#endif

    return out;
}

StarbytesObject makeProcessResult(const ProcessRunOutput &result) {
    auto object = StarbytesObjectNew(StarbytesMakeClass("ProcessResult"));
    StarbytesObjectAddProperty(object,(char *)"exitCode",makeInt(result.exitCode));
    StarbytesObjectAddProperty(object,(char *)"output",StarbytesStrNewWithData(result.output.c_str()));
    StarbytesObjectAddProperty(object,(char *)"success",makeBool(result.exitCode == 0));
    return object;
}

STARBYTES_FUNC(process_run) {
    skipOptionalModuleReceiver(args,1);

    std::string command;
    if(!readStringArg(args,command) || command.empty()) {
        return nullptr;
    }

    auto result = runCommandCapture(command);
    if(!result.started) {
        return nullptr;
    }
    return makeProcessResult(result);
}

STARBYTES_FUNC(process_runArgs) {
    skipOptionalModuleReceiver(args,2);

    std::string program;
    std::vector<std::string> argv;
    if(!readStringArg(args,program) || !readStringArrayArg(args,argv) || program.empty()) {
        return nullptr;
    }

    auto command = buildCommandFromArgs(program,argv);
    auto result = runCommandCapture(command);
    if(!result.started) {
        return nullptr;
    }
    return makeProcessResult(result);
}

STARBYTES_FUNC(process_shellQuote) {
    skipOptionalModuleReceiver(args,1);

    std::string value;
    if(!readStringArg(args,value)) {
        return StarbytesStrNewWithData("");
    }
    auto quoted = quoteShellArg(value);
    return StarbytesStrNewWithData(quoted.c_str());
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

    addFunc(module,"process_run",1,process_run);
    addFunc(module,"process_runArgs",2,process_runArgs);
    addFunc(module,"process_shellQuote",1,process_shellQuote);

    return module;
}
