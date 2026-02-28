#include "starbytes/linguistics/LintEngine.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <string>
#include <utility>
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

bool isIdentifierChar(char ch) {
    return std::isalnum(static_cast<unsigned char>(ch)) != 0 || ch == '_';
}

bool startsWith(const std::string &value, const std::string &prefix) {
    return value.rfind(prefix, 0) == 0;
}

std::string toLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string trimLeftCopy(const std::string &value) {
    size_t i = 0;
    while(i < value.size() && std::isspace(static_cast<unsigned char>(value[i])) != 0) {
        ++i;
    }
    return value.substr(i);
}

std::string trimRightCopy(const std::string &value) {
    if(value.empty()) {
        return value;
    }
    size_t i = value.size();
    while(i > 0 && std::isspace(static_cast<unsigned char>(value[i - 1])) != 0) {
        --i;
    }
    return value.substr(0, i);
}

std::string trimCopy(const std::string &value) {
    return trimRightCopy(trimLeftCopy(value));
}

TextSpan makeSpan(unsigned line, unsigned start, unsigned end) {
    TextSpan span;
    span.start.line = line == 0 ? 0 : line - 1;
    span.end.line = line == 0 ? 0 : line - 1;
    span.start.character = start;
    span.end.character = end;
    return span;
}

FindingSeverity strictSeverity(FindingSeverity severity, bool strict) {
    if(!strict) {
        return severity;
    }
    return FindingSeverity::Error;
}

std::string categoryName(LintCategory category) {
    switch(category) {
        case LintCategory::Style:
            return "style";
        case LintCategory::Correctness:
            return "correctness";
        case LintCategory::Performance:
            return "performance";
        case LintCategory::Safety:
            return "safety";
        case LintCategory::Docs:
            return "docs";
    }
    return "style";
}

std::vector<LintRuleDescriptor> allRuleDescriptors() {
    return {
        {
            "style.trailing_whitespace",
            "SB-LINT-STYLE-S0001",
            LintCategory::Style,
            FindingSeverity::Warning,
            true,
            "Line has trailing whitespace.",
            {"style", "whitespace"}
        },
        {
            "correctness.assignment_in_condition",
            "SB-LINT-CORR-C0001",
            LintCategory::Correctness,
            FindingSeverity::Warning,
            true,
            "Condition contains an assignment expression.",
            {"correctness", "condition"}
        },
        {
            "performance.new_in_loop",
            "SB-LINT-PERF-P0001",
            LintCategory::Performance,
            FindingSeverity::Information,
            true,
            "Object allocation appears inside a loop body.",
            {"performance", "allocation"}
        },
        {
            "safety.untyped_catch",
            "SB-LINT-SAFE-A0001",
            LintCategory::Safety,
            FindingSeverity::Warning,
            true,
            "Catch clause is missing an explicit typed error binding.",
            {"safety", "exceptions"}
        },
        {
            "docs.missing_decl_comment",
            "SB-LINT-DOC-D0001",
            LintCategory::Docs,
            FindingSeverity::Information,
            true,
            "Top-level declaration is missing a leading documentation comment.",
            {"docs", "comments"}
        }
    };
}

bool selectorMatchesRule(const std::string &selectorLower, const LintRuleDescriptor &rule) {
    auto idLower = toLower(rule.id);
    if(selectorLower == idLower) {
        return true;
    }

    auto codeLower = toLower(rule.code);
    if(selectorLower == codeLower) {
        return true;
    }

    auto categoryLower = toLower(categoryName(rule.category));
    if(selectorLower == categoryLower) {
        return true;
    }

    for(const auto &tag : rule.tags) {
        if(selectorLower == toLower(tag)) {
            return true;
        }
    }
    return false;
}

bool isRuleEnabled(const LintRuleDescriptor &rule,
                   const std::vector<std::string> &enabledSelectors,
                   const std::vector<std::string> &disabledSelectors) {
    bool enabled = rule.enabledByDefault;

    if(!enabledSelectors.empty()) {
        enabled = false;
        for(const auto &selector : enabledSelectors) {
            if(selectorMatchesRule(selector, rule)) {
                enabled = true;
                break;
            }
        }
    }

    if(!enabled) {
        return false;
    }

    for(const auto &selector : disabledSelectors) {
        if(selectorMatchesRule(selector, rule)) {
            return false;
        }
    }
    return true;
}

LintFinding makeFinding(const LintRuleDescriptor &rule,
                        const LintConfig &config,
                        const std::string &message,
                        const TextSpan &span) {
    LintFinding finding;
    finding.id = rule.id;
    finding.code = rule.code;
    finding.severity = strictSeverity(rule.defaultSeverity, config.strict);
    finding.message = message;
    finding.span = span;
    finding.tags = rule.tags;
    finding.tags.push_back(categoryName(rule.category));
    if(config.strict) {
        finding.notes.push_back("Lint strict mode escalated this finding to error severity.");
    }
    return finding;
}

