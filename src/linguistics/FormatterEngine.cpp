#include "starbytes/linguistics/FormatterEngine.h"

#include "starbytes/base/Diagnostic.h"
#include "starbytes/compiler/AST.h"
#include "starbytes/compiler/ASTDecl.h"
#include "starbytes/compiler/ASTExpr.h"
#include "starbytes/compiler/ASTNodes.def"
#include "starbytes/compiler/Lexer.h"
#include "starbytes/compiler/Parser.h"
#include "starbytes/compiler/Toks.def"
#include "starbytes/compiler/Type.h"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace starbytes::linguistics {

namespace {

class CollectingASTConsumer final : public ASTStreamConsumer {
public:
    bool acceptsSymbolTableContext() override {
        return true;
    }

    void consumeSTableContext(Semantics::STableContext *table) override {
        (void)table;
    }

    void consumeStmt(ASTStmt *stmt) override {
        statements.push_back(stmt);
    }

    void consumeDecl(ASTDecl *stmt) override {
        statements.push_back(stmt);
    }

    const std::vector<ASTStmt *> &getStatements() const {
        return statements;
    }

private:
    std::vector<ASTStmt *> statements;
};

bool containsCommentMarkers(const std::string &source) {
    return source.find("//") != std::string::npos || source.find("/*") != std::string::npos;
}

bool tokenizeSource(const std::string &source,
                    std::vector<Syntax::Tok> &tokens,
                    std::string &error) {
    std::ostringstream diagStream;
    auto diagnostics = DiagnosticHandler::createDefault(diagStream);
    diagnostics->setOutputMode(DiagnosticHandler::OutputMode::MachineJson);
    Syntax::Lexer lexer(*diagnostics);
    std::istringstream in(source);
    lexer.tokenizeFromIStream(in, tokens);
    if(diagnostics->hasErrored()) {
        error = "Lexer reported errors while probing formatter compatibility.";
        return false;
    }
    return true;
}

bool containsUnsupportedRoundTripTokens(const std::vector<Syntax::Tok> &tokens,
                                        std::string &reason) {
    for(const auto &tok : tokens) {
        if(tok.type == Syntax::Tok::Keyword && tok.content == KW_STRUCT) {
            reason = "Formatter fallback: `struct` round-trip normalization is deferred to phase 3.";
            return true;
        }
        if(tok.type == Syntax::Tok::Keyword && tok.content == KW_ENUM) {
            reason = "Formatter fallback: `enum` round-trip normalization is deferred to phase 3.";
            return true;
        }
    }
    return false;
}

bool regionLess(const Region &lhs, const Region &rhs) {
    if(lhs.startLine != rhs.startLine) {
        return lhs.startLine < rhs.startLine;
    }
    if(lhs.startCol != rhs.startCol) {
        return lhs.startCol < rhs.startCol;
    }
    if(lhs.endLine != rhs.endLine) {
        return lhs.endLine < rhs.endLine;
    }
    return lhs.endCol < rhs.endCol;
}

std::string escapeString(const std::string &value) {
    std::string escaped;
    escaped.reserve(value.size() + 4);
    for(char ch : value) {
        switch(ch) {
            case '\\':
                escaped += "\\\\";
                break;
            case '"':
                escaped += "\\\"";
                break;
            case '\n':
                escaped += "\\n";
                break;
            case '\t':
                escaped += "\\t";
                break;
            case '\r':
                escaped += "\\r";
                break;
            default:
                escaped.push_back(ch);
                break;
        }
    }
    return escaped;
}

std::string toStdString(string_ref value) {
    return std::string(value.getBuffer(), value.size());
}

std::string formatType(ASTType *type) {
    if(!type) {
        return "Any";
    }

    auto name = toStdString(type->getName());
    auto functionTypeName = toStdString(FUNCTION_TYPE->getName());
    auto arrayTypeName = toStdString(ARRAY_TYPE->getName());

    std::string rendered;
    if(name == functionTypeName) {
        rendered += "(";
        if(!type->typeParams.empty()) {
            for(size_t i = 1; i < type->typeParams.size(); ++i) {
                if(i > 1) {
                    rendered += ", ";
                }
                rendered += formatType(type->typeParams[i]);
            }
            rendered += ") ";
            rendered += formatType(type->typeParams.front());
        }
        else {
            rendered += ") Void";
        }
    }
    else if(name == arrayTypeName && type->typeParams.size() == 1) {
        rendered = formatType(type->typeParams.front()) + "[]";
    }
    else {
        rendered = name;
        if(!type->typeParams.empty()) {
            rendered += "<";
            for(size_t i = 0; i < type->typeParams.size(); ++i) {
                if(i > 0) {
                    rendered += ", ";
                }
                rendered += formatType(type->typeParams[i]);
            }
            rendered += ">";
        }
    }

    if(type->isOptional) {
        rendered += "?";
    }
    if(type->isThrowable) {
        rendered += "!";
    }

    return rendered;
}

class Writer {
public:
    void write(const std::string &text) {
        if(text.empty()) {
            return;
        }
        ensureIndent();
        out += text;
    }

