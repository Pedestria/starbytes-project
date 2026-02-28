#include "starbytes/linguistics/CodeActionEngine.h"
#include "starbytes/linguistics/Config.h"
#include "starbytes/linguistics/LintEngine.h"
#include "starbytes/linguistics/Session.h"
#include "starbytes/linguistics/SuggestionEngine.h"

#include <iostream>
#include <string>

namespace {

int fail(const std::string &message) {
    std::cerr << "LinguisticsPhase4SuggestionActionTest failure: " << message << '\n';
    return 1;
}

bool expect(bool condition, const std::string &message) {
    if(!condition) {
        std::cerr << "LinguisticsPhase4SuggestionActionTest assertion failed: " << message << '\n';
        return false;
    }
    return true;
}

bool hasSuggestionIdPrefix(const starbytes::linguistics::SuggestionResult &result, const std::string &prefix) {
    for(const auto &suggestion : result.suggestions) {
        if(suggestion.id.rfind(prefix, 0) == 0) {
            return true;
        }
    }
    return false;
}

} // namespace

int main() {
    starbytes::linguistics::LinguisticsSession session(
        "memory://phase4.starb",
        "decl value = 1   \n"
        "if (value = 2) {\n"
        "  print(value)\n"
        "}\n"
        "secure(decl ok = \"x\") catch {\n"
        "  print(\"err\")\n"
        "}\n"
        "func missingDocs(x:Int) Int {\n"
        "  return x\n"
        "}");

    auto config = starbytes::linguistics::LinguisticsConfig::defaults();

    starbytes::linguistics::SuggestionEngine suggestionEngine;
    starbytes::linguistics::SuggestionRequest suggestionRequest;
    suggestionRequest.includeLowConfidence = false;
    auto suggestionResult = suggestionEngine.run(session, config, suggestionRequest);
    if(!expect(!suggestionResult.suggestions.empty(), "expected non-empty suggestions")) {
        return fail("suggestions-empty");
    }
    if(!expect(hasSuggestionIdPrefix(suggestionResult, "suggest.style.trailing_newline"),
               "expected trailing newline suggestion")) {
        return fail("missing-trailing-newline-suggestion");
    }

    for(size_t i = 1; i < suggestionResult.suggestions.size(); ++i) {
        if(!expect(suggestionResult.suggestions[i - 1].confidence >= suggestionResult.suggestions[i].confidence,
                   "suggestions should be sorted by descending confidence")) {
            return fail("suggestion-order");
        }
    }

    starbytes::linguistics::LintEngine lintEngine;
    auto lintResult = lintEngine.run(session, config);
    if(!expect(!lintResult.findings.empty(), "expected lint findings for phase 4 action generation")) {
        return fail("lint-findings-empty");
    }

    starbytes::linguistics::CodeActionEngine actionEngine;
    starbytes::linguistics::CodeActionRequest actionRequest;
    actionRequest.safeOnly = true;
    auto actionResult = actionEngine.build(lintResult.findings, suggestionResult.suggestions, config, actionRequest);
    if(!expect(!actionResult.actions.empty(), "expected non-empty safe code actions")) {
        return fail("actions-empty");
    }

    for(const auto &action : actionResult.actions) {
        if(!expect(action.isSafe, "safeOnly request should emit only safe actions")) {
            return fail("unsafe-action-emitted");
        }
        if(!expect(!action.edits.empty(), "code actions should contain edits")) {
            return fail("action-missing-edits");
        }
        for(size_t i = 1; i < action.edits.size(); ++i) {
            const auto &prev = action.edits[i - 1].span.start;
            const auto &curr = action.edits[i].span.start;
            if(!expect(prev.line < curr.line || (prev.line == curr.line && prev.character <= curr.character),
                       "action edits should be deterministically sorted")) {
                return fail("edit-order");
            }
        }
    }

    return 0;
}
