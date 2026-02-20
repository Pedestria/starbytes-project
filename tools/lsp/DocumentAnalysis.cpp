#include "DocumentAnalysis.h"

#include "starbytes/compiler/AST.h"
#include "starbytes/compiler/ASTDecl.h"
#include "starbytes/compiler/Lexer.h"
#include "starbytes/compiler/Parser.h"
#include "starbytes/compiler/SyntaxA.h"
#include "starbytes/compiler/Type.h"

#include <algorithm>
#include <cctype>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <utility>

namespace starbytes::lsp {

namespace {

const std::regex kFuncSignatureRegex(
    R"(\bfunc\s+([A-Za-z_][A-Za-z0-9_]*)\s*\(([^)]*)\)\s*([A-Za-z_][A-Za-z0-9_<>!?]*)?)");

struct ParsedDocument {
  std::vector<Syntax::Tok> tokenStream;
  std::vector<SymbolEntry> symbols;
};

class NullAstConsumer final : public ASTStreamConsumer {
public:
  bool acceptsSymbolTableContext() override {
    return false;
  }

  void consumeDecl(ASTDecl *stmt) override {
    (void)stmt;
  }

  void consumeStmt(ASTStmt *stmt) override {
    (void)stmt;
  }
};

std::string toLower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return value;
}

std::string toStdString(string_ref ref) {
  return std::string(ref.getBuffer(), ref.size());
}

std::string trimRightCopy(std::string value) {
  while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
    value.pop_back();
  }
  return value;
}

std::string trimLeftCopy(std::string value) {
  size_t i = 0;
  while (i < value.size() && std::isspace(static_cast<unsigned char>(value[i]))) {
    ++i;
  }
  return value.substr(i);
}

std::string trimCopy(std::string value) {
  return trimLeftCopy(trimRightCopy(std::move(value)));
}

bool startsWith(const std::string &value, const std::string &prefix) {
  return value.rfind(prefix, 0) == 0;
}

bool endsWith(const std::string &value, const std::string &suffix) {
  if (suffix.size() > value.size()) {
    return false;
  }
  return std::equal(suffix.rbegin(), suffix.rend(), value.rbegin());
}

std::vector<std::string> splitLines(const std::string &text) {
  std::vector<std::string> lines;
  std::istringstream in(text);
  std::string line;
  while (std::getline(in, line)) {
    lines.push_back(line);
  }
  return lines;
}

std::string typeToString(ASTType *type) {
  if (!type) {
    return "Any";
  }

  auto name = toStdString(type->getName());
  auto functionTypeName = toStdString(FUNCTION_TYPE->getName());
  if (name == functionTypeName) {
    std::string rendered = "(";
    if (!type->typeParams.empty()) {
      for (size_t i = 1; i < type->typeParams.size(); ++i) {
        if (i > 1) {
          rendered += ", ";
        }
        rendered += typeToString(type->typeParams[i]);
      }
      rendered += ")";
      rendered += " ";
      rendered += typeToString(type->typeParams.front());
    } else {
      rendered += ") Void";
    }
    if (type->isOptional) {
      rendered += "?";
    }
    if (type->isThrowable) {
      rendered += "!";
    }
    return rendered;
  }

  std::string rendered = name;
  if (!type->typeParams.empty()) {
    rendered += "<";
    for (size_t i = 0; i < type->typeParams.size(); ++i) {
      if (i > 0) {
        rendered += ", ";
      }
      rendered += typeToString(type->typeParams[i]);
    }
    rendered += ">";
  }
  if (type->isOptional) {
    rendered += "?";
  }
  if (type->isThrowable) {
    rendered += "!";
  }
  return rendered;
}

std::string genericParamsToString(const std::vector<ASTIdentifier *> &params) {
  if (params.empty()) {
    return {};
  }
  std::string value = "<";
  for (size_t i = 0; i < params.size(); ++i) {
    if (i > 0) {
      value += ", ";
    }
    value += params[i] ? params[i]->val : "T";
  }
  value += ">";
  return value;
}

