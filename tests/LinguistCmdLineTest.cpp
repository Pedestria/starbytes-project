#include "starbytes/base/CmdLine.h"

#include <initializer_list>
#include <iostream>
#include <string>
#include <vector>

namespace {

int fail(const std::string &message) {
    std::cerr << "LinguistCmdLineTest failure: " << message << '\n';
    return 1;
}

bool expect(bool condition, const std::string &message) {
    if(!condition) {
        std::cerr << "LinguistCmdLineTest assertion failed: " << message << '\n';
        return false;
    }
    return true;
}

std::vector<const char *> makeArgv(std::initializer_list<const char *> values) {
    return std::vector<const char *>(values);
}

bool testPrettyWriteParse() {
    starbytes::cl::Parser parser;
    parser.addCommand("help");
    parser.addFlagOption("help", {"h"});
    parser.addFlagOption("version", {"V"});
    parser.addFlagOption("pretty-write");
    parser.addFlagOption("lint");
    parser.addFlagOption("suggest");
    parser.addFlagOption("code-actions");
    parser.addFlagOption("apply-safe-fixes");
    parser.addFlagOption("dry-run");
    parser.addFlagOption("modpath-aware");
    parser.addMultiValueOption("include");
    parser.addMultiValueOption("exclude");
    parser.addValueOption("max-files");

    auto argv = makeArgv({"starbytes-ling", "--pretty-write", "app.starb"});
    auto result = parser.parse((int)argv.size(), argv.data());

    return expect(result.ok, "expected successful parse")
        && expect(result.command.empty(), "expected no command")
        && expect(result.hasFlag("pretty-write"), "expected pretty-write flag")
        && expect(result.positionals.size() == 1 && result.positionals[0] == "app.starb", "expected single positional input");
}

bool testHelpAndVersionParse() {
    starbytes::cl::Parser parser;
    parser.addCommand("help");
    parser.addFlagOption("help", {"h"});
    parser.addFlagOption("version", {"V"});
    parser.addFlagOption("pretty-write");
    parser.addFlagOption("lint");
    parser.addFlagOption("suggest");
    parser.addFlagOption("code-actions");
    parser.addFlagOption("apply-safe-fixes");
    parser.addFlagOption("dry-run");
    parser.addFlagOption("modpath-aware");
    parser.addMultiValueOption("include");
    parser.addMultiValueOption("exclude");
    parser.addValueOption("max-files");

    auto helpArgv = makeArgv({"starbytes-ling", "help"});
    auto helpResult = parser.parse((int)helpArgv.size(), helpArgv.data());
    if(!expect(helpResult.ok, "expected help parse ok")
       || !expect(helpResult.command == "help", "expected help command")) {
        return false;
    }

    auto versionArgv = makeArgv({"starbytes-ling", "-V"});
    auto versionResult = parser.parse((int)versionArgv.size(), versionArgv.data());
    return expect(versionResult.ok, "expected version parse ok")
        && expect(versionResult.hasFlag("version"), "expected version flag");
}

bool testRejectInlineFlagValue() {
    starbytes::cl::Parser parser;
    parser.addFlagOption("pretty-write");
    parser.addFlagOption("apply-safe-fixes");

    auto argv = makeArgv({"starbytes-ling", "--pretty-write=true", "app.starb"});
    auto result = parser.parse((int)argv.size(), argv.data());
    return expect(!result.ok, "expected flag inline value rejection")
        && expect(result.error.find("does not accept a value") != std::string::npos,
                  "expected inline value rejection text");
}

bool testApplySafeFixesParse() {
    starbytes::cl::Parser parser;
    parser.addFlagOption("apply-safe-fixes");
    parser.addFlagOption("dry-run");
    parser.addFlagOption("modpath-aware");
    parser.addMultiValueOption("include");
    parser.addMultiValueOption("exclude");
    parser.addValueOption("max-files");

    auto argv = makeArgv({"starbytes-ling", "--apply-safe-fixes", "--dry-run", "--modpath-aware",
                          "--include", "src/*", "--exclude", "vendor/*", "--max-files", "64", "app.starb"});
    auto result = parser.parse((int)argv.size(), argv.data());
    return expect(result.ok, "expected apply-safe-fixes parse ok")
        && expect(result.hasFlag("apply-safe-fixes"), "expected apply-safe-fixes flag")
        && expect(result.hasFlag("dry-run"), "expected dry-run flag")
        && expect(result.hasFlag("modpath-aware"), "expected modpath-aware flag")
        && expect(result.values("include").size() == 1 && result.values("include")[0] == "src/*",
                  "expected include glob")
        && expect(result.values("exclude").size() == 1 && result.values("exclude")[0] == "vendor/*",
                  "expected exclude glob")
        && expect(result.firstValue("max-files").has_value() && *result.firstValue("max-files") == "64",
                  "expected max-files value")
        && expect(result.positionals.size() == 1 && result.positionals[0] == "app.starb",
                  "expected single positional for apply-safe-fixes");
}

} // namespace

int main() {
    if(!testPrettyWriteParse()) {
        return fail("testPrettyWriteParse");
    }
    if(!testHelpAndVersionParse()) {
        return fail("testHelpAndVersionParse");
    }
    if(!testRejectInlineFlagValue()) {
        return fail("testRejectInlineFlagValue");
    }
    if(!testApplySafeFixesParse()) {
        return fail("testApplySafeFixesParse");
    }
    return 0;
}
