#include "starbytes/linguistics/Serializer.h"

#include <sstream>

namespace starbytes::linguistics {

namespace {

const char *severityName(FindingSeverity severity) {
    switch(severity) {
        case FindingSeverity::Error:
            return "error";
        case FindingSeverity::Warning:
            return "warning";
        case FindingSeverity::Information:
            return "info";
        case FindingSeverity::Hint:
            return "hint";
    }
    return "warning";
}

}

std::string LinguisticsSerializer::toText(const LintResult &result) {
    std::ostringstream out;
    out << "lint-findings: " << result.findings.size();
    for(const auto &finding : result.findings) {
        out << "\n"
            << "[" << finding.code << "] "
            << severityName(finding.severity)
            << " L" << (finding.span.start.line + 1)
            << ":" << finding.span.start.character
            << " " << finding.message;
    }
    return out.str();
}

std::string LinguisticsSerializer::toText(const SuggestionResult &result) {
    std::ostringstream out;
    out << "suggestions: " << result.suggestions.size();
    return out.str();
}

std::string LinguisticsSerializer::toText(const CodeActionResult &result) {
    std::ostringstream out;
    out << "code-actions: " << result.actions.size();
    return out.str();
}

} // namespace starbytes::linguistics