bool lineHasDeclKeyword(const std::vector<std::string> &lines, unsigned line, const std::string &keyword) {
  if (line >= lines.size()) {
    return false;
  }
  auto trimmed = trimLeftCopy(lines[line]);
  if (!startsWith(trimmed, keyword)) {
    return false;
  }
  if (trimmed.size() == keyword.size()) {
    return true;
  }
  return std::isspace(static_cast<unsigned char>(trimmed[keyword.size()])) != 0;
}

std::string extractLeadingDocComment(const std::vector<std::string> &lines, unsigned declarationLine) {
  if (declarationLine == 0 || declarationLine - 1 >= lines.size()) {
    return {};
  }

  int i = static_cast<int>(declarationLine) - 1;
  auto first = trimCopy(lines[i]);
  if (first.empty()) {
    return {};
  }

  std::vector<std::string> collected;
  bool isLineComment = false;
  bool isBlockComment = false;
  if (startsWith(first, "//")) {
    isLineComment = true;
    while (i >= 0) {
      auto candidate = trimCopy(lines[i]);
      if (!startsWith(candidate, "//")) {
        break;
      }
      collected.push_back(lines[i]);
      --i;
    }
  } else if (endsWith(first, "*/")) {
    isBlockComment = true;
    bool foundStart = false;
    while (i >= 0) {
      auto raw = lines[i];
      auto candidate = trimCopy(raw);
      if (candidate.empty() && collected.empty()) {
        return {};
      }
      collected.push_back(raw);
      if (candidate.find("/*") != std::string::npos) {
        foundStart = true;
        break;
      }
      --i;
    }
    if (!foundStart) {
      return {};
    }
  } else {
    return {};
  }

  std::reverse(collected.begin(), collected.end());
  std::vector<std::string> normalized;
  normalized.reserve(collected.size());
  for (size_t idx = 0; idx < collected.size(); ++idx) {
    auto line = trimLeftCopy(collected[idx]);
    if (isLineComment) {
      if (startsWith(line, "///")) {
        line = line.substr(3);
      } else if (startsWith(line, "//")) {
        line = line.substr(2);
      }
      line = trimLeftCopy(line);
      normalized.push_back(trimRightCopy(line));
      continue;
    }

    if (isBlockComment) {
      if (idx == 0) {
        auto pos = line.find("/*");
        if (pos != std::string::npos) {
          line = line.substr(pos + 2);
        }
      }
      if (idx + 1 == collected.size()) {
        auto pos = line.rfind("*/");
        if (pos != std::string::npos) {
          line = line.substr(0, pos);
        }
      }
      line = trimLeftCopy(line);
      if (startsWith(line, "*")) {
        line = line.substr(1);
        if (!line.empty() && line[0] == ' ') {
          line = line.substr(1);
        }
      }
      normalized.push_back(trimRightCopy(line));
    }
  }

  while (!normalized.empty() && trimCopy(normalized.front()).empty()) {
    normalized.erase(normalized.begin());
  }
  while (!normalized.empty() && trimCopy(normalized.back()).empty()) {
    normalized.pop_back();
  }
  if (normalized.empty()) {
    return {};
  }

  std::string out;
  for (size_t idx = 0; idx < normalized.size(); ++idx) {
    if (idx > 0) {
      out += "\n";
    }
    out += normalized[idx];
  }
  return out;
}

