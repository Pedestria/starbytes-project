#ifndef STARBYTES_LINGUISTICS_SERIALIZER_H
#define STARBYTES_LINGUISTICS_SERIALIZER_H

#include <string>

#include "CodeActionEngine.h"
#include "LintEngine.h"
#include "SuggestionEngine.h"

namespace starbytes::linguistics {

class LinguisticsSerializer {
public:
    static std::string toText(const LintResult &result);
    static std::string toText(const SuggestionResult &result);
    static std::string toText(const CodeActionResult &result);
};

} // namespace starbytes::linguistics

#endif // STARBYTES_LINGUISTICS_SERIALIZER_H
