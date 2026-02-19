#include "ServerMain.h"
#include "starbytes/base/Diagnostic.h"
#include "starbytes/compiler/AST.h"
#include "starbytes/compiler/ASTDecl.h"
#include "starbytes/compiler/ASTExpr.h"
#include "starbytes/compiler/Lexer.h"
#include "starbytes/compiler/SyntaxA.h"
#include "starbytes/compiler/Type.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <utility>

#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

namespace starbytes::lsp {

namespace {

constexpr int TEXT_DOCUMENT_SYNC_NONE = 0;
constexpr int TEXT_DOCUMENT_SYNC_FULL = 1;
constexpr int TEXT_DOCUMENT_SYNC_INCREMENTAL = 2;

constexpr int JSONRPC_PARSE_ERROR = -32700;
constexpr int JSONRPC_INVALID_REQUEST = -32600;
constexpr int JSONRPC_METHOD_NOT_FOUND = -32601;
constexpr int JSONRPC_INVALID_PARAMS = -32602;
constexpr int JSONRPC_INTERNAL_ERROR = -32603;
constexpr int LSP_SERVER_NOT_INITIALIZED = -32002;
constexpr int LSP_REQUEST_CANCELLED = -32800;

constexpr int COMPLETION_KIND_TEXT = 1;
constexpr int COMPLETION_KIND_METHOD = 2;
constexpr int COMPLETION_KIND_FUNCTION = 3;
constexpr int COMPLETION_KIND_CONSTRUCTOR = 4;
constexpr int COMPLETION_KIND_FIELD = 5;
constexpr int COMPLETION_KIND_VARIABLE = 6;
constexpr int COMPLETION_KIND_CLASS = 7;
constexpr int COMPLETION_KIND_INTERFACE = 8;
constexpr int COMPLETION_KIND_MODULE = 9;
constexpr int COMPLETION_KIND_PROPERTY = 10;
constexpr int COMPLETION_KIND_UNIT = 11;
constexpr int COMPLETION_KIND_VALUE = 12;
constexpr int COMPLETION_KIND_ENUM = 13;
constexpr int COMPLETION_KIND_KEYWORD = 14;
constexpr int COMPLETION_KIND_SNIPPET = 15;
constexpr int COMPLETION_KIND_STRUCT = 22;

constexpr int SYMBOL_KIND_NAMESPACE = 3;
constexpr int SYMBOL_KIND_CLASS = 5;
constexpr int SYMBOL_KIND_METHOD = 6;
constexpr int SYMBOL_KIND_FIELD = 8;
constexpr int SYMBOL_KIND_ENUM = 10;
constexpr int SYMBOL_KIND_INTERFACE = 11;
constexpr int SYMBOL_KIND_FUNCTION = 12;
constexpr int SYMBOL_KIND_VARIABLE = 13;
constexpr int SYMBOL_KIND_CONSTANT = 14;
constexpr int SYMBOL_KIND_STRUCT = 23;

constexpr unsigned TOKEN_TYPE_NAMESPACE = 0;
constexpr unsigned TOKEN_TYPE_CLASS = 1;
constexpr unsigned TOKEN_TYPE_FUNCTION = 2;
constexpr unsigned TOKEN_TYPE_VARIABLE = 3;
constexpr unsigned TOKEN_TYPE_KEYWORD = 4;

struct CompletionEntry {
  std::string label;
  int kind = COMPLETION_KIND_TEXT;
  std::string detail;
};

struct SymbolEntry {
  std::string name;
  int kind = SYMBOL_KIND_VARIABLE;
  std::string detail;
  std::string signature;
  std::string documentation;
  unsigned line = 0;
  unsigned start = 0;
  unsigned length = 0;
};

struct SemanticTokenEntry {
  unsigned line = 0;
  unsigned start = 0;
  unsigned length = 0;
  unsigned type = TOKEN_TYPE_KEYWORD;
};

struct DocumentAnalysis {
  std::vector<Syntax::Tok> tokenStream;
  std::vector<SymbolEntry> symbols;
};

const std::vector<std::string> kKeywordCompletions = {
    "decl", "imut", "func", "class", "struct", "interface", "enum", "scope", "import", "if",
    "elif", "else", "for", "while", "return", "new", "secure", "catch", "def", "lazy", "await",
    "true", "false"};

const std::regex kFuncSignatureRegex(
    R"(\bfunc\s+([A-Za-z_][A-Za-z0-9_]*)\s*\(([^)]*)\)\s*([A-Za-z_][A-Za-z0-9_<>!?]*)?)");

std::string toLower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return value;
}

std::string idToKey(const rapidjson::Value &id) {
  if (id.IsString()) {
    return std::string("s:") + id.GetString();
  }
  if (id.IsInt64()) {
    return std::string("i:") + std::to_string(id.GetInt64());
  }
  if (id.IsUint64()) {
    return std::string("u:") + std::to_string(id.GetUint64());
  }
  if (id.IsInt()) {
    return std::string("i:") + std::to_string(id.GetInt());
  }
  if (id.IsUint()) {
    return std::string("u:") + std::to_string(id.GetUint());
  }
  return "unknown";
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

std::string toStdString(string_ref ref) {
  return std::string(ref.getBuffer(), ref.size());
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
                const std::vector<std::string> &lines) {
  if (!id) {
    return;
  }

  SymbolEntry symbol;
  symbol.name = id->val;
  symbol.kind = kind;
  symbol.detail = detail;
  symbol.signature = signature;
  unsigned sourceLine = id->codeRegion.startLine;
  symbol.line = sourceLine > 0 ? sourceLine - 1 : 0;
  symbol.start = id->codeRegion.startCol;
  symbol.length = id->codeRegion.endCol > id->codeRegion.startCol ? id->codeRegion.endCol - id->codeRegion.startCol
                                                                   : static_cast<unsigned>(symbol.name.size());
  symbol.documentation = extractLeadingDocComment(lines, symbol.line);
  symbols.push_back(std::move(symbol));
}

void collectSymbolsFromStmt(ASTStmt *stmt, const std::vector<std::string> &lines, std::vector<SymbolEntry> &symbols);

void collectSymbolsFromDecl(ASTDecl *decl, const std::vector<std::string> &lines, std::vector<SymbolEntry> &symbols) {
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
        std::string detail = varDecl->isConst ? "constant" : "variable";
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
        pushSymbol(symbols, spec.id, varDecl->isConst ? SYMBOL_KIND_CONSTANT : SYMBOL_KIND_VARIABLE, detail, signature,
                   lines);
      }
      break;
    }
    case FUNC_DECL: {
      auto *funcDecl = static_cast<ASTFuncDecl *>(decl);
      std::string detail = funcDecl->isLazy ? "lazy function" : "function";
      pushSymbol(symbols, funcDecl->funcId, SYMBOL_KIND_FUNCTION, detail, functionSignature(funcDecl), lines);
      break;
    }
    case CLASS_DECL: {
      auto *classDecl = static_cast<ASTClassDecl *>(decl);
      if (!classDecl->id) {
        break;
      }
      bool isStructDecl = lineHasDeclKeyword(lines, classDecl->id->codeRegion.startLine > 0 ? classDecl->id->codeRegion.startLine - 1 : 0, "struct");
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
      pushSymbol(symbols, classDecl->id, kind, detail, signature, lines);
      for (auto *field : classDecl->fields) {
        collectSymbolsFromDecl(field, lines, symbols);
      }
      for (auto *method : classDecl->methods) {
        collectSymbolsFromDecl(method, lines, symbols);
      }
      break;
    }
    case INTERFACE_DECL: {
      auto *interfaceDecl = static_cast<ASTInterfaceDecl *>(decl);
      std::string signature = "interface ";
      signature += interfaceDecl->id ? interfaceDecl->id->val : "Interface";
      signature += genericParamsToString(interfaceDecl->genericTypeParams);
      pushSymbol(symbols, interfaceDecl->id, SYMBOL_KIND_INTERFACE, "interface", signature, lines);
      for (auto *field : interfaceDecl->fields) {
        collectSymbolsFromDecl(field, lines, symbols);
      }
      for (auto *method : interfaceDecl->methods) {
        collectSymbolsFromDecl(method, lines, symbols);
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
      pushSymbol(symbols, scopeDecl->scopeId, kind, detail, signature, lines);
      if (scopeDecl->blockStmt) {
        for (auto *stmt : scopeDecl->blockStmt->body) {
          collectSymbolsFromStmt(stmt, lines, symbols);
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
      pushSymbol(symbols, aliasDecl->id, SYMBOL_KIND_CLASS, "type alias", signature, lines);
      break;
    }
    default:
      break;
  }
}

void collectSymbolsFromStmt(ASTStmt *stmt, const std::vector<std::string> &lines, std::vector<SymbolEntry> &symbols) {
  if (!stmt || !(stmt->type & DECL)) {
    return;
  }
  collectSymbolsFromDecl(static_cast<ASTDecl *>(stmt), lines, symbols);
}

DocumentAnalysis analyzeDocument(const std::string &text) {
  DocumentAnalysis analysis;
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
    collectSymbolsFromStmt(stmt, lines, analysis.symbols);
  }
  return analysis;
}

std::vector<SymbolEntry> collectSymbolsFromText(const std::string &text) {
  return analyzeDocument(text).symbols;
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

std::vector<SemanticTokenEntry> collectSemanticTokenEntries(const std::string &text,
                                                            unsigned rangeStartLine,
                                                            unsigned rangeEndLine,
                                                            bool useRange) {
  auto analysis = analyzeDocument(text);
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

} // namespace

Server::Server(starbytes::lsp::ServerOptions &options) : in(options.in), out(options.os) {}

std::string Server::trim(const std::string &inValue) {
  size_t start = 0;
  while (start < inValue.size() && std::isspace(static_cast<unsigned char>(inValue[start]))) {
    ++start;
  }

  size_t end = inValue.size();
  while (end > start && std::isspace(static_cast<unsigned char>(inValue[end - 1]))) {
    --end;
  }
  return inValue.substr(start, end - start);
}

bool Server::isIdentifierChar(char c) {
  return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
}

bool Server::isIdentifierStart(char c) {
  return std::isalpha(static_cast<unsigned char>(c)) != 0 || c == '_';
}

bool Server::parseUriToPath(const std::string &uri, std::string &pathOut) {
  if (uri.rfind("file://", 0) != 0) {
    return false;
  }

  auto raw = uri.substr(7);
  std::string decoded;
  decoded.reserve(raw.size());
  for (size_t i = 0; i < raw.size(); ++i) {
    if (raw[i] == '%' && i + 2 < raw.size()) {
      auto h1 = raw[i + 1];
      auto h2 = raw[i + 2];
      auto hex = [](char c) -> int {
        if (c >= '0' && c <= '9') {
          return c - '0';
        }
        if (c >= 'a' && c <= 'f') {
          return 10 + (c - 'a');
        }
        if (c >= 'A' && c <= 'F') {
          return 10 + (c - 'A');
        }
        return -1;
      };
      int v1 = hex(h1);
      int v2 = hex(h2);
      if (v1 >= 0 && v2 >= 0) {
        decoded.push_back(static_cast<char>((v1 << 4) | v2));
        i += 2;
        continue;
      }
    }
    decoded.push_back(raw[i]);
  }

#ifdef _WIN32
  if (!decoded.empty() && decoded[0] == '/' && decoded.size() > 2 && std::isalpha((unsigned char)decoded[1]) &&
      decoded[2] == ':') {
    decoded.erase(decoded.begin());
  }
#endif

  pathOut = decoded;
  return true;
}

std::string Server::pathToUri(const std::string &path) {
  std::string uri = "file://";
  for (unsigned char c : path) {
    if (std::isalnum(c) || c == '/' || c == '-' || c == '_' || c == '.' || c == '~') {
      uri.push_back(static_cast<char>(c));
      continue;
    }
    static const char *hex = "0123456789ABCDEF";
    uri.push_back('%');
    uri.push_back(hex[(c >> 4) & 0xF]);
    uri.push_back(hex[c & 0xF]);
  }
  return uri;
}

size_t Server::offsetFromPosition(const std::string &text, unsigned line, unsigned character) {
  size_t offset = 0;
  unsigned currentLine = 0;

  while (offset < text.size() && currentLine < line) {
    if (text[offset] == '\n') {
      ++currentLine;
    }
    ++offset;
  }

  if (currentLine != line) {
    return text.size();
  }

  unsigned currentChar = 0;
  while (offset < text.size() && text[offset] != '\n' && currentChar < character) {
    ++offset;
    ++currentChar;
  }

  return offset;
}

void Server::positionFromOffset(const std::string &text, size_t offset, unsigned &lineOut, unsigned &charOut) {
  lineOut = 0;
  charOut = 0;

  auto capped = std::min(offset, text.size());
  for (size_t i = 0; i < capped; ++i) {
    if (text[i] == '\n') {
      ++lineOut;
      charOut = 0;
    } else {
      ++charOut;
    }
  }
}

bool Server::wordRangeAtPosition(const std::string &text,
                                 unsigned line,
                                 unsigned character,
                                 std::string &wordOut,
                                 size_t &startOut,
                                 size_t &endOut) {
  if (text.empty()) {
    return false;
  }

  size_t cursor = offsetFromPosition(text, line, character);
  if (cursor >= text.size()) {
    if (cursor == 0 || !isIdentifierChar(text[cursor - 1])) {
      return false;
    }
    cursor -= 1;
  } else if (!isIdentifierChar(text[cursor])) {
    if (cursor > 0 && isIdentifierChar(text[cursor - 1])) {
      cursor -= 1;
    } else {
      return false;
    }
  }

  size_t start = cursor;
  while (start > 0 && isIdentifierChar(text[start - 1])) {
    --start;
  }
  size_t end = cursor + 1;
  while (end < text.size() && isIdentifierChar(text[end])) {
    ++end;
  }

  if (start >= end || !isIdentifierStart(text[start])) {
    return false;
  }

  wordOut = text.substr(start, end - start);
  startOut = start;
  endOut = end;
  return !wordOut.empty();
}

std::string Server::extractPrefixAtPosition(const std::string &text, unsigned line, unsigned character) {
  std::istringstream stream(text);
  std::string current;
  unsigned currentLine = 0;
  while (std::getline(stream, current)) {
    if (currentLine == line) {
      break;
    }
    ++currentLine;
  }
  if (currentLine != line) {
    return {};
  }

  unsigned cursor = std::min<unsigned>(character, static_cast<unsigned>(current.size()));
  unsigned start = cursor;
  while (start > 0 && isIdentifierChar(current[start - 1])) {
    --start;
  }
  return current.substr(start, cursor - start);
}

void Server::appendOccurrences(const std::string &text,
                               const std::string &name,
                               std::vector<size_t> &offsetsOut,
                               size_t rangeStart,
                               size_t rangeEnd) {
  if (name.empty()) {
    return;
  }

  auto cappedEnd = std::min(rangeEnd, text.size());
  size_t pos = std::min(rangeStart, text.size());
  while (pos < cappedEnd) {
    auto found = text.find(name, pos);
    if (found == std::string::npos || found >= cappedEnd) {
      break;
    }

    bool leftOk = found == 0 || !isIdentifierChar(text[found - 1]);
    auto right = found + name.size();
    bool rightOk = right >= text.size() || !isIdentifierChar(text[right]);

    if (leftOk && rightOk) {
      offsetsOut.push_back(found);
    }
    pos = found + name.size();
  }
}

bool Server::readMessage(std::string &body) {
  std::string headerLine;
  size_t contentLength = 0;
  bool sawHeader = false;

  while (std::getline(in, headerLine)) {
    if (!headerLine.empty() && headerLine.back() == '\r') {
      headerLine.pop_back();
    }
    if (headerLine.empty()) {
      break;
    }
    sawHeader = true;
    auto separator = headerLine.find(':');
    if (separator == std::string::npos) {
      continue;
    }
    auto key = headerLine.substr(0, separator);
    auto value = trim(headerLine.substr(separator + 1));
    if (key == "Content-Length") {
      try {
        contentLength = static_cast<size_t>(std::stoul(value));
      } catch (...) {
        return false;
      }
    }
  }

  if (!sawHeader) {
    return false;
  }

  body.clear();
  if (contentLength == 0) {
    return true;
  }

  body.resize(contentLength);
  in.read(body.data(), static_cast<std::streamsize>(contentLength));
  return static_cast<size_t>(in.gcount()) == contentLength;
}

void Server::writeMessage(rapidjson::Document &doc) {
  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
  doc.Accept(writer);
  out << "Content-Length: " << buffer.GetSize() << "\r\n\r\n" << buffer.GetString() << std::flush;
}

void Server::writeResult(const rapidjson::Value &id, rapidjson::Value &result) {
  rapidjson::Document response(rapidjson::kObjectType);
  auto &alloc = response.GetAllocator();
  response.AddMember("jsonrpc", rapidjson::Value("2.0", alloc), alloc);
  rapidjson::Value idCopy;
  idCopy.CopyFrom(id, alloc);
  response.AddMember("id", idCopy, alloc);
  rapidjson::Value resultCopy;
  resultCopy.CopyFrom(result, alloc);
  response.AddMember("result", resultCopy, alloc);
  writeMessage(response);
}

void Server::writeError(const rapidjson::Value &id, int code, const char *message) {
  rapidjson::Document response(rapidjson::kObjectType);
  auto &alloc = response.GetAllocator();
  response.AddMember("jsonrpc", rapidjson::Value("2.0", alloc), alloc);
  rapidjson::Value idCopy;
  idCopy.CopyFrom(id, alloc);
  response.AddMember("id", idCopy, alloc);

  rapidjson::Value errorObj(rapidjson::kObjectType);
  errorObj.AddMember("code", code, alloc);
  errorObj.AddMember("message", rapidjson::Value(message, alloc), alloc);
  response.AddMember("error", errorObj, alloc);
  writeMessage(response);
}

void Server::writeNotification(const char *method, rapidjson::Value &params) {
  rapidjson::Document payload(rapidjson::kObjectType);
  auto &alloc = payload.GetAllocator();
  payload.AddMember("jsonrpc", rapidjson::Value("2.0", alloc), alloc);
  payload.AddMember("method", rapidjson::Value(method, alloc), alloc);
  rapidjson::Value paramsCopy;
  paramsCopy.CopyFrom(params, alloc);
  payload.AddMember("params", paramsCopy, alloc);
  writeMessage(payload);
}

void Server::loadWorkspaceDocuments() {
  for (auto it = documents.begin(); it != documents.end();) {
    if (!it->second.isOpen) {
      it = documents.erase(it);
    } else {
      ++it;
    }
  }

  std::vector<std::string> roots = workspaceRoots;
  if (roots.empty()) {
    roots.push_back(pathToUri(std::filesystem::current_path().string()));
  }

  for (const auto &rootUri : roots) {
    std::string rootPath;
    if (!parseUriToPath(rootUri, rootPath)) {
      continue;
    }

    std::error_code ec;
    if (!std::filesystem::exists(rootPath, ec) || ec || !std::filesystem::is_directory(rootPath, ec)) {
      continue;
    }

    for (std::filesystem::recursive_directory_iterator it(rootPath, ec), end; it != end; it.increment(ec)) {
      if (ec) {
        break;
      }
      if (!it->is_regular_file()) {
        continue;
      }
      auto ext = it->path().extension().string();
      if (ext != ".starb" && ext != ".starbint") {
        continue;
      }

      std::ifstream file(it->path(), std::ios::in);
      if (!file.is_open()) {
        continue;
      }

      std::ostringstream buffer;
      buffer << file.rdbuf();
      auto uri = pathToUri(it->path().string());
      auto existing = documents.find(uri);
      if (existing != documents.end() && existing->second.isOpen) {
        continue;
      }

      documents[uri] = {buffer.str(), 0, false};
    }
  }
}

bool Server::getDocumentTextByUri(const std::string &uri, std::string &textOut) {
  auto it = documents.find(uri);
  if (it != documents.end()) {
    textOut = it->second.text;
    return true;
  }

  std::string path;
  if (!parseUriToPath(uri, path)) {
    return false;
  }

  std::ifstream file(path, std::ios::in);
  if (!file.is_open()) {
    return false;
  }

  std::ostringstream buffer;
  buffer << file.rdbuf();
  textOut = buffer.str();
  documents[uri] = {textOut, 0, false};
  return true;
}

void Server::setDocumentTextByUri(const std::string &uri, const std::string &text, int version, bool isOpen) {
  documents[uri] = {text, version, isOpen};
  semanticSnapshots.erase(uri);
}

void Server::removeOpenDocumentByUri(const std::string &uri) {
  auto it = documents.find(uri);
  if (it == documents.end()) {
    return;
  }

  std::string path;
  if (parseUriToPath(uri, path)) {
    std::ifstream file(path, std::ios::in);
    if (file.is_open()) {
      std::ostringstream buffer;
      buffer << file.rdbuf();
      it->second.text = buffer.str();
      it->second.version = 0;
      it->second.isOpen = false;
      semanticSnapshots.erase(uri);
      return;
    }
  }

  documents.erase(it);
  semanticSnapshots.erase(uri);
}

void Server::publishDiagnostics(const std::string &uri) {
  rapidjson::Document paramsDoc(rapidjson::kObjectType);
  auto &alloc = paramsDoc.GetAllocator();

  paramsDoc.AddMember("uri", rapidjson::Value(uri.c_str(), alloc), alloc);
  rapidjson::Value diagnostics(rapidjson::kArrayType);

  std::string text;
  if (getDocumentTextByUri(uri, text)) {
    std::vector<std::pair<unsigned, unsigned>> braceStack;
    unsigned line = 0;
    unsigned character = 0;

    auto pushDiagnostic = [&](unsigned startLine, unsigned startChar, unsigned endLine, unsigned endChar,
                              const char *message) {
      rapidjson::Value diag(rapidjson::kObjectType);
      rapidjson::Value range(rapidjson::kObjectType);
      rapidjson::Value start(rapidjson::kObjectType);
      rapidjson::Value end(rapidjson::kObjectType);
      start.AddMember("line", startLine, alloc);
      start.AddMember("character", startChar, alloc);
      end.AddMember("line", endLine, alloc);
      end.AddMember("character", endChar, alloc);
      range.AddMember("start", start, alloc);
      range.AddMember("end", end, alloc);
      diag.AddMember("range", range, alloc);
      diag.AddMember("severity", 1, alloc);
      diag.AddMember("source", rapidjson::Value("starbytes-lsp", alloc), alloc);
      diag.AddMember("message", rapidjson::Value(message, alloc), alloc);
      diagnostics.PushBack(diag, alloc);
    };

    for (char ch : text) {
      if (ch == '{') {
        braceStack.push_back({line, character});
      } else if (ch == '}') {
        if (braceStack.empty()) {
          pushDiagnostic(line, character, line, character + 1, "Unmatched closing brace `}`.");
        } else {
          braceStack.pop_back();
        }
      }

      if (ch == '\n') {
        ++line;
        character = 0;
      } else {
        ++character;
      }
    }

    for (const auto &entry : braceStack) {
      pushDiagnostic(entry.first, entry.second, entry.first, entry.second + 1, "Unmatched opening brace `{`.");
    }
  }

  paramsDoc.AddMember("diagnostics", diagnostics, alloc);
  writeNotification("textDocument/publishDiagnostics", paramsDoc);
}

void Server::maybeApplyIncrementalChange(std::string &text, const rapidjson::Value &change) {
  if (!change.IsObject() || !change.HasMember("text") || !change["text"].IsString()) {
    return;
  }

  auto replacementText = std::string(change["text"].GetString());

  if (!change.HasMember("range") || !change["range"].IsObject()) {
    text = replacementText;
    return;
  }

  auto &range = change["range"];
  if (!range.HasMember("start") || !range.HasMember("end") || !range["start"].IsObject() ||
      !range["end"].IsObject()) {
    text = replacementText;
    return;
  }

  auto &start = range["start"];
  auto &end = range["end"];
  if (!start.HasMember("line") || !start.HasMember("character") || !end.HasMember("line") ||
      !end.HasMember("character") || !start["line"].IsUint() || !start["character"].IsUint() ||
      !end["line"].IsUint() || !end["character"].IsUint()) {
    text = replacementText;
    return;
  }

  size_t startOffset = offsetFromPosition(text, start["line"].GetUint(), start["character"].GetUint());
  size_t endOffset = offsetFromPosition(text, end["line"].GetUint(), end["character"].GetUint());
  if (startOffset > endOffset) {
    std::swap(startOffset, endOffset);
  }
  startOffset = std::min(startOffset, text.size());
  endOffset = std::min(endOffset, text.size());
  text.replace(startOffset, endOffset - startOffset, replacementText);
}

void Server::handleInitialize(rapidjson::Document &request) {
  if (!request.HasMember("id")) {
    return;
  }

  workspaceRoots.clear();
  if (request.HasMember("params") && request["params"].IsObject()) {
    auto &params = request["params"];

    if (params.HasMember("workspaceFolders") && params["workspaceFolders"].IsArray()) {
      for (auto &folder : params["workspaceFolders"].GetArray()) {
        if (!folder.IsObject() || !folder.HasMember("uri") || !folder["uri"].IsString()) {
          continue;
        }
        workspaceRoots.emplace_back(folder["uri"].GetString());
      }
    }

    if (params.HasMember("rootUri") && params["rootUri"].IsString()) {
      workspaceRoots.emplace_back(params["rootUri"].GetString());
    } else if (params.HasMember("rootPath") && params["rootPath"].IsString()) {
      workspaceRoots.push_back(pathToUri(params["rootPath"].GetString()));
    }
  }

  if (workspaceRoots.empty()) {
    workspaceRoots.push_back(pathToUri(std::filesystem::current_path().string()));
  }

  loadWorkspaceDocuments();
  initialized = true;

  rapidjson::Document payloadDoc(rapidjson::kObjectType);
  auto &alloc = payloadDoc.GetAllocator();

  rapidjson::Value capabilities(rapidjson::kObjectType);
  rapidjson::Value textDocSync(rapidjson::kObjectType);
  textDocSync.AddMember("openClose", true, alloc);
  textDocSync.AddMember("change", TEXT_DOCUMENT_SYNC_INCREMENTAL, alloc);
  rapidjson::Value saveOptions(rapidjson::kObjectType);
  saveOptions.AddMember("includeText", true, alloc);
  textDocSync.AddMember("save", saveOptions, alloc);
  capabilities.AddMember("textDocumentSync", textDocSync, alloc);

  rapidjson::Value completionProvider(rapidjson::kObjectType);
  completionProvider.AddMember("resolveProvider", true, alloc);
  rapidjson::Value completionTriggers(rapidjson::kArrayType);
  completionTriggers.PushBack(rapidjson::Value(".", alloc), alloc);
  completionTriggers.PushBack(rapidjson::Value(":", alloc), alloc);
  completionProvider.AddMember("triggerCharacters", completionTriggers, alloc);
  capabilities.AddMember("completionProvider", completionProvider, alloc);

  capabilities.AddMember("hoverProvider", true, alloc);
  capabilities.AddMember("definitionProvider", true, alloc);
  capabilities.AddMember("declarationProvider", true, alloc);
  capabilities.AddMember("typeDefinitionProvider", true, alloc);
  capabilities.AddMember("implementationProvider", true, alloc);
  capabilities.AddMember("referencesProvider", true, alloc);
  capabilities.AddMember("documentSymbolProvider", true, alloc);
  capabilities.AddMember("workspaceSymbolProvider", true, alloc);

  rapidjson::Value renameProvider(rapidjson::kObjectType);
  renameProvider.AddMember("prepareProvider", true, alloc);
  capabilities.AddMember("renameProvider", renameProvider, alloc);

  rapidjson::Value signatureHelp(rapidjson::kObjectType);
  rapidjson::Value sigTriggers(rapidjson::kArrayType);
  sigTriggers.PushBack(rapidjson::Value("(", alloc), alloc);
  sigTriggers.PushBack(rapidjson::Value(",", alloc), alloc);
  rapidjson::Value sigRetriggers(rapidjson::kArrayType);
  sigRetriggers.PushBack(rapidjson::Value(",", alloc), alloc);
  signatureHelp.AddMember("triggerCharacters", sigTriggers, alloc);
  signatureHelp.AddMember("retriggerCharacters", sigRetriggers, alloc);
  capabilities.AddMember("signatureHelpProvider", signatureHelp, alloc);

  capabilities.AddMember("documentHighlightProvider", true, alloc);
  capabilities.AddMember("documentFormattingProvider", true, alloc);
  capabilities.AddMember("documentRangeFormattingProvider", true, alloc);
  capabilities.AddMember("foldingRangeProvider", true, alloc);
  capabilities.AddMember("selectionRangeProvider", true, alloc);
  capabilities.AddMember("codeActionProvider", true, alloc);

  rapidjson::Value semanticProvider(rapidjson::kObjectType);
  rapidjson::Value legend(rapidjson::kObjectType);
  rapidjson::Value tokenTypes(rapidjson::kArrayType);
  tokenTypes.PushBack(rapidjson::Value("namespace", alloc), alloc);
  tokenTypes.PushBack(rapidjson::Value("class", alloc), alloc);
  tokenTypes.PushBack(rapidjson::Value("function", alloc), alloc);
  tokenTypes.PushBack(rapidjson::Value("variable", alloc), alloc);
  tokenTypes.PushBack(rapidjson::Value("keyword", alloc), alloc);
  rapidjson::Value tokenModifiers(rapidjson::kArrayType);
  legend.AddMember("tokenTypes", tokenTypes, alloc);
  legend.AddMember("tokenModifiers", tokenModifiers, alloc);

  semanticProvider.AddMember("legend", legend, alloc);
  semanticProvider.AddMember("range", true, alloc);
  rapidjson::Value full(rapidjson::kObjectType);
  full.AddMember("delta", true, alloc);
  semanticProvider.AddMember("full", full, alloc);
  capabilities.AddMember("semanticTokensProvider", semanticProvider, alloc);

  rapidjson::Value executeCommandProvider(rapidjson::kObjectType);
  rapidjson::Value commands(rapidjson::kArrayType);
  executeCommandProvider.AddMember("commands", commands, alloc);
  capabilities.AddMember("executeCommandProvider", executeCommandProvider, alloc);

  rapidjson::Value serverInfo(rapidjson::kObjectType);
  serverInfo.AddMember("name", rapidjson::Value("Starbytes LSP", alloc), alloc);
  serverInfo.AddMember("version", rapidjson::Value("0.6.0", alloc), alloc);

  rapidjson::Value result(rapidjson::kObjectType);
  result.AddMember("capabilities", capabilities, alloc);
  result.AddMember("serverInfo", serverInfo, alloc);
  writeResult(request["id"], result);
}

void Server::handleInitialized(rapidjson::Document &request) {
  (void)request;
}

void Server::handleShutdown(rapidjson::Document &request) {
  if (!request.HasMember("id")) {
    return;
  }
  shutdownRequested = true;
  rapidjson::Document payloadDoc(rapidjson::kObjectType);
  rapidjson::Value result;
  result.SetNull();
  writeResult(request["id"], result);
}

void Server::handleCancelRequest(rapidjson::Document &request) {
  if (!request.HasMember("params") || !request["params"].IsObject()) {
    return;
  }
  auto &params = request["params"];
  if (!params.HasMember("id")) {
    return;
  }
  canceledRequestIds.insert(idToKey(params["id"]));
}

void Server::handleSetTrace(rapidjson::Document &request) {
  if (!request.HasMember("params") || !request["params"].IsObject()) {
    return;
  }
  auto &params = request["params"];
  if (params.HasMember("value") && params["value"].IsString()) {
    traceLevel = params["value"].GetString();
  }
}

void Server::handleLogTrace(rapidjson::Document &request) {
  (void)request;
}

void Server::handleWorkDoneProgressCreate(rapidjson::Document &request) {
  if (!request.HasMember("id")) {
    return;
  }
  rapidjson::Value result;
  result.SetNull();
  writeResult(request["id"], result);
}

void Server::handleDidOpen(rapidjson::Document &request) {
  if (!request.HasMember("params") || !request["params"].IsObject()) {
    return;
  }
  auto &params = request["params"];
  if (!params.HasMember("textDocument") || !params["textDocument"].IsObject()) {
    return;
  }
  auto &textDoc = params["textDocument"];
  if (!textDoc.HasMember("uri") || !textDoc["uri"].IsString() || !textDoc.HasMember("text") ||
      !textDoc["text"].IsString()) {
    return;
  }

  int version = 0;
  if (textDoc.HasMember("version") && textDoc["version"].IsInt()) {
    version = textDoc["version"].GetInt();
  }
  auto uri = std::string(textDoc["uri"].GetString());
  setDocumentTextByUri(uri, textDoc["text"].GetString(), version, true);
  publishDiagnostics(uri);
}

void Server::handleDidChange(rapidjson::Document &request) {
  if (!request.HasMember("params") || !request["params"].IsObject()) {
    return;
  }
  auto &params = request["params"];
  if (!params.HasMember("textDocument") || !params["textDocument"].IsObject() || !params.HasMember("contentChanges") ||
      !params["contentChanges"].IsArray()) {
    return;
  }

  auto &textDoc = params["textDocument"];
  auto &changes = params["contentChanges"];
  if (!textDoc.HasMember("uri") || !textDoc["uri"].IsString()) {
    return;
  }

  auto uri = std::string(textDoc["uri"].GetString());
  std::string text;
  if (!getDocumentTextByUri(uri, text)) {
    text.clear();
  }

  for (auto &change : changes.GetArray()) {
    maybeApplyIncrementalChange(text, change);
  }

  int version = 0;
  if (textDoc.HasMember("version") && textDoc["version"].IsInt()) {
    version = textDoc["version"].GetInt();
  }
  setDocumentTextByUri(uri, text, version, true);
  publishDiagnostics(uri);
}

void Server::handleDidSave(rapidjson::Document &request) {
  if (!request.HasMember("params") || !request["params"].IsObject()) {
    return;
  }
  auto &params = request["params"];
  if (!params.HasMember("textDocument") || !params["textDocument"].IsObject() ||
      !params["textDocument"].HasMember("uri") || !params["textDocument"]["uri"].IsString()) {
    return;
  }

  auto uri = std::string(params["textDocument"]["uri"].GetString());
  if (params.HasMember("text") && params["text"].IsString()) {
    auto it = documents.find(uri);
    int version = (it != documents.end()) ? it->second.version : 0;
    setDocumentTextByUri(uri, params["text"].GetString(), version, true);
  }
  publishDiagnostics(uri);
}

void Server::handleDidClose(rapidjson::Document &request) {
  if (!request.HasMember("params") || !request["params"].IsObject()) {
    return;
  }
  auto &params = request["params"];
  if (!params.HasMember("textDocument") || !params["textDocument"].IsObject() ||
      !params["textDocument"].HasMember("uri") || !params["textDocument"]["uri"].IsString()) {
    return;
  }

  auto uri = std::string(params["textDocument"]["uri"].GetString());
  removeOpenDocumentByUri(uri);

  rapidjson::Document paramsDoc(rapidjson::kObjectType);
  auto &alloc = paramsDoc.GetAllocator();
  paramsDoc.AddMember("uri", rapidjson::Value(uri.c_str(), alloc), alloc);
  rapidjson::Value diagnostics(rapidjson::kArrayType);
  paramsDoc.AddMember("diagnostics", diagnostics, alloc);
  writeNotification("textDocument/publishDiagnostics", paramsDoc);
}

void Server::handleCompletion(rapidjson::Document &request) {
  if (!request.HasMember("id")) {
    return;
  }
  if (!request.HasMember("params") || !request["params"].IsObject()) {
    writeError(request["id"], JSONRPC_INVALID_PARAMS, "Missing completion params");
    return;
  }

  auto &params = request["params"];
  if (!params.HasMember("textDocument") || !params["textDocument"].IsObject() ||
      !params["textDocument"].HasMember("uri") || !params["textDocument"]["uri"].IsString()) {
    writeError(request["id"], JSONRPC_INVALID_PARAMS, "Missing textDocument.uri");
    return;
  }

  auto uri = std::string(params["textDocument"]["uri"].GetString());
  std::string text;
  getDocumentTextByUri(uri, text);

  unsigned line = 0;
  unsigned character = 0;
  if (params.HasMember("position") && params["position"].IsObject()) {
    auto &position = params["position"];
    if (position.HasMember("line") && position["line"].IsUint()) {
      line = position["line"].GetUint();
    }
    if (position.HasMember("character") && position["character"].IsUint()) {
      character = position["character"].GetUint();
    }
  }

  auto prefix = extractPrefixAtPosition(text, line, character);
  auto loweredPrefix = toLower(prefix);

  std::vector<CompletionEntry> entries;
  std::set<std::string> seen;
  auto pushEntry = [&](const std::string &label, int kind, const std::string &detail) {
    auto loweredLabel = toLower(label);
    if (!loweredPrefix.empty() && loweredLabel.rfind(loweredPrefix, 0) != 0) {
      return;
    }
    if (seen.insert(label).second) {
      entries.push_back({label, kind, detail});
    }
  };

  for (const auto &keyword : kKeywordCompletions) {
    pushEntry(keyword, COMPLETION_KIND_KEYWORD, "keyword");
  }

  for (const auto &doc : documents) {
    auto symbols = collectSymbolsFromText(doc.second.text);
    for (const auto &symbol : symbols) {
      int kind = COMPLETION_KIND_TEXT;
      if (symbol.kind == SYMBOL_KIND_CLASS) {
        kind = COMPLETION_KIND_CLASS;
      } else if (symbol.kind == SYMBOL_KIND_METHOD) {
        kind = COMPLETION_KIND_METHOD;
      } else if (symbol.kind == SYMBOL_KIND_FIELD) {
        kind = COMPLETION_KIND_FIELD;
      } else if (symbol.kind == SYMBOL_KIND_FUNCTION) {
        kind = COMPLETION_KIND_FUNCTION;
      } else if (symbol.kind == SYMBOL_KIND_NAMESPACE) {
        kind = COMPLETION_KIND_MODULE;
      } else if (symbol.kind == SYMBOL_KIND_INTERFACE) {
        kind = COMPLETION_KIND_INTERFACE;
      } else if (symbol.kind == SYMBOL_KIND_STRUCT) {
        kind = COMPLETION_KIND_STRUCT;
      } else if (symbol.kind == SYMBOL_KIND_ENUM) {
        kind = COMPLETION_KIND_ENUM;
      } else if (symbol.kind == SYMBOL_KIND_CONSTANT) {
        kind = COMPLETION_KIND_VALUE;
      } else {
        kind = COMPLETION_KIND_VARIABLE;
      }
      pushEntry(symbol.name, kind, symbol.detail);
    }
  }

  rapidjson::Document payloadDoc(rapidjson::kObjectType);
  auto &alloc = payloadDoc.GetAllocator();
  rapidjson::Value items(rapidjson::kArrayType);
  for (const auto &entry : entries) {
    rapidjson::Value item(rapidjson::kObjectType);
    item.AddMember("label", rapidjson::Value(entry.label.c_str(), alloc), alloc);
    item.AddMember("kind", entry.kind, alloc);
    item.AddMember("detail", rapidjson::Value(entry.detail.c_str(), alloc), alloc);
    item.AddMember("insertText", rapidjson::Value(entry.label.c_str(), alloc), alloc);
    items.PushBack(item, alloc);
  }

  rapidjson::Value result(rapidjson::kObjectType);
  result.AddMember("isIncomplete", false, alloc);
  result.AddMember("items", items, alloc);
  writeResult(request["id"], result);
}

void Server::handleCompletionResolve(rapidjson::Document &request) {
  if (!request.HasMember("id")) {
    return;
  }
  if (!request.HasMember("params") || !request["params"].IsObject()) {
    writeError(request["id"], JSONRPC_INVALID_PARAMS, "Missing completion item params");
    return;
  }

  rapidjson::Document payloadDoc(rapidjson::kObjectType);
  auto &alloc = payloadDoc.GetAllocator();
  rapidjson::Value item;
  item.CopyFrom(request["params"], alloc);
  if (!item.HasMember("documentation")) {
    item.AddMember("documentation", rapidjson::Value("Starbytes symbol", alloc), alloc);
  }
  writeResult(request["id"], item);
}

void Server::handleHover(rapidjson::Document &request) {
  if (!request.HasMember("id")) {
    return;
  }
  if (!request.HasMember("params") || !request["params"].IsObject()) {
    writeError(request["id"], JSONRPC_INVALID_PARAMS, "Missing hover params");
    return;
  }

  auto &params = request["params"];
  if (!params.HasMember("textDocument") || !params["textDocument"].IsObject() ||
      !params["textDocument"].HasMember("uri") || !params["textDocument"]["uri"].IsString() ||
      !params.HasMember("position") || !params["position"].IsObject() || !params["position"].HasMember("line") ||
      !params["position"].HasMember("character") || !params["position"]["line"].IsUint() ||
      !params["position"]["character"].IsUint()) {
    writeError(request["id"], JSONRPC_INVALID_PARAMS, "Invalid hover params");
    return;
  }

  auto uri = std::string(params["textDocument"]["uri"].GetString());
  std::string text;
  if (!getDocumentTextByUri(uri, text)) {
    rapidjson::Value nullResult;
    nullResult.SetNull();
    writeResult(request["id"], nullResult);
    return;
  }

  std::string word;
  size_t wordStart = 0;
  size_t wordEnd = 0;
  if (!wordRangeAtPosition(text, params["position"]["line"].GetUint(), params["position"]["character"].GetUint(), word,
                           wordStart, wordEnd)) {
    rapidjson::Value nullResult;
    nullResult.SetNull();
    writeResult(request["id"], nullResult);
    return;
  }

  std::optional<SymbolEntry> bestSymbol;
  std::string bestUri = uri;

  auto scanDefinitions = [&](const std::string &candidateUri, const std::string &candidateText) {
    auto symbols = collectSymbolsFromText(candidateText);
    for (const auto &symbol : symbols) {
      if (symbol.name != word) {
        continue;
      }
      if (!bestSymbol.has_value() || candidateUri == uri) {
        bestSymbol = symbol;
        bestUri = candidateUri;
      }
    }
  };

  for (const auto &doc : documents) {
    scanDefinitions(doc.first, doc.second.text);
  }

  rapidjson::Document payloadDoc(rapidjson::kObjectType);
  auto &alloc = payloadDoc.GetAllocator();

  rapidjson::Value result(rapidjson::kObjectType);
  rapidjson::Value contents(rapidjson::kObjectType);
  contents.AddMember("kind", rapidjson::Value("markdown", alloc), alloc);

  std::string markdown = "**" + word + "**";
  if (bestSymbol.has_value()) {
    if (!bestSymbol->signature.empty()) {
      markdown = "```starbytes\n" + bestSymbol->signature + "\n```";
    } else {
      markdown = "**" + bestSymbol->name + "**";
    }
    if (!bestSymbol->detail.empty()) {
      markdown += "\n\n_" + bestSymbol->detail + "_";
    }
    if (!bestSymbol->documentation.empty()) {
      markdown += "\n\n" + bestSymbol->documentation;
    }
    markdown += "\n\nDeclared in `" + bestUri + "`";
  }
  contents.AddMember("value", rapidjson::Value(markdown.c_str(), alloc), alloc);
  result.AddMember("contents", contents, alloc);

  unsigned startLine = 0;
  unsigned startCharacter = 0;
  unsigned endLine = 0;
  unsigned endCharacter = 0;
  positionFromOffset(text, wordStart, startLine, startCharacter);
  positionFromOffset(text, wordEnd, endLine, endCharacter);

  rapidjson::Value range(rapidjson::kObjectType);
  rapidjson::Value start(rapidjson::kObjectType);
  rapidjson::Value end(rapidjson::kObjectType);
  start.AddMember("line", startLine, alloc);
  start.AddMember("character", startCharacter, alloc);
  end.AddMember("line", endLine, alloc);
  end.AddMember("character", endCharacter, alloc);
  range.AddMember("start", start, alloc);
  range.AddMember("end", end, alloc);
  result.AddMember("range", range, alloc);

  writeResult(request["id"], result);
}

void Server::handleDefinition(rapidjson::Document &request) {
  if (!request.HasMember("id")) {
    return;
  }
  if (!request.HasMember("params") || !request["params"].IsObject()) {
    writeError(request["id"], JSONRPC_INVALID_PARAMS, "Missing definition params");
    return;
  }

  auto &params = request["params"];
  if (!params.HasMember("textDocument") || !params["textDocument"].IsObject() ||
      !params["textDocument"].HasMember("uri") || !params["textDocument"]["uri"].IsString() ||
      !params.HasMember("position") || !params["position"].IsObject() || !params["position"].HasMember("line") ||
      !params["position"].HasMember("character") || !params["position"]["line"].IsUint() ||
      !params["position"]["character"].IsUint()) {
    writeError(request["id"], JSONRPC_INVALID_PARAMS, "Invalid definition params");
    return;
  }

  auto uri = std::string(params["textDocument"]["uri"].GetString());
  std::string text;
  if (!getDocumentTextByUri(uri, text)) {
    rapidjson::Value locations(rapidjson::kArrayType);
    writeResult(request["id"], locations);
    return;
  }

  std::string word;
  size_t start = 0;
  size_t end = 0;
  if (!wordRangeAtPosition(text, params["position"]["line"].GetUint(), params["position"]["character"].GetUint(), word,
                           start, end)) {
    rapidjson::Value locations(rapidjson::kArrayType);
    writeResult(request["id"], locations);
    return;
  }

  rapidjson::Document payloadDoc(rapidjson::kObjectType);
  auto &alloc = payloadDoc.GetAllocator();
  rapidjson::Value locations(rapidjson::kArrayType);

  for (const auto &doc : documents) {
    auto symbols = collectSymbolsFromText(doc.second.text);
    for (const auto &symbol : symbols) {
      if (symbol.name != word) {
        continue;
      }

      rapidjson::Value location(rapidjson::kObjectType);
      location.AddMember("uri", rapidjson::Value(doc.first.c_str(), alloc), alloc);

      rapidjson::Value range(rapidjson::kObjectType);
      rapidjson::Value rangeStart(rapidjson::kObjectType);
      rapidjson::Value rangeEnd(rapidjson::kObjectType);
      rangeStart.AddMember("line", symbol.line, alloc);
      rangeStart.AddMember("character", symbol.start, alloc);
      rangeEnd.AddMember("line", symbol.line, alloc);
      rangeEnd.AddMember("character", symbol.start + symbol.length, alloc);
      range.AddMember("start", rangeStart, alloc);
      range.AddMember("end", rangeEnd, alloc);
      location.AddMember("range", range, alloc);

      locations.PushBack(location, alloc);
    }
  }

  writeResult(request["id"], locations);
}

void Server::handleDeclaration(rapidjson::Document &request) {
  handleDefinition(request);
}

void Server::handleTypeDefinition(rapidjson::Document &request) {
  handleDefinition(request);
}

void Server::handleImplementation(rapidjson::Document &request) {
  handleDefinition(request);
}

void Server::handleReferences(rapidjson::Document &request) {
  if (!request.HasMember("id")) {
    return;
  }
  if (!request.HasMember("params") || !request["params"].IsObject()) {
    writeError(request["id"], JSONRPC_INVALID_PARAMS, "Missing references params");
    return;
  }

  auto &params = request["params"];
  if (!params.HasMember("textDocument") || !params["textDocument"].IsObject() ||
      !params["textDocument"].HasMember("uri") || !params["textDocument"]["uri"].IsString() ||
      !params.HasMember("position") || !params["position"].IsObject() || !params["position"].HasMember("line") ||
      !params["position"].HasMember("character") || !params["position"]["line"].IsUint() ||
      !params["position"]["character"].IsUint()) {
    writeError(request["id"], JSONRPC_INVALID_PARAMS, "Invalid references params");
    return;
  }

  auto uri = std::string(params["textDocument"]["uri"].GetString());
  std::string text;
  if (!getDocumentTextByUri(uri, text)) {
    rapidjson::Value refs(rapidjson::kArrayType);
    writeResult(request["id"], refs);
    return;
  }

  bool includeDeclaration = true;
  if (params.HasMember("context") && params["context"].IsObject() && params["context"].HasMember("includeDeclaration") &&
      params["context"]["includeDeclaration"].IsBool()) {
    includeDeclaration = params["context"]["includeDeclaration"].GetBool();
  }

  std::string word;
  size_t wordStart = 0;
  size_t wordEnd = 0;
  if (!wordRangeAtPosition(text, params["position"]["line"].GetUint(), params["position"]["character"].GetUint(), word,
                           wordStart, wordEnd)) {
    rapidjson::Value refs(rapidjson::kArrayType);
    writeResult(request["id"], refs);
    return;
  }

  rapidjson::Document payloadDoc(rapidjson::kObjectType);
  auto &alloc = payloadDoc.GetAllocator();
  rapidjson::Value refs(rapidjson::kArrayType);

  for (const auto &doc : documents) {
    std::vector<size_t> occurrences;
    appendOccurrences(doc.second.text, word, occurrences);

    std::set<size_t> declarationOffsets;
    if (!includeDeclaration) {
      auto symbols = collectSymbolsFromText(doc.second.text);
      for (const auto &symbol : symbols) {
        if (symbol.name != word) {
          continue;
        }
        declarationOffsets.insert(offsetFromPosition(doc.second.text, symbol.line, symbol.start));
      }
    }

    for (size_t offset : occurrences) {
      if (!includeDeclaration && declarationOffsets.find(offset) != declarationOffsets.end()) {
        continue;
      }

      unsigned startLine = 0;
      unsigned startChar = 0;
      unsigned endLine = 0;
      unsigned endChar = 0;
      positionFromOffset(doc.second.text, offset, startLine, startChar);
      positionFromOffset(doc.second.text, offset + word.size(), endLine, endChar);

      rapidjson::Value location(rapidjson::kObjectType);
      location.AddMember("uri", rapidjson::Value(doc.first.c_str(), alloc), alloc);

      rapidjson::Value range(rapidjson::kObjectType);
      rapidjson::Value rangeStart(rapidjson::kObjectType);
      rapidjson::Value rangeEnd(rapidjson::kObjectType);
      rangeStart.AddMember("line", startLine, alloc);
      rangeStart.AddMember("character", startChar, alloc);
      rangeEnd.AddMember("line", endLine, alloc);
      rangeEnd.AddMember("character", endChar, alloc);
      range.AddMember("start", rangeStart, alloc);
      range.AddMember("end", rangeEnd, alloc);
      location.AddMember("range", range, alloc);
      refs.PushBack(location, alloc);
    }
  }

  writeResult(request["id"], refs);
}

void Server::handleDocumentSymbol(rapidjson::Document &request) {
  if (!request.HasMember("id")) {
    return;
  }
  if (!request.HasMember("params") || !request["params"].IsObject() || !request["params"].HasMember("textDocument") ||
      !request["params"]["textDocument"].IsObject() || !request["params"]["textDocument"].HasMember("uri") ||
      !request["params"]["textDocument"]["uri"].IsString()) {
    writeError(request["id"], JSONRPC_INVALID_PARAMS, "Invalid document symbol params");
    return;
  }

  auto uri = std::string(request["params"]["textDocument"]["uri"].GetString());
  std::string text;
  getDocumentTextByUri(uri, text);
  auto symbols = collectSymbolsFromText(text);

  rapidjson::Document payloadDoc(rapidjson::kObjectType);
  auto &alloc = payloadDoc.GetAllocator();
  rapidjson::Value result(rapidjson::kArrayType);

  for (const auto &symbol : symbols) {
    rapidjson::Value docSymbol(rapidjson::kObjectType);
    docSymbol.AddMember("name", rapidjson::Value(symbol.name.c_str(), alloc), alloc);
    docSymbol.AddMember("kind", symbol.kind, alloc);
    docSymbol.AddMember("detail", rapidjson::Value(symbol.detail.c_str(), alloc), alloc);

    rapidjson::Value range(rapidjson::kObjectType);
    rapidjson::Value start(rapidjson::kObjectType);
    rapidjson::Value end(rapidjson::kObjectType);
    start.AddMember("line", symbol.line, alloc);
    start.AddMember("character", symbol.start, alloc);
    end.AddMember("line", symbol.line, alloc);
    end.AddMember("character", symbol.start + symbol.length, alloc);
    range.AddMember("start", start, alloc);
    range.AddMember("end", end, alloc);

    rapidjson::Value selectionRange(rapidjson::kObjectType);
    rapidjson::Value selStart(rapidjson::kObjectType);
    rapidjson::Value selEnd(rapidjson::kObjectType);
    selStart.AddMember("line", symbol.line, alloc);
    selStart.AddMember("character", symbol.start, alloc);
    selEnd.AddMember("line", symbol.line, alloc);
    selEnd.AddMember("character", symbol.start + symbol.length, alloc);
    selectionRange.AddMember("start", selStart, alloc);
    selectionRange.AddMember("end", selEnd, alloc);

    docSymbol.AddMember("range", range, alloc);
    docSymbol.AddMember("selectionRange", selectionRange, alloc);

    rapidjson::Value children(rapidjson::kArrayType);
    docSymbol.AddMember("children", children, alloc);
    result.PushBack(docSymbol, alloc);
  }

  writeResult(request["id"], result);
}

void Server::handleWorkspaceSymbol(rapidjson::Document &request) {
  if (!request.HasMember("id")) {
    return;
  }
  if (!request.HasMember("params") || !request["params"].IsObject()) {
    writeError(request["id"], JSONRPC_INVALID_PARAMS, "Invalid workspace symbol params");
    return;
  }

  std::string query;
  if (request["params"].HasMember("query") && request["params"]["query"].IsString()) {
    query = request["params"]["query"].GetString();
  }
  auto loweredQuery = toLower(query);

  rapidjson::Document payloadDoc(rapidjson::kObjectType);
  auto &alloc = payloadDoc.GetAllocator();
  rapidjson::Value result(rapidjson::kArrayType);

  for (const auto &doc : documents) {
    auto symbols = collectSymbolsFromText(doc.second.text);
    for (const auto &symbol : symbols) {
      auto loweredName = toLower(symbol.name);
      if (!loweredQuery.empty() && loweredName.find(loweredQuery) == std::string::npos) {
        continue;
      }

      rapidjson::Value symbolInfo(rapidjson::kObjectType);
      symbolInfo.AddMember("name", rapidjson::Value(symbol.name.c_str(), alloc), alloc);
      symbolInfo.AddMember("kind", symbol.kind, alloc);

      rapidjson::Value location(rapidjson::kObjectType);
      location.AddMember("uri", rapidjson::Value(doc.first.c_str(), alloc), alloc);

      rapidjson::Value range(rapidjson::kObjectType);
      rapidjson::Value start(rapidjson::kObjectType);
      rapidjson::Value end(rapidjson::kObjectType);
      start.AddMember("line", symbol.line, alloc);
      start.AddMember("character", symbol.start, alloc);
      end.AddMember("line", symbol.line, alloc);
      end.AddMember("character", symbol.start + symbol.length, alloc);
      range.AddMember("start", start, alloc);
      range.AddMember("end", end, alloc);
      location.AddMember("range", range, alloc);

      symbolInfo.AddMember("location", location, alloc);
      symbolInfo.AddMember("containerName", rapidjson::Value("", alloc), alloc);
      result.PushBack(symbolInfo, alloc);
    }
  }

  writeResult(request["id"], result);
}

void Server::handlePrepareRename(rapidjson::Document &request) {
  if (!request.HasMember("id")) {
    return;
  }
  if (!request.HasMember("params") || !request["params"].IsObject()) {
    writeError(request["id"], JSONRPC_INVALID_PARAMS, "Invalid prepare rename params");
    return;
  }

  auto &params = request["params"];
  if (!params.HasMember("textDocument") || !params["textDocument"].IsObject() ||
      !params["textDocument"].HasMember("uri") || !params["textDocument"]["uri"].IsString() ||
      !params.HasMember("position") || !params["position"].IsObject() || !params["position"].HasMember("line") ||
      !params["position"].HasMember("character") || !params["position"]["line"].IsUint() ||
      !params["position"]["character"].IsUint()) {
    writeError(request["id"], JSONRPC_INVALID_PARAMS, "Invalid prepare rename params");
    return;
  }

  auto uri = std::string(params["textDocument"]["uri"].GetString());
  std::string text;
  if (!getDocumentTextByUri(uri, text)) {
    rapidjson::Value nullResult;
    nullResult.SetNull();
    writeResult(request["id"], nullResult);
    return;
  }

  std::string word;
  size_t startOffset = 0;
  size_t endOffset = 0;
  if (!wordRangeAtPosition(text, params["position"]["line"].GetUint(), params["position"]["character"].GetUint(), word,
                           startOffset, endOffset)) {
    rapidjson::Value nullResult;
    nullResult.SetNull();
    writeResult(request["id"], nullResult);
    return;
  }

  rapidjson::Document payloadDoc(rapidjson::kObjectType);
  auto &alloc = payloadDoc.GetAllocator();
  rapidjson::Value result(rapidjson::kObjectType);

  unsigned startLine = 0;
  unsigned startChar = 0;
  unsigned endLine = 0;
  unsigned endChar = 0;
  positionFromOffset(text, startOffset, startLine, startChar);
  positionFromOffset(text, endOffset, endLine, endChar);

  rapidjson::Value range(rapidjson::kObjectType);
  rapidjson::Value rangeStart(rapidjson::kObjectType);
  rapidjson::Value rangeEnd(rapidjson::kObjectType);
  rangeStart.AddMember("line", startLine, alloc);
  rangeStart.AddMember("character", startChar, alloc);
  rangeEnd.AddMember("line", endLine, alloc);
  rangeEnd.AddMember("character", endChar, alloc);
  range.AddMember("start", rangeStart, alloc);
  range.AddMember("end", rangeEnd, alloc);

  result.AddMember("range", range, alloc);
  result.AddMember("placeholder", rapidjson::Value(word.c_str(), alloc), alloc);
  writeResult(request["id"], result);
}

void Server::handleRename(rapidjson::Document &request) {
  if (!request.HasMember("id")) {
    return;
  }
  if (!request.HasMember("params") || !request["params"].IsObject()) {
    writeError(request["id"], JSONRPC_INVALID_PARAMS, "Invalid rename params");
    return;
  }

  auto &params = request["params"];
  if (!params.HasMember("textDocument") || !params["textDocument"].IsObject() ||
      !params["textDocument"].HasMember("uri") || !params["textDocument"]["uri"].IsString() ||
      !params.HasMember("position") || !params["position"].IsObject() || !params["position"].HasMember("line") ||
      !params["position"].HasMember("character") || !params["position"]["line"].IsUint() ||
      !params["position"]["character"].IsUint() || !params.HasMember("newName") || !params["newName"].IsString()) {
    writeError(request["id"], JSONRPC_INVALID_PARAMS, "Invalid rename params");
    return;
  }

  auto newName = std::string(params["newName"].GetString());
  if (newName.empty() || !isIdentifierStart(newName[0]) ||
      std::any_of(newName.begin(), newName.end(), [](char c) { return !Server::isIdentifierChar(c); })) {
    writeError(request["id"], JSONRPC_INVALID_PARAMS, "newName must be a valid identifier");
    return;
  }

  auto uri = std::string(params["textDocument"]["uri"].GetString());
  std::string text;
  if (!getDocumentTextByUri(uri, text)) {
    rapidjson::Value nullResult;
    nullResult.SetNull();
    writeResult(request["id"], nullResult);
    return;
  }

  std::string oldName;
  size_t startOffset = 0;
  size_t endOffset = 0;
  if (!wordRangeAtPosition(text, params["position"]["line"].GetUint(), params["position"]["character"].GetUint(), oldName,
                           startOffset, endOffset)) {
    rapidjson::Value nullResult;
    nullResult.SetNull();
    writeResult(request["id"], nullResult);
    return;
  }

  rapidjson::Document payloadDoc(rapidjson::kObjectType);
  auto &alloc = payloadDoc.GetAllocator();

  rapidjson::Value changes(rapidjson::kObjectType);

  for (const auto &doc : documents) {
    std::vector<size_t> offsets;
    appendOccurrences(doc.second.text, oldName, offsets);
    if (offsets.empty()) {
      continue;
    }

    rapidjson::Value edits(rapidjson::kArrayType);
    for (size_t occurrence : offsets) {
      unsigned sLine = 0;
      unsigned sChar = 0;
      unsigned eLine = 0;
      unsigned eChar = 0;
      positionFromOffset(doc.second.text, occurrence, sLine, sChar);
      positionFromOffset(doc.second.text, occurrence + oldName.size(), eLine, eChar);

      rapidjson::Value edit(rapidjson::kObjectType);
      rapidjson::Value range(rapidjson::kObjectType);
      rapidjson::Value rs(rapidjson::kObjectType);
      rapidjson::Value re(rapidjson::kObjectType);
      rs.AddMember("line", sLine, alloc);
      rs.AddMember("character", sChar, alloc);
      re.AddMember("line", eLine, alloc);
      re.AddMember("character", eChar, alloc);
      range.AddMember("start", rs, alloc);
      range.AddMember("end", re, alloc);
      edit.AddMember("range", range, alloc);
      edit.AddMember("newText", rapidjson::Value(newName.c_str(), alloc), alloc);
      edits.PushBack(edit, alloc);
    }

    changes.AddMember(rapidjson::Value(doc.first.c_str(), alloc), edits, alloc);
  }

  rapidjson::Value result(rapidjson::kObjectType);
  result.AddMember("changes", changes, alloc);
  writeResult(request["id"], result);
}

void Server::handleSignatureHelp(rapidjson::Document &request) {
  if (!request.HasMember("id")) {
    return;
  }
  if (!request.HasMember("params") || !request["params"].IsObject()) {
    writeError(request["id"], JSONRPC_INVALID_PARAMS, "Invalid signature help params");
    return;
  }

  auto &params = request["params"];
  if (!params.HasMember("textDocument") || !params["textDocument"].IsObject() ||
      !params["textDocument"].HasMember("uri") || !params["textDocument"]["uri"].IsString() ||
      !params.HasMember("position") || !params["position"].IsObject() || !params["position"].HasMember("line") ||
      !params["position"].HasMember("character") || !params["position"]["line"].IsUint() ||
      !params["position"]["character"].IsUint()) {
    writeError(request["id"], JSONRPC_INVALID_PARAMS, "Invalid signature help params");
    return;
  }

  auto uri = std::string(params["textDocument"]["uri"].GetString());
  std::string text;
  if (!getDocumentTextByUri(uri, text)) {
    rapidjson::Value nullResult;
    nullResult.SetNull();
    writeResult(request["id"], nullResult);
    return;
  }

  auto cursor = offsetFromPosition(text, params["position"]["line"].GetUint(), params["position"]["character"].GetUint());
  cursor = std::min(cursor, text.size());

  size_t openParen = text.rfind('(', cursor == 0 ? 0 : cursor - 1);
  if (openParen == std::string::npos) {
    rapidjson::Value nullResult;
    nullResult.SetNull();
    writeResult(request["id"], nullResult);
    return;
  }

  size_t nameEnd = openParen;
  while (nameEnd > 0 && std::isspace(static_cast<unsigned char>(text[nameEnd - 1])) != 0) {
    --nameEnd;
  }
  size_t nameStart = nameEnd;
  while (nameStart > 0 && isIdentifierChar(text[nameStart - 1])) {
    --nameStart;
  }
  if (nameStart >= nameEnd || !isIdentifierStart(text[nameStart])) {
    rapidjson::Value nullResult;
    nullResult.SetNull();
    writeResult(request["id"], nullResult);
    return;
  }

  auto functionName = text.substr(nameStart, nameEnd - nameStart);

  std::string signatureParams;
  std::string signatureReturn;
  bool found = false;
  for (const auto &doc : documents) {
    for (std::sregex_iterator it(doc.second.text.begin(), doc.second.text.end(), kFuncSignatureRegex), end; it != end; ++it) {
      if ((*it)[1].str() != functionName) {
        continue;
      }
      signatureParams = (*it)[2].str();
      signatureReturn = (*it)[3].matched ? (*it)[3].str() : "";
      found = true;
      break;
    }
    if (found) {
      break;
    }
  }

  if (!found) {
    rapidjson::Value nullResult;
    nullResult.SetNull();
    writeResult(request["id"], nullResult);
    return;
  }

  unsigned activeParam = 0;
  for (size_t i = openParen + 1; i < cursor && i < text.size(); ++i) {
    if (text[i] == ',') {
      ++activeParam;
    }
  }

  rapidjson::Document payloadDoc(rapidjson::kObjectType);
  auto &alloc = payloadDoc.GetAllocator();

  rapidjson::Value signatures(rapidjson::kArrayType);
  rapidjson::Value signature(rapidjson::kObjectType);

  std::string label = functionName + "(" + signatureParams + ")";
  if (!signatureReturn.empty()) {
    label += " " + signatureReturn;
  }
  signature.AddMember("label", rapidjson::Value(label.c_str(), alloc), alloc);

  rapidjson::Value parameters(rapidjson::kArrayType);
  std::istringstream paramStream(signatureParams);
  std::string param;
  while (std::getline(paramStream, param, ',')) {
    auto trimmed = trim(param);
    if (trimmed.empty()) {
      continue;
    }
    rapidjson::Value paramObj(rapidjson::kObjectType);
    paramObj.AddMember("label", rapidjson::Value(trimmed.c_str(), alloc), alloc);
    parameters.PushBack(paramObj, alloc);
  }
  signature.AddMember("parameters", parameters, alloc);
  signatures.PushBack(signature, alloc);

  rapidjson::Value result(rapidjson::kObjectType);
  result.AddMember("signatures", signatures, alloc);
  result.AddMember("activeSignature", 0, alloc);
  result.AddMember("activeParameter", activeParam, alloc);

  writeResult(request["id"], result);
}

void Server::handleDocumentHighlight(rapidjson::Document &request) {
  if (!request.HasMember("id")) {
    return;
  }
  if (!request.HasMember("params") || !request["params"].IsObject()) {
    writeError(request["id"], JSONRPC_INVALID_PARAMS, "Invalid document highlight params");
    return;
  }

  auto &params = request["params"];
  if (!params.HasMember("textDocument") || !params["textDocument"].IsObject() ||
      !params["textDocument"].HasMember("uri") || !params["textDocument"]["uri"].IsString() ||
      !params.HasMember("position") || !params["position"].IsObject() || !params["position"].HasMember("line") ||
      !params["position"].HasMember("character") || !params["position"]["line"].IsUint() ||
      !params["position"]["character"].IsUint()) {
    writeError(request["id"], JSONRPC_INVALID_PARAMS, "Invalid document highlight params");
    return;
  }

  auto uri = std::string(params["textDocument"]["uri"].GetString());
  std::string text;
  if (!getDocumentTextByUri(uri, text)) {
    rapidjson::Value highlights(rapidjson::kArrayType);
    writeResult(request["id"], highlights);
    return;
  }

  std::string word;
  size_t wordStart = 0;
  size_t wordEnd = 0;
  if (!wordRangeAtPosition(text, params["position"]["line"].GetUint(), params["position"]["character"].GetUint(), word,
                           wordStart, wordEnd)) {
    rapidjson::Value highlights(rapidjson::kArrayType);
    writeResult(request["id"], highlights);
    return;
  }

  std::vector<size_t> offsets;
  appendOccurrences(text, word, offsets);

  rapidjson::Document payloadDoc(rapidjson::kObjectType);
  auto &alloc = payloadDoc.GetAllocator();
  rapidjson::Value highlights(rapidjson::kArrayType);

  for (size_t occurrence : offsets) {
    unsigned sLine = 0;
    unsigned sChar = 0;
    unsigned eLine = 0;
    unsigned eChar = 0;
    positionFromOffset(text, occurrence, sLine, sChar);
    positionFromOffset(text, occurrence + word.size(), eLine, eChar);

    rapidjson::Value highlight(rapidjson::kObjectType);
    rapidjson::Value range(rapidjson::kObjectType);
    rapidjson::Value start(rapidjson::kObjectType);
    rapidjson::Value end(rapidjson::kObjectType);
    start.AddMember("line", sLine, alloc);
    start.AddMember("character", sChar, alloc);
    end.AddMember("line", eLine, alloc);
    end.AddMember("character", eChar, alloc);
    range.AddMember("start", start, alloc);
    range.AddMember("end", end, alloc);
    highlight.AddMember("range", range, alloc);
    highlight.AddMember("kind", 1, alloc);
    highlights.PushBack(highlight, alloc);
  }

  writeResult(request["id"], highlights);
}

void Server::handleFormatting(rapidjson::Document &request) {
  if (!request.HasMember("id")) {
    return;
  }
  rapidjson::Value edits(rapidjson::kArrayType);
  writeResult(request["id"], edits);
}

void Server::handleRangeFormatting(rapidjson::Document &request) {
  if (!request.HasMember("id")) {
    return;
  }
  rapidjson::Value edits(rapidjson::kArrayType);
  writeResult(request["id"], edits);
}

void Server::handleFoldingRange(rapidjson::Document &request) {
  if (!request.HasMember("id")) {
    return;
  }
  if (!request.HasMember("params") || !request["params"].IsObject() || !request["params"].HasMember("textDocument") ||
      !request["params"]["textDocument"].IsObject() || !request["params"]["textDocument"].HasMember("uri") ||
      !request["params"]["textDocument"]["uri"].IsString()) {
    writeError(request["id"], JSONRPC_INVALID_PARAMS, "Invalid folding range params");
    return;
  }

  auto uri = std::string(request["params"]["textDocument"]["uri"].GetString());
  std::string text;
  getDocumentTextByUri(uri, text);

  std::vector<unsigned> stack;
  std::vector<std::pair<unsigned, unsigned>> ranges;
  unsigned line = 0;
  for (char ch : text) {
    if (ch == '{') {
      stack.push_back(line);
    } else if (ch == '}') {
      if (!stack.empty()) {
        auto startLine = stack.back();
        stack.pop_back();
        if (line > startLine) {
          ranges.emplace_back(startLine, line);
        }
      }
    }

    if (ch == '\n') {
      ++line;
    }
  }

  rapidjson::Document payloadDoc(rapidjson::kObjectType);
  auto &alloc = payloadDoc.GetAllocator();
  rapidjson::Value result(rapidjson::kArrayType);
  for (const auto &rangePair : ranges) {
    rapidjson::Value item(rapidjson::kObjectType);
    item.AddMember("startLine", rangePair.first, alloc);
    item.AddMember("startCharacter", 0, alloc);
    item.AddMember("endLine", rangePair.second, alloc);
    item.AddMember("endCharacter", 0, alloc);
    item.AddMember("kind", rapidjson::Value("region", alloc), alloc);
    result.PushBack(item, alloc);
  }

  writeResult(request["id"], result);
}

void Server::handleSelectionRange(rapidjson::Document &request) {
  if (!request.HasMember("id")) {
    return;
  }
  if (!request.HasMember("params") || !request["params"].IsObject() || !request["params"].HasMember("textDocument") ||
      !request["params"]["textDocument"].IsObject() || !request["params"]["textDocument"].HasMember("uri") ||
      !request["params"]["textDocument"]["uri"].IsString() || !request["params"].HasMember("positions") ||
      !request["params"]["positions"].IsArray()) {
    writeError(request["id"], JSONRPC_INVALID_PARAMS, "Invalid selection range params");
    return;
  }

  auto uri = std::string(request["params"]["textDocument"]["uri"].GetString());
  std::string text;
  getDocumentTextByUri(uri, text);

  rapidjson::Document payloadDoc(rapidjson::kObjectType);
  auto &alloc = payloadDoc.GetAllocator();
  rapidjson::Value result(rapidjson::kArrayType);

  for (auto &position : request["params"]["positions"].GetArray()) {
    if (!position.IsObject() || !position.HasMember("line") || !position.HasMember("character") ||
        !position["line"].IsUint() || !position["character"].IsUint()) {
      continue;
    }

    unsigned line = position["line"].GetUint();
    unsigned character = position["character"].GetUint();

    size_t lineStartOffset = offsetFromPosition(text, line, 0);
    size_t lineEndOffset = lineStartOffset;
    while (lineEndOffset < text.size() && text[lineEndOffset] != '\n') {
      ++lineEndOffset;
    }
    unsigned lineEndChar = static_cast<unsigned>(lineEndOffset - lineStartOffset);

    rapidjson::Value lineRange(rapidjson::kObjectType);
    rapidjson::Value ls(rapidjson::kObjectType);
    rapidjson::Value le(rapidjson::kObjectType);
    ls.AddMember("line", line, alloc);
    ls.AddMember("character", 0, alloc);
    le.AddMember("line", line, alloc);
    le.AddMember("character", lineEndChar, alloc);
    lineRange.AddMember("start", ls, alloc);
    lineRange.AddMember("end", le, alloc);

    std::string word;
    size_t wordStart = 0;
    size_t wordEnd = 0;

    if (!wordRangeAtPosition(text, line, character, word, wordStart, wordEnd)) {
      rapidjson::Value rangeObj(rapidjson::kObjectType);
      rangeObj.AddMember("range", lineRange, alloc);
      result.PushBack(rangeObj, alloc);
      continue;
    }

    unsigned wsLine = 0;
    unsigned wsChar = 0;
    unsigned weLine = 0;
    unsigned weChar = 0;
    positionFromOffset(text, wordStart, wsLine, wsChar);
    positionFromOffset(text, wordEnd, weLine, weChar);

    rapidjson::Value wordRange(rapidjson::kObjectType);
    rapidjson::Value ws(rapidjson::kObjectType);
    rapidjson::Value we(rapidjson::kObjectType);
    ws.AddMember("line", wsLine, alloc);
    ws.AddMember("character", wsChar, alloc);
    we.AddMember("line", weLine, alloc);
    we.AddMember("character", weChar, alloc);
    wordRange.AddMember("start", ws, alloc);
    wordRange.AddMember("end", we, alloc);

    rapidjson::Value parent(rapidjson::kObjectType);
    parent.AddMember("range", lineRange, alloc);

    rapidjson::Value item(rapidjson::kObjectType);
    item.AddMember("range", wordRange, alloc);
    item.AddMember("parent", parent, alloc);
    result.PushBack(item, alloc);
  }

  writeResult(request["id"], result);
}

void Server::handleCodeAction(rapidjson::Document &request) {
  if (!request.HasMember("id")) {
    return;
  }
  rapidjson::Value actions(rapidjson::kArrayType);
  writeResult(request["id"], actions);
}

void Server::handleSemanticTokens(rapidjson::Document &request) {
  if (!request.HasMember("id")) {
    return;
  }
  if (!request.HasMember("params") || !request["params"].IsObject() || !request["params"].HasMember("textDocument") ||
      !request["params"]["textDocument"].IsObject() || !request["params"]["textDocument"].HasMember("uri") ||
      !request["params"]["textDocument"]["uri"].IsString()) {
    writeError(request["id"], JSONRPC_INVALID_PARAMS, "Missing semantic token params");
    return;
  }

  auto uri = std::string(request["params"]["textDocument"]["uri"].GetString());
  std::string text;
  getDocumentTextByUri(uri, text);

  auto entries = collectSemanticTokenEntries(text, 0, 0, false);
  auto encoded = encodeSemanticTokens(entries);

  rapidjson::Document payloadDoc(rapidjson::kObjectType);
  auto &alloc = payloadDoc.GetAllocator();

  rapidjson::Value data(rapidjson::kArrayType);
  for (auto item : encoded) {
    data.PushBack(item, alloc);
  }

  auto resultId = std::to_string(semanticResultCounter++);
  semanticSnapshots[uri] = {encoded, resultId};

  rapidjson::Value result(rapidjson::kObjectType);
  result.AddMember("data", data, alloc);
  result.AddMember("resultId", rapidjson::Value(resultId.c_str(), alloc), alloc);
  writeResult(request["id"], result);
}

void Server::handleSemanticTokensRange(rapidjson::Document &request) {
  if (!request.HasMember("id")) {
    return;
  }
  if (!request.HasMember("params") || !request["params"].IsObject() || !request["params"].HasMember("textDocument") ||
      !request["params"]["textDocument"].IsObject() || !request["params"]["textDocument"].HasMember("uri") ||
      !request["params"]["textDocument"]["uri"].IsString() || !request["params"].HasMember("range") ||
      !request["params"]["range"].IsObject() || !request["params"]["range"].HasMember("start") ||
      !request["params"]["range"].HasMember("end") || !request["params"]["range"]["start"].IsObject() ||
      !request["params"]["range"]["end"].IsObject() || !request["params"]["range"]["start"].HasMember("line") ||
      !request["params"]["range"]["end"].HasMember("line") || !request["params"]["range"]["start"]["line"].IsUint() ||
      !request["params"]["range"]["end"]["line"].IsUint()) {
    writeError(request["id"], JSONRPC_INVALID_PARAMS, "Invalid semantic token range params");
    return;
  }

  auto uri = std::string(request["params"]["textDocument"]["uri"].GetString());
  auto startLine = request["params"]["range"]["start"]["line"].GetUint();
  auto endLine = request["params"]["range"]["end"]["line"].GetUint();

  std::string text;
  getDocumentTextByUri(uri, text);
  auto entries = collectSemanticTokenEntries(text, startLine, endLine, true);
  auto encoded = encodeSemanticTokens(entries);

  rapidjson::Document payloadDoc(rapidjson::kObjectType);
  auto &alloc = payloadDoc.GetAllocator();
  rapidjson::Value result(rapidjson::kObjectType);
  rapidjson::Value data(rapidjson::kArrayType);
  for (auto item : encoded) {
    data.PushBack(item, alloc);
  }
  result.AddMember("data", data, alloc);
  writeResult(request["id"], result);
}

void Server::handleSemanticTokensDelta(rapidjson::Document &request) {
  if (!request.HasMember("id")) {
    return;
  }
  if (!request.HasMember("params") || !request["params"].IsObject() || !request["params"].HasMember("textDocument") ||
      !request["params"]["textDocument"].IsObject() || !request["params"]["textDocument"].HasMember("uri") ||
      !request["params"]["textDocument"]["uri"].IsString()) {
    writeError(request["id"], JSONRPC_INVALID_PARAMS, "Invalid semantic token delta params");
    return;
  }

  auto uri = std::string(request["params"]["textDocument"]["uri"].GetString());
  std::string previousResultId;
  if (request["params"].HasMember("previousResultId") && request["params"]["previousResultId"].IsString()) {
    previousResultId = request["params"]["previousResultId"].GetString();
  }

  std::string text;
  getDocumentTextByUri(uri, text);
  auto entries = collectSemanticTokenEntries(text, 0, 0, false);
  auto encoded = encodeSemanticTokens(entries);

  std::vector<unsigned> oldData;
  bool hasPrevious = false;
  auto oldIt = semanticSnapshots.find(uri);
  if (oldIt != semanticSnapshots.end() && oldIt->second.resultId == previousResultId) {
    hasPrevious = true;
    oldData = oldIt->second.data;
  }

  auto newResultId = std::to_string(semanticResultCounter++);
  semanticSnapshots[uri] = {encoded, newResultId};

  rapidjson::Document payloadDoc(rapidjson::kObjectType);
  auto &alloc = payloadDoc.GetAllocator();
  rapidjson::Value result(rapidjson::kObjectType);
  rapidjson::Value edits(rapidjson::kArrayType);

  if (!hasPrevious || oldData != encoded) {
    rapidjson::Value edit(rapidjson::kObjectType);
    edit.AddMember("start", 0, alloc);
    edit.AddMember("deleteCount", static_cast<unsigned>(oldData.size()), alloc);
    rapidjson::Value data(rapidjson::kArrayType);
    for (auto item : encoded) {
      data.PushBack(item, alloc);
    }
    edit.AddMember("data", data, alloc);
    edits.PushBack(edit, alloc);
  }

  result.AddMember("edits", edits, alloc);
  result.AddMember("resultId", rapidjson::Value(newResultId.c_str(), alloc), alloc);
  writeResult(request["id"], result);
}

void Server::processRequest(rapidjson::Document &request) {
  if (!request.HasMember("method") || !request["method"].IsString() || !request.HasMember("id")) {
    rapidjson::Value nullId;
    nullId.SetNull();
    writeError(nullId, JSONRPC_INVALID_REQUEST, "Invalid request");
    return;
  }

  if (canceledRequestIds.find(idToKey(request["id"])) != canceledRequestIds.end()) {
    writeError(request["id"], LSP_REQUEST_CANCELLED, "Request cancelled");
    return;
  }

  std::string method = request["method"].GetString();

  if (!initialized && method != "initialize" && method != "shutdown") {
    writeError(request["id"], LSP_SERVER_NOT_INITIALIZED, "Server not initialized");
    return;
  }

  if (method == "initialize") {
    handleInitialize(request);
    return;
  }
  if (method == "shutdown") {
    handleShutdown(request);
    return;
  }

  if (method == "textDocument/completion") {
    handleCompletion(request);
    return;
  }
  if (method == "completionItem/resolve") {
    handleCompletionResolve(request);
    return;
  }
  if (method == "textDocument/hover") {
    handleHover(request);
    return;
  }
  if (method == "textDocument/definition") {
    handleDefinition(request);
    return;
  }
  if (method == "textDocument/declaration") {
    handleDeclaration(request);
    return;
  }
  if (method == "textDocument/typeDefinition") {
    handleTypeDefinition(request);
    return;
  }
  if (method == "textDocument/implementation") {
    handleImplementation(request);
    return;
  }
  if (method == "textDocument/references") {
    handleReferences(request);
    return;
  }
  if (method == "textDocument/documentSymbol") {
    handleDocumentSymbol(request);
    return;
  }
  if (method == "workspace/symbol") {
    handleWorkspaceSymbol(request);
    return;
  }
  if (method == "textDocument/prepareRename") {
    handlePrepareRename(request);
    return;
  }
  if (method == "textDocument/rename") {
    handleRename(request);
    return;
  }
  if (method == "textDocument/signatureHelp") {
    handleSignatureHelp(request);
    return;
  }
  if (method == "textDocument/documentHighlight") {
    handleDocumentHighlight(request);
    return;
  }
  if (method == "textDocument/formatting") {
    handleFormatting(request);
    return;
  }
  if (method == "textDocument/rangeFormatting") {
    handleRangeFormatting(request);
    return;
  }
  if (method == "textDocument/foldingRange") {
    handleFoldingRange(request);
    return;
  }
  if (method == "textDocument/selectionRange") {
    handleSelectionRange(request);
    return;
  }
  if (method == "textDocument/codeAction") {
    handleCodeAction(request);
    return;
  }
  if (method == "textDocument/semanticTokens/full") {
    handleSemanticTokens(request);
    return;
  }
  if (method == "textDocument/semanticTokens/range") {
    handleSemanticTokensRange(request);
    return;
  }
  if (method == "textDocument/semanticTokens/full/delta") {
    handleSemanticTokensDelta(request);
    return;
  }
  if (method == "window/workDoneProgress/create") {
    handleWorkDoneProgressCreate(request);
    return;
  }

  if (method == "workspace/executeCommand") {
    rapidjson::Value result;
    result.SetNull();
    writeResult(request["id"], result);
    return;
  }

  if (method == "codeLens/resolve" || method == "documentLink/resolve") {
    if (request.HasMember("params")) {
      rapidjson::Document payloadDoc(rapidjson::kObjectType);
      auto &alloc = payloadDoc.GetAllocator();
      rapidjson::Value passthrough;
      passthrough.CopyFrom(request["params"], alloc);
      writeResult(request["id"], passthrough);
      return;
    }
  }

  if (method == "textDocument/diagnostic") {
    rapidjson::Document payloadDoc(rapidjson::kObjectType);
    auto &alloc = payloadDoc.GetAllocator();
    rapidjson::Value result(rapidjson::kObjectType);
    result.AddMember("kind", rapidjson::Value("full", alloc), alloc);
    rapidjson::Value items(rapidjson::kArrayType);
    result.AddMember("items", items, alloc);
    writeResult(request["id"], result);
    return;
  }

  if (method == "workspace/diagnostic") {
    rapidjson::Document payloadDoc(rapidjson::kObjectType);
    auto &alloc = payloadDoc.GetAllocator();
    rapidjson::Value result(rapidjson::kObjectType);
    rapidjson::Value items(rapidjson::kArrayType);
    result.AddMember("items", items, alloc);
    writeResult(request["id"], result);
    return;
  }

  if (method == "textDocument/codeLens" || method == "textDocument/documentLink" || method == "textDocument/inlayHint" ||
      method == "textDocument/willSaveWaitUntil" || method == "textDocument/linkedEditingRange" ||
      method == "textDocument/prepareCallHierarchy" || method == "callHierarchy/incomingCalls" ||
      method == "callHierarchy/outgoingCalls" || method == "typeHierarchy/prepare" ||
      method == "typeHierarchy/supertypes" || method == "typeHierarchy/subtypes" ||
      method == "textDocument/inlineValue" || method == "textDocument/moniker" ||
      method == "textDocument/documentColor" || method == "textDocument/colorPresentation") {
    rapidjson::Value result(rapidjson::kArrayType);
    writeResult(request["id"], result);
    return;
  }

  writeError(request["id"], JSONRPC_METHOD_NOT_FOUND, "Method not found");
}

void Server::processNotification(rapidjson::Document &request) {
  if (!request.HasMember("method") || !request["method"].IsString()) {
    return;
  }

  auto method = std::string(request["method"].GetString());

  if (method == "exit") {
    serverOn = false;
    return;
  }
  if (method == "initialized") {
    handleInitialized(request);
    return;
  }
  if (method == "textDocument/didOpen") {
    handleDidOpen(request);
    return;
  }
  if (method == "textDocument/didChange") {
    handleDidChange(request);
    return;
  }
  if (method == "textDocument/didSave") {
    handleDidSave(request);
    return;
  }
  if (method == "textDocument/didClose") {
    handleDidClose(request);
    return;
  }
  if (method == "$/cancelRequest") {
    handleCancelRequest(request);
    return;
  }
  if (method == "$/setTrace") {
    handleSetTrace(request);
    return;
  }
  if (method == "$/logTrace") {
    handleLogTrace(request);
    return;
  }
  if (method == "workspace/didChangeConfiguration" || method == "workspace/didChangeWorkspaceFolders" ||
      method == "workspace/didChangeWatchedFiles") {
    loadWorkspaceDocuments();
    return;
  }
}

void Server::run() {
  while (serverOn) {
    std::string body;
    if (!readMessage(body)) {
      break;
    }
    if (body.empty()) {
      continue;
    }

    rapidjson::Document request;
    request.Parse(body.c_str(), body.size());
    if (request.HasParseError() || !request.IsObject()) {
      rapidjson::Value nullId;
      nullId.SetNull();
      writeError(nullId, JSONRPC_PARSE_ERROR, "Invalid JSON payload");
      continue;
    }

    if (request.HasMember("id")) {
      processRequest(request);
    } else {
      processNotification(request);
    }
  }
}

} // namespace starbytes::lsp