std::string functionSignature(ASTFuncDecl *funcDecl) {
  if (!funcDecl || !funcDecl->funcId) {
    return {};
  }
  std::vector<std::pair<ASTIdentifier *, ASTType *>> params(funcDecl->params.begin(), funcDecl->params.end());
  std::sort(params.begin(), params.end(), [](const auto &lhs, const auto &rhs) {
    auto *leftId = lhs.first;
    auto *rightId = rhs.first;
    if (!leftId || !rightId) {
      return leftId != nullptr;
    }
    if (leftId->codeRegion.startLine != rightId->codeRegion.startLine) {
      return leftId->codeRegion.startLine < rightId->codeRegion.startLine;
    }
    return leftId->codeRegion.startCol < rightId->codeRegion.startCol;
  });

  std::string signature;
  if (funcDecl->isLazy) {
    signature += "lazy ";
  }
  signature += "func ";
  signature += funcDecl->funcId->val;
  signature += "(";
  for (size_t idx = 0; idx < params.size(); ++idx) {
    if (idx > 0) {
      signature += ", ";
    }
    signature += params[idx].first ? params[idx].first->val : "arg";
    signature += ":";
    signature += typeToString(params[idx].second);
  }
  signature += ")";
  signature += " ";
  signature += typeToString(funcDecl->returnType ? funcDecl->returnType : VOID_TYPE);
  return signature;
}

void pushSymbol(std::vector<SymbolEntry> &symbols,
                ASTIdentifier *id,
                int kind,
                const std::string &detail,
                const std::string &signature,
                const std::vector<std::string> &lines,
                const std::string &containerName,
                bool isMember) {
  if (!id) {
    return;
  }

  SymbolEntry symbol;
  symbol.name = id->val;
  symbol.kind = kind;
  symbol.detail = detail;
  symbol.signature = signature;
  symbol.containerName = containerName;
  symbol.isMember = isMember;
  unsigned sourceLine = id->codeRegion.startLine;
  symbol.line = sourceLine > 0 ? sourceLine - 1 : 0;
  symbol.start = id->codeRegion.startCol;
  symbol.length = id->codeRegion.endCol > id->codeRegion.startCol ? id->codeRegion.endCol - id->codeRegion.startCol
                                                                   : static_cast<unsigned>(symbol.name.size());
  symbol.documentation = extractLeadingDocComment(lines, symbol.line);
  symbols.push_back(std::move(symbol));
}

void collectSymbolsFromStmt(ASTStmt *stmt,
                            const std::vector<std::string> &lines,
                            std::vector<SymbolEntry> &symbols,
                            const std::string &containerName);

