#include "starbytes/linguistics/LintEngine.h"

#include "starbytes/compiler/ASTDecl.h"
#include "starbytes/compiler/ASTExpr.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <optional>
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
    if(span.end.character < span.start.character) {
        span.end.character = span.start.character;
    }
    return span;
}

TextSpan spanFromRegion(const Region &region) {
    TextSpan span;
    span.start.line = region.startLine > 0 ? region.startLine - 1 : 0;
    span.end.line = region.endLine > 0 ? region.endLine - 1 : span.start.line;
    span.start.character = region.startCol;
    span.end.character = region.endCol;
    if(span.end.line < span.start.line) {
        span.end.line = span.start.line;
    }
    if(span.end.line == span.start.line && span.end.character <= span.start.character) {
        span.end.character = span.start.character + 1;
    }
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
        },
        {
            "correctness.shadowing",
            "SB-LINT-CORR-C0004",
            LintCategory::Correctness,
            FindingSeverity::Information,
            true,
            "Declaration shadows an outer binding.",
            {"correctness", "scope", "shadowing"}
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

void appendFinding(std::vector<LintFinding> &outFindings,
                   LintFinding finding,
                   const LintConfig &config,
                   bool &hitLimit) {
    outFindings.push_back(std::move(finding));
    if(outFindings.size() >= config.maxFindings) {
        hitLimit = true;
    }
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

        appendFinding(outFindings, std::move(finding), config, hitLimit);
        if(hitLimit) {
            return;
        }
    }
}

ASTExpr *findFirstSimpleAssignmentExpr(ASTExpr *expr) {
    if(!expr) {
        return nullptr;
    }
    if(expr->type == ASSIGN_EXPR && expr->oprtr_str.has_value() && *expr->oprtr_str == "=") {
        return expr;
    }
    if(auto *nested = findFirstSimpleAssignmentExpr(expr->callee)) {
        return nested;
    }
    if(auto *nested = findFirstSimpleAssignmentExpr(expr->leftExpr)) {
        return nested;
    }
    if(auto *nested = findFirstSimpleAssignmentExpr(expr->middleExpr)) {
        return nested;
    }
    if(auto *nested = findFirstSimpleAssignmentExpr(expr->rightExpr)) {
        return nested;
    }
    for(auto *arg : expr->exprArrayData) {
        if(auto *nested = findFirstSimpleAssignmentExpr(arg)) {
            return nested;
        }
    }
    for(const auto &entry : expr->dictExpr) {
        if(auto *nested = findFirstSimpleAssignmentExpr(entry.first)) {
            return nested;
        }
        if(auto *nested = findFirstSimpleAssignmentExpr(entry.second)) {
            return nested;
        }
    }
    return nullptr;
}

const SourceLine *lineForNumber(const std::vector<SourceLine> &lines, unsigned oneBasedLine);

