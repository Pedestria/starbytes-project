#include "starbytes/linguistics/Analysis.h"
#include "starbytes/linguistics/Config.h"
#include "starbytes/linguistics/LintEngine.h"
#include "starbytes/linguistics/Session.h"

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

namespace {

int fail(const std::string &message) {
    std::cerr << "LinguisticsPhase3LintTest failure: " << message << '\n';
    return 1;
}

bool expect(bool condition, const std::string &message) {
    if(!condition) {
        std::cerr << "LinguisticsPhase3LintTest assertion failed: " << message << '\n';
        return false;
    }
    return true;
}

bool containsCode(const std::vector<starbytes::linguistics::LintFinding> &findings,
                  const std::string &code) {
    for(const auto &finding : findings) {
        if(finding.code == code) {
            return true;
        }
    }
    return false;
}

} // namespace

int main() {
    starbytes::linguistics::LinguisticsSession session(
        "memory://phase3-lint.starb",
        "decl value = 1   \n"
        "if (value = 2) {\n"
        "  for (value < 5) {\n"
        "    decl worker = new Worker()\n"
        "  }\n"
        "}\n"
        "secure(decl ok = \"x\") catch {\n"
        "  print(\"err\")\n"
        "}\n"
        "func undocumented(x:Int) Int {\n"
        "  return x\n"
        "}\n");

    auto config = starbytes::linguistics::LinguisticsConfig::defaults();
    starbytes::linguistics::LintEngine lint;
    auto analysis = starbytes::linguistics::buildCompilerLintAnalysis(session);

    if(!expect(analysis.view().readyForSemanticLint(), "compiler-backed lint analysis should be available")) {
        return fail("analysis-ready");
    }
    if(!expect(analysis.syntaxDiagnostics.empty(), "baseline lint fixture should parse without syntax diagnostics")) {
        return fail("analysis-syntax-clean");
    }
    if(!expect(analysis.semanticHadError,
               "baseline lint fixture should still preserve raw AST even when semantic diagnostics exist")) {
        return fail("analysis-semantic-errors");
    }

    auto descriptors = lint.ruleDescriptors();
    if(!expect(descriptors.size() >= 5, "expected baseline rule registry descriptors")) {
        return fail("rule-descriptors");
    }

    auto lintResult = lint.run(analysis.view(), config);
    if(!expect(lintResult.findings.size() >= 5, "expected at least one finding per phase 3 baseline category")) {
        return fail("baseline-findings-count");
    }

    if(!expect(containsCode(lintResult.findings, "SB-LINT-STYLE-S0001"), "expected style finding")) {
        return fail("missing-style");
    }
    if(!expect(containsCode(lintResult.findings, "SB-LINT-CORR-C0001"), "expected correctness finding")) {
        return fail("missing-correctness");
    }
    if(!expect(containsCode(lintResult.findings, "SB-LINT-PERF-P0001"), "expected performance finding")) {
        return fail("missing-performance");
    }
    if(!expect(containsCode(lintResult.findings, "SB-LINT-SAFE-A0001"), "expected safety finding")) {
        return fail("missing-safety");
    }
    if(!expect(containsCode(lintResult.findings, "SB-LINT-DOC-D0001"), "expected docs finding")) {
        return fail("missing-docs");
    }

    for(size_t i = 1; i < lintResult.findings.size(); ++i) {
        const auto &prev = lintResult.findings[i - 1];
        const auto &curr = lintResult.findings[i];
        if(prev.span.start.line > curr.span.start.line) {
            return fail("deterministic-order-line");
        }
        if(prev.span.start.line == curr.span.start.line && prev.span.start.character > curr.span.start.character) {
            return fail("deterministic-order-char");
        }
    }

    auto strictConfig = config;
    strictConfig.lint.strict = true;
    auto strictResult = lint.run(session, strictConfig);
    if(!expect(!strictResult.findings.empty(), "strict lint should still emit findings")) {
        return fail("strict-findings");
    }
    for(const auto &finding : strictResult.findings) {
        if(!expect(finding.severity == starbytes::linguistics::FindingSeverity::Error,
                   "strict mode should escalate finding severity to error")) {
            return fail("strict-severity");
        }
    }

    auto filteredConfig = config;
    filteredConfig.lint.enabledRules = {"safety.untyped_catch"};
    auto filteredResult = lint.run(session, filteredConfig);
    if(!expect(!filteredResult.findings.empty(), "filtered lint should emit selected findings")) {
        return fail("filtered-findings");
    }
    for(const auto &finding : filteredResult.findings) {
        if(!expect(finding.id == "safety.untyped_catch", "enabledRules should scope results to selected rule")) {
            return fail("filtered-selection");
        }
    }

    auto disabledConfig = config;
    disabledConfig.lint.disabledRules = {"docs"};
    auto disabledResult = lint.run(session, disabledConfig);
    if(!expect(!containsCode(disabledResult.findings, "SB-LINT-DOC-D0001"),
               "disabled docs selector should suppress docs finding")) {
        return fail("disabled-selector");
    }

    auto cappedConfig = config;
    cappedConfig.lint.maxFindings = 2;
    auto cappedResult = lint.run(session, cappedConfig);
    if(!expect(cappedResult.findings.size() <= 2, "maxFindings should cap lint result length")) {
        return fail("max-findings-cap");
    }

    return 0;
}