void collectSymbolsFromDecl(ASTDecl *decl,
                            const std::vector<std::string> &lines,
                            std::vector<SymbolEntry> &symbols,
                            const std::string &containerName) {
  if (!decl) {
    return;
  }

  switch (decl->type) {
    case VAR_DECL: {
      auto *varDecl = static_cast<ASTVarDecl *>(decl);
      for (auto &spec : varDecl->specs) {
        if (!spec.id) {
          continue;
        }
        bool isMember = !containerName.empty();
        std::string detail = varDecl->isConst ? "constant" : "variable";
        int kind = varDecl->isConst ? SYMBOL_KIND_CONSTANT : SYMBOL_KIND_VARIABLE;
        if (isMember) {
          detail = "field";
          kind = SYMBOL_KIND_FIELD;
        }
        if (spec.type) {
          detail += ": ";
          detail += typeToString(spec.type);
        }
        std::string signature = varDecl->isConst ? "decl imut " : "decl ";
        signature += spec.id->val;
        if (spec.type) {
          signature += ":";
          signature += typeToString(spec.type);
        }
        pushSymbol(symbols, spec.id, kind, detail, signature, lines, containerName, isMember);
      }
      break;
    }
    case FUNC_DECL: {
      auto *funcDecl = static_cast<ASTFuncDecl *>(decl);
      bool isMember = !containerName.empty();
      std::string detail = isMember ? "method" : (funcDecl->isLazy ? "lazy function" : "function");
      pushSymbol(symbols,
                 funcDecl->funcId,
                 isMember ? SYMBOL_KIND_METHOD : SYMBOL_KIND_FUNCTION,
                 detail,
                 functionSignature(funcDecl),
                 lines,
                 containerName,
                 isMember);
      break;
    }
    case CLASS_DECL: {
      auto *classDecl = static_cast<ASTClassDecl *>(decl);
      if (!classDecl->id) {
        break;
      }
      bool isStructDecl = lineHasDeclKeyword(
          lines, classDecl->id->codeRegion.startLine > 0 ? classDecl->id->codeRegion.startLine - 1 : 0, "struct");
      int kind = isStructDecl ? SYMBOL_KIND_STRUCT : SYMBOL_KIND_CLASS;
      std::string detail = isStructDecl ? "struct" : "class";
      std::string signature = detail + " " + classDecl->id->val + genericParamsToString(classDecl->genericTypeParams);
      if (classDecl->superClass) {
        signature += " : ";
        signature += typeToString(classDecl->superClass);
      }
      if (!classDecl->interfaces.empty()) {
        if (!classDecl->superClass) {
          signature += " : ";
        } else {
          signature += ", ";
        }
        for (size_t idx = 0; idx < classDecl->interfaces.size(); ++idx) {
          if (idx > 0) {
            signature += ", ";
          }
          signature += typeToString(classDecl->interfaces[idx]);
        }
      }
      pushSymbol(symbols, classDecl->id, kind, detail, signature, lines, containerName, !containerName.empty());
      for (auto *field : classDecl->fields) {
        collectSymbolsFromDecl(field, lines, symbols, classDecl->id ? classDecl->id->val : std::string());
      }
      for (auto *method : classDecl->methods) {
        collectSymbolsFromDecl(method, lines, symbols, classDecl->id ? classDecl->id->val : std::string());
      }
      break;
    }
    case INTERFACE_DECL: {
      auto *interfaceDecl = static_cast<ASTInterfaceDecl *>(decl);
      std::string signature = "interface ";
      signature += interfaceDecl->id ? interfaceDecl->id->val : "Interface";
      signature += genericParamsToString(interfaceDecl->genericTypeParams);
      pushSymbol(symbols,
                 interfaceDecl->id,
                 SYMBOL_KIND_INTERFACE,
                 "interface",
                 signature,
                 lines,
                 containerName,
                 !containerName.empty());
      for (auto *field : interfaceDecl->fields) {
        collectSymbolsFromDecl(field, lines, symbols, interfaceDecl->id ? interfaceDecl->id->val : std::string());
      }
      for (auto *method : interfaceDecl->methods) {
        collectSymbolsFromDecl(method, lines, symbols, interfaceDecl->id ? interfaceDecl->id->val : std::string());
      }
      break;
    }
    case SCOPE_DECL: {
      auto *scopeDecl = static_cast<ASTScopeDecl *>(decl);
      if (!scopeDecl->scopeId) {
        break;
      }
      unsigned line = scopeDecl->scopeId->codeRegion.startLine > 0 ? scopeDecl->scopeId->codeRegion.startLine - 1 : 0;
      bool isEnumDecl = lineHasDeclKeyword(lines, line, "enum");
      int kind = isEnumDecl ? SYMBOL_KIND_ENUM : SYMBOL_KIND_NAMESPACE;
      std::string detail = isEnumDecl ? "enum" : "scope";
      std::string signature = detail + " " + scopeDecl->scopeId->val;
      pushSymbol(symbols, scopeDecl->scopeId, kind, detail, signature, lines, containerName, !containerName.empty());
      if (scopeDecl->blockStmt) {
        for (auto *stmt : scopeDecl->blockStmt->body) {
          collectSymbolsFromStmt(stmt, lines, symbols, scopeDecl->scopeId ? scopeDecl->scopeId->val : std::string());
        }
      }
      break;
    }
    case TYPE_ALIAS_DECL: {
      auto *aliasDecl = static_cast<ASTTypeAliasDecl *>(decl);
      std::string signature = "def ";
      signature += aliasDecl->id ? aliasDecl->id->val : "TypeAlias";
      signature += genericParamsToString(aliasDecl->genericTypeParams);
      signature += " = ";
      signature += typeToString(aliasDecl->aliasedType);
      pushSymbol(symbols,
                 aliasDecl->id,
                 SYMBOL_KIND_CLASS,
                 "type alias",
                 signature,
                 lines,
                 containerName,
                 !containerName.empty());
      break;
    }
    default:
      break;
  }
}

