#ifndef STARBYTES_LINGUISTICS_CONFIG_H
#define STARBYTES_LINGUISTICS_CONFIG_H

#include <cstddef>
#include <string>
#include <vector>

namespace starbytes::linguistics {

struct FormattingConfig {
    unsigned maxLineWidth = 100;
    bool trimTrailingWhitespace = true;
    bool ensureTrailingNewline = true;
};

struct LintConfig {
    bool enabled = true;
    bool strict = false;
    size_t maxFindings = 200;
    std::vector<std::string> enabledRules;
    std::vector<std::string> disabledRules;
};

struct SuggestionConfig {
    bool enabled = true;
    size_t maxSuggestions = 200;
};

struct ActionConfig {
    bool preferSafeActions = true;
};

struct LinguisticsConfig {
    FormattingConfig formatting;
    LintConfig lint;
    SuggestionConfig suggestions;
    ActionConfig actions;

    static LinguisticsConfig defaults();
};

} // namespace starbytes::linguistics

#endif // STARBYTES_LINGUISTICS_CONFIG_H