void addTrailingWhitespaceRule(const LintRuleDescriptor &rule,
                               const std::vector<SourceLine> &lines,
                               const LintConfig &config,
                               std::vector<LintFinding> &outFindings,
                               bool &hitLimit) {
    for(const auto &line : lines) {
        size_t end = line.text.size();
        size_t trimEnd = end;
        while(trimEnd > 0 && (line.text[trimEnd - 1] == ' ' || line.text[trimEnd - 1] == '\t')) {
            --trimEnd;
        }
        if(trimEnd == end) {
            continue;
        }

        auto span = makeSpan(line.line, static_cast<unsigned>(trimEnd), static_cast<unsigned>(end));
        auto finding = makeFinding(rule, config, "Trailing whitespace should be removed.", span);

        FixCandidate fix;
        fix.id = rule.id + ".trim";
        fix.title = "Trim trailing whitespace";
        fix.preferred = true;
        fix.isSafe = true;
        fix.edits.push_back(TextEdit{span, ""});
        finding.fixes.push_back(std::move(fix));

        outFindings.push_back(std::move(finding));
        if(outFindings.size() >= config.maxFindings) {
            hitLimit = true;
            return;
        }
    }
}

bool containsSingleAssignment(const std::string &conditionText, size_t &positionOut) {
    for(size_t i = 0; i < conditionText.size(); ++i) {
        if(conditionText[i] != '=') {
            continue;
        }
        char prev = i > 0 ? conditionText[i - 1] : '\0';
        char next = (i + 1) < conditionText.size() ? conditionText[i + 1] : '\0';
        if(next == '=') {
            continue;
        }
        if(prev == '=' || prev == '!' || prev == '<' || prev == '>' || prev == '+' || prev == '-'
           || prev == '*' || prev == '/' || prev == '%' || prev == '&' || prev == '|' || prev == '^') {
            continue;
        }
        positionOut = i;
        return true;
    }
    return false;
}

bool startsWithControlKeyword(const std::string &trimmed, const std::string &keyword) {
    if(!startsWith(trimmed, keyword)) {
        return false;
    }
    if(trimmed.size() == keyword.size()) {
        return true;
    }
    char next = trimmed[keyword.size()];
    return std::isspace(static_cast<unsigned char>(next)) != 0 || next == '(';
}

void addAssignmentInConditionRule(const LintRuleDescriptor &rule,
                                  const std::vector<SourceLine> &lines,
                                  const LintConfig &config,
                                  std::vector<LintFinding> &outFindings,
                                  bool &hitLimit) {
    const std::vector<std::string> keywords = {"if", "elif", "while", "for"};
    for(const auto &line : lines) {
        auto trimmed = trimLeftCopy(line.text);
        size_t leading = line.text.size() - trimmed.size();

        bool matchedKeyword = false;
        size_t keywordOffset = 0;
        for(const auto &keyword : keywords) {
            if(startsWithControlKeyword(trimmed, keyword)) {
                matchedKeyword = true;
                keywordOffset = keyword.size();
                break;
            }
        }
        if(!matchedKeyword) {
            continue;
        }

        auto open = trimmed.find('(', keywordOffset);
        if(open == std::string::npos) {
            continue;
        }

        int depth = 0;
        size_t close = std::string::npos;
        for(size_t i = open; i < trimmed.size(); ++i) {
            if(trimmed[i] == '(') {
                ++depth;
            }
            else if(trimmed[i] == ')') {
                --depth;
                if(depth == 0) {
                    close = i;
                    break;
                }
            }
        }
        if(close == std::string::npos || close <= (open + 1)) {
            continue;
        }

        auto conditionText = trimmed.substr(open + 1, close - open - 1);
        size_t assignmentPos = 0;
        if(!containsSingleAssignment(conditionText, assignmentPos)) {
            continue;
        }

        unsigned start = static_cast<unsigned>(leading + open + 1 + assignmentPos);
        unsigned end = start + 1;
        auto finding = makeFinding(rule,
                                   config,
                                   "Suspicious assignment in condition; did you mean `==`?",
                                   makeSpan(line.line, start, end));
        outFindings.push_back(std::move(finding));
        if(outFindings.size() >= config.maxFindings) {
            hitLimit = true;
            return;
        }
    }
}

size_t countChar(const std::string &line, char ch) {
    return static_cast<size_t>(std::count(line.begin(), line.end(), ch));
}