void collectSymbolsFromStmt(ASTStmt *stmt,
                            const std::vector<std::string> &lines,
                            std::vector<SymbolEntry> &symbols,
                            const std::string &containerName) {
  if (!stmt || !(stmt->type & DECL)) {
    return;
  }
  collectSymbolsFromDecl(static_cast<ASTDecl *>(stmt), lines, symbols, containerName);
}

ParsedDocument parseDocument(const std::string &text) {
  ParsedDocument analysis;
  std::ostringstream diagSink;
  auto diagnostics = DiagnosticHandler::createDefault(diagSink);
  Syntax::Lexer lexer(*diagnostics);
  std::istringstream in(text);
  lexer.tokenizeFromIStream(in, analysis.tokenStream);

  Syntax::SyntaxA syntax;
  syntax.setTokenStream(analysis.tokenStream);
  auto lines = splitLines(text);

  while (true) {
    ASTStmt *stmt = syntax.nextStatement();
    if (!stmt) {
      if (syntax.isAtEnd()) {
        break;
      }
      syntax.consumeCurrentTok();
      continue;
    }
    collectSymbolsFromStmt(stmt, lines, analysis.symbols, std::string());
  }
  return analysis;
}

std::string trimTypeQualifiers(std::string typeName) {
  while (!typeName.empty() && (typeName.back() == '?' || typeName.back() == '!')) {
    typeName.pop_back();
  }
  return trimCopy(typeName);
}

std::string baseTypeName(std::string typeName) {
  typeName = trimTypeQualifiers(std::move(typeName));
  auto genericPos = typeName.find('<');
  if (genericPos != std::string::npos) {
    typeName = typeName.substr(0, genericPos);
  }
  typeName = trimCopy(typeName);
  if (typeName.size() >= 2 && endsWith(typeName, "[]")) {
    return "Array";
  }
  return typeName;
}

std::optional<std::string> symbolDeclaredType(const SymbolEntry &symbol) {
  auto colonPos = symbol.signature.find(':');
  if (colonPos == std::string::npos) {
    return std::nullopt;
  }
  auto typePart = symbol.signature.substr(colonPos + 1);
  auto eqPos = typePart.find('=');
  if (eqPos != std::string::npos) {
    typePart = typePart.substr(0, eqPos);
  }
  typePart = trimCopy(typePart);
  if (typePart.empty()) {
    return std::nullopt;
  }
  return baseTypeName(typePart);
}

unsigned symbolKindToSemanticTokenType(int kind) {
  switch (kind) {
    case SYMBOL_KIND_NAMESPACE:
      return TOKEN_TYPE_NAMESPACE;
    case SYMBOL_KIND_CLASS:
    case SYMBOL_KIND_INTERFACE:
    case SYMBOL_KIND_STRUCT:
    case SYMBOL_KIND_ENUM:
      return TOKEN_TYPE_CLASS;
    case SYMBOL_KIND_FUNCTION:
    case SYMBOL_KIND_METHOD:
      return TOKEN_TYPE_FUNCTION;
    case SYMBOL_KIND_VARIABLE:
    case SYMBOL_KIND_FIELD:
    case SYMBOL_KIND_CONSTANT:
    default:
      return TOKEN_TYPE_VARIABLE;
  }
}

std::string spanKey(unsigned line, unsigned start, unsigned length) {
  return std::to_string(line) + ":" + std::to_string(start) + ":" + std::to_string(length);
}

unsigned semanticTokenTypePriority(unsigned tokenType) {
  switch (tokenType) {
    case TOKEN_TYPE_NAMESPACE:
      return 4;
    case TOKEN_TYPE_CLASS:
      return 3;
    case TOKEN_TYPE_FUNCTION:
      return 2;
    case TOKEN_TYPE_VARIABLE:
      return 1;
    default:
      return 0;
  }
}