    void newline() {
        if(out.empty() || out.back() != '\n') {
            out.push_back('\n');
        }
        atLineStart = true;
    }

    void writeLine(const std::string &line) {
        write(line);
        newline();
    }

    void indentIn() {
        ++indent;
    }

    void indentOut() {
        if(indent > 0) {
            --indent;
        }
    }

    std::string str() const {
        return out;
    }

private:
    void ensureIndent() {
        if(!atLineStart) {
            return;
        }
        out.append(static_cast<size_t>(indent * 2), ' ');
        atLineStart = false;
    }

    std::string out;
    int indent = 0;
    bool atLineStart = true;
};

class AstFormatter {
public:
    explicit AstFormatter(const std::string &sourceText) : sourceLines(splitLines(sourceText)) {}

    bool format(const std::vector<ASTStmt *> &statements,
                std::string &output,
                std::string &reason) {
        for(auto *stmt : statements) {
            if(containsUnsupportedNode(stmt)) {
                reason = "Formatter fallback: inline function expression formatting is deferred to phase 3.";
                return false;
            }
        }

        for(size_t i = 0; i < statements.size(); ++i) {
            emitStatement(statements[i]);
            if(i + 1 < statements.size()) {
                writer.newline();
            }
        }

        output = writer.str();
        if(!output.empty() && output.back() != '\n') {
            output.push_back('\n');
        }
        return true;
    }

private:
    static std::vector<std::string> splitLines(const std::string &text) {
        std::vector<std::string> lines;
        std::istringstream in(text);
        std::string line;
        while(std::getline(in, line)) {
            lines.push_back(line);
        }
        if(!text.empty() && text.back() == '\n') {
            lines.push_back("");
        }
        return lines;
    }

    bool hasSourceAuthoredVarType(const ASTVarDecl::VarSpec &spec) const {
        if(!spec.id) {
            return spec.type != nullptr;
        }
        unsigned lineNumber = spec.id->codeRegion.startLine;
        if(lineNumber == 0 || static_cast<size_t>(lineNumber) > sourceLines.size()) {
            return spec.type != nullptr;
        }

        const auto &line = sourceLines[static_cast<size_t>(lineNumber - 1)];
        size_t cursor = static_cast<size_t>(spec.id->codeRegion.endCol);
        if(cursor >= line.size()) {
            if(spec.id->val.size() <= line.size()) {
                auto found = line.find(spec.id->val);
                if(found != std::string::npos) {
                    cursor = found + spec.id->val.size();
                }
                else {
                    cursor = 0;
                }
            }
            else {
                cursor = 0;
            }
        }

        while(cursor < line.size()) {
            char ch = line[cursor];
            if(ch == ':') {
                return true;
            }
            if(ch == '[') {
                size_t lookahead = cursor + 1;
                while(lookahead < line.size() && line[lookahead] == ' ') {
                    ++lookahead;
                }
                if(lookahead < line.size() && line[lookahead] == ']') {
                    return true;
                }
            }
            if(ch == '=' || ch == ',') {
                return false;
            }
            ++cursor;
        }
        return false;
    }

    static bool containsUnsupportedExpr(const ASTExpr *expr) {
        if(!expr) {
            return false;
        }
        if(expr->type == INLINE_FUNC_EXPR) {
            return true;
        }
        if(containsUnsupportedExpr(expr->callee)) {
            return true;
        }
        if(containsUnsupportedExpr(expr->leftExpr)) {
            return true;
        }
        if(containsUnsupportedExpr(expr->middleExpr)) {
            return true;
        }
        if(containsUnsupportedExpr(expr->rightExpr)) {
            return true;
        }
        for(auto *arg : expr->exprArrayData) {
            if(containsUnsupportedExpr(arg)) {
                return true;
            }
        }
        for(const auto &entry : expr->dictExpr) {
            if(containsUnsupportedExpr(entry.first) || containsUnsupportedExpr(entry.second)) {
                return true;
            }
        }
        return false;
    }

