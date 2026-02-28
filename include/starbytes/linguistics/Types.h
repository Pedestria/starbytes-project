#ifndef STARBYTES_LINGUISTICS_TYPES_H
#define STARBYTES_LINGUISTICS_TYPES_H

#include <cstdint>
#include <string>
#include <vector>

namespace starbytes::linguistics {

struct TextPosition {
    unsigned line = 0;
    unsigned character = 0;
};

struct TextSpan {
    TextPosition start;
    TextPosition end;

    bool isValid() const {
        return end.line > start.line || (end.line == start.line && end.character >= start.character);
    }
};

struct TextEdit {
    TextSpan span;
    std::string replacement;
};

enum class FindingSeverity : uint8_t {
    Error,
    Warning,
    Information,
    Hint
};

enum class SuggestionKind : uint8_t {
    Style,
    Correctness,
    Performance,
    Safety,
    Docs,
    Refactor
};

struct FixCandidate {
    std::string id;
    std::string title;
    std::vector<TextEdit> edits;
    bool isSafe = true;
    bool preferred = false;
};

struct LintFinding {
    std::string id;
    std::string code;
    FindingSeverity severity = FindingSeverity::Warning;
    std::string message;
    TextSpan span;
    std::vector<std::string> notes;
    std::vector<TextSpan> related;
    std::vector<FixCandidate> fixes;
    std::vector<std::string> tags;
};

struct Suggestion {
    std::string id;
    SuggestionKind kind = SuggestionKind::Style;
    std::string title;
    std::string message;
    TextSpan span;
    double confidence = 0.0;
    std::vector<TextEdit> edits;
};

struct CodeAction {
    std::string id;
    std::string title;
    std::string kind;
    std::vector<std::string> diagnosticRefs;
    std::vector<TextEdit> edits;
    bool preferred = false;
    bool isSafe = true;
};

} // namespace starbytes::linguistics

#endif // STARBYTES_LINGUISTICS_TYPES_H
