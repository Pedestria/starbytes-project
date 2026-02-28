#include "starbytes/linguistics/CodeActionEngine.h"
#include "starbytes/linguistics/Config.h"
#include "starbytes/linguistics/FormatterEngine.h"
#include "starbytes/linguistics/LintEngine.h"
#include "starbytes/linguistics/Session.h"
#include "starbytes/linguistics/SuggestionEngine.h"

#include <chrono>
#include <iostream>
#include <sstream>
#include <string>

namespace {

using Clock = std::chrono::steady_clock;
using Ms = std::chrono::milliseconds;

int fail(const std::string &message) {
    std::cerr << "LinguisticsPhase7PerformanceTest failure: " << message << '\n';
    return 1;
}

bool expect(bool condition, const std::string &message) {
    if(!condition) {
        std::cerr << "LinguisticsPhase7PerformanceTest assertion failed: " << message << '\n';
        return false;
    }
    return true;
}

std::string buildLargeSource(size_t functions) {
    std::ostringstream out;
    for(size_t i = 0; i < functions; ++i) {
        out << "decl v" << i << " = " << i << "   \n";
        out << "if (v" << i << " = " << i + 1 << ") {\n";
        out << "  print(v" << i << ")\n";
        out << "}\n";
    }
    out << "func terminal(x:Int) Int {\n";
    out << "  return x\n";
    out << "}\n";
    return out.str();
}

} // namespace

int main() {
    auto config = starbytes::linguistics::LinguisticsConfig::defaults();
    config.lint.maxFindings = 200000;
    config.suggestions.maxSuggestions = 200000;

    starbytes::linguistics::LintEngine lintEngine;
    starbytes::linguistics::SuggestionEngine suggestionEngine;
    starbytes::linguistics::CodeActionEngine actionEngine;
    starbytes::linguistics::FormatterEngine formatterEngine;

    auto largeSource = buildLargeSource(700);
    starbytes::linguistics::LinguisticsSession session("memory://phase7-perf-large.starb", largeSource);

    auto fileStart = Clock::now();
    auto lintResult = lintEngine.run(session, config);
    auto suggestionResult = suggestionEngine.run(session, config);
    starbytes::linguistics::CodeActionRequest safeRequest;
    safeRequest.safeOnly = true;
    auto actionResult = actionEngine.build(lintResult.findings, suggestionResult.suggestions, config, safeRequest);
    auto formatResult = formatterEngine.format(session, config);
    auto fileElapsed = std::chrono::duration_cast<Ms>(Clock::now() - fileStart).count();

    if(!expect(!lintResult.findings.empty(), "large source should produce lint findings")) {
        return fail("large-lint");
    }
    if(!expect(!suggestionResult.suggestions.empty(), "large source should produce suggestions")) {
        return fail("large-suggestions");
    }
    if(!expect(!actionResult.actions.empty(), "large source should produce actions")) {
        return fail("large-actions");
    }
    if(!expect(formatResult.ok, "large source formatting should succeed")) {
        return fail("large-format");
    }
    if(!expect(fileElapsed < 12000, "large file pipeline should complete within performance gate")) {
        return fail("large-file-threshold");
    }

    auto workspaceStart = Clock::now();
    size_t workspaceFindings = 0;
    for(size_t i = 0; i < 120; ++i) {
        std::ostringstream src;
        src << "decl x = " << i << "   \n";
        src << "if (x = " << i + 1 << ") {\n";
        src << "  print(x)\n";
        src << "}\n";
        src << "func w" << i << "(x:Int) Int {\n";
        src << "  return x\n";
        src << "}";
        starbytes::linguistics::LinguisticsSession wsSession("memory://ws-" + std::to_string(i) + ".starb", src.str());
        auto wsLint = lintEngine.run(wsSession, config);
        workspaceFindings += wsLint.findings.size();
    }
    auto workspaceElapsed = std::chrono::duration_cast<Ms>(Clock::now() - workspaceStart).count();

    if(!expect(workspaceFindings > 0, "workspace simulation should produce findings")) {
        return fail("workspace-findings");
    }
    if(!expect(workspaceElapsed < 8000, "workspace simulation should complete within performance gate")) {
        return fail("workspace-threshold");
    }

    return 0;
}