    static bool containsUnsupportedNode(const ASTStmt *stmt) {
        if(!stmt) {
            return false;
        }
        if((stmt->type & EXPR) != 0) {
            return containsUnsupportedExpr(static_cast<const ASTExpr *>(stmt));
        }
        if((stmt->type & DECL) == 0) {
            return false;
        }

        auto *decl = static_cast<const ASTDecl *>(stmt);
        for(const auto &attribute : decl->attributes) {
            for(const auto &arg : attribute.args) {
                if(containsUnsupportedExpr(arg.value)) {
                    return true;
                }
            }
        }

        switch(decl->type) {
            case VAR_DECL: {
                auto *varDecl = static_cast<const ASTVarDecl *>(decl);
                for(const auto &spec : varDecl->specs) {
                    if(containsUnsupportedExpr(spec.expr)) {
                        return true;
                    }
                }
                return false;
            }
            case FUNC_DECL: {
                auto *funcDecl = static_cast<const ASTFuncDecl *>(decl);
                if(funcDecl->blockStmt) {
                    for(auto *bodyStmt : funcDecl->blockStmt->body) {
                        if(containsUnsupportedNode(bodyStmt)) {
                            return true;
                        }
                    }
                }
                return false;
            }
            case CLASS_CTOR_DECL: {
                auto *ctorDecl = static_cast<const ASTConstructorDecl *>(decl);
                if(ctorDecl->blockStmt) {
                    for(auto *bodyStmt : ctorDecl->blockStmt->body) {
                        if(containsUnsupportedNode(bodyStmt)) {
                            return true;
                        }
                    }
                }
                return false;
            }
            case CLASS_DECL: {
                auto *classDecl = static_cast<const ASTClassDecl *>(decl);
                for(auto *field : classDecl->fields) {
                    if(containsUnsupportedNode(field)) {
                        return true;
                    }
                }
                for(auto *method : classDecl->methods) {
                    if(containsUnsupportedNode(method)) {
                        return true;
                    }
                }
                for(auto *ctor : classDecl->constructors) {
                    if(containsUnsupportedNode(ctor)) {
                        return true;
                    }
                }
                return false;
            }
            case INTERFACE_DECL: {
                auto *interfaceDecl = static_cast<const ASTInterfaceDecl *>(decl);
                for(auto *field : interfaceDecl->fields) {
                    if(containsUnsupportedNode(field)) {
                        return true;
                    }
                }
                for(auto *method : interfaceDecl->methods) {
                    if(containsUnsupportedNode(method)) {
                        return true;
                    }
                }
                return false;
            }
            case SCOPE_DECL: {
                auto *scopeDecl = static_cast<const ASTScopeDecl *>(decl);
                if(scopeDecl->blockStmt) {
                    for(auto *bodyStmt : scopeDecl->blockStmt->body) {
                        if(containsUnsupportedNode(bodyStmt)) {
                            return true;
                        }
                    }
                }
                return false;
            }
            case RETURN_DECL: {
                auto *retDecl = static_cast<const ASTReturnDecl *>(decl);
                return containsUnsupportedExpr(retDecl->expr);
            }
            case COND_DECL: {
                auto *condDecl = static_cast<const ASTConditionalDecl *>(decl);
                for(const auto &spec : condDecl->specs) {
                    if(containsUnsupportedExpr(spec.expr)) {
                        return true;
                    }
                    if(spec.blockStmt) {
                        for(auto *bodyStmt : spec.blockStmt->body) {
                            if(containsUnsupportedNode(bodyStmt)) {
                                return true;
                            }
                        }
                    }
                }
                return false;
            }
            case FOR_DECL: {
                auto *forDecl = static_cast<const ASTForDecl *>(decl);
                if(containsUnsupportedExpr(forDecl->expr)) {
                    return true;
                }
                if(forDecl->blockStmt) {
                    for(auto *bodyStmt : forDecl->blockStmt->body) {
                        if(containsUnsupportedNode(bodyStmt)) {
                            return true;
                        }
                    }
                }
                return false;
            }
            case WHILE_DECL: {
                auto *whileDecl = static_cast<const ASTWhileDecl *>(decl);
                if(containsUnsupportedExpr(whileDecl->expr)) {
                    return true;
                }
                if(whileDecl->blockStmt) {
                    for(auto *bodyStmt : whileDecl->blockStmt->body) {
                        if(containsUnsupportedNode(bodyStmt)) {
                            return true;
                        }
                    }
                }
                return false;
            }
            case SECURE_DECL: {
                auto *secureDecl = static_cast<const ASTSecureDecl *>(decl);
                if(secureDecl->guardedDecl && containsUnsupportedNode(secureDecl->guardedDecl)) {
                    return true;
                }
                if(secureDecl->catchBlock) {
                    for(auto *bodyStmt : secureDecl->catchBlock->body) {
                        if(containsUnsupportedNode(bodyStmt)) {
                            return true;
                        }
                    }
                }
                return false;
            }
            default:
                return false;
        }
    }

    static int precedenceForExpr(const ASTExpr *expr) {
        if(!expr) {
            return 0;
        }
        switch(expr->type) {
            case ASSIGN_EXPR:
                return 1;
            case TERNARY_EXPR:
                return 2;
            case BINARY_EXPR:
            case LOGIC_EXPR:
                return precedenceForOperator(expr->oprtr_str.value_or(""));
            case UNARY_EXPR:
                return 13;
            case MEMBER_EXPR:
            case INDEX_EXPR:
            case IVKE_EXPR:
                return 14;
            default:
                return 15;
        }
    }

    static int precedenceForOperator(const std::string &op) {
        if(op == "||") {
            return 3;
        }
        if(op == "&&") {
            return 4;
        }
        if(op == "|") {
            return 5;
        }
        if(op == "^") {
            return 6;
        }
        if(op == "&") {
            return 7;
        }
        if(op == "==" || op == "!=") {
            return 8;
        }
        if(op == "<" || op == "<=" || op == ">" || op == ">=" || op == KW_IS) {
            return 9;
        }
        if(op == "<<" || op == ">>") {
            return 10;
        }
        if(op == "+" || op == "-") {
            return 11;
        }
        if(op == "*" || op == "/" || op == "%") {
            return 12;
        }
        return 11;
    }

