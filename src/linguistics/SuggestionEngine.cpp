#include "starbytes/linguistics/SuggestionEngine.h"

#include "starbytes/linguistics/LintEngine.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <string>
#include <vector>

namespace starbytes::linguistics {

namespace {

struct SourceLine {
    unsigned line = 1;
    std::string text;
};

std::vector<SourceLine> splitLines(const std::string &source) {
    std::vector<SourceLine> lines;
    if(source.empty()) {
        lines.push_back({1, ""});
        return lines;
    }

    size_t start = 0;
    unsigned lineNo = 1;
    while(start < source.size()) {
        size_t end = source.find('\n', start);
        if(end == std::string::npos) {
            lines.push_back({lineNo, source.substr(start)});
            break;
        }
        lines.push_back({lineNo, source.substr(start, end - start)});
        start = end + 1;
        ++lineNo;
    }
    if(lines.empty()) {
        lines.push_back({1, ""});
    }
    return lines;
}

bool startsWith(const std::string &value, const std::string &prefix) {
    return value.rfind(prefix, 0) == 0;
}

std::string trimLeftCopy(const std::string &value) {
    size_t i = 0;
    while(i < value.size() && std::isspace(static_cast<unsigned char>(value[i])) != 0) {
        ++i;
    }
    return value.substr(i);
}

TextSpan makeSpan(unsigned oneBasedLine, unsigned start, unsigned end) {
    TextSpan span;
    span.start.line = oneBasedLine > 0 ? oneBasedLine - 1 : 0;
    span.end.line = oneBasedLine > 0 ? oneBasedLine - 1 : 0;
    span.start.character = start;
    span.end.character = end;
    return span;
}

TextSpan eofSpan(const std::string &source) {
    TextSpan span;
    unsigned line = 0;
    unsigned character = 0;
    for(char ch : source) {
        if(ch == '\n') {
            ++line;
            character = 0;
        }
        else {
            ++character;
        }
    }
    span.start.line = line;
    span.end.line = line;
    span.start.character = character;
    span.end.character = character;
    return span;
}

SuggestionKind kindForFinding(const LintFinding &finding) {
    if(startsWith(finding.id, "style.")) {
        return SuggestionKind::Style;
    }
    if(startsWith(finding.id, "correctness.")) {
        return SuggestionKind::Correctness;
    }
    if(startsWith(finding.id, "performance.")) {
        return SuggestionKind::Performance;
    }
    if(startsWith(finding.id, "safety.")) {
        return SuggestionKind::Safety;
    }
    if(startsWith(finding.id, "docs.")) {
        return SuggestionKind::Docs;
    }
    return SuggestionKind::Refactor;
}

double confidenceForFinding(const LintFinding &finding) {
    if(finding.id == "style.trailing_whitespace") {
        return 0.98;
    }
    if(finding.id == "correctness.assignment_in_condition") {
        return 0.96;
    }
    if(finding.id == "performance.new_in_loop") {
        return 0.66;
    }
    if(finding.id == "safety.untyped_catch") {
        return 0.80;
    }
    if(finding.id == "docs.missing_decl_comment") {
        return 0.76;
    }
    return 0.60;
}

std::string titleForFinding(const LintFinding &finding) {
    if(finding.id == "style.trailing_whitespace") {
        return "Trim trailing whitespace";
    }
    if(finding.id == "correctness.assignment_in_condition") {
        return "Replace assignment with equality check";
    }
    if(finding.id == "performance.new_in_loop") {
        return "Reduce allocations in loops";
    }
    if(finding.id == "safety.untyped_catch") {
        return "Use typed catch bindings";
    }
    if(finding.id == "docs.missing_decl_comment") {
        return "Add declaration documentation";
    }
    return "Improve code quality";
}

std::string messageForFinding(const LintFinding &finding) {
    if(finding.id == "style.trailing_whitespace") {
        return "Removing trailing whitespace improves readability and keeps diffs clean.";
    }
    if(finding.id == "correctness.assignment_in_condition") {
        return "Use `==` for comparisons in conditions unless assignment is explicitly intended.";
    }
    if(finding.id == "performance.new_in_loop") {
        return "Move `new` out of tight loops when possible to reduce allocation overhead.";
    }
    if(finding.id == "safety.untyped_catch") {
        return "Typed catch bindings make error handling explicit and easier to reason about.";
    }
    if(finding.id == "docs.missing_decl_comment") {
        return "Document top-level declarations so API intent is visible in tooling.";
    }
    return finding.message;
}

std::string inferDeclName(const std::string &trimmedLine) {
    auto consumeIdentifier = [&](size_t from) -> std::string {
        if(from >= trimmedLine.size()) {
            return {};
        }
        size_t i = from;
        if(!(std::isalpha(static_cast<unsigned char>(trimmedLine[i])) != 0 || trimmedLine[i] == '_')) {
            return {};
        }
        ++i;
        while(i < trimmedLine.size()) {
            char ch = trimmedLine[i];
            if(std::isalnum(static_cast<unsigned char>(ch)) == 0 && ch != '_') {
                break;
            }
            ++i;
        }
        return trimmedLine.substr(from, i - from);
    };

    if(startsWith(trimmedLine, "lazy func ")) {
        return consumeIdentifier(10);
    }
    if(startsWith(trimmedLine, "func ")) {
        return consumeIdentifier(5);
    }
    if(startsWith(trimmedLine, "class ")) {
        return consumeIdentifier(6);
    }
    if(startsWith(trimmedLine, "struct ")) {
        return consumeIdentifier(7);
    }
    if(startsWith(trimmedLine, "interface ")) {
        return consumeIdentifier(10);
    }
    if(startsWith(trimmedLine, "enum ")) {
        return consumeIdentifier(5);
    }
    if(startsWith(trimmedLine, "scope ")) {
        return consumeIdentifier(6);
    }
    if(startsWith(trimmedLine, "def ")) {
        return consumeIdentifier(4);
    }
    return {};
}

void appendFindingSuggestion(const LintFinding &finding,
                             const std::vector<SourceLine> &lines,
                             std::vector<Suggestion> &suggestionsOut) {
    Suggestion suggestion;
    suggestion.id = "suggest." + finding.id + ".L" + std::to_string(finding.span.start.line + 1)
        + ".C" + std::to_string(finding.span.start.character);
    suggestion.kind = kindForFinding(finding);
    suggestion.title = titleForFinding(finding);
    suggestion.message = messageForFinding(finding);
    suggestion.span = finding.span;
    suggestion.confidence = confidenceForFinding(finding);

    if(!finding.fixes.empty() && !finding.fixes.front().edits.empty()) {
        suggestion.edits = finding.fixes.front().edits;
    }
    else if(finding.id == "correctness.assignment_in_condition") {
        suggestion.edits.push_back(TextEdit{finding.span, "=="});
    }
    else if(finding.id == "safety.untyped_catch") {
        suggestion.edits.push_back(TextEdit{finding.span, "catch (err:String)"});
    }
    else if(finding.id == "docs.missing_decl_comment") {
        size_t index = static_cast<size_t>(finding.span.start.line);
        if(index < lines.size()) {
            auto trimmed = trimLeftCopy(lines[index].text);
            auto name = inferDeclName(trimmed);
            if(name.empty()) {
                name = "declaration";
            }
            TextSpan insertion = makeSpan(static_cast<unsigned>(index + 1), 0, 0);
            suggestion.edits.push_back(TextEdit{insertion, "/// TODO: Document " + name + ".\n"});
        }
    }

    suggestionsOut.push_back(std::move(suggestion));
}

void appendTrailingNewlineSuggestion(const std::string &source, std::vector<Suggestion> &suggestionsOut) {
    if(source.empty() || source.back() == '\n') {
        return;
    }

    Suggestion suggestion;
    suggestion.id = "suggest.style.trailing_newline";
    suggestion.kind = SuggestionKind::Style;
    suggestion.title = "Ensure trailing newline";
    suggestion.message = "Adding a trailing newline improves tool interoperability and patch stability.";
    suggestion.span = eofSpan(source);
    suggestion.confidence = 0.94;
    suggestion.edits.push_back(TextEdit{suggestion.span, "\n"});
    suggestionsOut.push_back(std::move(suggestion));
}

void sortSuggestions(std::vector<Suggestion> &suggestions) {
    std::sort(suggestions.begin(), suggestions.end(), [](const Suggestion &lhs, const Suggestion &rhs) {
        if(lhs.confidence != rhs.confidence) {
            return lhs.confidence > rhs.confidence;
        }
        if(lhs.span.start.line != rhs.span.start.line) {
            return lhs.span.start.line < rhs.span.start.line;
        }
        if(lhs.span.start.character != rhs.span.start.character) {
            return lhs.span.start.character < rhs.span.start.character;
        }
        return lhs.id < rhs.id;
    });
}

} // namespace

SuggestionResult SuggestionEngine::run(const LinguisticsSession &session,
                                       const LinguisticsConfig &config,
                                       const SuggestionRequest &request) const {
    SuggestionResult result;
    if(!config.suggestions.enabled || config.suggestions.maxSuggestions == 0) {
        return result;
    }

    auto lines = splitLines(session.getSourceText());

    LintEngine lintEngine;
    LintRequest lintRequest;
    lintRequest.includeSuggestions = false;
    auto lintResult = lintEngine.run(session, config, lintRequest);
    for(const auto &finding : lintResult.findings) {
        appendFindingSuggestion(finding, lines, result.suggestions);
    }
    appendTrailingNewlineSuggestion(session.getSourceText(), result.suggestions);

    sortSuggestions(result.suggestions);

    if(!request.includeLowConfidence) {
        auto newEnd = std::remove_if(result.suggestions.begin(), result.suggestions.end(), [](const Suggestion &item) {
            return item.confidence < 0.65;
        });
        result.suggestions.erase(newEnd, result.suggestions.end());
    }

    if(result.suggestions.size() > config.suggestions.maxSuggestions) {
        result.suggestions.resize(config.suggestions.maxSuggestions);
    }

    return result;
}

} // namespace starbytes::linguistics