std::vector<size_t> findWordOccurrences(const std::string &line, const std::string &word) {
    std::vector<size_t> offsets;
    if(word.empty() || line.size() < word.size()) {
        return offsets;
    }

    for(size_t i = 0; i + word.size() <= line.size(); ++i) {
        if(line.compare(i, word.size(), word) != 0) {
            continue;
        }
        bool leftBoundary = i == 0 || !isIdentifierChar(line[i - 1]);
        bool rightBoundary = (i + word.size()) == line.size() || !isIdentifierChar(line[i + word.size()]);
        if(leftBoundary && rightBoundary) {
            offsets.push_back(i);
        }
    }
    return offsets;
}

void addNewInLoopRule(const LintRuleDescriptor &rule,
                      const std::vector<SourceLine> &lines,
                      const LintConfig &config,
                      std::vector<LintFinding> &outFindings,
                      bool &hitLimit) {
    int braceDepth = 0;
    std::vector<int> loopDepthStack;
    bool awaitingLoopBrace = false;

    for(const auto &line : lines) {
        auto trimmed = trimLeftCopy(line.text);
        bool startsLoop = startsWithControlKeyword(trimmed, "for") || startsWithControlKeyword(trimmed, "while");

        bool inLoopContext = startsLoop || !loopDepthStack.empty() || awaitingLoopBrace;
        if(inLoopContext) {
            auto news = findWordOccurrences(line.text, "new");
            for(size_t pos : news) {
                auto finding = makeFinding(rule,
                                           config,
                                           "Object allocation inside a loop can increase runtime and GC pressure.",
                                           makeSpan(line.line,
                                                    static_cast<unsigned>(pos),
                                                    static_cast<unsigned>(pos + 3)));
                outFindings.push_back(std::move(finding));
                if(outFindings.size() >= config.maxFindings) {
                    hitLimit = true;
                    return;
                }
            }
        }

        size_t opens = countChar(line.text, '{');
        size_t closes = countChar(line.text, '}');

        if(startsLoop) {
            if(opens > 0) {
                loopDepthStack.push_back(braceDepth + static_cast<int>(opens));
                awaitingLoopBrace = false;
            }
            else {
                awaitingLoopBrace = true;
            }
        }
        else if(awaitingLoopBrace && opens > 0) {
            loopDepthStack.push_back(braceDepth + static_cast<int>(opens));
            awaitingLoopBrace = false;
        }

        braceDepth += static_cast<int>(opens);
        braceDepth -= static_cast<int>(closes);
        if(braceDepth < 0) {
            braceDepth = 0;
        }

        while(!loopDepthStack.empty() && braceDepth < loopDepthStack.back()) {
            loopDepthStack.pop_back();
        }
    }
}

void addUntypedCatchRule(const LintRuleDescriptor &rule,
                         const std::vector<SourceLine> &lines,
                         const LintConfig &config,
                         std::vector<LintFinding> &outFindings,
                         bool &hitLimit) {
    for(const auto &line : lines) {
        auto catches = findWordOccurrences(line.text, "catch");
        for(size_t pos : catches) {
            size_t cursor = pos + 5;
            while(cursor < line.text.size() && std::isspace(static_cast<unsigned char>(line.text[cursor])) != 0) {
                ++cursor;
            }

            bool typed = false;
            if(cursor < line.text.size() && line.text[cursor] == '(') {
                auto close = line.text.find(')', cursor + 1);
                if(close != std::string::npos) {
                    auto signature = line.text.substr(cursor + 1, close - cursor - 1);
                    typed = signature.find(':') != std::string::npos;
                }
            }

            if(typed) {
                continue;
            }

            auto finding = makeFinding(rule,
                                       config,
                                       "Catch clause should declare a typed error binding, e.g. `catch (err:String)`.",
                                       makeSpan(line.line,
                                                static_cast<unsigned>(pos),
                                                static_cast<unsigned>(pos + 5)));
            outFindings.push_back(std::move(finding));
            if(outFindings.size() >= config.maxFindings) {
                hitLimit = true;
                return;
            }
        }
    }
}

bool endsWith(const std::string &value, const std::string &suffix) {
    if(suffix.size() > value.size()) {
        return false;
    }
    return std::equal(suffix.rbegin(), suffix.rend(), value.rbegin());
}

bool hasDocCommentAbove(const std::vector<SourceLine> &lines, size_t index) {
    if(index == 0) {
        return false;
    }

    auto prev = trimCopy(lines[index - 1].text);
    if(startsWith(prev, "///")) {
        return true;
    }

    if(!endsWith(prev, "*/")) {
        return false;
    }

    for(size_t i = index; i > 0; --i) {
        auto current = trimCopy(lines[i - 1].text);
        if(current.empty()) {
            return false;
        }
        if(current.find("/*") != std::string::npos) {
            return true;
        }
    }
    return false;
}

