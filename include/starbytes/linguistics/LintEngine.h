#ifndef STARBYTES_LINGUISTICS_LINTENGINE_H
#define STARBYTES_LINGUISTICS_LINTENGINE_H

#include <vector>

#include "Analysis.h"
#include "Config.h"
#include "Session.h"
#include "Types.h"

namespace starbytes::linguistics {

enum class LintCategory : uint8_t {
    Style,
    Correctness,
    Performance,
    Safety,
    Docs
};

struct LintRuleDescriptor {
    std::string id;
    std::string code;
    LintCategory category = LintCategory::Style;
    FindingSeverity defaultSeverity = FindingSeverity::Warning;
    bool enabledByDefault = true;
    std::string summary;
    std::vector<std::string> tags;
};

struct LintRequest {
    bool includeSuggestions = true;
};

struct LintResult {
    std::vector<LintFinding> findings;
};

class LintEngine {
public:
    std::vector<LintRuleDescriptor> ruleDescriptors() const;

    LintResult run(const CompilerLintAnalysis &analysis,
                   const LinguisticsConfig &config,
                   const LintRequest &request = LintRequest()) const;

    LintResult run(const LinguisticsSession &session,
                   const LinguisticsConfig &config,
                   const LintRequest &request = LintRequest()) const;
};

} // namespace starbytes::linguistics

#endif // STARBYTES_LINGUISTICS_LINTENGINE_H