void recordIdentifierType(std::unordered_map<std::string, unsigned> &out, const std::string &name, unsigned tokenType) {
  auto it = out.find(name);
  if (it == out.end() || semanticTokenTypePriority(tokenType) > semanticTokenTypePriority(it->second)) {
    out[name] = tokenType;
  }
}

bool semanticTokenInRange(unsigned line, unsigned rangeStartLine, unsigned rangeEndLine, bool useRange) {
  if (!useRange) {
    return true;
  }
  return line >= rangeStartLine && line <= rangeEndLine;
}

}

bool isBuiltinsInterfaceUri(const std::string &uri) {
  auto lowered = toLower(uri);
  const std::string unixSuffix = "/stdlib/builtins.starbint";
  const std::string winSuffix = "\\stdlib\\builtins.starbint";
  auto hasSuffix = [](const std::string &value, const std::string &suffix) -> bool {
    if (suffix.size() > value.size()) {
      return false;
    }
    return std::equal(suffix.rbegin(), suffix.rend(), value.rbegin());
  };
  return hasSuffix(lowered, unixSuffix) || hasSuffix(lowered, winSuffix);
}

int completionKindFromSymbolKind(int symbolKind) {
  if (symbolKind == SYMBOL_KIND_CLASS) {
    return COMPLETION_KIND_CLASS;
  }
  if (symbolKind == SYMBOL_KIND_METHOD) {
    return COMPLETION_KIND_METHOD;
  }
  if (symbolKind == SYMBOL_KIND_FIELD) {
    return COMPLETION_KIND_FIELD;
  }
  if (symbolKind == SYMBOL_KIND_FUNCTION) {
    return COMPLETION_KIND_FUNCTION;
  }
  if (symbolKind == SYMBOL_KIND_NAMESPACE) {
    return COMPLETION_KIND_MODULE;
  }
  if (symbolKind == SYMBOL_KIND_INTERFACE) {
    return COMPLETION_KIND_INTERFACE;
  }
  if (symbolKind == SYMBOL_KIND_STRUCT) {
    return COMPLETION_KIND_STRUCT;
  }
  if (symbolKind == SYMBOL_KIND_ENUM) {
    return COMPLETION_KIND_ENUM;
  }
  if (symbolKind == SYMBOL_KIND_CONSTANT) {
    return COMPLETION_KIND_VALUE;
  }
  return COMPLETION_KIND_VARIABLE;
}

std::vector<CompilerDiagnosticEntry> collectCompilerDiagnosticsForText(const std::string &sourceName,
                                                                       const std::string &text) {
  std::vector<CompilerDiagnosticEntry> diagnosticsOut;
  std::ostringstream sink;
  auto diagnostics = DiagnosticHandler::createDefault(sink);
  auto *handler = diagnostics.get();

  NullAstConsumer consumer;
  Parser parser(consumer, std::move(diagnostics));
  auto parseContext = ModuleParseContext::Create(sourceName);
  parseContext.name = sourceName;
  std::istringstream in(text);
  parser.parseFromStream(in, parseContext);

  if (handler) {
    auto buffered = handler->snapshot();
    diagnosticsOut.reserve(buffered.size());
    for (const auto &diag : buffered) {
      if (!diag) {
        continue;
      }
      CompilerDiagnosticEntry mapped;
      if (diag->location.has_value()) {
        mapped.region = *diag->location;
      }
      mapped.severity = diag->isError() ? 1 : 2;
      mapped.message = diag->message;
      diagnosticsOut.push_back(std::move(mapped));
    }
    handler->clear();
  }
  return diagnosticsOut;
}

std::vector<SymbolEntry> collectSymbolsFromText(const std::string &text) {
  return parseDocument(text).symbols;
}

BuiltinApiIndex buildBuiltinApiIndex(const std::string &uri, const std::string &text) {
  BuiltinApiIndex index;
  index.uri = uri;
  if (text.empty()) {
    return index;
  }

  auto symbols = collectSymbolsFromText(text);
  for (const auto &symbol : symbols) {
    if (symbol.isMember && !symbol.containerName.empty()) {
      index.membersByQualifiedName[symbol.containerName + "." + symbol.name] = symbol;
      index.membersByType[symbol.containerName][symbol.name] = symbol;
      continue;
    }
    index.topLevelByName[symbol.name] = symbol;
  }
  return index;
}