    static std::string formatLiteral(const ASTLiteralExpr *literal) {
        if(!literal) {
            return "nil";
        }
        if(literal->strValue.has_value()) {
            return std::string("\"") + escapeString(literal->strValue.value()) + "\"";
        }
        if(literal->regexPattern.has_value()) {
            std::string rendered = "/" + escapeString(literal->regexPattern.value()) + "/";
            if(literal->regexFlags.has_value()) {
                rendered += literal->regexFlags.value();
            }
            return rendered;
        }
        if(literal->boolValue.has_value()) {
            return literal->boolValue.value() ? TOK_TRUE : TOK_FALSE;
        }
        if(literal->intValue.has_value()) {
            return std::to_string(literal->intValue.value());
        }
        if(literal->floatValue.has_value()) {
            std::ostringstream out;
            out << literal->floatValue.value();
            return out.str();
        }
        return "nil";
    }

    std::string formatExpression(const ASTExpr *expr,
                                 int parentPrecedence = 0,
                                 bool rightOperand = false) const {
        if(!expr) {
            return "nil";
        }

        std::string rendered;
        switch(expr->type) {
            case STR_LITERAL:
            case REGEX_LITERAL:
            case BOOL_LITERAL:
            case NUM_LITERAL:
                rendered = formatLiteral(static_cast<const ASTLiteralExpr *>(expr));
                break;
            case ID_EXPR:
                rendered = expr->id ? expr->id->val : "_";
                break;
            case ARRAY_EXPR: {
                rendered = "[";
                for(size_t i = 0; i < expr->exprArrayData.size(); ++i) {
                    if(i > 0) {
                        rendered += ", ";
                    }
                    rendered += formatExpression(expr->exprArrayData[i]);
                }
                rendered += "]";
                break;
            }
            case DICT_EXPR: {
                std::vector<std::pair<const ASTExpr *, const ASTExpr *>> entries;
                entries.reserve(expr->dictExpr.size());
                for(const auto &entry : expr->dictExpr) {
                    entries.emplace_back(entry.first, entry.second);
                }
                std::sort(entries.begin(), entries.end(), [](const auto &lhs, const auto &rhs) {
                    const auto *leftExpr = lhs.first;
                    const auto *rightExpr = rhs.first;
                    if(!leftExpr || !rightExpr) {
                        return leftExpr < rightExpr;
                    }
                    if(regionLess(leftExpr->codeRegion, rightExpr->codeRegion)) {
                        return true;
                    }
                    if(regionLess(rightExpr->codeRegion, leftExpr->codeRegion)) {
                        return false;
                    }
                    return leftExpr < rightExpr;
                });

                rendered = "{";
                for(size_t i = 0; i < entries.size(); ++i) {
                    if(i > 0) {
                        rendered += ", ";
                    }
                    rendered += formatExpression(entries[i].first);
                    rendered += ": ";
                    rendered += formatExpression(entries[i].second);
                }
                rendered += "}";
                break;
            }
            case MEMBER_EXPR:
                rendered = formatExpression(expr->leftExpr, 14) + "." + formatExpression(expr->rightExpr, 14);
                break;
            case INDEX_EXPR:
                rendered = formatExpression(expr->leftExpr, 14) + "[" + formatExpression(expr->rightExpr) + "]";
                break;
            case IVKE_EXPR: {
                if(expr->isConstructorCall) {
                    rendered = "new ";
                    rendered += formatExpression(expr->callee, 14);
                    if(!expr->genericTypeArgs.empty()) {
                        rendered += "<";
                        for(size_t i = 0; i < expr->genericTypeArgs.size(); ++i) {
                            if(i > 0) {
                                rendered += ", ";
                            }
                            rendered += formatType(expr->genericTypeArgs[i]);
                        }
                        rendered += ">";
                    }
                }
                else {
                    rendered = formatExpression(expr->callee, 14);
                }
                rendered += "(";
                for(size_t i = 0; i < expr->exprArrayData.size(); ++i) {
                    if(i > 0) {
                        rendered += ", ";
                    }
                    rendered += formatExpression(expr->exprArrayData[i]);
                }
                rendered += ")";
                break;
            }
            case UNARY_EXPR: {
                auto op = expr->oprtr_str.value_or("");
                if(op == KW_AWAIT) {
                    rendered = op + " " + formatExpression(expr->leftExpr, 13);
                }
                else {
                    rendered = op + formatExpression(expr->leftExpr, 13);
                }
                break;
            }
            case BINARY_EXPR:
            case LOGIC_EXPR: {
                auto op = expr->oprtr_str.value_or("+");
                int precedence = precedenceForOperator(op);
                auto lhs = formatExpression(expr->leftExpr, precedence, false);
                auto rhs = formatExpression(expr->rightExpr, precedence, true);
                rendered = lhs + " " + op + " " + rhs;
                break;
            }
            case ASSIGN_EXPR: {
                auto op = expr->oprtr_str.value_or("=");
                auto lhs = formatExpression(expr->leftExpr, 1, false);
                auto rhs = formatExpression(expr->rightExpr, 1, true);
                rendered = lhs + " " + op + " " + rhs;
                break;
            }
            case TERNARY_EXPR:
                rendered = formatExpression(expr->leftExpr, 2, false)
                    + " ? " + formatExpression(expr->middleExpr, 2, false)
                    + " : " + formatExpression(expr->rightExpr, 2, true);
                break;
            default:
                rendered = "_";
                break;
        }

        int precedence = precedenceForExpr(expr);
        bool needsParens = precedence < parentPrecedence;
        if(!needsParens && rightOperand && precedence == parentPrecedence
           && (expr->type == BINARY_EXPR || expr->type == LOGIC_EXPR || expr->type == TERNARY_EXPR || expr->type == ASSIGN_EXPR)) {
            needsParens = true;
        }
        if(needsParens) {
            return "(" + rendered + ")";
        }
        return rendered;
    }