std::optional<TextSpan> findAssignmentSpanFromAst(const std::vector<SourceLine> &lines, ASTExpr *assignExpr) {
    if(!assignExpr) {
        return std::nullopt;
    }

    unsigned startLine = assignExpr->leftExpr ? assignExpr->leftExpr->codeRegion.startLine : assignExpr->codeRegion.startLine;
    unsigned endLine = assignExpr->rightExpr ? assignExpr->rightExpr->codeRegion.startLine : startLine;
    if(startLine == 0) {
        startLine = assignExpr->codeRegion.startLine;
    }
    if(startLine == 0) {
        return std::nullopt;
    }
    if(endLine < startLine) {
        endLine = startLine;
    }

    for(unsigned lineNo = startLine; lineNo <= endLine; ++lineNo) {
        auto *line = lineForNumber(lines, lineNo);
        if(!line) {
            continue;
        }
        size_t startColumn = 0;
        if(lineNo == startLine) {
            if(assignExpr->leftExpr) {
                startColumn = assignExpr->leftExpr->codeRegion.startCol;
            }
            else {
                startColumn = assignExpr->codeRegion.startCol;
            }
        }
        for(size_t i = startColumn; i < line->text.size(); ++i) {
            if(line->text[i] != '=') {
                continue;
            }
            char prev = i > 0 ? line->text[i - 1] : '\0';
            char next = (i + 1) < line->text.size() ? line->text[i + 1] : '\0';
            if(next == '=') {
                continue;
            }
            if(prev == '=' || prev == '!' || prev == '<' || prev == '>' || prev == '+' || prev == '-'
               || prev == '*' || prev == '/' || prev == '%' || prev == '&' || prev == '|' || prev == '^') {
                continue;
            }
            return makeSpan(lineNo, static_cast<unsigned>(i), static_cast<unsigned>(i + 1));
        }
    }
    return std::nullopt;
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

const SourceLine *lineForNumber(const std::vector<SourceLine> &lines, unsigned oneBasedLine);

void scanStmtForAssignmentInCondition(const LintRuleDescriptor &rule,
                                      ASTStmt *stmt,
                                      const std::vector<SourceLine> &lines,
                                      const LintConfig &config,
                                      std::vector<LintFinding> &outFindings,
                                      bool &hitLimit);

void scanBlockForAssignmentInCondition(const LintRuleDescriptor &rule,
                                       ASTBlockStmt *block,
                                       const std::vector<SourceLine> &lines,
                                       const LintConfig &config,
                                       std::vector<LintFinding> &outFindings,
                                       bool &hitLimit) {
    if(!block) {
        return;
    }
    for(auto *stmt : block->body) {
        scanStmtForAssignmentInCondition(rule, stmt, lines, config, outFindings, hitLimit);
        if(hitLimit) {
            return;
        }
    }
}

void emitAssignmentInConditionFinding(const LintRuleDescriptor &rule,
                                      ASTExpr *conditionExpr,
                                      ASTExpr *assignExpr,
                                      const std::vector<SourceLine> &lines,
                                      const LintConfig &config,
                                      std::vector<LintFinding> &outFindings,
                                      bool &hitLimit) {
    TextSpan span = conditionExpr ? spanFromRegion(conditionExpr->codeRegion) : makeSpan(1, 0, 1);
    if(auto assignmentSpan = findAssignmentSpanFromAst(lines, assignExpr)) {
        span = *assignmentSpan;
    }
    auto finding = makeFinding(rule,
                               config,
                               "Suspicious assignment in condition; did you mean `==`?",
                               span);
    appendFinding(outFindings, std::move(finding), config, hitLimit);
}

void scanStmtForAssignmentInCondition(const LintRuleDescriptor &rule,
                                      ASTStmt *stmt,
                                      const std::vector<SourceLine> &lines,
                                      const LintConfig &config,
                                      std::vector<LintFinding> &outFindings,
                                      bool &hitLimit) {
    if(!stmt || hitLimit) {
        return;
    }

    if(!(stmt->type & DECL)) {
        return;
    }

    auto *decl = static_cast<ASTDecl *>(stmt);
    switch(decl->type) {
        case COND_DECL: {
            auto *condDecl = static_cast<ASTConditionalDecl *>(decl);
            for(const auto &spec : condDecl->specs) {
                if(auto *assignmentExpr = findFirstSimpleAssignmentExpr(spec.expr)) {
                    emitAssignmentInConditionFinding(rule,
                                                     spec.expr,
                                                     assignmentExpr,
                                                     lines,
                                                     config,
                                                     outFindings,
                                                     hitLimit);
                    if(hitLimit) {
                        return;
                    }
                }
                scanBlockForAssignmentInCondition(rule, spec.blockStmt, lines, config, outFindings, hitLimit);
                if(hitLimit) {
                    return;
                }
            }
            return;
        }
        case FOR_DECL: {
            auto *forDecl = static_cast<ASTForDecl *>(decl);
            if(auto *assignmentExpr = findFirstSimpleAssignmentExpr(forDecl->expr)) {
                emitAssignmentInConditionFinding(rule,
                                                 forDecl->expr,
                                                 assignmentExpr,
                                                 lines,
                                                 config,
                                                 outFindings,
                                                 hitLimit);
                if(hitLimit) {
                    return;
                }
            }
            scanBlockForAssignmentInCondition(rule, forDecl->blockStmt, lines, config, outFindings, hitLimit);
            return;
        }
        case WHILE_DECL: {
            auto *whileDecl = static_cast<ASTWhileDecl *>(decl);
            if(auto *assignmentExpr = findFirstSimpleAssignmentExpr(whileDecl->expr)) {
                emitAssignmentInConditionFinding(rule,
                                                 whileDecl->expr,
                                                 assignmentExpr,
                                                 lines,
                                                 config,
                                                 outFindings,
                                                 hitLimit);
                if(hitLimit) {
                    return;
                }
            }
            scanBlockForAssignmentInCondition(rule, whileDecl->blockStmt, lines, config, outFindings, hitLimit);
            return;
        }
        case SECURE_DECL: {
            auto *secureDecl = static_cast<ASTSecureDecl *>(decl);
            scanBlockForAssignmentInCondition(rule, secureDecl->catchBlock, lines, config, outFindings, hitLimit);
            return;
        }
        case FUNC_DECL:
        case CLASS_FUNC_DECL:
            scanBlockForAssignmentInCondition(rule,
                                              static_cast<ASTFuncDecl *>(decl)->blockStmt,
                                              lines,
                                              config,
                                              outFindings,
                                              hitLimit);
            return;
        case CLASS_CTOR_DECL:
            scanBlockForAssignmentInCondition(rule,
                                              static_cast<ASTConstructorDecl *>(decl)->blockStmt,
                                              lines,
                                              config,
                                              outFindings,
                                              hitLimit);
            return;
        case CLASS_DECL: {
            auto *classDecl = static_cast<ASTClassDecl *>(decl);
            for(auto *method : classDecl->methods) {
                scanBlockForAssignmentInCondition(rule, method->blockStmt, lines, config, outFindings, hitLimit);
                if(hitLimit) {
                    return;
                }
            }
            for(auto *ctor : classDecl->constructors) {
                scanBlockForAssignmentInCondition(rule, ctor->blockStmt, lines, config, outFindings, hitLimit);
                if(hitLimit) {
                    return;
                }
            }
            return;
        }
        case INTERFACE_DECL: {
            auto *interfaceDecl = static_cast<ASTInterfaceDecl *>(decl);
            for(auto *method : interfaceDecl->methods) {
                scanBlockForAssignmentInCondition(rule, method->blockStmt, lines, config, outFindings, hitLimit);
                if(hitLimit) {
                    return;
                }
            }
            return;
        }
        case SCOPE_DECL:
            scanBlockForAssignmentInCondition(rule,
                                              static_cast<ASTScopeDecl *>(decl)->blockStmt,
                                              lines,
                                              config,
                                              outFindings,
                                              hitLimit);
            return;
        default:
            return;
    }
}

void addAssignmentInConditionRule(const LintRuleDescriptor &rule,
                                  const CompilerLintAnalysis &analysis,
                                  const std::vector<SourceLine> &lines,
                                  const LintConfig &config,
                                  std::vector<LintFinding> &outFindings,
                                  bool &hitLimit) {
    if(analysis.statements) {
        for(auto *stmt : *analysis.statements) {
            scanStmtForAssignmentInCondition(rule, stmt, lines, config, outFindings, hitLimit);
            if(hitLimit) {
                return;
            }
        }
    }
}

const SourceLine *lineForNumber(const std::vector<SourceLine> &lines, unsigned oneBasedLine) {
    if(oneBasedLine == 0 || oneBasedLine > lines.size()) {
        return nullptr;
    }
    return &lines[oneBasedLine - 1];
}

std::optional<TextSpan> findWordOnLineBeforeColumn(const std::vector<SourceLine> &lines,
                                                   unsigned oneBasedLine,
                                                   unsigned beforeColumn,
                                                   const std::string &word) {
    auto *line = lineForNumber(lines, oneBasedLine);
    if(!line) {
        return std::nullopt;
    }

    size_t best = std::string::npos;
    for(size_t pos : findWordOccurrences(line->text, word)) {
        if(pos <= beforeColumn) {
            best = pos;
        }
    }
    if(best == std::string::npos) {
        return std::nullopt;
    }
    return makeSpan(oneBasedLine,
                    static_cast<unsigned>(best),
                    static_cast<unsigned>(best + word.size()));
}

std::optional<TextSpan> findWordInLineRange(const std::vector<SourceLine> &lines,
                                            unsigned startLine,
                                            unsigned endLine,
                                            const std::string &word) {
    if(lines.empty()) {
        return std::nullopt;
    }
    if(startLine == 0) {
        startLine = 1;
    }
    if(endLine < startLine) {
        endLine = startLine;
    }
    endLine = std::min<unsigned>(endLine, static_cast<unsigned>(lines.size()));
    for(unsigned lineNo = startLine; lineNo <= endLine; ++lineNo) {
        auto *line = lineForNumber(lines, lineNo);
        if(!line) {
            continue;
        }
        auto matches = findWordOccurrences(line->text, word);
        if(!matches.empty()) {
            auto pos = static_cast<unsigned>(matches.front());
            return makeSpan(lineNo, pos, pos + static_cast<unsigned>(word.size()));
        }
    }
    return std::nullopt;
}

void scanExprForLoopAllocations(const LintRuleDescriptor &rule,
                                ASTExpr *expr,
                                bool inLoop,
                                const std::vector<SourceLine> &lines,
                                const LintConfig &config,
                                std::vector<LintFinding> &outFindings,
                                bool &hitLimit);

void scanStmtForLoopAllocations(const LintRuleDescriptor &rule,
                                ASTStmt *stmt,
                                bool inLoop,
                                const std::vector<SourceLine> &lines,
                                const LintConfig &config,
                                std::vector<LintFinding> &outFindings,
                                bool &hitLimit);

void scanBlockForLoopAllocations(const LintRuleDescriptor &rule,
                                 ASTBlockStmt *block,
                                 bool inLoop,
                                 const std::vector<SourceLine> &lines,
                                 const LintConfig &config,
                                 std::vector<LintFinding> &outFindings,
                                 bool &hitLimit) {
    if(!block) {
        return;
    }
    for(auto *stmt : block->body) {
        scanStmtForLoopAllocations(rule, stmt, inLoop, lines, config, outFindings, hitLimit);
        if(hitLimit) {
            return;
        }
    }
}

void emitLoopAllocationFinding(const LintRuleDescriptor &rule,
                               ASTExpr *expr,
                               const std::vector<SourceLine> &lines,
                               const LintConfig &config,
                               std::vector<LintFinding> &outFindings,
                               bool &hitLimit) {
    TextSpan span = spanFromRegion(expr->codeRegion);
    if(auto newSpan = findWordOnLineBeforeColumn(lines,
                                                 expr->codeRegion.startLine,
                                                 expr->codeRegion.startCol,
                                                 "new")) {
        span = *newSpan;
    }
    auto finding = makeFinding(rule,
                               config,
                               "Object allocation inside a loop can increase runtime and GC pressure.",
                               span);
    appendFinding(outFindings, std::move(finding), config, hitLimit);
}

void scanExprForLoopAllocations(const LintRuleDescriptor &rule,
                                ASTExpr *expr,
                                bool inLoop,
                                const std::vector<SourceLine> &lines,
                                const LintConfig &config,
                                std::vector<LintFinding> &outFindings,
                                bool &hitLimit) {
    if(!expr || hitLimit) {
        return;
    }

    if(inLoop && expr->type == IVKE_EXPR && expr->isConstructorCall) {
        emitLoopAllocationFinding(rule, expr, lines, config, outFindings, hitLimit);
        if(hitLimit) {
            return;
        }
    }

    scanExprForLoopAllocations(rule, expr->callee, inLoop, lines, config, outFindings, hitLimit);
    scanExprForLoopAllocations(rule, expr->leftExpr, inLoop, lines, config, outFindings, hitLimit);
    scanExprForLoopAllocations(rule, expr->middleExpr, inLoop, lines, config, outFindings, hitLimit);
    scanExprForLoopAllocations(rule, expr->rightExpr, inLoop, lines, config, outFindings, hitLimit);
    for(auto *arg : expr->exprArrayData) {
        scanExprForLoopAllocations(rule, arg, inLoop, lines, config, outFindings, hitLimit);
        if(hitLimit) {
            return;
        }
    }
    for(const auto &entry : expr->dictExpr) {
        scanExprForLoopAllocations(rule, entry.first, inLoop, lines, config, outFindings, hitLimit);
        scanExprForLoopAllocations(rule, entry.second, inLoop, lines, config, outFindings, hitLimit);
        if(hitLimit) {
            return;
        }
    }
}

void scanVarDeclForLoopAllocations(const LintRuleDescriptor &rule,
                                   ASTVarDecl *decl,
                                   bool inLoop,
                                   const std::vector<SourceLine> &lines,
                                   const LintConfig &config,
                                   std::vector<LintFinding> &outFindings,
                                   bool &hitLimit) {
    if(!decl) {
        return;
    }
    for(const auto &spec : decl->specs) {
        scanExprForLoopAllocations(rule, spec.expr, inLoop, lines, config, outFindings, hitLimit);
        if(hitLimit) {
            return;
        }
    }
}

void scanStmtForLoopAllocations(const LintRuleDescriptor &rule,
                                ASTStmt *stmt,
                                bool inLoop,
                                const std::vector<SourceLine> &lines,
                                const LintConfig &config,
                                std::vector<LintFinding> &outFindings,
                                bool &hitLimit) {
    if(!stmt || hitLimit) {
        return;
    }

    if(stmt->type & DECL) {
        auto *decl = static_cast<ASTDecl *>(stmt);
        switch(decl->type) {
            case VAR_DECL:
                scanVarDeclForLoopAllocations(rule,
                                              static_cast<ASTVarDecl *>(decl),
                                              inLoop,
                                              lines,
                                              config,
                                              outFindings,
                                              hitLimit);
                return;
            case COND_DECL: {
                auto *condDecl = static_cast<ASTConditionalDecl *>(decl);
                for(const auto &spec : condDecl->specs) {
                    scanExprForLoopAllocations(rule, spec.expr, inLoop, lines, config, outFindings, hitLimit);
                    scanBlockForLoopAllocations(rule, spec.blockStmt, inLoop, lines, config, outFindings, hitLimit);
                    if(hitLimit) {
                        return;
                    }
                }
                return;
            }
            case FOR_DECL: {
                auto *forDecl = static_cast<ASTForDecl *>(decl);
                scanExprForLoopAllocations(rule, forDecl->expr, true, lines, config, outFindings, hitLimit);
                scanBlockForLoopAllocations(rule, forDecl->blockStmt, true, lines, config, outFindings, hitLimit);
                return;
            }
            case WHILE_DECL: {
                auto *whileDecl = static_cast<ASTWhileDecl *>(decl);
                scanExprForLoopAllocations(rule, whileDecl->expr, true, lines, config, outFindings, hitLimit);
                scanBlockForLoopAllocations(rule, whileDecl->blockStmt, true, lines, config, outFindings, hitLimit);
                return;
            }
            case SECURE_DECL: {
                auto *secureDecl = static_cast<ASTSecureDecl *>(decl);
                scanVarDeclForLoopAllocations(rule,
                                              secureDecl->guardedDecl,
                                              inLoop,
                                              lines,
                                              config,
                                              outFindings,
                                              hitLimit);
                scanBlockForLoopAllocations(rule,
                                            secureDecl->catchBlock,
                                            inLoop,
                                            lines,
                                            config,
                                            outFindings,
                                            hitLimit);
                return;
            }
            case FUNC_DECL:
            case CLASS_FUNC_DECL: {
                auto *funcDecl = static_cast<ASTFuncDecl *>(decl);
                scanBlockForLoopAllocations(rule, funcDecl->blockStmt, false, lines, config, outFindings, hitLimit);
                return;
            }
            case CLASS_CTOR_DECL: {
                auto *ctorDecl = static_cast<ASTConstructorDecl *>(decl);
                scanBlockForLoopAllocations(rule, ctorDecl->blockStmt, false, lines, config, outFindings, hitLimit);
                return;
            }
            case CLASS_DECL: {
                auto *classDecl = static_cast<ASTClassDecl *>(decl);
                for(auto *field : classDecl->fields) {
                    scanVarDeclForLoopAllocations(rule, field, false, lines, config, outFindings, hitLimit);
                    if(hitLimit) {
                        return;
                    }
                }
                for(auto *method : classDecl->methods) {
                    scanBlockForLoopAllocations(rule, method->blockStmt, false, lines, config, outFindings, hitLimit);
                    if(hitLimit) {
                        return;
                    }
                }
                for(auto *ctor : classDecl->constructors) {
                    scanBlockForLoopAllocations(rule, ctor->blockStmt, false, lines, config, outFindings, hitLimit);
                    if(hitLimit) {
                        return;
                    }
                }
                return;
            }
            case INTERFACE_DECL: {
                auto *interfaceDecl = static_cast<ASTInterfaceDecl *>(decl);
                for(auto *field : interfaceDecl->fields) {
                    scanVarDeclForLoopAllocations(rule, field, false, lines, config, outFindings, hitLimit);
                    if(hitLimit) {
                        return;
                    }
                }
                for(auto *method : interfaceDecl->methods) {
                    scanBlockForLoopAllocations(rule, method->blockStmt, false, lines, config, outFindings, hitLimit);
                    if(hitLimit) {
                        return;
                    }
                }
                return;
            }
            case SCOPE_DECL: {
                auto *scopeDecl = static_cast<ASTScopeDecl *>(decl);
                scanBlockForLoopAllocations(rule, scopeDecl->blockStmt, false, lines, config, outFindings, hitLimit);
                return;
            }
            case RETURN_DECL: {
                auto *returnDecl = static_cast<ASTReturnDecl *>(decl);
                scanExprForLoopAllocations(rule, returnDecl->expr, inLoop, lines, config, outFindings, hitLimit);
                return;
            }
            default:
                return;
        }
    }

    scanExprForLoopAllocations(rule,
                               static_cast<ASTExpr *>(stmt),
                               inLoop,
                               lines,
                               config,
                               outFindings,
                               hitLimit);
}

void addNewInLoopRule(const LintRuleDescriptor &rule,
                      const CompilerLintAnalysis &analysis,
                      const std::vector<SourceLine> &lines,
                      const LintConfig &config,
                      std::vector<LintFinding> &outFindings,
                      bool &hitLimit) {
    if(!analysis.statements) {
        return;
    }
    for(auto *stmt : *analysis.statements) {
        scanStmtForLoopAllocations(rule, stmt, false, lines, config, outFindings, hitLimit);
        if(hitLimit) {
            return;
        }
    }
}

void scanStmtForUntypedCatch(const LintRuleDescriptor &rule,
                             ASTStmt *stmt,
                             const std::vector<SourceLine> &lines,
                             const LintConfig &config,
                             std::vector<LintFinding> &outFindings,
                             bool &hitLimit);

void scanBlockForUntypedCatch(const LintRuleDescriptor &rule,
                              ASTBlockStmt *block,
                              const std::vector<SourceLine> &lines,
                              const LintConfig &config,
                              std::vector<LintFinding> &outFindings,
                              bool &hitLimit) {
    if(!block) {
        return;
    }
    for(auto *stmt : block->body) {
        scanStmtForUntypedCatch(rule, stmt, lines, config, outFindings, hitLimit);
        if(hitLimit) {
            return;
        }
    }
}

void emitUntypedCatchFinding(const LintRuleDescriptor &rule,
                             ASTSecureDecl *secureDecl,
                             const std::vector<SourceLine> &lines,
                             const LintConfig &config,
                             std::vector<LintFinding> &outFindings,
                             bool &hitLimit) {
    unsigned startLine = 1;
    if(secureDecl->guardedDecl && !secureDecl->guardedDecl->specs.empty()) {
        startLine = secureDecl->guardedDecl->specs.front().id
            ? secureDecl->guardedDecl->specs.front().id->codeRegion.startLine
            : secureDecl->guardedDecl->codeRegion.startLine;
    }
    if(startLine == 0) {
        startLine = 1;
    }
    unsigned endLine = startLine + 2;
    if(secureDecl->catchBlock && !secureDecl->catchBlock->body.empty() && secureDecl->catchBlock->body.front()) {
        endLine = std::max(endLine, secureDecl->catchBlock->body.front()->codeRegion.startLine + 1);
    }

    TextSpan span = secureDecl->guardedDecl ? spanFromRegion(secureDecl->guardedDecl->codeRegion)
                                            : makeSpan(startLine, 0, 5);
    if(auto catchSpan = findWordInLineRange(lines, startLine, endLine, "catch")) {
        span = *catchSpan;
    }
    auto finding = makeFinding(rule,
                               config,
                               "Catch clause should declare a typed error binding, e.g. `catch (err:String)`.",
                               span);
    appendFinding(outFindings, std::move(finding), config, hitLimit);
}

void scanStmtForUntypedCatch(const LintRuleDescriptor &rule,
                             ASTStmt *stmt,
                             const std::vector<SourceLine> &lines,
                             const LintConfig &config,
                             std::vector<LintFinding> &outFindings,
                             bool &hitLimit) {
    if(!stmt || hitLimit) {
        return;
    }

    if(stmt->type & DECL) {
        auto *decl = static_cast<ASTDecl *>(stmt);
        switch(decl->type) {
            case SECURE_DECL: {
                auto *secureDecl = static_cast<ASTSecureDecl *>(decl);
                if(secureDecl->catchErrorType == nullptr || secureDecl->catchErrorId == nullptr) {
                    emitUntypedCatchFinding(rule, secureDecl, lines, config, outFindings, hitLimit);
                    if(hitLimit) {
                        return;
                    }
                }
                scanBlockForUntypedCatch(rule, secureDecl->catchBlock, lines, config, outFindings, hitLimit);
                return;
            }
            case COND_DECL: {
                auto *condDecl = static_cast<ASTConditionalDecl *>(decl);
                for(const auto &spec : condDecl->specs) {
                    scanBlockForUntypedCatch(rule, spec.blockStmt, lines, config, outFindings, hitLimit);
                    if(hitLimit) {
                        return;
                    }
                }
                return;
            }
            case FOR_DECL:
                scanBlockForUntypedCatch(rule,
                                         static_cast<ASTForDecl *>(decl)->blockStmt,
                                         lines,
                                         config,
                                         outFindings,
                                         hitLimit);
                return;
            case WHILE_DECL:
                scanBlockForUntypedCatch(rule,
                                         static_cast<ASTWhileDecl *>(decl)->blockStmt,
                                         lines,
                                         config,
                                         outFindings,
                                         hitLimit);
                return;
            case FUNC_DECL:
            case CLASS_FUNC_DECL:
                scanBlockForUntypedCatch(rule,
                                         static_cast<ASTFuncDecl *>(decl)->blockStmt,
                                         lines,
                                         config,
                                         outFindings,
                                         hitLimit);
                return;
            case CLASS_CTOR_DECL:
                scanBlockForUntypedCatch(rule,
                                         static_cast<ASTConstructorDecl *>(decl)->blockStmt,
                                         lines,
                                         config,
                                         outFindings,
                                         hitLimit);
                return;
            case CLASS_DECL: {
                auto *classDecl = static_cast<ASTClassDecl *>(decl);
                for(auto *method : classDecl->methods) {
                    scanBlockForUntypedCatch(rule, method->blockStmt, lines, config, outFindings, hitLimit);
                    if(hitLimit) {
                        return;
                    }
                }
                for(auto *ctor : classDecl->constructors) {
                    scanBlockForUntypedCatch(rule, ctor->blockStmt, lines, config, outFindings, hitLimit);
                    if(hitLimit) {
                        return;
                    }
                }
                return;
            }
            case INTERFACE_DECL: {
                auto *interfaceDecl = static_cast<ASTInterfaceDecl *>(decl);
                for(auto *method : interfaceDecl->methods) {
                    scanBlockForUntypedCatch(rule, method->blockStmt, lines, config, outFindings, hitLimit);
                    if(hitLimit) {
                        return;
                    }
                }
                return;
            }
            case SCOPE_DECL:
                scanBlockForUntypedCatch(rule,
                                         static_cast<ASTScopeDecl *>(decl)->blockStmt,
                                         lines,
                                         config,
                                         outFindings,
                                         hitLimit);
                return;
            default:
                return;
        }
    }
}

void addUntypedCatchRule(const LintRuleDescriptor &rule,
                         const CompilerLintAnalysis &analysis,
                         const std::vector<SourceLine> &lines,
                         const LintConfig &config,
                         std::vector<LintFinding> &outFindings,
                         bool &hitLimit) {
    if(!analysis.statements) {
        return;
    }
    for(auto *stmt : *analysis.statements) {
        scanStmtForUntypedCatch(rule, stmt, lines, config, outFindings, hitLimit);
        if(hitLimit) {
            return;
        }
    }
}

bool endsWith(const std::string &value, const std::string &suffix) {
    if(suffix.size() > value.size()) {
        return false;
    }
    return std::equal(suffix.rbegin(), suffix.rend(), value.rbegin());
}

bool hasDocCommentAbove(const std::vector<SourceLine> &lines, unsigned oneBasedLine) {
    if(oneBasedLine < 2 || oneBasedLine > lines.size() + 1) {
        return false;
    }

    size_t index = static_cast<size_t>(oneBasedLine - 1);
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

bool isDocRequiredDeclaration(const ASTDecl *decl) {
    if(!decl || !decl->scope || decl->scope->parentScope != nullptr) {
        return false;
    }
    return decl->type == FUNC_DECL || decl->type == CLASS_DECL || decl->type == INTERFACE_DECL
        || decl->type == SCOPE_DECL || decl->type == TYPE_ALIAS_DECL;
}

unsigned docKeywordLength(const ASTDecl *decl) {
    switch(decl->type) {
        case FUNC_DECL:
            return 4;
        case CLASS_DECL:
            return 5;
        case INTERFACE_DECL:
            return 9;
        case SCOPE_DECL:
            return 5;
        case TYPE_ALIAS_DECL:
            return 3;
        default:
            return 1;
    }
}

void addMissingDocRule(const LintRuleDescriptor &rule,
                       const CompilerLintAnalysis &analysis,
                       const std::vector<SourceLine> &lines,
                       const LintConfig &config,
                       std::vector<LintFinding> &outFindings,
                       bool &hitLimit) {
    if(!analysis.statements) {
        return;
    }
    for(auto *stmt : *analysis.statements) {
        if(!stmt || !(stmt->type & DECL)) {
            continue;
        }
        auto *decl = static_cast<ASTDecl *>(stmt);
        if(!isDocRequiredDeclaration(decl) || decl->codeRegion.startLine == 0) {
            continue;
        }
        if(hasDocCommentAbove(lines, decl->codeRegion.startLine)) {
            continue;
        }
        auto startCol = decl->codeRegion.startCol;
        auto finding = makeFinding(rule,
                                   config,
                                   "Top-level declaration should include a leading documentation comment.",
                                   makeSpan(decl->codeRegion.startLine,
                                            startCol,
                                            startCol + docKeywordLength(decl)));
        appendFinding(outFindings, std::move(finding), config, hitLimit);
        if(hitLimit) {
            return;
        }
    }
}

enum class FlowBindingKind : uint8_t {
    Local,
    Parameter,
    Catch
};

struct FlowBinding {
    std::string name;
    ASTIdentifier *declId = nullptr;
    FlowBindingKind kind = FlowBindingKind::Local;
    bool initialized = false;
};

struct FlowState {
    std::vector<std::vector<FlowBinding>> frames;
};

struct FlowResult {
    FlowState state;
    bool continues = true;
};

const char *flowBindingKindName(FlowBindingKind kind) {
    switch(kind) {
        case FlowBindingKind::Parameter:
            return "parameter";
        case FlowBindingKind::Catch:
            return "catch binding";
        case FlowBindingKind::Local:
        default:
            return "local binding";
    }
}

TextSpan spanForIdentifier(ASTIdentifier *id) {
    if(!id) {
        return makeSpan(1, 0, 1);
    }
    unsigned line = id->codeRegion.startLine == 0 ? 1 : id->codeRegion.startLine;
    unsigned start = id->codeRegion.startCol;
    unsigned end = id->codeRegion.endCol > start ? id->codeRegion.endCol : start + static_cast<unsigned>(id->val.size());
    return makeSpan(line, start, end);
}

FlowBinding *findBindingInFrame(std::vector<FlowBinding> &frame, const std::string &name) {
    for(auto it = frame.rbegin(); it != frame.rend(); ++it) {
        if(it->name == name) {
            return &*it;
        }
    }
    return nullptr;
}

const FlowBinding *findBindingInFrame(const std::vector<FlowBinding> &frame, const std::string &name) {
    for(auto it = frame.rbegin(); it != frame.rend(); ++it) {
        if(it->name == name) {
            return &*it;
        }
    }
    return nullptr;
}

FlowBinding *findNearestBinding(FlowState &state, const std::string &name) {
    for(auto it = state.frames.rbegin(); it != state.frames.rend(); ++it) {
        if(auto *binding = findBindingInFrame(*it, name)) {
            return binding;
        }
    }
    return nullptr;
}

const FlowBinding *findNearestBinding(const FlowState &state, const std::string &name) {
    for(auto it = state.frames.rbegin(); it != state.frames.rend(); ++it) {
        if(auto *binding = findBindingInFrame(*it, name)) {
            return binding;
        }
    }
    return nullptr;
}

const FlowBinding *findOuterBinding(const FlowState &state, const std::string &name) {
    if(state.frames.empty()) {
        return nullptr;
    }
    bool skippedCurrent = false;
    for(auto it = state.frames.rbegin(); it != state.frames.rend(); ++it) {
        if(!skippedCurrent) {
            skippedCurrent = true;
            continue;
        }
        if(auto *binding = findBindingInFrame(*it, name)) {
            return binding;
        }
    }
    return nullptr;
}

void pushFlowFrame(FlowState &state, const std::vector<FlowBinding> &bindings = {}) {
    state.frames.push_back(bindings);
}

void popFlowFrame(FlowState &state) {
    if(!state.frames.empty()) {
        state.frames.pop_back();
    }
}

void emitUseBeforeInitializationFinding(const LintRuleDescriptor &rule,
                                        ASTIdentifier *id,
                                        const FlowBinding &binding,
                                        const LintConfig &config,
                                        std::vector<LintFinding> &outFindings,
                                        bool &hitLimit) {
    auto finding = makeFinding(rule,
                               config,
                               "Binding `" + binding.name + "` may be read before it is definitely initialized.",
                               spanForIdentifier(id));
    finding.notes.push_back(std::string("The referenced symbol is a ") + flowBindingKindName(binding.kind) + ".");
    if(binding.declId) {
        finding.related.push_back(spanForIdentifier(binding.declId));
    }
    appendFinding(outFindings, std::move(finding), config, hitLimit);
}

void emitShadowingFinding(const LintRuleDescriptor &rule,
                          ASTIdentifier *id,
                          const FlowBinding &shadowed,
                          const LintConfig &config,
                          std::vector<LintFinding> &outFindings,
                          bool &hitLimit) {
    auto finding = makeFinding(rule,
                               config,
                               "Declaration `" + id->val + "` shadows an outer "
                                   + flowBindingKindName(shadowed.kind) + ".",
                               spanForIdentifier(id));
    if(shadowed.declId) {
        finding.related.push_back(spanForIdentifier(shadowed.declId));
    }
    appendFinding(outFindings, std::move(finding), config, hitLimit);
}

void declareFlowBinding(FlowState &state,
                        ASTIdentifier *id,
                        FlowBindingKind kind,
                        bool initialized,
                        const LintRuleDescriptor *shadowRule,
                        const LintConfig &config,
                        std::vector<LintFinding> &outFindings,
                        bool &hitLimit) {
    if(!id) {
        return;
    }
    if(state.frames.empty()) {
        pushFlowFrame(state);
    }
    if(shadowRule != nullptr) {
        if(const auto *shadowed = findOuterBinding(state, id->val)) {
            emitShadowingFinding(*shadowRule, id, *shadowed, config, outFindings, hitLimit);
            if(hitLimit) {
                return;
            }
        }
    }
    state.frames.back().push_back(FlowBinding{id->val, id, kind, initialized});
}

FlowState mergeContinuingFlowStates(const std::vector<FlowState> &states) {
    if(states.empty()) {
        return FlowState{};
    }
    FlowState merged = states.front();
    for(size_t frameIndex = 0; frameIndex < merged.frames.size(); ++frameIndex) {
        for(auto &binding : merged.frames[frameIndex]) {
            bool initialized = true;
            for(const auto &state : states) {
                if(frameIndex >= state.frames.size()) {
                    initialized = false;
                    break;
                }
                const auto *other = findBindingInFrame(state.frames[frameIndex], binding.name);
                if(other == nullptr || !other->initialized) {
                    initialized = false;
                    break;
                }
            }
            binding.initialized = initialized;
        }
    }
    return merged;
}

std::vector<FlowBinding> makeParamBindings(const std::map<ASTIdentifier *, ASTType *> &params) {
    std::vector<FlowBinding> bindings;
    bindings.reserve(params.size());
    for(const auto &entry : params) {
        if(entry.first) {
            bindings.push_back(FlowBinding{entry.first->val, entry.first, FlowBindingKind::Parameter, true});
        }
    }
    return bindings;
}

void analyzeStmtForMustReturn(const LintRuleDescriptor &rule,
                              ASTStmt *stmt,
                              const LintConfig &config,
                              std::vector<LintFinding> &outFindings,
                              bool &hitLimit);

void scanExprForFlowIssues(const LintRuleDescriptor *useBeforeRule,
                           const LintRuleDescriptor *shadowRule,
                           ASTExpr *expr,
                           FlowState &state,
                           const LintConfig &config,
                           std::vector<LintFinding> &outFindings,
                           bool &hitLimit);

FlowResult analyzeStatementSequenceForFlow(const LintRuleDescriptor *useBeforeRule,
                                           const LintRuleDescriptor *shadowRule,
                                           const std::vector<ASTStmt *> &statements,
                                           FlowState state,
                                           const LintConfig &config,
                                           std::vector<LintFinding> &outFindings,
                                           bool &hitLimit);

FlowResult analyzeBlockWithBindingsForFlow(const LintRuleDescriptor *useBeforeRule,
                                           const LintRuleDescriptor *shadowRule,
                                           ASTBlockStmt *block,
                                           FlowState state,
                                           const std::vector<FlowBinding> &bindings,
                                           const LintConfig &config,
                                           std::vector<LintFinding> &outFindings,
                                           bool &hitLimit) {
    if(!block) {
        return FlowResult{std::move(state), true};
    }
    pushFlowFrame(state, bindings);
    auto result = analyzeStatementSequenceForFlow(useBeforeRule,
                                                  shadowRule,
                                                  block->body,
                                                  std::move(state),
                                                  config,
                                                  outFindings,
                                                  hitLimit);
    popFlowFrame(result.state);
    return result;
}

FlowResult analyzeBlockForFlow(const LintRuleDescriptor *useBeforeRule,
                               const LintRuleDescriptor *shadowRule,
                               ASTBlockStmt *block,
                               FlowState state,
                               const LintConfig &config,
                               std::vector<LintFinding> &outFindings,
                               bool &hitLimit) {
    return analyzeBlockWithBindingsForFlow(useBeforeRule,
                                           shadowRule,
                                           block,
                                           std::move(state),
                                           {},
                                           config,
                                           outFindings,
                                           hitLimit);
}

void analyzeInlineFunctionForFlow(const LintRuleDescriptor *useBeforeRule,
                                  const LintRuleDescriptor *shadowRule,
                                  ASTExpr *expr,
                                  const LintConfig &config,
                                  std::vector<LintFinding> &outFindings,
                                  bool &hitLimit) {
    if(!expr || expr->type != INLINE_FUNC_EXPR || !expr->inlineFuncBlock) {
        return;
    }
    FlowState state;
    pushFlowFrame(state, makeParamBindings(expr->inlineFuncParams));
    analyzeBlockForFlow(useBeforeRule,
                        shadowRule,
                        expr->inlineFuncBlock,
                        std::move(state),
                        config,
                        outFindings,
                        hitLimit);
}

void scanExprForFlowIssues(const LintRuleDescriptor *useBeforeRule,
                           const LintRuleDescriptor *shadowRule,
                           ASTExpr *expr,
                           FlowState &state,
                           const LintConfig &config,
                           std::vector<LintFinding> &outFindings,
                           bool &hitLimit) {
    if(!expr || hitLimit) {
        return;
    }

    if(expr->type == INLINE_FUNC_EXPR) {
        analyzeInlineFunctionForFlow(useBeforeRule, shadowRule, expr, config, outFindings, hitLimit);
        return;
    }

    if(expr->type == ID_EXPR) {
        if(useBeforeRule != nullptr && expr->id) {
            if(const auto *binding = findNearestBinding(state, expr->id->val)) {
                if(!binding->initialized) {
                    emitUseBeforeInitializationFinding(*useBeforeRule,
                                                       expr->id,
                                                       *binding,
                                                       config,
                                                       outFindings,
                                                       hitLimit);
                }
            }
        }
        return;
    }

    if(expr->type == MEMBER_EXPR) {
        scanExprForFlowIssues(useBeforeRule, shadowRule, expr->leftExpr, state, config, outFindings, hitLimit);
        return;
    }

    if(expr->type == ASSIGN_EXPR) {
        bool simpleIdentifierAssignment = expr->oprtr_str.has_value() && *expr->oprtr_str == "="
            && expr->leftExpr != nullptr && expr->leftExpr->type == ID_EXPR && expr->leftExpr->id != nullptr;
        if(simpleIdentifierAssignment) {
            scanExprForFlowIssues(useBeforeRule, shadowRule, expr->rightExpr, state, config, outFindings, hitLimit);
            if(hitLimit) {
                return;
            }
            if(auto *binding = findNearestBinding(state, expr->leftExpr->id->val)) {
                binding->initialized = true;
            }
            return;
        }
    }

    scanExprForFlowIssues(useBeforeRule, shadowRule, expr->callee, state, config, outFindings, hitLimit);
    scanExprForFlowIssues(useBeforeRule, shadowRule, expr->leftExpr, state, config, outFindings, hitLimit);
    scanExprForFlowIssues(useBeforeRule, shadowRule, expr->middleExpr, state, config, outFindings, hitLimit);
    scanExprForFlowIssues(useBeforeRule, shadowRule, expr->rightExpr, state, config, outFindings, hitLimit);
    for(auto *item : expr->exprArrayData) {
        scanExprForFlowIssues(useBeforeRule, shadowRule, item, state, config, outFindings, hitLimit);
        if(hitLimit) {
            return;
        }
    }
    for(const auto &entry : expr->dictExpr) {
        scanExprForFlowIssues(useBeforeRule, shadowRule, entry.first, state, config, outFindings, hitLimit);
        if(hitLimit) {
            return;
        }
        scanExprForFlowIssues(useBeforeRule, shadowRule, entry.second, state, config, outFindings, hitLimit);
        if(hitLimit) {
            return;
        }
    }
}

void analyzeFunctionDeclForFlow(const LintRuleDescriptor *useBeforeRule,
                                const LintRuleDescriptor *shadowRule,
                                ASTFuncDecl *funcDecl,
                                const LintConfig &config,
                                std::vector<LintFinding> &outFindings,
                                bool &hitLimit) {
    if(!funcDecl || funcDecl->declarationOnly || !funcDecl->blockStmt) {
        return;
    }
    FlowState state;
    pushFlowFrame(state, makeParamBindings(funcDecl->params));
    analyzeBlockForFlow(useBeforeRule,
                        shadowRule,
                        funcDecl->blockStmt,
                        std::move(state),
                        config,
                        outFindings,
                        hitLimit);
}

void analyzeConstructorDeclForFlow(const LintRuleDescriptor *useBeforeRule,
                                   const LintRuleDescriptor *shadowRule,
                                   ASTConstructorDecl *ctorDecl,
                                   const LintConfig &config,
                                   std::vector<LintFinding> &outFindings,
                                   bool &hitLimit) {
    if(!ctorDecl || !ctorDecl->blockStmt) {
        return;
    }
    FlowState state;
    pushFlowFrame(state, makeParamBindings(ctorDecl->params));
    analyzeBlockForFlow(useBeforeRule,
                        shadowRule,
                        ctorDecl->blockStmt,
                        std::move(state),
                        config,
                        outFindings,
                        hitLimit);
}

FlowResult analyzeStatementForFlow(const LintRuleDescriptor *useBeforeRule,
                                   const LintRuleDescriptor *shadowRule,
                                   ASTStmt *stmt,
                                   FlowState state,
                                   const LintConfig &config,
                                   std::vector<LintFinding> &outFindings,
                                   bool &hitLimit) {
    if(!stmt || hitLimit) {
        return FlowResult{std::move(state), true};
    }

    if(!(stmt->type & DECL)) {
        scanExprForFlowIssues(useBeforeRule, shadowRule, static_cast<ASTExpr *>(stmt), state, config, outFindings, hitLimit);
        return FlowResult{std::move(state), true};
    }

    auto *decl = static_cast<ASTDecl *>(stmt);
    switch(decl->type) {
        case VAR_DECL: {
            auto *varDecl = static_cast<ASTVarDecl *>(decl);
            for(const auto &spec : varDecl->specs) {
                if(spec.expr) {
                    scanExprForFlowIssues(useBeforeRule, shadowRule, spec.expr, state, config, outFindings, hitLimit);
                    if(hitLimit) {
                        return FlowResult{std::move(state), true};
                    }
                }
                declareFlowBinding(state,
                                   spec.id,
                                   FlowBindingKind::Local,
                                   spec.expr != nullptr,
                                   shadowRule,
                                   config,
                                   outFindings,
                                   hitLimit);
                if(hitLimit) {
                    return FlowResult{std::move(state), true};
                }
            }
            return FlowResult{std::move(state), true};
        }
        case RETURN_DECL: {
            auto *returnDecl = static_cast<ASTReturnDecl *>(decl);
            scanExprForFlowIssues(useBeforeRule, shadowRule, returnDecl->expr, state, config, outFindings, hitLimit);
            return FlowResult{std::move(state), false};
        }
        case COND_DECL: {
            auto *condDecl = static_cast<ASTConditionalDecl *>(decl);
            std::vector<FlowState> continuingStates;
            bool hasElse = false;
            for(const auto &spec : condDecl->specs) {
                FlowState branchStart = state;
                if(spec.expr) {
                    scanExprForFlowIssues(useBeforeRule, shadowRule, spec.expr, branchStart, config, outFindings, hitLimit);
                    if(hitLimit) {
                        return FlowResult{std::move(state), true};
                    }
                }
                auto branchResult = analyzeBlockForFlow(useBeforeRule,
                                                        shadowRule,
                                                        spec.blockStmt,
                                                        std::move(branchStart),
                                                        config,
                                                        outFindings,
                                                        hitLimit);
                if(hitLimit) {
                    return FlowResult{std::move(state), true};
                }
                if(spec.expr == nullptr) {
                    hasElse = true;
                }
                if(branchResult.continues) {
                    continuingStates.push_back(std::move(branchResult.state));
                }
            }
            if(!hasElse) {
                continuingStates.push_back(std::move(state));
            }
            if(continuingStates.empty()) {
                return FlowResult{FlowState{}, false};
            }
            return FlowResult{mergeContinuingFlowStates(continuingStates), true};
        }
        case FOR_DECL: {
            auto *forDecl = static_cast<ASTForDecl *>(decl);
            scanExprForFlowIssues(useBeforeRule, shadowRule, forDecl->expr, state, config, outFindings, hitLimit);
            if(hitLimit) {
                return FlowResult{std::move(state), true};
            }
            auto loopState = state;
            (void)analyzeBlockForFlow(useBeforeRule,
                                      shadowRule,
                                      forDecl->blockStmt,
                                      std::move(loopState),
                                      config,
                                      outFindings,
                                      hitLimit);
            return FlowResult{std::move(state), true};
        }
        case WHILE_DECL: {
            auto *whileDecl = static_cast<ASTWhileDecl *>(decl);
            scanExprForFlowIssues(useBeforeRule, shadowRule, whileDecl->expr, state, config, outFindings, hitLimit);
            if(hitLimit) {
                return FlowResult{std::move(state), true};
            }
            auto loopState = state;
            (void)analyzeBlockForFlow(useBeforeRule,
                                      shadowRule,
                                      whileDecl->blockStmt,
                                      std::move(loopState),
                                      config,
                                      outFindings,
                                      hitLimit);
            return FlowResult{std::move(state), true};
        }
        case SECURE_DECL: {
            auto *secureDecl = static_cast<ASTSecureDecl *>(decl);
            if(!secureDecl->guardedDecl || secureDecl->guardedDecl->specs.empty()) {
                return FlowResult{std::move(state), true};
            }

            const auto &guardedSpec = secureDecl->guardedDecl->specs.front();
            if(guardedSpec.expr) {
                scanExprForFlowIssues(useBeforeRule, shadowRule, guardedSpec.expr, state, config, outFindings, hitLimit);
                if(hitLimit) {
                    return FlowResult{std::move(state), true};
                }
            }

            declareFlowBinding(state,
                               guardedSpec.id,
                               FlowBindingKind::Local,
                               true,
                               shadowRule,
                               config,
                               outFindings,
                               hitLimit);
            if(hitLimit) {
                return FlowResult{std::move(state), true};
            }

            FlowState catchState = state;
            if(secureDecl->catchErrorId) {
                pushFlowFrame(catchState);
                declareFlowBinding(catchState,
                                   secureDecl->catchErrorId,
                                   FlowBindingKind::Catch,
                                   true,
                                   shadowRule,
                                   config,
                                   outFindings,
                                   hitLimit);
                if(hitLimit) {
                    return FlowResult{std::move(state), true};
                }
            }
            auto catchResult = analyzeBlockForFlow(useBeforeRule,
                                                   shadowRule,
                                                   secureDecl->catchBlock,
                                                   std::move(catchState),
                                                   config,
                                                   outFindings,
                                                   hitLimit);
            if(hitLimit) {
                return FlowResult{std::move(state), true};
            }
            if(secureDecl->catchErrorId) {
                popFlowFrame(catchResult.state);
            }
            return FlowResult{std::move(state), true};
        }
        case FUNC_DECL:
        case CLASS_FUNC_DECL:
            analyzeFunctionDeclForFlow(useBeforeRule,
                                       shadowRule,
                                       static_cast<ASTFuncDecl *>(decl),
                                       config,
                                       outFindings,
                                       hitLimit);
            return FlowResult{std::move(state), true};
        case CLASS_CTOR_DECL:
            analyzeConstructorDeclForFlow(useBeforeRule,
                                          shadowRule,
                                          static_cast<ASTConstructorDecl *>(decl),
                                          config,
                                          outFindings,
                                          hitLimit);
            return FlowResult{std::move(state), true};
        case CLASS_DECL: {
            auto *classDecl = static_cast<ASTClassDecl *>(decl);
            for(auto *field : classDecl->fields) {
                analyzeStatementForFlow(useBeforeRule,
                                        shadowRule,
                                        field,
                                        FlowState{},
                                        config,
                                        outFindings,
                                        hitLimit);
                if(hitLimit) {
                    return FlowResult{std::move(state), true};
                }
            }
            for(auto *method : classDecl->methods) {
                analyzeFunctionDeclForFlow(useBeforeRule, shadowRule, method, config, outFindings, hitLimit);
                if(hitLimit) {
                    return FlowResult{std::move(state), true};
                }
            }
            for(auto *ctor : classDecl->constructors) {
                analyzeConstructorDeclForFlow(useBeforeRule, shadowRule, ctor, config, outFindings, hitLimit);
                if(hitLimit) {
                    return FlowResult{std::move(state), true};
                }
            }
            return FlowResult{std::move(state), true};
        }
        case INTERFACE_DECL: {
            auto *interfaceDecl = static_cast<ASTInterfaceDecl *>(decl);
            for(auto *field : interfaceDecl->fields) {
                analyzeStatementForFlow(useBeforeRule,
                                        shadowRule,
                                        field,
                                        FlowState{},
                                        config,
                                        outFindings,
                                        hitLimit);
                if(hitLimit) {
                    return FlowResult{std::move(state), true};
                }
            }
            for(auto *method : interfaceDecl->methods) {
                analyzeFunctionDeclForFlow(useBeforeRule, shadowRule, method, config, outFindings, hitLimit);
                if(hitLimit) {
                    return FlowResult{std::move(state), true};
                }
            }
            return FlowResult{std::move(state), true};
        }
        case SCOPE_DECL: {
            auto *scopeDecl = static_cast<ASTScopeDecl *>(decl);
            (void)analyzeBlockForFlow(useBeforeRule,
                                      shadowRule,
                                      scopeDecl->blockStmt,
                                      FlowState{},
                                      config,
                                      outFindings,
                                      hitLimit);
            return FlowResult{std::move(state), true};
        }
        default:
            return FlowResult{std::move(state), true};
    }
}

FlowResult analyzeStatementSequenceForFlow(const LintRuleDescriptor *useBeforeRule,
                                           const LintRuleDescriptor *shadowRule,
                                           const std::vector<ASTStmt *> &statements,
                                           FlowState state,
                                           const LintConfig &config,
                                           std::vector<LintFinding> &outFindings,
                                           bool &hitLimit) {
    FlowResult result{std::move(state), true};
    for(auto *stmt : statements) {
        if(hitLimit || !result.continues) {
            break;
        }
        result = analyzeStatementForFlow(useBeforeRule,
                                         shadowRule,
                                         stmt,
                                         std::move(result.state),
                                         config,
                                         outFindings,
                                         hitLimit);
    }
    return result;
}

void addShadowingRule(const LintRuleDescriptor &rule,
                      const CompilerLintAnalysis &analysis,
                      const LintConfig &config,
                      std::vector<LintFinding> &outFindings,
                      bool &hitLimit) {
    if(!analysis.statements) {
        return;
    }
    FlowState root;
    pushFlowFrame(root);
    (void)analyzeStatementSequenceForFlow(nullptr,
                                          &rule,
                                          *analysis.statements,
                                          std::move(root),
                                          config,
                                          outFindings,
                                          hitLimit);
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

LintResult LintEngine::run(const CompilerLintAnalysis &analysis,
                           const LinguisticsConfig &config,
                           const LintRequest &request) const {
    (void)request;

    LintResult result;
    if(!config.lint.enabled || config.lint.maxFindings == 0 || analysis.session == nullptr) {
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

    auto lines = splitLines(analysis.session->getSourceText());
    bool hitLimit = false;

    for(const auto &rule : allRuleDescriptors()) {
        if(!isRuleEnabled(rule, enabledSelectors, disabledSelectors)) {
            continue;
        }

        if(rule.id == "style.trailing_whitespace") {
            addTrailingWhitespaceRule(rule, lines, config.lint, result.findings, hitLimit);
        }
        else if(rule.id == "correctness.assignment_in_condition") {
            addAssignmentInConditionRule(rule, analysis, lines, config.lint, result.findings, hitLimit);
        }
        else if(rule.id == "performance.new_in_loop") {
            addNewInLoopRule(rule, analysis, lines, config.lint, result.findings, hitLimit);
        }
        else if(rule.id == "safety.untyped_catch") {
            addUntypedCatchRule(rule, analysis, lines, config.lint, result.findings, hitLimit);
        }
        else if(rule.id == "docs.missing_decl_comment") {
            addMissingDocRule(rule, analysis, lines, config.lint, result.findings, hitLimit);
        }
        else if(rule.id == "correctness.shadowing") {
            addShadowingRule(rule, analysis, config.lint, result.findings, hitLimit);
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

LintResult LintEngine::run(const LinguisticsSession &session,
                           const LinguisticsConfig &config,
                           const LintRequest &request) const {
    auto analysis = buildCompilerLintAnalysis(session);
    return run(analysis.view(), config, request);
}

} // namespace starbytes::linguistics