bool extractReceiverBeforeOffset(const std::string &text, size_t memberStartOffset, std::string &receiverOut) {
  if (memberStartOffset == 0 || memberStartOffset > text.size()) {
    return false;
  }

  size_t cursor = memberStartOffset;
  while (cursor > 0 && std::isspace(static_cast<unsigned char>(text[cursor - 1])) != 0) {
    --cursor;
  }
  if (cursor == 0 || text[cursor - 1] != '.') {
    return false;
  }

  size_t recvEnd = cursor - 1;
  while (recvEnd > 0 && std::isspace(static_cast<unsigned char>(text[recvEnd - 1])) != 0) {
    --recvEnd;
  }
  size_t recvStart = recvEnd;
  while (recvStart > 0) {
    unsigned char c = static_cast<unsigned char>(text[recvStart - 1]);
    if (!(std::isalnum(c) != 0 || c == '_')) {
      break;
    }
    --recvStart;
  }
  if (recvStart >= recvEnd || !(std::isalpha(static_cast<unsigned char>(text[recvStart])) != 0 || text[recvStart] == '_')) {
    return false;
  }

  receiverOut = text.substr(recvStart, recvEnd - recvStart);
  return !receiverOut.empty();
}

bool extractMemberCompletionContext(const std::string &text,
                                    size_t cursorOffset,
                                    std::string &receiverOut,
                                    std::string &memberPrefixOut) {
  if (cursorOffset > text.size()) {
    cursorOffset = text.size();
  }

  size_t prefixStart = cursorOffset;
  while (prefixStart > 0) {
    unsigned char c = static_cast<unsigned char>(text[prefixStart - 1]);
    if (!(std::isalnum(c) != 0 || c == '_')) {
      break;
    }
    --prefixStart;
  }
  memberPrefixOut = text.substr(prefixStart, cursorOffset - prefixStart);
  return extractReceiverBeforeOffset(text, prefixStart, receiverOut);
}

std::optional<std::string> inferBuiltinTypeForReceiver(const std::string &text,
                                                       const std::string &receiver,
                                                       unsigned maxLine,
                                                       const BuiltinApiIndex &index) {
  if (receiver.empty()) {
    return std::nullopt;
  }

  auto directType = index.topLevelByName.find(receiver);
  if (directType != index.topLevelByName.end()) {
    if (index.membersByType.find(receiver) != index.membersByType.end()) {
      return receiver;
    }
  }

  auto symbols = collectSymbolsFromText(text);
  const SymbolEntry *best = nullptr;
  for (const auto &symbol : symbols) {
    if (symbol.name != receiver || symbol.kind != SYMBOL_KIND_VARIABLE && symbol.kind != SYMBOL_KIND_CONSTANT) {
      continue;
    }
    if (symbol.line > maxLine) {
      continue;
    }
    if (!best || symbol.line > best->line || (symbol.line == best->line && symbol.start >= best->start)) {
      best = &symbol;
    }
  }
  if (!best) {
    return std::nullopt;
  }

  auto typeName = symbolDeclaredType(*best);
  if (!typeName.has_value()) {
    return std::nullopt;
  }
  if (index.membersByType.find(*typeName) == index.membersByType.end()) {
    return std::nullopt;
  }
  return typeName;
}

bool signaturePartsFromSignature(const std::string &signature,
                                 std::string &nameOut,
                                 std::string &paramsOut,
                                 std::string &returnOut) {
  std::smatch match;
  if (!std::regex_search(signature, match, kFuncSignatureRegex)) {
    return false;
  }
  nameOut = match[1].str();
  paramsOut = match[2].str();
  returnOut = match[3].matched ? match[3].str() : "";
  return true;
}

