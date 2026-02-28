#include "starbytes/linguistics/CodeActionEngine.h"
#include "starbytes/linguistics/Config.h"
#include "starbytes/linguistics/FormatterEngine.h"
#include "starbytes/linguistics/LintEngine.h"
#include "starbytes/linguistics/Serializer.h"
#include "starbytes/linguistics/Session.h"
#include "starbytes/linguistics/SuggestionEngine.h"

#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace {

int fail(const std::string &message) {
    std::cerr << "LinguisticsPhase7GoldenTest failure: " << message << '\n';
    return 1;
}

bool expect(bool condition, const std::string &message) {
    if(!condition) {
        std::cerr << "LinguisticsPhase7GoldenTest assertion failed: " << message << '\n';
        return false;
    }
    return true;
}

std::string snapshotSuggestions(const starbytes::linguistics::SuggestionResult &result) {
    std::ostringstream out;
    out << "suggestions:" << result.suggestions.size();
    for(const auto &item : result.suggestions) {
        out << "\n[" << item.id << "] "
            << std::fixed << std::setprecision(2) << item.confidence
            << " " << item.title;
    }
    return out.str();
}

std::string snapshotActions(const starbytes::linguistics::CodeActionResult &result) {
    std::ostringstream out;
    out << "actions:" << result.actions.size();
    for(const auto &item : result.actions) {
        out << "\n[" << item.id << "] "
            << item.kind << " "
            << (item.isSafe ? "safe" : "unsafe")
            << " " << item.title;
    }
    return out.str();
}

} // namespace

int main() {
    auto config = starbytes::linguistics::LinguisticsConfig::defaults();

    // Golden formatter snapshot.
    {
        starbytes::linguistics::LinguisticsSession formatSession(
            "memory://golden-format.starb",
            "decl x=1\nfunc add(a:Int,b:Int)Int{return a+b}\nadd(x,2)\n");
        starbytes::linguistics::FormatterEngine formatter;
        auto formatResult = formatter.format(formatSession, config);
        if(!expect(formatResult.ok, "formatter should succeed")) {
            return fail("formatter-ok");
        }

        const std::string expectedFormatted =
            "decl x = 1\n"
            "func add(a: Int, b: Int) Int {\n"
            "  return a + b\n"
            "}\n"
            "add(x, 2)\n";
        if(!expect(formatResult.formattedText == expectedFormatted, "formatter golden output mismatch")) {
            return fail("formatter-golden");
        }
    }

    // Golden lint/suggestion/action snapshots.
    {
        const std::string source =
            "decl x = 1   \n"
            "if (x = 2) {\n"
            "  print(x)\n"
            "}\n"
            "func f(x:Int) Int {\n"
            "  return x\n"
            "}";

        starbytes::linguistics::LinguisticsSession session("memory://golden-lint.starb", source);
        starbytes::linguistics::LintEngine lintEngine;
        auto lintResult = lintEngine.run(session, config);

        const std::string expectedLintText =
            "lint-findings: 3\n"
            "[SB-LINT-STYLE-S0001] warning L1:10 Trailing whitespace should be removed.\n"
            "[SB-LINT-CORR-C0001] warning L2:6 Suspicious assignment in condition; did you mean `==`?\n"
            "[SB-LINT-DOC-D0001] info L5:0 Top-level declaration should include a leading documentation comment.";
        auto lintText = starbytes::linguistics::LinguisticsSerializer::toText(lintResult);
        if(!expect(lintText == expectedLintText, "lint golden output mismatch")) {
            return fail("lint-golden");
        }

        starbytes::linguistics::SuggestionEngine suggestionEngine;
        starbytes::linguistics::SuggestionRequest suggestionRequest;
        suggestionRequest.includeLowConfidence = false;
        auto suggestionResult = suggestionEngine.run(session, config, suggestionRequest);

        const std::string expectedSuggestionSnapshot =
            "suggestions:4\n"
            "[suggest.style.trailing_whitespace.L1.C10] 0.98 Trim trailing whitespace\n"
            "[suggest.correctness.assignment_in_condition.L2.C6] 0.96 Replace assignment with equality check\n"
            "[suggest.style.trailing_newline] 0.94 Ensure trailing newline\n"
            "[suggest.docs.missing_decl_comment.L5.C0] 0.76 Add declaration documentation";
        auto suggestionSnapshot = snapshotSuggestions(suggestionResult);
        if(!expect(suggestionSnapshot == expectedSuggestionSnapshot, "suggestion golden output mismatch")) {
            return fail("suggestion-golden");
        }

        starbytes::linguistics::CodeActionEngine actionEngine;
        starbytes::linguistics::CodeActionRequest actionRequest;
        actionRequest.safeOnly = true;
        auto actionResult = actionEngine.build(lintResult.findings, suggestionResult.suggestions, config, actionRequest);

        const std::string expectedActionSnapshot =
            "actions:4\n"
            "[style.trailing_whitespace.trim] quickfix safe Trim trailing whitespace\n"
            "[suggest.correctness.assignment_in_condition.L2.C6.apply] quickfix safe Replace assignment with equality check\n"
            "[suggest.style.trailing_newline.apply] quickfix safe Ensure trailing newline\n"
            "[suggest.docs.missing_decl_comment.L5.C0.apply] quickfix safe Add declaration documentation";
        auto actionSnapshot = snapshotActions(actionResult);
        if(!expect(actionSnapshot == expectedActionSnapshot, "action golden output mismatch")) {
            return fail("action-golden");
        }
    }

    return 0;
}
