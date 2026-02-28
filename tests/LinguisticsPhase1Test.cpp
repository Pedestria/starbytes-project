#include "starbytes/linguistics/CodeActionEngine.h"
#include "starbytes/linguistics/Config.h"
#include "starbytes/linguistics/FormatterEngine.h"
#include "starbytes/linguistics/LintEngine.h"
#include "starbytes/linguistics/Serializer.h"
#include "starbytes/linguistics/Session.h"
#include "starbytes/linguistics/SuggestionEngine.h"

#include <iostream>
#include <string>

namespace {

int fail(const std::string &message) {
    std::cerr << "LinguisticsPhase1Test failure: " << message << '\n';
    return 1;
}

bool expect(bool condition, const std::string &message) {
    if(!condition) {
        std::cerr << "LinguisticsPhase1Test assertion failed: " << message << '\n';
        return false;
    }
    return true;
}

bool startsWith(const std::string &value, const std::string &prefix) {
    return value.rfind(prefix, 0) == 0;
}

} // namespace

int main() {
    auto config = starbytes::linguistics::LinguisticsConfig::defaults();
    starbytes::linguistics::LinguisticsSession session(
        "memory://test.starb",
        "decl x=1\nfunc add(a:Int,b:Int)Int{return a+b}\nadd(x,2)\n");

    starbytes::linguistics::FormatterEngine formatter;
    auto formatResult = formatter.format(session, config);
    if(!expect(formatResult.ok, "formatter should succeed")) {
        return fail("formatter-run");
    }
    if(!expect(formatResult.formattedText
                   == "decl x = 1\nfunc add(a: Int, b: Int) Int {\n  return a + b\n}\nadd(x, 2)\n",
               "compiler-backed formatting output mismatch")) {
        return fail("formatter-output");
    }
    if(!expect(!formatResult.notes.empty(), "formatter should emit phase notes")) {
        return fail("formatter-notes");
    }

    starbytes::linguistics::LinguisticsSession idempotenceSession("memory://idempotence.starb", formatResult.formattedText);
    auto secondPass = formatter.format(idempotenceSession, config);
    if(!expect(secondPass.ok, "second pass formatting should succeed")) {
        return fail("formatter-second-pass");
    }
    if(!expect(secondPass.formattedText == formatResult.formattedText, "formatter should be idempotent")) {
        return fail("formatter-idempotence");
    }

    starbytes::linguistics::LinguisticsSession commentSession(
        "memory://comments.starb",
        "/// comment\nfunc id(x:Int) Int {\n  return x\n}\n");
    auto commentPass = formatter.format(commentSession, config);
    if(!expect(commentPass.ok, "comment fallback formatting should succeed")) {
        return fail("formatter-comment-ok");
    }
    if(!expect(commentPass.formattedText == "/// comment\nfunc id(x:Int) Int {\n  return x\n}\n",
               "comment fallback should preserve comment source layout (preview mode)")) {
        return fail("formatter-comment-fallback");
    }
    if(!expect(!commentPass.notes.empty(), "comment fallback should emit a note")) {
        return fail("formatter-comment-note");
    }

    starbytes::linguistics::LintEngine lint;
    auto lintResult = lint.run(session, config);
    if(!expect(!lintResult.findings.empty(), "phase 3 lint should emit baseline findings")) {
        return fail("lint-non-empty");
    }
    bool foundDocsRule = false;
    for(const auto &finding : lintResult.findings) {
        if(!expect(startsWith(finding.code, "SB-LINT-"), "lint finding should include standardized code")) {
            return fail("lint-code-family");
        }
        if(finding.id == "docs.missing_decl_comment") {
            foundDocsRule = true;
        }
    }
    if(!expect(foundDocsRule, "expected docs baseline lint finding")) {
        return fail("lint-docs-finding");
    }

    starbytes::linguistics::SuggestionEngine suggestions;
    auto suggestionResult = suggestions.run(session, config);
    if(!expect(!suggestionResult.suggestions.empty(), "phase 4 suggestions should be emitted")) {
        return fail("suggest-non-empty");
    }

    starbytes::linguistics::CodeActionEngine actions;
    auto actionResult = actions.build(lintResult.findings, suggestionResult.suggestions, config);
    if(!expect(!actionResult.actions.empty(), "phase 4 code actions should be emitted")) {
        return fail("actions-non-empty");
    }

    auto lintText = starbytes::linguistics::LinguisticsSerializer::toText(lintResult);
    auto suggestionText = starbytes::linguistics::LinguisticsSerializer::toText(suggestionResult);
    auto actionText = starbytes::linguistics::LinguisticsSerializer::toText(actionResult);

    if(!expect(startsWith(lintText, "lint-findings: "), "lint serializer output mismatch")) {
        return fail("serializer-lint");
    }
    if(!expect(startsWith(suggestionText, "suggestions: "), "suggestion serializer output mismatch")) {
        return fail("serializer-suggestion");
    }
    if(!expect(startsWith(actionText, "code-actions: "), "action serializer output mismatch")) {
        return fail("serializer-action");
    }

    return 0;
}
