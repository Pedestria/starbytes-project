#ifndef STARBYTES_LINGUISTICS_SUGGESTIONENGINE_H
#define STARBYTES_LINGUISTICS_SUGGESTIONENGINE_H

#include <vector>

#include "Config.h"
#include "Session.h"
#include "Types.h"

namespace starbytes::linguistics {

struct SuggestionRequest {
    bool includeLowConfidence = false;
};

struct SuggestionResult {
    std::vector<Suggestion> suggestions;
};

class SuggestionEngine {
public:
    SuggestionResult run(const LinguisticsSession &session,
                         const LinguisticsConfig &config,
                         const SuggestionRequest &request = SuggestionRequest()) const;
};

} // namespace starbytes::linguistics

#endif // STARBYTES_LINGUISTICS_SUGGESTIONENGINE_H