    std::string formatAttributesInline(const ASTDecl *decl) const {
        if(!decl || decl->attributes.empty()) {
            return "";
        }

        std::string rendered;
        for(size_t i = 0; i < decl->attributes.size(); ++i) {
            const auto &attr = decl->attributes[i];
            if(i > 0) {
                rendered += "\n";
            }
            rendered += "@" + attr.name;
            if(!attr.args.empty()) {
                rendered += "(";
                for(size_t j = 0; j < attr.args.size(); ++j) {
                    if(j > 0) {
                        rendered += ", ";
                    }
                    if(attr.args[j].key.has_value()) {
                        rendered += attr.args[j].key.value();
                        rendered += " = ";
                    }
                    rendered += formatExpression(attr.args[j].value);
                }
                rendered += ")";
            }
        }
        return rendered;
    }

    std::string formatGenericParams(const std::vector<ASTGenericParamDecl *> &params) const {
        if(params.empty()) {
            return "";
        }
        std::string rendered = "<";
        for(size_t i = 0; i < params.size(); ++i) {
            if(i > 0) {
                rendered += ", ";
            }
            auto *param = params[i];
            if(param && param->variance == ASTGenericVariance::In) {
                rendered += "in ";
            }
            else if(param && param->variance == ASTGenericVariance::Out) {
                rendered += "out ";
            }
            rendered += (param && param->id) ? param->id->val : "T";
            if(param && !param->bounds.empty()) {
                rendered += ": ";
                for(size_t boundIndex = 0; boundIndex < param->bounds.size(); ++boundIndex) {
                    if(boundIndex > 0) {
                        rendered += " & ";
                    }
                    rendered += formatType(param->bounds[boundIndex]);
                }
            }
            if(param && param->defaultType) {
                rendered += " = ";
                rendered += formatType(param->defaultType);
            }
        }
        rendered += ">";
        return rendered;
    }

    std::vector<std::pair<ASTIdentifier *, ASTType *>> sortParams(const std::map<ASTIdentifier *, ASTType *> &params) const {
        std::vector<std::pair<ASTIdentifier *, ASTType *>> sorted;
        sorted.reserve(params.size());
        for(const auto &entry : params) {
            sorted.push_back(entry);
        }
        std::sort(sorted.begin(), sorted.end(), [](const auto &lhs, const auto &rhs) {
            auto *lhsId = lhs.first;
            auto *rhsId = rhs.first;
            if(!lhsId || !rhsId) {
                return lhsId < rhsId;
            }
            if(regionLess(lhsId->codeRegion, rhsId->codeRegion)) {
                return true;
            }
            if(regionLess(rhsId->codeRegion, lhsId->codeRegion)) {
                return false;
            }
            return lhsId->val < rhsId->val;
        });
        return sorted;
    }

    std::string formatVarDeclInline(const ASTVarDecl *decl) const {
        std::string line = "decl ";
        if(decl && decl->isConst) {
            line += "imut ";
        }
        if(!decl) {
            line += "_";
            return line;
        }
        for(size_t i = 0; i < decl->specs.size(); ++i) {
            if(i > 0) {
                line += ", ";
            }
            const auto &spec = decl->specs[i];
            line += spec.id ? spec.id->val : "_";
            if(spec.type && hasSourceAuthoredVarType(spec)) {
                line += ": ";
                line += formatType(spec.type);
            }
            if(spec.expr) {
                line += " = ";
                line += formatExpression(spec.expr);
            }
        }
        return line;
    }

    void emitBlock(const ASTBlockStmt *block) {
        writer.write("{");
        if(!block || block->body.empty()) {
            writer.write("}");
            return;
        }

        writer.newline();
        writer.indentIn();
        for(size_t i = 0; i < block->body.size(); ++i) {
            emitStatement(block->body[i]);
            if(i + 1 < block->body.size()) {
                writer.newline();
            }
        }
        writer.indentOut();
        writer.newline();
        writer.write("}");
    }

