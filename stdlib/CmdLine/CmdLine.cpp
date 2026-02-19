#include <starbytes/interop.h>

#include <string>
#include <unordered_set>
#include <vector>

namespace {

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

StarbytesObject stringListToArray(const std::vector<std::string> &values) {
    auto out = StarbytesArrayNew();
    for(const auto &value : values) {
        StarbytesArrayPush(out,StarbytesStrNewWithData(value.c_str()));
    }
    return out;
}

std::vector<std::string> getRuntimeArgs() {
    std::vector<std::string> out;
    auto count = StarbytesRuntimeGetScriptArgCount();
    out.reserve(count);
    for(unsigned i = 0; i < count; ++i) {
        auto value = StarbytesRuntimeGetScriptArg(i);
        out.emplace_back(value ? value : "");
    }
    return out;
}

std::string normalizeOptionName(const std::string &value) {
    if(value.rfind("--",0) == 0) {
        return value.substr(2);
    }
    if(value.rfind("-",0) == 0) {
        return value.substr(1);
    }
    return value;
}

std::unordered_set<std::string> buildOptionNames(const std::string &name,
                                                 const std::vector<std::string> &aliases) {
    std::unordered_set<std::string> names;
    auto base = normalizeOptionName(name);
    if(!base.empty()) {
        names.insert(base);
    }
    for(const auto &alias : aliases) {
        auto normalized = normalizeOptionName(alias);
        if(!normalized.empty()) {
            names.insert(normalized);
        }
    }
    return names;
}

bool isOptionToken(const std::string &token) {
    return token.size() > 1 && token[0] == '-';
}

struct ParsedOptionToken {
    bool isOption = false;
    std::string key;
    bool hasInlineValue = false;
    std::string inlineValue;
};

ParsedOptionToken parseOptionToken(const std::string &token) {
    ParsedOptionToken parsed;
    if(token == "--" || !isOptionToken(token)) {
        return parsed;
    }

    parsed.isOption = true;
    auto keyStart = (token.rfind("--",0) == 0) ? 2u : 1u;
    auto body = token.substr(keyStart);
    auto eqPos = body.find('=');
    if(eqPos == std::string::npos) {
        parsed.key = body;
        return parsed;
    }

    parsed.key = body.substr(0,eqPos);
    parsed.hasInlineValue = true;
    parsed.inlineValue = body.substr(eqPos + 1);
    return parsed;
}

STARBYTES_FUNC(cmd_rawArgs) {
    skipOptionalModuleReceiver(args,0);
    return stringListToArray(getRuntimeArgs());
}

STARBYTES_FUNC(cmd_argCount) {
    skipOptionalModuleReceiver(args,0);
    return makeInt((int)StarbytesRuntimeGetScriptArgCount());
}

STARBYTES_FUNC(cmd_executablePath) {
    skipOptionalModuleReceiver(args,0);
    auto path = StarbytesRuntimeGetExecutablePath();
    return StarbytesStrNewWithData(path ? path : "");
}

STARBYTES_FUNC(cmd_scriptPath) {
    skipOptionalModuleReceiver(args,0);
    auto path = StarbytesRuntimeGetScriptPath();
    return StarbytesStrNewWithData(path ? path : "");
}

STARBYTES_FUNC(cmd_hasFlag) {
    skipOptionalModuleReceiver(args,2);

    std::string name;
    std::vector<std::string> aliases;
    if(!readStringArg(args,name) || !readStringArrayArg(args,aliases)) {
        return makeBool(false);
    }

    auto names = buildOptionNames(name,aliases);
    if(names.empty()) {
        return makeBool(false);
    }

    auto argv = getRuntimeArgs();
    for(const auto &token : argv) {
        auto parsed = parseOptionToken(token);
        if(!parsed.isOption || parsed.key.empty()) {
            continue;
        }
        if(names.find(parsed.key) != names.end()) {
            return makeBool(true);
        }
    }
    return makeBool(false);
}

STARBYTES_FUNC(cmd_flagValue) {
    skipOptionalModuleReceiver(args,2);

    std::string name;
    std::vector<std::string> aliases;
    if(!readStringArg(args,name) || !readStringArrayArg(args,aliases)) {
        return nullptr;
    }

    auto names = buildOptionNames(name,aliases);
    if(names.empty()) {
        return nullptr;
    }

    auto argv = getRuntimeArgs();
    for(size_t i = 0; i < argv.size(); ++i) {
        auto parsed = parseOptionToken(argv[i]);
        if(!parsed.isOption || parsed.key.empty() || names.find(parsed.key) == names.end()) {
            continue;
        }
        if(parsed.hasInlineValue) {
            return StarbytesStrNewWithData(parsed.inlineValue.c_str());
        }
        if((i + 1) < argv.size() && !isOptionToken(argv[i + 1])) {
            return StarbytesStrNewWithData(argv[i + 1].c_str());
        }
        return nullptr;
    }
    return nullptr;
}

STARBYTES_FUNC(cmd_flagValues) {
    skipOptionalModuleReceiver(args,2);

    std::string name;
    std::vector<std::string> aliases;
    if(!readStringArg(args,name) || !readStringArrayArg(args,aliases)) {
        return StarbytesArrayNew();
    }

    auto names = buildOptionNames(name,aliases);
    if(names.empty()) {
        return StarbytesArrayNew();
    }

    std::vector<std::string> values;
    auto argv = getRuntimeArgs();
    for(size_t i = 0; i < argv.size(); ++i) {
        auto parsed = parseOptionToken(argv[i]);
        if(!parsed.isOption || parsed.key.empty() || names.find(parsed.key) == names.end()) {
            continue;
        }
        if(parsed.hasInlineValue) {
            values.push_back(parsed.inlineValue);
            continue;
        }
        if((i + 1) < argv.size() && !isOptionToken(argv[i + 1])) {
            values.push_back(argv[i + 1]);
            ++i;
        }
    }
    return stringListToArray(values);
}

STARBYTES_FUNC(cmd_positionals) {
    skipOptionalModuleReceiver(args,0);

    std::vector<std::string> positionals;
    auto argv = getRuntimeArgs();
    bool onlyPositionals = false;
    for(size_t i = 0; i < argv.size(); ++i) {
        const auto &token = argv[i];
        if(!onlyPositionals && token == "--") {
            onlyPositionals = true;
            continue;
        }
        if(onlyPositionals) {
            positionals.push_back(token);
            continue;
        }

        auto parsed = parseOptionToken(token);
        if(!parsed.isOption) {
            positionals.push_back(token);
            continue;
        }

        if(!parsed.hasInlineValue && (i + 1) < argv.size() && !isOptionToken(argv[i + 1])) {
            ++i;
        }
    }

    return stringListToArray(positionals);
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

    addFunc(module,"cmd_rawArgs",0,cmd_rawArgs);
    addFunc(module,"cmd_argCount",0,cmd_argCount);
    addFunc(module,"cmd_executablePath",0,cmd_executablePath);
    addFunc(module,"cmd_scriptPath",0,cmd_scriptPath);
    addFunc(module,"cmd_hasFlag",2,cmd_hasFlag);
    addFunc(module,"cmd_flagValue",2,cmd_flagValue);
    addFunc(module,"cmd_flagValues",2,cmd_flagValues);
    addFunc(module,"cmd_positionals",0,cmd_positionals);

    return module;
}
