#include "starbytes/linguistics/CodeActionEngine.h"
#include "starbytes/linguistics/Config.h"
#include "starbytes/linguistics/FormatterEngine.h"
#include "starbytes/linguistics/LintEngine.h"
#include "starbytes/linguistics/Session.h"
#include "starbytes/linguistics/SuggestionEngine.h"

#include <iostream>
#include <string>
#include <vector>

namespace {

int fail(const std::string &message) {
    std::cerr << "LinguisticsPhase7RegressionTest failure: " << message << '\n';
    return 1;
}

bool expect(bool condition, const std::string &message) {
    if(!condition) {
        std::cerr << "LinguisticsPhase7RegressionTest assertion failed: " << message << '\n';
        return false;
    }
    return true;
}

std::string repeatedPattern(const std::string &base, size_t count) {
    std::string out;
    out.reserve(base.size() * count);
    for(size_t i = 0; i < count; ++i) {
        out += base;
    }
    return out;
}

bool runCase(const std::string &name,
             const std::string &source,
             const starbytes::linguistics::LinguisticsConfig &config) {
    starbytes::linguistics::LinguisticsSession session("memory://" + name + ".starb", source);
    starbytes::linguistics::LintEngine lintEngine;
    starbytes::linguistics::SuggestionEngine suggestionEngine;
    starbytes::linguistics::CodeActionEngine actionEngine;
    starbytes::linguistics::FormatterEngine formatterEngine;

    auto lintResult = lintEngine.run(session, config);
    for(const auto &finding : lintResult.findings) {
        if(!expect(finding.span.isValid(), "lint finding span must be valid")) {
            return false;
        }
    }

    starbytes::linguistics::SuggestionRequest suggestionRequest;
    suggestionRequest.includeLowConfidence = true;
    auto suggestionResult = suggestionEngine.run(session, config, suggestionRequest);
    for(const auto &suggestion : suggestionResult.suggestions) {
        if(!expect(suggestion.span.isValid(), "suggestion span must be valid")) {
            return false;
        }
    }

    starbytes::linguistics::CodeActionRequest actionRequest;
    actionRequest.safeOnly = false;
    auto actionResult = actionEngine.build(lintResult.findings, suggestionResult.suggestions, config, actionRequest);
    for(const auto &action : actionResult.actions) {
        for(const auto &edit : action.edits) {
            if(!expect(edit.span.isValid(), "code action edit span must be valid")) {
                return false;
            }
        }
    }

    auto formatResult = formatterEngine.format(session, config);
    if(!expect(!formatResult.formattedText.empty() || source.empty(), "formatter should return text for non-empty sources")) {
        return false;
    }
    return true;
}

} // namespace

int main() {
    auto config = starbytes::linguistics::LinguisticsConfig::defaults();
    config.lint.maxFindings = 100000;
    config.suggestions.maxSuggestions = 100000;

    const std::vector<std::pair<std::string, std::string>> cases = {
        {"empty", ""},
        {"malformed-parens", "func broken(x:Int Int {\n  return x\n"},
        {"unterminated-string", "decl msg:String = \"unterminated\nprint(msg)\n"},
        {"deep-nesting", repeatedPattern("if (x == 1) {\n", 48) + repeatedPattern("}\n", 48)}
    };

    for(const auto &testCase : cases) {
        if(!runCase(testCase.first, testCase.second, config)) {
            return fail(testCase.first);
        }
    }

    return 0;
}
