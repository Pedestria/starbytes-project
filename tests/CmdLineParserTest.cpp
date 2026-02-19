#include "starbytes/base/CmdLine.h"

#include <initializer_list>
#include <iostream>
#include <string>
#include <vector>

namespace {

int fail(const std::string &message) {
    std::cerr << "CmdLineParserTest failure: " << message << '\n';
    return 1;
}

bool expect(bool condition,const std::string &message) {
    if(!condition) {
        std::cerr << "CmdLineParserTest assertion failed: " << message << '\n';
        return false;
    }
    return true;
}

std::vector<const char *> makeArgv(std::initializer_list<const char *> values) {
    return std::vector<const char *>(values);
}

bool testDriverLikeParse() {
    starbytes::cl::Parser parser;
    parser.addCommand("run");
    parser.addCommand("compile");
    parser.addCommand("check");
    parser.addCommand("help");

    parser.addFlagOption("help",{"h"});
    parser.addFlagOption("version",{"V"});
    parser.addValueOption("modulename",{"m"});
    parser.addValueOption("output",{"o"});
    parser.addValueOption("out-dir",{"d"});
    parser.addMultiValueOption("native",{"n"});
    parser.addMultiValueOption("native-dir",{"L"});
    parser.addFlagOption("clean");
    parser.addFlagOption("print-module-path");
    parser.addFlagOption("profile-compile");
    parser.addValueOption("profile-compile-out");
    parser.addFlagOption("no-diagnostics");
    parser.addFlagOption("no-native-auto");

    auto argv = makeArgv({
        "starbytes",
        "run",
        "app.starb",
        "-m=App",
        "--output=build/out.starbmod",
        "--out-dir",
        ".starbytes",
        "--native",
        "./libA.ntstarbmod",
        "-n=./libB.ntstarbmod",
        "--native-dir",
        "./mods",
        "-L=./build/stdlib",
        "--clean",
        "--print-module-path",
        "--profile-compile",
        "--profile-compile-out=profile.json",
        "--no-diagnostics",
        "--no-native-auto",
        "--",
        "--user-arg",
        "tail"
    });

    auto result = parser.parse((int)argv.size(),argv.data());
    return expect(result.ok,"expected successful parse")
        && expect(result.command == "run","expected run command")
        && expect(result.positionals.size() == 1 && result.positionals[0] == "app.starb","expected single positional input")
        && expect(result.hasFlag("clean"),"expected clean flag")
        && expect(result.hasFlag("print-module-path"),"expected print-module-path flag")
        && expect(result.hasFlag("profile-compile"),"expected profile-compile flag")
        && expect(result.hasFlag("no-diagnostics"),"expected no-diagnostics flag")
        && expect(result.hasFlag("no-native-auto"),"expected no-native-auto flag")
        && expect(result.firstValue("modulename").value_or("") == "App","expected modulename value")
        && expect(result.firstValue("output").value_or("") == "build/out.starbmod","expected output value")
        && expect(result.firstValue("out-dir").value_or("") == ".starbytes","expected out-dir value")
        && expect(result.firstValue("profile-compile-out").value_or("") == "profile.json","expected profile-compile-out value")
        && expect(result.values("native").size() == 2,"expected two native values")
        && expect(result.values("native")[0] == "./libA.ntstarbmod","expected first native value")
        && expect(result.values("native")[1] == "./libB.ntstarbmod","expected second native value")
        && expect(result.values("native-dir").size() == 2,"expected two native-dir values")
        && expect(result.passthroughArgs.size() == 2,"expected passthrough args")
        && expect(result.passthroughArgs[0] == "--user-arg","expected passthrough arg[0]")
        && expect(result.passthroughArgs[1] == "tail","expected passthrough arg[1]");
}

bool testHelpAndVersion() {
    starbytes::cl::Parser parser;
    parser.addFlagOption("help",{"h"});
    parser.addFlagOption("version",{"V"});

    auto helpArgv = makeArgv({"starbytes","--help"});
    auto helpResult = parser.parse((int)helpArgv.size(),helpArgv.data());
    if(!expect(helpResult.ok,"expected help parse ok")
       || !expect(helpResult.hasFlag("help"),"expected help flag")) {
        return false;
    }

    auto versionArgv = makeArgv({"starbytes","-V"});
    auto versionResult = parser.parse((int)versionArgv.size(),versionArgv.data());
    return expect(versionResult.ok,"expected version parse ok")
        && expect(versionResult.hasFlag("version"),"expected version flag");
}

bool testUnknownOption() {
    starbytes::cl::Parser parser;
    parser.addFlagOption("help",{"h"});
    auto argv = makeArgv({"starbytes","--bad-option"});
    auto result = parser.parse((int)argv.size(),argv.data());
    return expect(!result.ok,"expected unknown option parse failure")
        && expect(result.error.find("Unknown option") != std::string::npos,"expected unknown option error text");
}

bool testMissingValue() {
    starbytes::cl::Parser parser;
    parser.addValueOption("modulename",{"m"});
    auto argv = makeArgv({"starbytes","--modulename"});
    auto result = parser.parse((int)argv.size(),argv.data());
    return expect(!result.ok,"expected missing value parse failure")
        && expect(result.error.find("Missing value for option") != std::string::npos,"expected missing value error text");
}

bool testFlagRejectsInlineValue() {
    starbytes::cl::Parser parser;
    parser.addFlagOption("clean");
    auto argv = makeArgv({"starbytes","--clean=true"});
    auto result = parser.parse((int)argv.size(),argv.data());
    return expect(!result.ok,"expected flag inline value rejection")
        && expect(result.error.find("does not accept a value") != std::string::npos,"expected flag rejection text");
}

bool testUnknownCommandFallsBackToPositional() {
    starbytes::cl::Parser parser;
    parser.addCommand("run");
    parser.addCommand("check");
    auto argv = makeArgv({"starbytes","app.starb"});
    auto result = parser.parse((int)argv.size(),argv.data());
    return expect(result.ok,"expected positional parse success")
        && expect(result.command.empty(),"expected no command")
        && expect(result.positionals.size() == 1 && result.positionals[0] == "app.starb","expected script positional");
}

}

int main() {
    if(!testDriverLikeParse()) {
        return fail("testDriverLikeParse");
    }
    if(!testHelpAndVersion()) {
        return fail("testHelpAndVersion");
    }
    if(!testUnknownOption()) {
        return fail("testUnknownOption");
    }
    if(!testMissingValue()) {
        return fail("testMissingValue");
    }
    if(!testFlagRejectsInlineValue()) {
        return fail("testFlagRejectsInlineValue");
    }
    if(!testUnknownCommandFallsBackToPositional()) {
        return fail("testUnknownCommandFallsBackToPositional");
    }
    return 0;
}