    void emitStatement(const ASTStmt *stmt) {
        if(!stmt) {
            writer.writeLine("_");
            return;
        }

        if((stmt->type & EXPR) != 0) {
            writer.writeLine(formatExpression(static_cast<const ASTExpr *>(stmt)));
            return;
        }

        if((stmt->type & DECL) == 0) {
            writer.writeLine("_");
            return;
        }

        auto *decl = static_cast<const ASTDecl *>(stmt);
        auto attrs = formatAttributesInline(decl);
        if(!attrs.empty()) {
            std::istringstream in(attrs);
            std::string line;
            while(std::getline(in, line)) {
                writer.writeLine(line);
            }
        }

        switch(decl->type) {
            case IMPORT_DECL: {
                auto *importDecl = static_cast<const ASTImportDecl *>(decl);
                writer.write("import ");
                writer.write(importDecl->moduleName ? importDecl->moduleName->val : "_");
                writer.newline();
                break;
            }
            case SCOPE_DECL: {
                auto *scopeDecl = static_cast<const ASTScopeDecl *>(decl);
                writer.write("scope ");
                writer.write(scopeDecl->scopeId ? scopeDecl->scopeId->val : "_");
                writer.write(" ");
                emitBlock(scopeDecl->blockStmt);
                writer.newline();
                break;
            }
            case VAR_DECL: {
                writer.writeLine(formatVarDeclInline(static_cast<const ASTVarDecl *>(decl)));
                break;
            }
            case FUNC_DECL: {
                auto *funcDecl = static_cast<const ASTFuncDecl *>(decl);
                std::string signature;
                if(funcDecl->isLazy) {
                    signature += "lazy ";
                }
                signature += "func ";
                signature += funcDecl->funcId ? funcDecl->funcId->val : "_";
                signature += formatGenericParams(funcDecl->genericParams);
                signature += "(";
                auto sortedParams = sortParams(funcDecl->params);
                for(size_t i = 0; i < sortedParams.size(); ++i) {
                    if(i > 0) {
                        signature += ", ";
                    }
                    signature += sortedParams[i].first ? sortedParams[i].first->val : "_";
                    signature += ": ";
                    signature += formatType(sortedParams[i].second);
                }
                signature += ")";
                if(funcDecl->returnType) {
                    signature += " ";
                    signature += formatType(funcDecl->returnType);
                }
                if(funcDecl->declarationOnly || !funcDecl->blockStmt) {
                    writer.writeLine(signature);
                }
                else {
                    writer.write(signature);
                    writer.write(" ");
                    emitBlock(funcDecl->blockStmt);
                    writer.newline();
                }
                break;
            }
            case CLASS_CTOR_DECL: {
                auto *ctorDecl = static_cast<const ASTConstructorDecl *>(decl);
                std::string signature = "new";
                signature += formatGenericParams(ctorDecl->genericParams);
                signature += "(";
                auto sortedParams = sortParams(ctorDecl->params);
                for(size_t i = 0; i < sortedParams.size(); ++i) {
                    if(i > 0) {
                        signature += ", ";
                    }
                    signature += sortedParams[i].first ? sortedParams[i].first->val : "_";
                    signature += ": ";
                    signature += formatType(sortedParams[i].second);
                }
                signature += ")";
                writer.write(signature);
                writer.write(" ");
                emitBlock(ctorDecl->blockStmt);
                writer.newline();
                break;
            }
            case CLASS_DECL: {
                auto *classDecl = static_cast<const ASTClassDecl *>(decl);
                std::string header = "class ";
                header += classDecl->id ? classDecl->id->val : "_";
                header += formatGenericParams(classDecl->genericParams);
                if(!classDecl->interfaces.empty()) {
                    header += " : ";
                    for(size_t i = 0; i < classDecl->interfaces.size(); ++i) {
                        if(i > 0) {
                            header += ", ";
                        }
                        header += formatType(classDecl->interfaces[i]);
                    }
                }
                writer.write(header);
                writer.write(" ");
                writer.write("{");
                writer.newline();
                writer.indentIn();
                bool firstMember = true;
                for(auto *field : classDecl->fields) {
                    if(!firstMember) {
                        writer.newline();
                    }
                    emitStatement(field);
                    firstMember = false;
                }
                for(auto *ctor : classDecl->constructors) {
                    if(!firstMember) {
                        writer.newline();
                    }
                    emitStatement(ctor);
                    firstMember = false;
                }
                for(auto *method : classDecl->methods) {
                    if(!firstMember) {
                        writer.newline();
                    }
                    emitStatement(method);
                    firstMember = false;
                }
                writer.indentOut();
                writer.write("}");
                writer.newline();
                break;
            }
            case INTERFACE_DECL: {
                auto *interfaceDecl = static_cast<const ASTInterfaceDecl *>(decl);
                std::string header = "interface ";
                header += interfaceDecl->id ? interfaceDecl->id->val : "_";
                header += formatGenericParams(interfaceDecl->genericParams);
                writer.write(header);
                writer.write(" ");
                writer.write("{");
                writer.newline();
                writer.indentIn();
                bool firstMember = true;
                for(auto *field : interfaceDecl->fields) {
                    if(!firstMember) {
                        writer.newline();
                    }
                    emitStatement(field);
                    firstMember = false;
                }
                for(auto *method : interfaceDecl->methods) {
                    if(!firstMember) {
                        writer.newline();
                    }
                    emitStatement(method);
                    firstMember = false;
                }
                writer.indentOut();
                writer.write("}");
                writer.newline();
                break;
            }
            case TYPE_ALIAS_DECL: {
                auto *typeAliasDecl = static_cast<const ASTTypeAliasDecl *>(decl);
                std::string line = "def ";
                line += typeAliasDecl->id ? typeAliasDecl->id->val : "_";
                line += formatGenericParams(typeAliasDecl->genericParams);
                line += " = ";
                line += formatType(typeAliasDecl->aliasedType);
                writer.writeLine(line);
                break;
            }
            case RETURN_DECL: {
                auto *retDecl = static_cast<const ASTReturnDecl *>(decl);
                if(retDecl->expr) {
                    writer.writeLine(std::string("return ") + formatExpression(retDecl->expr));
                }
                else {
                    writer.writeLine("return");
                }
                break;
            }
            case COND_DECL: {
                auto *condDecl = static_cast<const ASTConditionalDecl *>(decl);
                for(size_t i = 0; i < condDecl->specs.size(); ++i) {
                    const auto &spec = condDecl->specs[i];
                    if(i == 0) {
                        writer.write("if (");
                        writer.write(formatExpression(spec.expr));
                        writer.write(") ");
                    }
                    else if(spec.expr) {
                        writer.write("elif (");
                        writer.write(formatExpression(spec.expr));
                        writer.write(") ");
                    }
                    else {
                        writer.write("else ");
                    }
                    emitBlock(spec.blockStmt);
                    writer.newline();
                }
                break;
            }
            case FOR_DECL: {
                auto *forDecl = static_cast<const ASTForDecl *>(decl);
                writer.write("for (");
                writer.write(formatExpression(forDecl->expr));
                writer.write(") ");
                emitBlock(forDecl->blockStmt);
                writer.newline();
                break;
            }
            case WHILE_DECL: {
                auto *whileDecl = static_cast<const ASTWhileDecl *>(decl);
                writer.write("while (");
                writer.write(formatExpression(whileDecl->expr));
                writer.write(") ");
                emitBlock(whileDecl->blockStmt);
                writer.newline();
                break;
            }
            case SECURE_DECL: {
                auto *secureDecl = static_cast<const ASTSecureDecl *>(decl);
                writer.write("secure(");
                writer.write(formatVarDeclInline(secureDecl->guardedDecl));
                writer.write(") catch");
                if(secureDecl->catchErrorId || secureDecl->catchErrorType) {
                    writer.write(" (");
                    writer.write(secureDecl->catchErrorId ? secureDecl->catchErrorId->val : "err");
                    writer.write(": ");
                    writer.write(formatType(secureDecl->catchErrorType));
                    writer.write(")");
                }
                writer.write(" ");
                emitBlock(secureDecl->catchBlock);
                writer.newline();
                break;
            }
            default:
                writer.writeLine("_");
                break;
        }
    }

