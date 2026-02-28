#ifndef STARBYTES_LINGUISTICS_CODEACTIONENGINE_H
#define STARBYTES_LINGUISTICS_CODEACTIONENGINE_H

#include <vector>

#include "Config.h"
#include "Types.h"

namespace starbytes::linguistics {

struct CodeActionRequest {
    bool safeOnly = true;
};

struct CodeActionResult {
    std::vector<CodeAction> actions;
};

class CodeActionEngine {
public:
    CodeActionResult build(const std::vector<LintFinding> &findings,
                           const std::vector<Suggestion> &suggestions,
                           const LinguisticsConfig &config,
                           const CodeActionRequest &request = CodeActionRequest()) const;
};

} // namespace starbytes::linguistics

#endif // STARBYTES_LINGUISTICS_CODEACTIONENGINE_H