bool isDocRequiredDeclaration(const std::string &trimmed, size_t leadingIndent) {
    if(leadingIndent != 0) {
        return false;
    }
    return startsWith(trimmed, "func ") || startsWith(trimmed, "lazy func ")
        || startsWith(trimmed, "class ") || startsWith(trimmed, "struct ")
        || startsWith(trimmed, "interface ") || startsWith(trimmed, "enum ")
        || startsWith(trimmed, "scope ") || startsWith(trimmed, "def ");
}

void addMissingDocRule(const LintRuleDescriptor &rule,
                       const std::vector<SourceLine> &lines,
                       const LintConfig &config,
                       std::vector<LintFinding> &outFindings,
                       bool &hitLimit) {
    for(size_t i = 0; i < lines.size(); ++i) {
        const auto &line = lines[i];
        auto trimmed = trimLeftCopy(line.text);
        size_t leading = line.text.size() - trimmed.size();
        if(!isDocRequiredDeclaration(trimmed, leading)) {
            continue;
        }

        if(hasDocCommentAbove(lines, i)) {
            continue;
        }

        size_t keywordLen = trimmed.find(' ');
        if(keywordLen == std::string::npos) {
            keywordLen = trimmed.size();
        }
        auto finding = makeFinding(rule,
                                   config,
                                   "Top-level declaration should include a leading documentation comment.",
                                   makeSpan(line.line,
                                            static_cast<unsigned>(leading),
                                            static_cast<unsigned>(leading + keywordLen)));
        outFindings.push_back(std::move(finding));
        if(outFindings.size() >= config.maxFindings) {
            hitLimit = true;
            return;
        }
    }
}

void sortFindingsDeterministically(std::vector<LintFinding> &findings) {
    std::sort(findings.begin(), findings.end(), [](const LintFinding &lhs, const LintFinding &rhs) {
        if(lhs.span.start.line != rhs.span.start.line) {
            return lhs.span.start.line < rhs.span.start.line;
        }
        if(lhs.span.start.character != rhs.span.start.character) {
            return lhs.span.start.character < rhs.span.start.character;
        }
        if(lhs.code != rhs.code) {
            return lhs.code < rhs.code;
        }
        if(lhs.id != rhs.id) {
            return lhs.id < rhs.id;
        }
        return lhs.message < rhs.message;
    });
}

} // namespace

std::vector<LintRuleDescriptor> LintEngine::ruleDescriptors() const {
    return allRuleDescriptors();
}

LintResult LintEngine::run(const LinguisticsSession &session,
                           const LinguisticsConfig &config,
                           const LintRequest &request) const {
    (void)request;

    LintResult result;
    if(!config.lint.enabled || config.lint.maxFindings == 0) {
        return result;
    }

    std::vector<std::string> enabledSelectors;
    enabledSelectors.reserve(config.lint.enabledRules.size());
    for(const auto &selector : config.lint.enabledRules) {
        enabledSelectors.push_back(toLower(selector));
    }

    std::vector<std::string> disabledSelectors;
    disabledSelectors.reserve(config.lint.disabledRules.size());
    for(const auto &selector : config.lint.disabledRules) {
        disabledSelectors.push_back(toLower(selector));
    }

    auto lines = splitLines(session.getSourceText());
    bool hitLimit = false;

    for(const auto &rule : allRuleDescriptors()) {
        if(!isRuleEnabled(rule, enabledSelectors, disabledSelectors)) {
            continue;
        }

        if(rule.id == "style.trailing_whitespace") {
            addTrailingWhitespaceRule(rule, lines, config.lint, result.findings, hitLimit);
        }
        else if(rule.id == "correctness.assignment_in_condition") {
            addAssignmentInConditionRule(rule, lines, config.lint, result.findings, hitLimit);
        }
        else if(rule.id == "performance.new_in_loop") {
            addNewInLoopRule(rule, lines, config.lint, result.findings, hitLimit);
        }
        else if(rule.id == "safety.untyped_catch") {
            addUntypedCatchRule(rule, lines, config.lint, result.findings, hitLimit);
        }
        else if(rule.id == "docs.missing_decl_comment") {
            addMissingDocRule(rule, lines, config.lint, result.findings, hitLimit);
        }

        if(hitLimit) {
            break;
        }
    }

    sortFindingsDeterministically(result.findings);
    if(result.findings.size() > config.lint.maxFindings) {
        result.findings.resize(config.lint.maxFindings);
    }
    return result;
}

} // namespace starbytes::linguistics