    std::vector<std::string> sourceLines;
    Writer writer;
};

bool parseToAst(const std::string &sourceName,
                const std::string &source,
                std::vector<ASTStmt *> &statements,
                std::string &error) {
    CollectingASTConsumer consumer;
    std::ostringstream diagnosticsStream;
    auto diagnostics = DiagnosticHandler::createDefault(diagnosticsStream);
    diagnostics->setOutputMode(DiagnosticHandler::OutputMode::MachineJson);

    Parser parser(consumer, std::move(diagnostics));
    ModuleParseContext context = ModuleParseContext::Create(sourceName);
    std::istringstream in(source);
    parser.parseFromStream(in, context);

    auto *diag = parser.getDiagnosticHandler();
    if(diag && diag->hasErrored()) {
        error = diagnosticsStream.str();
        if(error.empty()) {
            error = "Parser reported errors.";
        }
        return false;
    }

    statements = consumer.getStatements();
    return true;
}

bool formatCompilerBackedSource(const std::string &sourceName,
                                const std::string &source,
                                std::string &formatted,
                                std::string &reason) {
    std::vector<ASTStmt *> statements;
    if(!parseToAst(sourceName, source, statements, reason)) {
        reason = "Formatter fallback: source did not pass parser validation for compiler-backed formatting.";
        return false;
    }

    AstFormatter formatter(source);
    if(!formatter.format(statements, formatted, reason)) {
        return false;
    }
    return true;
}

bool parseValidationPasses(const std::string &sourceName,
                           const std::string &formatted,
                           std::string &reason) {
    std::vector<ASTStmt *> statements;
    std::string parseError;
    if(!parseToAst(sourceName, formatted, statements, parseError)) {
        reason = "Formatter fallback: parse-after-format validation failed.";
        return false;
    }
    return true;
}

FormatResult makePreviewFallback(const std::string &source,
                                 const FormattingConfig &config,
                                 const std::string &note,
                                 bool allowFallback) {
    FormatResult result;
    if(!allowFallback) {
        result.ok = false;
        result.formattedText = source;
        result.notes.push_back(note);
        return result;
    }
    result.ok = true;
    result.formattedText = FormatterEngine::normalizePreview(source, config);
    result.notes.push_back(note);
    return result;
}

} // namespace