bool findFunctionSignatureInText(const std::string &text,
                                 const std::string &functionName,
                                 std::string &nameOut,
                                 std::string &paramsOut,
                                 std::string &returnOut) {
  for (std::sregex_iterator it(text.begin(), text.end(), kFuncSignatureRegex), end; it != end; ++it) {
    if ((*it)[1].str() != functionName) {
      continue;
    }
    nameOut = (*it)[1].str();
    paramsOut = (*it)[2].str();
    returnOut = (*it)[3].matched ? (*it)[3].str() : "";
    return true;
  }
  return false;
}

std::vector<SemanticTokenEntry> collectSemanticTokenEntries(const std::string &text,
                                                            unsigned rangeStartLine,
                                                            unsigned rangeEndLine,
                                                            bool useRange) {
  auto analysis = parseDocument(text);
  std::vector<SemanticTokenEntry> tokens;

  std::unordered_map<std::string, unsigned> declarationSpanTypes;
  std::unordered_map<std::string, unsigned> identifierTypes;
  declarationSpanTypes.reserve(analysis.symbols.size());
  identifierTypes.reserve(analysis.symbols.size());
  for (const auto &symbol : analysis.symbols) {
    auto semanticType = symbolKindToSemanticTokenType(symbol.kind);
    declarationSpanTypes[spanKey(symbol.line, symbol.start, symbol.length)] = semanticType;
    recordIdentifierType(identifierTypes, symbol.name, semanticType);
  }

  tokens.reserve(analysis.tokenStream.size());
  for (const auto &token : analysis.tokenStream) {
    unsigned line = token.srcPos.line > 0 ? token.srcPos.line - 1 : 0;
    if (!semanticTokenInRange(line, rangeStartLine, rangeEndLine, useRange)) {
      continue;
    }

    unsigned start = token.srcPos.startCol;
    unsigned length = static_cast<unsigned>(token.content.size());
    if (length == 0) {
      continue;
    }

    if (token.type == Syntax::Tok::Keyword || token.type == Syntax::Tok::BooleanLiteral) {
      tokens.push_back({line, start, length, TOKEN_TYPE_KEYWORD});
      continue;
    }

    if (token.type != Syntax::Tok::Identifier) {
      continue;
    }

    unsigned semanticType = TOKEN_TYPE_VARIABLE;
    auto declIt = declarationSpanTypes.find(spanKey(line, start, length));
    if (declIt != declarationSpanTypes.end()) {
      semanticType = declIt->second;
    } else {
      auto idIt = identifierTypes.find(token.content);
      if (idIt != identifierTypes.end()) {
        semanticType = idIt->second;
      }
    }

    tokens.push_back({line, start, length, semanticType});
  }

  std::sort(tokens.begin(), tokens.end(), [](const SemanticTokenEntry &lhs, const SemanticTokenEntry &rhs) {
    if (lhs.line != rhs.line) {
      return lhs.line < rhs.line;
    }
    if (lhs.start != rhs.start) {
      return lhs.start < rhs.start;
    }
    if (lhs.length != rhs.length) {
      return lhs.length < rhs.length;
    }
    return lhs.type < rhs.type;
  });
  return tokens;
}

std::vector<unsigned> encodeSemanticTokens(const std::vector<SemanticTokenEntry> &tokens) {
  std::vector<unsigned> encoded;
  encoded.reserve(tokens.size() * 5);

  unsigned prevLine = 0;
  unsigned prevStart = 0;
  bool first = true;
  for (const auto &token : tokens) {
    unsigned deltaLine = first ? token.line : (token.line - prevLine);
    unsigned deltaStart = first ? token.start : (deltaLine == 0 ? token.start - prevStart : token.start);
    encoded.push_back(deltaLine);
    encoded.push_back(deltaStart);
    encoded.push_back(token.length);
    encoded.push_back(token.type);
    encoded.push_back(0);
    prevLine = token.line;
    prevStart = token.start;
    first = false;
  }
  return encoded;
}

}
