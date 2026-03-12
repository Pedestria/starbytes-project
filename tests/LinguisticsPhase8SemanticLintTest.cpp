#include "starbytes/linguistics/Analysis.h"
#include "starbytes/linguistics/Config.h"
#include "starbytes/linguistics/LintEngine.h"
#include "starbytes/linguistics/Session.h"

#include <iostream>
#include <string>
#include <vector>

namespace {

int fail(const std::string &message) {
    std::cerr << "LinguisticsPhase8SemanticLintTest failure: " << message << '\n';
    return 1;
}

bool expect(bool condition, const std::string &message) {
    if(!condition) {
        std::cerr << "LinguisticsPhase8SemanticLintTest assertion failed: " << message << '\n';
        return false;
    }
    return true;
}

size_t countCode(const std::vector<starbytes::linguistics::LintFinding> &findings,
                 const std::string &code) {
    size_t count = 0;
    for(const auto &finding : findings) {
        if(finding.code == code) {
            ++count;
        }
    }
    return count;
}

bool containsMessageFragment(const std::vector<starbytes::linguistics::LintFinding> &findings,
                             const std::string &code,
                             const std::string &fragment) {
    for(const auto &finding : findings) {
        if(finding.code == code && finding.message.find(fragment) != std::string::npos) {
            return true;
        }
    }
    return false;
}

} // namespace

int main() {
    const std::string source =
        "decl top:Int\n"
        "print(top)\n"
        "\n"
        "func missing(flag: Bool) Int {\n"
        "  if (flag) {\n"
        "    return 1\n"
        "  }\n"
        "}\n"
        "\n"
        "func safe(flag: Bool) Int {\n"
        "  decl value:Int\n"
        "  if (flag) {\n"
        "    value = 1\n"
        "  } else {\n"
        "    value = 2\n"
        "  }\n"
        "  return value\n"
        "}\n"
        "\n"
        "func maybe(flag: Bool) Int {\n"
        "  decl value:Int\n"
        "  if (flag) {\n"
        "    value = 1\n"
        "  }\n"
        "  return value\n"
        "}\n"
        "\n"
        "func shadowed(x: Int) Int {\n"
        "  decl x:Int = 7\n"
        "  secure(decl ok = \"x\") catch (x: String) {\n"
        "    print(x)\n"
        "  }\n"
        "  return x\n"
        "}\n";

    starbytes::linguistics::LinguisticsSession session("memory://phase8-semantic-lint.starb", source);
    auto config = starbytes::linguistics::LinguisticsConfig::defaults();
    config.lint.maxFindings = 1000;

    starbytes::linguistics::LintEngine lint;
    auto analysis = starbytes::linguistics::buildCompilerLintAnalysis(session);
    if(!expect(analysis.view().readyForSemanticLint(), "analysis should be ready")) {
        return fail("analysis-ready");
    }
    if(!expect(analysis.syntaxDiagnostics.empty(), "fixture should parse without syntax errors")) {
        return fail("analysis-syntax");
    }

    auto result = lint.run(analysis.view(), config);

    if(!expect(countCode(result.findings, "SB-LINT-CORR-C0002") == 0,
               "must-return should no longer be emitted as a lint finding")) {
        return fail("must-return-removed");
    }
    if(!expect(countCode(result.findings, "SB-LINT-CORR-C0003") == 0,
               "use-before-init should no longer be emitted as a lint finding")) {
        return fail("use-before-init-removed");
    }
    if(!expect(countCode(result.findings, "SB-LINT-CORR-C0004") == 2,
               "expected exactly two shadowing findings")) {
        return fail("shadowing-count");
    }

    if(!expect(containsMessageFragment(result.findings, "SB-LINT-CORR-C0004", "shadows an outer parameter"),
               "shadowing finding should report outer parameter shadowing")) {
        return fail("shadowing-message");
    }
    if(!expect(!containsMessageFragment(result.findings, "SB-LINT-CORR-C0004", "safe"),
               "safe() should not produce any shadowing finding")) {
        return fail("safe-no-shadowing");
    }

    return 0;
}