std::string FormatterEngine::normalizePreview(const std::string &source,
                                              const FormattingConfig &config) {
    if(!config.trimTrailingWhitespace && !config.ensureTrailingNewline) {
        return source;
    }

    std::string normalized;
    normalized.reserve(source.size() + 16);

    size_t lineStart = 0;
    while(lineStart <= source.size()) {
        size_t lineEnd = source.find('\n', lineStart);
        bool hasNewline = lineEnd != std::string::npos;
        if(!hasNewline) {
            lineEnd = source.size();
        }

        size_t trimmedEnd = lineEnd;
        if(config.trimTrailingWhitespace) {
            while(trimmedEnd > lineStart && (source[trimmedEnd - 1] == ' ' || source[trimmedEnd - 1] == '\t')) {
                --trimmedEnd;
            }
        }

        normalized.append(source.data() + lineStart, trimmedEnd - lineStart);
        if(hasNewline) {
            normalized.push_back('\n');
            lineStart = lineEnd + 1;
            continue;
        }
        break;
    }

    if(config.ensureTrailingNewline && !normalized.empty() && normalized.back() != '\n') {
        normalized.push_back('\n');
    }

    return normalized;
}

FormatResult FormatterEngine::format(const LinguisticsSession &session,
                                     const LinguisticsConfig &config,
                                     const FormatRequest &request) const {
    const auto &source = session.getSourceText();
    if(request.previewMode) {
        FormatResult result;
        result.ok = true;
        result.formattedText = normalizePreview(source, config.formatting);
        result.notes.push_back("Formatter preview mode active.");
        return result;
    }

    if(containsCommentMarkers(source)) {
        return makePreviewFallback(source,
                                   config.formatting,
                                   "Formatter fallback: comment-aware round-trip formatting is deferred to phase 4.",
                                   request.allowFallbackToPreview);
    }

    std::vector<Syntax::Tok> tokens;
    std::string tokenError;
    if(!tokenizeSource(source, tokens, tokenError)) {
        return makePreviewFallback(source,
                                   config.formatting,
                                   "Formatter fallback: tokenization failed during compatibility probing.",
                                   request.allowFallbackToPreview);
    }

    std::string unsupportedReason;
    if(containsUnsupportedRoundTripTokens(tokens, unsupportedReason)) {
        return makePreviewFallback(source,
                                   config.formatting,
                                   unsupportedReason,
                                   request.allowFallbackToPreview);
    }

    std::string firstPass;
    std::string formatReason;
    if(!formatCompilerBackedSource(session.getSourceName(), source, firstPass, formatReason)) {
        if(formatReason.empty()) {
            formatReason = "Formatter fallback: compiler-backed formatting failed.";
        }
        return makePreviewFallback(source,
                                   config.formatting,
                                   formatReason,
                                   request.allowFallbackToPreview);
    }

    if(config.formatting.ensureTrailingNewline && !firstPass.empty() && firstPass.back() != '\n') {
        firstPass.push_back('\n');
    }

    if(request.requireParseValidation) {
        std::string validationReason;
        if(!parseValidationPasses(session.getSourceName(), firstPass, validationReason)) {
            return makePreviewFallback(source,
                                       config.formatting,
                                       validationReason,
                                       request.allowFallbackToPreview);
        }
    }

    std::string stable = firstPass;
    FormatResult result;
    result.ok = true;

    if(request.requireIdempotence) {
        std::string secondPass;
        std::string secondReason;
        if(!formatCompilerBackedSource(session.getSourceName(), stable, secondPass, secondReason)) {
            return makePreviewFallback(source,
                                       config.formatting,
                                       "Formatter fallback: idempotence verification failed.",
                                       request.allowFallbackToPreview);
        }

        if(secondPass != stable) {
            stable = secondPass;
            result.notes.push_back("Formatter idempotence normalization applied (second pass)."
            );

            if(request.requireParseValidation) {
                std::string validationReason;
                if(!parseValidationPasses(session.getSourceName(), stable, validationReason)) {
                    return makePreviewFallback(source,
                                               config.formatting,
                                               validationReason,
                                               request.allowFallbackToPreview);
                }
            }

            std::string thirdPass;
            std::string thirdReason;
            if(!formatCompilerBackedSource(session.getSourceName(), stable, thirdPass, thirdReason) || thirdPass != stable) {
                return makePreviewFallback(source,
                                           config.formatting,
                                           "Formatter fallback: output failed stability check.",
                                           request.allowFallbackToPreview);
            }
        }
    }

    result.formattedText = stable;
    if(result.notes.empty()) {
        result.notes.push_back("Compiler-backed formatter applied.");
    }
    return result;
}

} // namespace starbytes::linguistics
