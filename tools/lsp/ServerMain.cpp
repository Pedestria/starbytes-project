#include "ServerMain.h"
#include "DocumentAnalysis.h"
#include "SymbolTypes.h"
#include "starbytes/base/CodeView.h"
#include "starbytes/base/Diagnostic.h"
#include "starbytes/base/DoxygenDoc.h"
#include "starbytes/compiler/AST.h"
#include "starbytes/compiler/ASTDecl.h"
#include "starbytes/compiler/ASTExpr.h"
#include "starbytes/compiler/Lexer.h"
#include "starbytes/compiler/Parser.h"
#include "starbytes/compiler/SymTable.h"
#include "starbytes/compiler/Type.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <regex>
#include <utility>

#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

namespace starbytes::lsp {

struct SemanticResolvedDocument {
  std::vector<ASTStmt *> statements;
  std::vector<std::string> imports;
  std::shared_ptr<Semantics::SymbolTable> mainTable;
};

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

struct CompletionEntry {
  std::string label;
  int kind = COMPLETION_KIND_TEXT;
  std::string detail;
  bool hasResolvedSymbol = false;
  std::string resolvedUri;
  SymbolEntry resolvedSymbol;
};

const std::vector<std::string> kKeywordCompletions = {
    "decl", "imut", "func", "class", "struct", "interface", "enum", "scope", "import", "if",
    "elif", "else", "for", "while", "return", "new", "secure", "catch", "def", "lazy", "await",
    "true", "false"};

std::string toLower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return value;
}

std::string trimCopy(std::string value) {
  size_t start = 0;
  while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) {
    ++start;
  }
  size_t end = value.size();
  while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
    --end;
  }
  return value.substr(start, end - start);
}

bool envTruthy(const char *value) {
  if(value == nullptr) {
    return false;
  }
  auto text = toLower(trimCopy(value));
  return !(text.empty() || text == "0" || text == "false" || text == "off" || text == "no");
}

std::string diagnosticPhaseToStringLocal(Diagnostic::Phase phase) {
  switch (phase) {
    case Diagnostic::Phase::Parser:
      return "parser";
    case Diagnostic::Phase::Semantic:
      return "semantic";
    case Diagnostic::Phase::Runtime:
      return "runtime";
    case Diagnostic::Phase::Lsp:
      return "lsp";
    case Diagnostic::Phase::Unknown:
    default:
      return "unknown";
  }
}

std::vector<std::string> splitCommaList(const std::string &text) {
  std::vector<std::string> out;
  std::string current;
  for (char ch : text) {
    if (ch == ',') {
      auto trimmed = trimCopy(current);
      if (!trimmed.empty()) {
        out.push_back(trimmed);
      }
      current.clear();
      continue;
    }
    current.push_back(ch);
  }
  auto trimmed = trimCopy(current);
  if (!trimmed.empty()) {
    out.push_back(trimmed);
  }
  return out;
}

std::string absolutePathString(const std::filesystem::path &path) {
  std::error_code ec;
  auto absolute = std::filesystem::absolute(path, ec);
  if (ec) {
    return path.lexically_normal().string();
  }
  return absolute.lexically_normal().string();
}

void appendUniquePath(std::vector<std::filesystem::path> &paths,
                      std::unordered_set<std::string> &seen,
                      const std::filesystem::path &path) {
  if (path.empty()) {
    return;
  }
  auto normalized = absolutePathString(path);
  if (seen.insert(normalized).second) {
    paths.emplace_back(normalized);
  }
}

bool lineIsCommentOrEmpty(const std::string &line) {
  size_t index = 0;
  while (index < line.size() && std::isspace(static_cast<unsigned char>(line[index])) != 0) {
    ++index;
  }
  if (index >= line.size()) {
    return true;
  }
  if (line[index] == '#') {
    return true;
  }
  return line[index] == '/' && (index + 1) < line.size() && line[index + 1] == '/';
}

void loadModulePathFileForLsp(const std::filesystem::path &path,
                              std::vector<std::filesystem::path> &outDirs,
                              std::unordered_set<std::string> &seenDirs) {
  std::ifstream in(path, std::ios::in);
  if (!in.is_open()) {
    return;
  }

  const auto baseDir = path.parent_path();
  std::string rawLine;
  while (std::getline(in, rawLine)) {
    if (lineIsCommentOrEmpty(rawLine)) {
      continue;
    }
    auto begin = rawLine.find_first_not_of(" \t\r\n");
    auto end = rawLine.find_last_not_of(" \t\r\n");
    if (begin == std::string::npos || end == std::string::npos) {
      continue;
    }
    std::filesystem::path entryPath(rawLine.substr(begin, end - begin + 1));
    if (entryPath.is_relative()) {
      entryPath = baseDir / entryPath;
    }

    std::error_code ec;
    if (!std::filesystem::exists(entryPath, ec) || ec || !std::filesystem::is_directory(entryPath, ec) || ec) {
      continue;
    }
    appendUniquePath(outDirs, seenDirs, entryPath);
  }
}

std::vector<std::string> splitLinesCopy(const std::string &text) {
  std::vector<std::string> lines;
  std::istringstream in(text);
  std::string line;
  while (std::getline(in, line)) {
    lines.push_back(line);
  }
  return lines;
}

bool regionContainsPosition(const Region &region, unsigned lineOneBased, unsigned character) {
  if (region.startLine == 0 || lineOneBased == 0) {
    return false;
  }
  if (lineOneBased < region.startLine || lineOneBased > region.endLine) {
    return false;
  }
  if (lineOneBased == region.startLine && character < region.startCol) {
    return false;
  }
  if (lineOneBased == region.endLine && character >= std::max(region.endCol, region.startCol + 1)) {
    return false;
  }
  return true;
}

std::string sourceSliceForRegion(const std::vector<std::string> &lines, const Region &region) {
  if (region.startLine == 0 || region.startLine != region.endLine) {
    return {};
  }
  auto zeroBasedLine = region.startLine - 1;
  if (zeroBasedLine >= lines.size()) {
    return {};
  }
  const auto &line = lines[zeroBasedLine];
  if (region.startCol >= line.size()) {
    return {};
  }
  auto endCol = std::min(region.endCol, static_cast<unsigned>(line.size()));
  if (endCol <= region.startCol) {
    return {};
  }
  return line.substr(region.startCol, endCol - region.startCol);
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

constexpr int LSP_COMPLETION_ITEM_TAG_DEPRECATED = 1;
constexpr int LSP_SYMBOL_TAG_DEPRECATED = 1;
constexpr int LSP_DIAGNOSTIC_TAG_UNNECESSARY = 1;
constexpr int LSP_DIAGNOSTIC_TAG_DEPRECATED = 2;

std::string hoverDocCommentForLine(const std::vector<std::string> &lines, unsigned declarationLineZeroBased) {
  if (declarationLineZeroBased == 0 || declarationLineZeroBased - 1 >= lines.size()) {
    return {};
  }

  int index = static_cast<int>(declarationLineZeroBased) - 1;
  auto first = trimCopy(lines[static_cast<size_t>(index)]);
  if (first.empty()) {
    return {};
  }

  std::vector<std::string> collected;
  if (first.rfind("//", 0) == 0) {
    while (index >= 0) {
      auto candidate = trimCopy(lines[static_cast<size_t>(index)]);
      if (candidate.rfind("//", 0) != 0) {
        break;
      }
      auto raw = trimLeftCopy(lines[static_cast<size_t>(index)]);
      if (raw.rfind("///", 0) == 0) {
        raw = raw.substr(3);
      } else {
        raw = raw.substr(2);
      }
      collected.push_back(trimLeftCopy(trimRightCopy(raw)));
      --index;
    }
    std::reverse(collected.begin(), collected.end());
  } else if (first.size() >= 2 && first.find("*/") != std::string::npos) {
    bool foundStart = false;
    while (index >= 0) {
      auto raw = trimLeftCopy(lines[static_cast<size_t>(index)]);
      collected.push_back(raw);
      if (raw.find("/*") != std::string::npos) {
        foundStart = true;
        break;
      }
      --index;
    }
    if (!foundStart) {
      return {};
    }
    std::reverse(collected.begin(), collected.end());
    for (auto &line : collected) {
      auto trimmed = trimLeftCopy(line);
      if (trimmed.rfind("/*", 0) == 0) {
        trimmed = trimmed.substr(2);
      }
      auto endPos = trimmed.find("*/");
      if (endPos != std::string::npos) {
        trimmed = trimmed.substr(0, endPos);
      }
      if (!trimmed.empty() && trimmed.front() == '*') {
        trimmed.erase(trimmed.begin());
      }
      line = trimLeftCopy(trimRightCopy(trimmed));
    }
  } else {
    return {};
  }

  std::ostringstream out;
  bool firstLine = true;
  for (const auto &line : collected) {
    if (!firstLine) {
      out << "\n";
    }
    out << line;
    firstLine = false;
  }
  return trimRightCopy(out.str());
}

std::string hoverTypeToString(ASTType *type) {
  if (!type) {
    return "Any";
  }

  auto renderType = [&](auto &&self, ASTType *subject) -> std::string {
    if (!subject) {
      return "Any";
    }
    auto name = subject->getName().str();
    if (subject->nameMatches(FUNCTION_TYPE)) {
      std::string rendered = "(";
      if (!subject->typeParams.empty()) {
        for (size_t i = 1; i < subject->typeParams.size(); ++i) {
          if (i > 1) {
            rendered += ", ";
          }
          rendered += self(self, subject->typeParams[i]);
        }
        rendered += ") ";
        rendered += self(self, subject->typeParams.front());
      } else {
        rendered += ") Void";
      }
      if (subject->isOptional) {
        rendered += "?";
      }
      if (subject->isThrowable) {
        rendered += "!";
      }
      return rendered;
    }

    std::string rendered = name;
    if (!subject->typeParams.empty()) {
      rendered += "<";
      for (size_t i = 0; i < subject->typeParams.size(); ++i) {
        if (i > 0) {
          rendered += ", ";
        }
        rendered += self(self, subject->typeParams[i]);
      }
      rendered += ">";
    }
    if (subject->isOptional) {
      rendered += "?";
    }
    if (subject->isThrowable) {
      rendered += "!";
    }
    return rendered;
  };

  return renderType(renderType, type);
}

std::string completionDeclaredBaseTypeFromSignature(const std::string &signature) {
  auto colonPos = signature.find(':');
  if (colonPos == std::string::npos) {
    return {};
  }
  auto typePart = trimCopy(signature.substr(colonPos + 1));
  auto eqPos = typePart.find('=');
  if (eqPos != std::string::npos) {
    typePart = trimCopy(typePart.substr(0, eqPos));
  }
  while (!typePart.empty() && (typePart.back() == '?' || typePart.back() == '!')) {
    typePart.pop_back();
  }
  auto genericPos = typePart.find('<');
  if (genericPos != std::string::npos) {
    typePart = typePart.substr(0, genericPos);
  }
  typePart = trimCopy(typePart);
  if (typePart.size() >= 2 && typePart.substr(typePart.size() - 2) == "[]") {
    return "Array";
  }
  return typePart;
}

std::string declaredTypeTextFromSignature(const std::string &signature) {
  auto colonPos = signature.find(':');
  if (colonPos == std::string::npos) {
    return {};
  }
  auto typePart = trimCopy(signature.substr(colonPos + 1));
  auto eqPos = typePart.find('=');
  if (eqPos != std::string::npos) {
    typePart = trimCopy(typePart.substr(0, eqPos));
  }
  return typePart;
}

std::string normalizeTypeTargetName(std::string typeName) {
  typeName = trimCopy(std::move(typeName));
  while (!typeName.empty() && (typeName.back() == '?' || typeName.back() == '!')) {
    typeName.pop_back();
  }
  typeName = trimCopy(std::move(typeName));
  if (typeName.empty() || typeName.front() == '(') {
    return {};
  }
  if (typeName.size() >= 2 && typeName.compare(typeName.size() - 2, 2, "[]") == 0) {
    return "Array";
  }
  auto genericPos = typeName.find('<');
  if (genericPos != std::string::npos) {
    typeName = trimCopy(typeName.substr(0, genericPos));
  }
  return typeName;
}

std::vector<std::string> splitTopLevelTypeText(const std::string &text, char delimiter) {
  std::vector<std::string> parts;
  std::string current;
  int parenDepth = 0;
  int angleDepth = 0;
  int bracketDepth = 0;
  for (char ch : text) {
    switch (ch) {
      case '(':
        ++parenDepth;
        break;
      case ')':
        if (parenDepth > 0) {
          --parenDepth;
        }
        break;
      case '<':
        ++angleDepth;
        break;
      case '>':
        if (angleDepth > 0) {
          --angleDepth;
        }
        break;
      case '[':
        ++bracketDepth;
        break;
      case ']':
        if (bracketDepth > 0) {
          --bracketDepth;
        }
        break;
      default:
        break;
    }

    if (ch == delimiter && parenDepth == 0 && angleDepth == 0 && bracketDepth == 0) {
      auto trimmed = trimCopy(current);
      if (!trimmed.empty()) {
        parts.push_back(trimmed);
      }
      current.clear();
      continue;
    }
    current.push_back(ch);
  }

  auto trimmed = trimCopy(current);
  if (!trimmed.empty()) {
    parts.push_back(trimmed);
  }
  return parts;
}

bool extractFunctionTypeParts(const std::string &typeText,
                              std::vector<std::string> &paramTypesOut,
                              std::string &returnTypeOut) {
  auto trimmed = trimCopy(typeText);
  if (trimmed.empty() || trimmed.front() != '(') {
    return false;
  }

  int parenDepth = 0;
  int angleDepth = 0;
  int bracketDepth = 0;
  size_t closePos = std::string::npos;
  for (size_t i = 0; i < trimmed.size(); ++i) {
    char ch = trimmed[i];
    switch (ch) {
      case '(':
        ++parenDepth;
        break;
      case ')':
        if (parenDepth > 0) {
          --parenDepth;
        }
        if (parenDepth == 0 && angleDepth == 0 && bracketDepth == 0) {
          closePos = i;
        }
        break;
      case '<':
        ++angleDepth;
        break;
      case '>':
        if (angleDepth > 0) {
          --angleDepth;
        }
        break;
      case '[':
        ++bracketDepth;
        break;
      case ']':
        if (bracketDepth > 0) {
          --bracketDepth;
        }
        break;
      default:
        break;
    }
    if (closePos != std::string::npos) {
      break;
    }
  }

  if (closePos == std::string::npos) {
    return false;
  }

  auto paramsText = trimmed.substr(1, closePos - 1);
  returnTypeOut = trimCopy(trimmed.substr(closePos + 1));
  if (returnTypeOut.empty()) {
    return false;
  }

  for (const auto &part : splitTopLevelTypeText(paramsText, ',')) {
    auto colonPos = part.rfind(':');
    auto typePart = colonPos == std::string::npos ? trimCopy(part) : trimCopy(part.substr(colonPos + 1));
    if (!typePart.empty()) {
      paramTypesOut.push_back(typePart);
    }
  }
  return true;
}

std::string aliasTargetTypeText(const SymbolEntry &symbol) {
  if (symbol.detail != "type alias") {
    return {};
  }
  auto eqPos = symbol.signature.find('=');
  if (eqPos == std::string::npos) {
    return {};
  }
  return trimCopy(symbol.signature.substr(eqPos + 1));
}

bool isTypeDefinitionSymbol(const SymbolEntry &symbol) {
  return symbol.kind == SYMBOL_KIND_CLASS || symbol.kind == SYMBOL_KIND_INTERFACE || symbol.kind == SYMBOL_KIND_STRUCT ||
         symbol.kind == SYMBOL_KIND_ENUM || symbol.detail == "type alias";
}

std::string typeTargetNameFromSymbol(const SymbolEntry &symbol) {
  if (symbol.detail == "type alias") {
    auto eqPos = symbol.signature.find('=');
    if (eqPos != std::string::npos) {
      return normalizeTypeTargetName(symbol.signature.substr(eqPos + 1));
    }
    return normalizeTypeTargetName(symbol.name);
  }

  if (symbol.kind == SYMBOL_KIND_CLASS || symbol.kind == SYMBOL_KIND_INTERFACE || symbol.kind == SYMBOL_KIND_STRUCT ||
      symbol.kind == SYMBOL_KIND_ENUM) {
    return normalizeTypeTargetName(symbol.name);
  }

  if (symbol.kind == SYMBOL_KIND_FUNCTION || symbol.kind == SYMBOL_KIND_METHOD) {
    std::string name;
    std::string params;
    std::string returnType;
    if (signaturePartsFromSignature(symbol.signature, name, params, returnType) && !returnType.empty()) {
      return normalizeTypeTargetName(returnType);
    }
    auto closePos = symbol.signature.rfind(')');
    if (closePos != std::string::npos) {
      return normalizeTypeTargetName(symbol.signature.substr(closePos + 1));
    }
    return {};
  }

  if (symbol.kind == SYMBOL_KIND_VARIABLE || symbol.kind == SYMBOL_KIND_FIELD || symbol.kind == SYMBOL_KIND_CONSTANT) {
    return normalizeTypeTargetName(declaredTypeTextFromSignature(symbol.signature));
  }

  return {};
}

unsigned semanticTokenTypeFromSymbolKindForLsp(int symbolKind) {
  switch (symbolKind) {
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

std::string hoverFunctionSignature(ASTFuncDecl *funcDecl, const std::vector<std::string> &lines) {
  if (!funcDecl || !funcDecl->funcId) {
    return "func <anonymous>()";
  }
  auto lineIndex = funcDecl->funcId->codeRegion.startLine > 0 ? funcDecl->funcId->codeRegion.startLine - 1 : 0;
  bool isStructDecl = lineIndex < lines.size() && trimLeftCopy(lines[lineIndex]).rfind("lazy func", 0) == 0;
  std::string signature = isStructDecl ? "lazy func " : "func ";
  signature += sourceSliceForRegion(lines, funcDecl->funcId->codeRegion);
  signature += "(";
  bool first = true;
  for (const auto &param : funcDecl->params) {
    if (!first) {
      signature += ", ";
    }
    first = false;
    auto *paramId = param.first;
    signature += paramId ? sourceSliceForRegion(lines, paramId->codeRegion) : "_";
    signature += ":";
    signature += hoverTypeToString(param.second);
  }
  signature += ")";
  if (funcDecl->returnType) {
    signature += " ";
    signature += hoverTypeToString(funcDecl->returnType);
  }
  return signature;
}

std::string hoverVarSignature(bool isConst,
                              const std::string &displayName,
                              const std::string &typeText,
                              const char *keyword = "decl") {
  std::string signature = keyword;
  if (isConst) {
    signature += " imut";
  }
  signature += " ";
  signature += displayName;
  if (!typeText.empty()) {
    signature += ":";
    signature += typeText;
  }
  return signature;
}

std::string hoverVarSignature(bool isConst,
                              const std::string &displayName,
                              ASTType *type,
                              const char *keyword = "decl") {
  return hoverVarSignature(isConst, displayName, type ? hoverTypeToString(type) : std::string(), keyword);
}

std::vector<std::string> extractImportsForHover(const std::string &sourceText) {
  static const std::regex importRegex(R"(^\s*import\s+([A-Za-z_][A-Za-z0-9_]*)\s*$)");
  std::vector<std::string> imports;
  std::istringstream in(sourceText);
  std::string line;
  while (std::getline(in, line)) {
    auto commentPos = line.find("//");
    if (commentPos != std::string::npos) {
      line = line.substr(0, commentPos);
    }
    std::smatch match;
    if (!std::regex_match(line, match, importRegex) || match.size() < 2) {
      continue;
    }
    auto moduleName = match[1].str();
    if (std::find(imports.begin(), imports.end(), moduleName) == imports.end()) {
      imports.push_back(std::move(moduleName));
    }
  }
  return imports;
}

void appendImportedSymbolTablesForHover(
    Semantics::STableContext &context,
    const std::vector<std::string> &dependencyKeys,
    const std::unordered_map<std::string, std::shared_ptr<Semantics::SymbolTable>> &depTables) {
  for (const auto &depKey : dependencyKeys) {
    auto depIt = depTables.find(depKey);
    if (depIt == depTables.end() || !depIt->second) {
      continue;
    }
    context.importTables.push_back(depIt->second);
    auto overlay = depIt->second->createImportNamespaceOverlay(depKey);
    if (overlay) {
      context.otherTables.push_back(std::move(overlay));
    }
  }
}

class SemanticCapturingConsumer final : public ASTStreamConsumer {
public:
  std::vector<ASTStmt *> statements;
  bool acceptsSymbolTableContext() override { return false; }
  void consumeDecl(ASTDecl *stmt) override { statements.push_back(stmt); }
  void consumeStmt(ASTStmt *stmt) override { statements.push_back(stmt); }
};

class SemanticNoopConsumer final : public ASTStreamConsumer {
public:
  bool acceptsSymbolTableContext() override { return false; }
  void consumeDecl(ASTDecl *stmt) override { (void)stmt; }
  void consumeStmt(ASTStmt *stmt) override { (void)stmt; }
};

struct HoverBinding {
  ASTIdentifier *declId = nullptr;
  ASTStmt *declOwner = nullptr;
  ASTType *type = nullptr;
  bool isConst = false;
  std::string lookupName;
  std::string displayName;
  std::string renderedType;
};

struct HoverFrame {
  std::shared_ptr<ASTScope> scope;
  bool functionBoundary = false;
  std::vector<HoverBinding> bindings;
};

struct HoverResolvedSymbol {
  std::string uri;
  SymbolEntry symbol;
};

bool isNumericTypeForHover(ASTType *type) {
  return type && (type->nameMatches(INT_TYPE) || type->nameMatches(LONG_TYPE) ||
                  type->nameMatches(FLOAT_TYPE) || type->nameMatches(DOUBLE_TYPE));
}

bool isStringTypeForHover(ASTType *type) { return type && type->nameMatches(STRING_TYPE); }
bool isArrayTypeForHover(ASTType *type) { return type && type->nameMatches(ARRAY_TYPE); }
bool isDictTypeForHover(ASTType *type) { return type && type->nameMatches(DICTIONARY_TYPE); }
bool isMapTypeForHover(ASTType *type) { return type && type->nameMatches(MAP_TYPE); }
bool isRegexTypeForHover(ASTType *type) { return type && type->nameMatches(REGEX_TYPE); }

void appendInheritanceMarkdown(std::string &markdown, const SymbolEntry &symbol) {
  if (symbol.kind != SYMBOL_KIND_CLASS && symbol.kind != SYMBOL_KIND_STRUCT &&
      symbol.kind != SYMBOL_KIND_INTERFACE) {
    return;
  }
  auto colonPos = symbol.signature.find(':');
  if (colonPos == std::string::npos) {
    return;
  }
  auto inheritPart = trimCopy(symbol.signature.substr(colonPos + 1));
  if (inheritPart.empty()) {
    return;
  }
  auto bases = splitCommaList(inheritPart);
  if (bases.empty()) {
    return;
  }

  std::vector<std::string> superclasses;
  std::vector<std::string> interfaces;
  if (symbol.kind == SYMBOL_KIND_INTERFACE) {
    interfaces = bases;
  } else {
    superclasses.push_back(bases.front());
    if (bases.size() > 1) {
      interfaces.insert(interfaces.end(), bases.begin() + 1, bases.end());
    }
  }

  if (superclasses.empty() && interfaces.empty()) {
    return;
  }

  markdown += "\n\n**Inheritance**";
  if (!superclasses.empty()) {
    markdown += "\n- Superclasses: ";
    for (size_t i = 0; i < superclasses.size(); ++i) {
      if (i > 0) {
        markdown += ", ";
      }
      markdown += "`" + superclasses[i] + "`";
    }
  }
  if (!interfaces.empty()) {
    markdown += "\n- Interfaces: ";
    for (size_t i = 0; i < interfaces.size(); ++i) {
      if (i > 0) {
        markdown += ", ";
      }
      markdown += "`" + interfaces[i] + "`";
    }
  }
}

std::vector<SemanticTokenEntry> filterSemanticTokensByLineRange(const std::vector<SemanticTokenEntry> &tokens,
                                                                unsigned startLine,
                                                                unsigned endLine) {
  std::vector<SemanticTokenEntry> filtered;
  filtered.reserve(tokens.size());
  for (const auto &token : tokens) {
    if (token.line < startLine || token.line > endLine) {
      continue;
    }
    filtered.push_back(token);
  }
  return filtered;
}

bool hasRegionLocation(const Region &region) {
  return region.startLine > 0 || region.endLine > 0 || region.startCol > 0 || region.endCol > 0;
}

bool compilerDiagnosticIsDeprecated(const CompilerDiagnosticEntry &entry) {
  return entry.severity == 2 && entry.message.rfind("Use of deprecated ", 0) == 0;
}

void appendDeprecatedTagArray(rapidjson::Value &object,
                              int tagValue,
                              rapidjson::Document::AllocatorType &alloc) {
  rapidjson::Value tags(rapidjson::kArrayType);
  tags.PushBack(tagValue, alloc);
  object.AddMember("tags", tags, alloc);
}

rapidjson::Value buildRangeFromRegion(const Region &region, rapidjson::Document::AllocatorType &alloc) {
  rapidjson::Value range(rapidjson::kObjectType);
  rapidjson::Value start(rapidjson::kObjectType);
  rapidjson::Value end(rapidjson::kObjectType);
  unsigned startLine = region.startLine > 0 ? region.startLine - 1 : 0;
  unsigned endLine = region.endLine > 0 ? region.endLine - 1 : startLine;
  unsigned startCol = region.startCol;
  unsigned endCol = region.endCol > region.startCol ? region.endCol : (region.startCol + 1);
  start.AddMember("line", startLine, alloc);
  start.AddMember("character", startCol, alloc);
  end.AddMember("line", endLine, alloc);
  end.AddMember("character", endCol, alloc);
  range.AddMember("start", start, alloc);
  range.AddMember("end", end, alloc);
  return range;
}

void appendCompilerDiagnosticJson(rapidjson::Value &diag,
                                  const CompilerDiagnosticEntry &entry,
                                  const std::string &uri,
                                  rapidjson::Document::AllocatorType &alloc) {
  diag.AddMember("range", buildRangeFromRegion(entry.region, alloc), alloc);
  diag.AddMember("severity", entry.severity, alloc);
  diag.AddMember("source", rapidjson::Value(entry.source.c_str(), alloc), alloc);
  diag.AddMember("message", rapidjson::Value(entry.message.c_str(), alloc), alloc);
  if (compilerDiagnosticIsDeprecated(entry)) {
    appendDeprecatedTagArray(diag, LSP_DIAGNOSTIC_TAG_DEPRECATED, alloc);
  }

  if(!entry.code.empty()) {
    diag.AddMember("code", rapidjson::Value(entry.code.c_str(), alloc), alloc);
  }

  if(!entry.relatedSpans.empty()) {
    rapidjson::Value relatedInfo(rapidjson::kArrayType);
    for(const auto &related : entry.relatedSpans) {
      if(!hasRegionLocation(related.span)) {
        continue;
      }
      rapidjson::Value info(rapidjson::kObjectType);
      rapidjson::Value location(rapidjson::kObjectType);
      location.AddMember("uri", rapidjson::Value(uri.c_str(), alloc), alloc);
      location.AddMember("range", buildRangeFromRegion(related.span, alloc), alloc);
      info.AddMember("location", location, alloc);
      info.AddMember("message", rapidjson::Value(related.message.c_str(), alloc), alloc);
      relatedInfo.PushBack(info, alloc);
    }
    if(!relatedInfo.Empty()) {
      diag.AddMember("relatedInformation", relatedInfo, alloc);
    }
  }

  rapidjson::Value data(rapidjson::kObjectType);
  bool hasData = false;
  if(!entry.id.empty()) {
    data.AddMember("id", rapidjson::Value(entry.id.c_str(), alloc), alloc);
    hasData = true;
  }
  if(!entry.phase.empty()) {
    data.AddMember("phase", rapidjson::Value(entry.phase.c_str(), alloc), alloc);
    hasData = true;
  }
  if(!entry.producerSource.empty()) {
    data.AddMember("producerSource", rapidjson::Value(entry.producerSource.c_str(), alloc), alloc);
    hasData = true;
  }
  if(!entry.notes.empty()) {
    rapidjson::Value notes(rapidjson::kArrayType);
    for(const auto &note : entry.notes) {
      notes.PushBack(rapidjson::Value(note.c_str(), alloc), alloc);
    }
    data.AddMember("notes", notes, alloc);
    hasData = true;
  }
  if(!entry.fixits.empty()) {
    rapidjson::Value fixits(rapidjson::kArrayType);
    for(const auto &fixit : entry.fixits) {
      if(!hasRegionLocation(fixit.span)) {
        continue;
      }
      rapidjson::Value fixitObj(rapidjson::kObjectType);
      fixitObj.AddMember("range", buildRangeFromRegion(fixit.span, alloc), alloc);
      fixitObj.AddMember("replacement", rapidjson::Value(fixit.replacement.c_str(), alloc), alloc);
      if(!fixit.message.empty()) {
        fixitObj.AddMember("message", rapidjson::Value(fixit.message.c_str(), alloc), alloc);
      }
      fixits.PushBack(fixitObj, alloc);
    }
    if(!fixits.Empty()) {
      data.AddMember("fixits", fixits, alloc);
      hasData = true;
    }
  }

  if(hasData) {
    diag.AddMember("data", data, alloc);
  }
}

int lspSeverityFromFinding(starbytes::linguistics::FindingSeverity severity) {
  using starbytes::linguistics::FindingSeverity;
  switch (severity) {
    case FindingSeverity::Error:
      return 1;
    case FindingSeverity::Warning:
      return 2;
    case FindingSeverity::Information:
      return 3;
    case FindingSeverity::Hint:
      return 4;
  }
  return 2;
}

const char *suggestionKindName(starbytes::linguistics::SuggestionKind kind) {
  using starbytes::linguistics::SuggestionKind;
  switch (kind) {
    case SuggestionKind::Style:
      return "style";
    case SuggestionKind::Correctness:
      return "correctness";
    case SuggestionKind::Performance:
      return "performance";
    case SuggestionKind::Safety:
      return "safety";
    case SuggestionKind::Docs:
      return "docs";
    case SuggestionKind::Refactor:
      return "refactor";
  }
  return "style";
}

rapidjson::Value buildRangeFromTextSpan(const starbytes::linguistics::TextSpan &span,
                                        rapidjson::Document::AllocatorType &alloc) {
  rapidjson::Value range(rapidjson::kObjectType);
  rapidjson::Value start(rapidjson::kObjectType);
  rapidjson::Value end(rapidjson::kObjectType);
  start.AddMember("line", span.start.line, alloc);
  start.AddMember("character", span.start.character, alloc);
  end.AddMember("line", span.end.line, alloc);
  end.AddMember("character", span.end.character, alloc);
  range.AddMember("start", start, alloc);
  range.AddMember("end", end, alloc);
  return range;
}

void appendLintDiagnosticJson(rapidjson::Value &diag,
                              const starbytes::linguistics::LintFinding &finding,
                              const std::string &uri,
                              rapidjson::Document::AllocatorType &alloc) {
  diag.AddMember("range", buildRangeFromTextSpan(finding.span, alloc), alloc);
  diag.AddMember("severity", lspSeverityFromFinding(finding.severity), alloc);
  diag.AddMember("source", rapidjson::Value("starbytes-linguistics", alloc), alloc);
  diag.AddMember("message", rapidjson::Value(finding.message.c_str(), alloc), alloc);
  if(!finding.code.empty()) {
    diag.AddMember("code", rapidjson::Value(finding.code.c_str(), alloc), alloc);
  }

  if(!finding.related.empty()) {
    rapidjson::Value relatedInfo(rapidjson::kArrayType);
    for(const auto &span : finding.related) {
      rapidjson::Value info(rapidjson::kObjectType);
      rapidjson::Value location(rapidjson::kObjectType);
      location.AddMember("uri", rapidjson::Value(uri.c_str(), alloc), alloc);
      location.AddMember("range", buildRangeFromTextSpan(span, alloc), alloc);
      info.AddMember("location", location, alloc);
      info.AddMember("message", rapidjson::Value("Related location", alloc), alloc);
      relatedInfo.PushBack(info, alloc);
    }
    if(!relatedInfo.Empty()) {
      diag.AddMember("relatedInformation", relatedInfo, alloc);
    }
  }

  rapidjson::Value data(rapidjson::kObjectType);
  data.AddMember("id", rapidjson::Value(finding.id.c_str(), alloc), alloc);
  data.AddMember("phase", rapidjson::Value("linguistics", alloc), alloc);
  if(!finding.notes.empty()) {
    rapidjson::Value notes(rapidjson::kArrayType);
    for(const auto &note : finding.notes) {
      notes.PushBack(rapidjson::Value(note.c_str(), alloc), alloc);
    }
    data.AddMember("notes", notes, alloc);
  }
  if(!finding.fixes.empty()) {
    rapidjson::Value fixits(rapidjson::kArrayType);
    for(const auto &fix : finding.fixes) {
      for(const auto &edit : fix.edits) {
        rapidjson::Value fixit(rapidjson::kObjectType);
        fixit.AddMember("range", buildRangeFromTextSpan(edit.span, alloc), alloc);
        fixit.AddMember("replacement", rapidjson::Value(edit.replacement.c_str(), alloc), alloc);
        if(!fix.title.empty()) {
          fixit.AddMember("message", rapidjson::Value(fix.title.c_str(), alloc), alloc);
        }
        fixits.PushBack(fixit, alloc);
      }
    }
    if(!fixits.Empty()) {
      data.AddMember("fixits", fixits, alloc);
    }
  }
  diag.AddMember("data", data, alloc);
}

void appendSuggestionDiagnosticJson(rapidjson::Value &diag,
                                    const starbytes::linguistics::Suggestion &suggestion,
                                    rapidjson::Document::AllocatorType &alloc) {
  diag.AddMember("range", buildRangeFromTextSpan(suggestion.span, alloc), alloc);
  diag.AddMember("severity", 4, alloc);
  diag.AddMember("source", rapidjson::Value("starbytes-linguistics", alloc), alloc);
  diag.AddMember("message", rapidjson::Value(suggestion.message.c_str(), alloc), alloc);
  diag.AddMember("code", rapidjson::Value(suggestion.id.c_str(), alloc), alloc);

  rapidjson::Value tags(rapidjson::kArrayType);
  diag.AddMember("tags", tags, alloc);

  rapidjson::Value data(rapidjson::kObjectType);
  data.AddMember("id", rapidjson::Value(suggestion.id.c_str(), alloc), alloc);
  data.AddMember("phase", rapidjson::Value("linguistics", alloc), alloc);
  data.AddMember("kind", rapidjson::Value(suggestionKindName(suggestion.kind), alloc), alloc);
  data.AddMember("confidence", suggestion.confidence, alloc);
  diag.AddMember("data", data, alloc);
}

int compareTextPosition(const starbytes::linguistics::TextPosition &lhs,
                        const starbytes::linguistics::TextPosition &rhs) {
  if(lhs.line < rhs.line) {
    return -1;
  }
  if(lhs.line > rhs.line) {
    return 1;
  }
  if(lhs.character < rhs.character) {
    return -1;
  }
  if(lhs.character > rhs.character) {
    return 1;
  }
  return 0;
}

bool spansIntersect(const starbytes::linguistics::TextSpan &lhs, const starbytes::linguistics::TextSpan &rhs) {
  if(compareTextPosition(lhs.end, rhs.start) < 0) {
    return false;
  }
  if(compareTextPosition(rhs.end, lhs.start) < 0) {
    return false;
  }
  return true;
}

bool parseLspRange(const rapidjson::Value &rangeValue, starbytes::linguistics::TextSpan &rangeOut) {
  if(!rangeValue.IsObject() || !rangeValue.HasMember("start") || !rangeValue.HasMember("end")
      || !rangeValue["start"].IsObject() || !rangeValue["end"].IsObject()) {
    return false;
  }
  const auto &start = rangeValue["start"];
  const auto &end = rangeValue["end"];
  if(!start.HasMember("line") || !start.HasMember("character")
      || !end.HasMember("line") || !end.HasMember("character")
      || !start["line"].IsUint() || !start["character"].IsUint()
      || !end["line"].IsUint() || !end["character"].IsUint()) {
    return false;
  }

  rangeOut.start.line = start["line"].GetUint();
  rangeOut.start.character = start["character"].GetUint();
  rangeOut.end.line = end["line"].GetUint();
  rangeOut.end.character = end["character"].GetUint();
  return rangeOut.isValid();
}

rapidjson::Value buildLspTextEdit(const starbytes::linguistics::TextEdit &edit, rapidjson::Document::AllocatorType &alloc) {
  rapidjson::Value output(rapidjson::kObjectType);
  output.AddMember("range", buildRangeFromTextSpan(edit.span, alloc), alloc);
  output.AddMember("newText", rapidjson::Value(edit.replacement.c_str(), alloc), alloc);
  return output;
}

bool actionTouchesRange(const starbytes::linguistics::CodeAction &action, const starbytes::linguistics::TextSpan &range) {
  if(action.edits.empty()) {
    return false;
  }
  for(const auto &edit : action.edits) {
    if(spansIntersect(edit.span, range)) {
      return true;
    }
  }
  return false;
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


} // namespace

Server::Server(starbytes::lsp::ServerOptions &options) : in(options.in), out(options.os) {
  linguisticsProfilingEnabled = envTruthy(std::getenv("STARBYTES_LSP_PROFILE"));
}

void Server::maybeLogLspProfileSample(const char *label, uint64_t elapsedNs, size_t itemCount) {
  if(!linguisticsProfilingEnabled || !label) {
    return;
  }
  std::cerr << "[starbytes-lsp-profile] op=" << label
            << " ns=" << elapsedNs
            << " items=" << itemCount
            << "\n";
}

void Server::maybeLogLspProfileSummary() {
  if(!linguisticsProfilingEnabled) {
    return;
  }

  size_t cachedLintFindings = 0;
  size_t cachedSuggestions = 0;
  size_t cachedSafeActions = 0;
  size_t cachedAllActions = 0;
  size_t cachedFormattedDocs = 0;
  for(const auto &doc : documents) {
    cachedLintFindings += doc.second.analysis.lintFindings.size();
    cachedSuggestions += doc.second.analysis.suggestions.size();
    cachedSafeActions += doc.second.analysis.safeActions.size();
    cachedAllActions += doc.second.analysis.allActions.size();
    if(doc.second.analysis.formatReady) {
      ++cachedFormattedDocs;
    }
  }

  size_t estimatedCacheBytes =
      cachedLintFindings * sizeof(starbytes::linguistics::LintFinding) +
      cachedSuggestions * sizeof(starbytes::linguistics::Suggestion) +
      (cachedSafeActions + cachedAllActions) * sizeof(starbytes::linguistics::CodeAction);

  std::cerr << "[starbytes-lsp-profile] summary"
            << " lint_ns=" << lspProfileLintNs
            << " suggest_ns=" << lspProfileSuggestionNs
            << " actions_ns=" << lspProfileActionNs
            << " format_ns=" << lspProfileFormatNs
            << " diagnostics_ns=" << lspProfileDiagnosticsNs
            << " formatting_handler_ns=" << lspProfileFormattingNs
            << " code_action_handler_ns=" << lspProfileCodeActionNs
            << " cached_lint_findings=" << cachedLintFindings
            << " cached_suggestions=" << cachedSuggestions
            << " cached_safe_actions=" << cachedSafeActions
            << " cached_all_actions=" << cachedAllActions
            << " cached_formatted_docs=" << cachedFormattedDocs
            << " cache_estimated_bytes=" << estimatedCacheBytes
            << "\n";
}

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

uint64_t Server::hashText(const std::string &text) {
  uint64_t hash = 1469598103934665603ULL;
  for (unsigned char c : text) {
    hash ^= static_cast<uint64_t>(c);
    hash *= 1099511628211ULL;
  }
  return hash;
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

      auto text = buffer.str();
      DocumentState state;
      state.text = text;
      state.version = 0;
      state.isOpen = false;
      state.textHash = hashText(text);
      documents[uri] = std::move(state);
    }
  }
}

bool Server::getBuiltinsDocument(std::string &uriOut, std::string &textOut) {
  for (const auto &doc : documents) {
    if (isBuiltinsInterfaceUri(doc.first)) {
      uriOut = doc.first;
      textOut = doc.second.text;
      return !textOut.empty();
    }
  }

  std::vector<std::string> candidateUris;
  candidateUris.reserve(workspaceRoots.size() + 1);

  for (const auto &workspaceUri : workspaceRoots) {
    std::string workspacePath;
    if (!parseUriToPath(workspaceUri, workspacePath)) {
      continue;
    }
    std::filesystem::path builtinsPath = std::filesystem::path(workspacePath) / "stdlib" / "builtins.starbint";
    candidateUris.push_back(pathToUri(builtinsPath.string()));
  }

  std::filesystem::path localBuiltins = std::filesystem::current_path() / "stdlib" / "builtins.starbint";
  candidateUris.push_back(pathToUri(localBuiltins.string()));

  std::filesystem::path probe = std::filesystem::current_path();
  for(unsigned depth = 0; depth < 6; ++depth) {
    auto nestedBuiltins = probe / "stdlib" / "builtins.starbint";
    candidateUris.push_back(pathToUri(nestedBuiltins.string()));
    auto parent = probe.parent_path();
    if(parent.empty() || parent == probe) {
      break;
    }
    probe = parent;
  }

  for (const auto &candidateUri : candidateUris) {
    std::string candidateText;
    if (!getDocumentTextByUri(candidateUri, candidateText)) {
      continue;
    }
    if (candidateText.empty()) {
      continue;
    }
    uriOut = candidateUri;
    textOut = std::move(candidateText);
    return true;
  }

  return false;
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
  DocumentState state;
  state.text = textOut;
  state.version = 0;
  state.isOpen = false;
  state.textHash = hashText(textOut);
  documents[uri] = std::move(state);
  return true;
}

void Server::configureSymbolCache() {
  std::filesystem::path cacheRoot = std::filesystem::current_path() / ".starbytes";
  if(!workspaceRoots.empty()) {
    std::string workspacePath;
    if(parseUriToPath(workspaceRoots.front(), workspacePath)) {
      cacheRoot = std::filesystem::path(workspacePath) / ".starbytes";
    }
  }
  symbolCache.setCachePath(cacheRoot / ".cache" / "lsp_symbols_cache.v1");
}

void Server::refreshAnalysisState(DocumentState &state) {
  if (state.analysis.version == state.version && state.analysis.textHash == state.textHash) {
    return;
  }
  state.analysis = {};
  state.analysis.version = state.version;
  state.analysis.textHash = state.textHash;
}

std::shared_ptr<SemanticResolvedDocument> Server::buildSemanticResolvedDocumentForUri(
    const std::string &uri,
    DocumentState &state,
    std::unordered_set<std::string> &activeUris) {
  refreshAnalysisState(state);
  if (state.analysis.semanticResolvedReady) {
    return state.analysis.semanticResolvedDocument;
  }
  if (!activeUris.insert(uri).second) {
    return nullptr;
  }

  auto imports = extractImportsForHover(state.text);
  std::unordered_map<std::string, std::shared_ptr<Semantics::SymbolTable>> depTables;
  for (const auto &importName : imports) {
    std::string depUri;
    std::string depText;
    if (!findModuleDocumentByName(importName, uri, depUri, depText)) {
      continue;
    }
    auto depIt = documents.find(depUri);
    if (depIt == documents.end()) {
      continue;
    }
    auto depDocument = buildSemanticResolvedDocumentForUri(depUri, depIt->second, activeUris);
    if (depDocument && depDocument->mainTable) {
      depTables[importName] = depDocument->mainTable;
    }
  }

  std::ostringstream sink;
  auto diagnostics = DiagnosticHandler::createDefault(sink);
  diagnostics->setOutputMode(DiagnosticHandler::OutputMode::Machine);
  diagnostics->setDefaultSourceName(uri);
  SemanticCapturingConsumer consumer;
  Parser parser(consumer, std::move(diagnostics));
  auto parseContext = ModuleParseContext::Create(uri);
  appendImportedSymbolTablesForHover(parseContext.sTableContext, imports, depTables);
  std::istringstream in(state.text);
  parser.parseFromStream(in, parseContext);
  parser.finish();

  std::shared_ptr<SemanticResolvedDocument> resolvedDocument;
  if (parseContext.sTableContext.main) {
    resolvedDocument = std::make_shared<SemanticResolvedDocument>();
    resolvedDocument->statements = std::move(consumer.statements);
    resolvedDocument->imports = std::move(imports);
    resolvedDocument->mainTable = std::shared_ptr<Semantics::SymbolTable>(parseContext.sTableContext.main.release());
  }

  state.analysis.semanticResolvedDocument = resolvedDocument;
  state.analysis.semanticResolvedReady = true;
  activeUris.erase(uri);
  return resolvedDocument;
}

const SemanticResolvedDocument *Server::getSemanticResolvedDocumentForUri(const std::string &uri, DocumentState &state) {
  refreshAnalysisState(state);
  if (!state.analysis.semanticResolvedReady) {
    std::unordered_set<std::string> activeUris;
    buildSemanticResolvedDocumentForUri(uri, state, activeUris);
  }
  return state.analysis.semanticResolvedDocument.get();
}

const std::vector<CompilerDiagnosticEntry> &Server::getCompilerDiagnosticsForDocument(const std::string &uri,
                                                                                       DocumentState &state) {
  refreshAnalysisState(state);
  if (!state.analysis.diagnosticsReady) {
    state.analysis.diagnostics = buildCompilerDiagnosticsFromSemanticContext(uri, state);
    state.analysis.diagnosticsReady = true;
  }
  return state.analysis.diagnostics;
}

std::vector<CompilerDiagnosticEntry> Server::buildCompilerDiagnosticsFromSemanticContext(const std::string &uri,
                                                                                         DocumentState &state) {
  std::vector<CompilerDiagnosticEntry> diagnosticsOut;
  auto imports = extractImportsForHover(state.text);
  std::unordered_map<std::string, std::shared_ptr<Semantics::SymbolTable>> depTables;
  std::unordered_set<std::string> activeUris;
  activeUris.insert(uri);

  for (const auto &importName : imports) {
    std::string depUri;
    std::string depText;
    if (!findModuleDocumentByName(importName, uri, depUri, depText)) {
      continue;
    }
    auto depIt = documents.find(depUri);
    if (depIt == documents.end()) {
      continue;
    }
    auto depDocument = buildSemanticResolvedDocumentForUri(depUri, depIt->second, activeUris);
    if (depDocument && depDocument->mainTable) {
      depTables[importName] = depDocument->mainTable;
    }
  }

  std::ostringstream sink;
  auto diagnostics = DiagnosticHandler::createDefault(sink);
  diagnostics->setOutputMode(DiagnosticHandler::OutputMode::Lsp);
  diagnostics->setDefaultSourceName(uri);
  auto *handler = diagnostics.get();

  SemanticNoopConsumer consumer;
  Parser parser(consumer, std::move(diagnostics));
  auto parseContext = ModuleParseContext::Create(uri);
  parseContext.name = uri;
  appendImportedSymbolTablesForHover(parseContext.sTableContext, imports, depTables);
  std::istringstream in(state.text);
  parser.parseFromStream(in, parseContext);

  if (!handler) {
    return diagnosticsOut;
  }

  auto buffered = handler->collectLspRecords();
  diagnosticsOut.reserve(buffered.size());
  for (const auto &diag : buffered) {
    CompilerDiagnosticEntry mapped;
    if (diag.location.has_value()) {
      mapped.region = *diag.location;
    }
    mapped.severity = diag.isError() ? 1 : 2;
    mapped.message = diag.message;
    mapped.id = diag.id;
    mapped.code = diag.code;
    mapped.phase = diagnosticPhaseToStringLocal(diag.phase);
    mapped.source = "starbytes-compiler";
    mapped.producerSource = diag.sourceName;
    mapped.relatedSpans = diag.relatedSpans;
    mapped.notes = diag.notes;
    mapped.fixits = diag.fixits;
    diagnosticsOut.push_back(std::move(mapped));
  }
  handler->clear();
  return diagnosticsOut;
}

std::vector<SemanticTokenEntry> Server::buildSemanticTokensFromSemanticCache(const std::string &uri, DocumentState &state) {
  const auto *parsedDocument = getSemanticResolvedDocumentForUri(uri, state);
  if (!parsedDocument || !parsedDocument->mainTable) {
    return collectSemanticTokenEntries(state.text, 0, 0, false);
  }

  BuiltinApiIndex builtinsIndex;
  if (const auto *cachedBuiltins = getBuiltinsApiIndex()) {
    builtinsIndex = *cachedBuiltins;
  }

  std::ostringstream sink;
  auto diagnostics = DiagnosticHandler::createDefault(sink);
  Syntax::Lexer lexer(*diagnostics);
  std::vector<Syntax::Tok> tokenStream;
  std::istringstream input(state.text);
  lexer.tokenizeFromIStream(input, tokenStream);

  std::vector<SemanticTokenEntry> tokens;
  tokens.reserve(tokenStream.size());
  for (const auto &token : tokenStream) {
    unsigned line = token.srcPos.line > 0 ? token.srcPos.line - 1 : 0;
    unsigned start = token.srcPos.startCol;
    unsigned length = token.srcPos.endCol > token.srcPos.startCol ? token.srcPos.endCol - token.srcPos.startCol
                                                                  : static_cast<unsigned>(token.content.size());
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
    bool resolved = false;
    auto tokenOffset = offsetFromPosition(state.text, line, start);

    if (std::find(parsedDocument->imports.begin(), parsedDocument->imports.end(), token.content) != parsedDocument->imports.end()) {
      semanticType = TOKEN_TYPE_NAMESPACE;
      resolved = true;
    }

    if (!resolved) {
      std::string memberReceiver;
      if (extractReceiverBeforeOffset(state.text, tokenOffset, memberReceiver) &&
          std::find(parsedDocument->imports.begin(), parsedDocument->imports.end(), memberReceiver) !=
              parsedDocument->imports.end()) {
        std::string moduleUri;
        std::string moduleText;
        if (findModuleDocumentByName(memberReceiver, uri, moduleUri, moduleText)) {
          auto moduleSymbols = collectSymbolsForUri(moduleUri, moduleText);
          for (const auto &moduleSymbol : moduleSymbols) {
            if (moduleSymbol.name != token.content || moduleSymbol.isMember) {
              continue;
            }
            semanticType = semanticTokenTypeFromSymbolKindForLsp(moduleSymbol.kind);
            resolved = true;
            break;
          }
        }
      }
    }

    if (!resolved) {
      SymbolEntry resolvedSymbol;
      std::string resolvedUri;
      unsigned lookupCharacter = start;
      if (length > 1) {
        lookupCharacter += 1;
      }
      if (resolveHoverSymbol(uri, state.text, line, lookupCharacter, token.content, tokenOffset, resolvedUri, resolvedSymbol)) {
        semanticType = semanticTokenTypeFromSymbolKindForLsp(resolvedSymbol.kind);
        resolved = true;
      }
    }

    std::string memberReceiver;
    if (!resolved && extractReceiverBeforeOffset(state.text, tokenOffset, memberReceiver) && !builtinsIndex.membersByType.empty()) {
      auto inferredType = inferBuiltinTypeForReceiver(state.text, memberReceiver, line, builtinsIndex);
      if (inferredType.has_value()) {
        auto typeMembers = builtinsIndex.membersByType.find(*inferredType);
        if (typeMembers != builtinsIndex.membersByType.end()) {
          auto memberIt = typeMembers->second.find(token.content);
          if (memberIt != typeMembers->second.end()) {
            semanticType = semanticTokenTypeFromSymbolKindForLsp(memberIt->second.kind);
            resolved = true;
          }
        }
      }
    }

    if (!resolved && !builtinsIndex.topLevelByName.empty()) {
      auto topLevel = builtinsIndex.topLevelByName.find(token.content);
      if (topLevel != builtinsIndex.topLevelByName.end()) {
        semanticType = semanticTokenTypeFromSymbolKindForLsp(topLevel->second.kind);
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

const std::vector<SemanticTokenEntry> &Server::getSemanticTokensForDocument(const std::string &uri, DocumentState &state) {
  refreshAnalysisState(state);
  if (!state.analysis.semanticTokensReady) {
    state.analysis.semanticTokens = buildSemanticTokensFromSemanticCache(uri, state);
    state.analysis.semanticTokensReady = true;
  }
  (void)uri;
  return state.analysis.semanticTokens;
}

const std::vector<starbytes::linguistics::LintFinding> &Server::getLintFindingsForDocument(const std::string &uri,
                                                                                            DocumentState &state) {
  refreshAnalysisState(state);
  if (!state.analysis.lintReady) {
    auto start = std::chrono::steady_clock::now();
    starbytes::linguistics::LinguisticsSession session(uri, state.text);
    auto lintResult = lintEngine.run(session, linguisticsConfig);
    state.analysis.lintFindings = std::move(lintResult.findings);
    state.analysis.lintReady = true;
    auto elapsed = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count());
    lspProfileLintNs += elapsed;
    maybeLogLspProfileSample("lint", elapsed, state.analysis.lintFindings.size());
  }
  return state.analysis.lintFindings;
}

const std::vector<starbytes::linguistics::Suggestion> &Server::getSuggestionsForDocument(const std::string &uri,
                                                                                          DocumentState &state) {
  refreshAnalysisState(state);
  if (!state.analysis.suggestionsReady) {
    auto start = std::chrono::steady_clock::now();
    starbytes::linguistics::LinguisticsSession session(uri, state.text);
    starbytes::linguistics::SuggestionRequest request;
    request.includeLowConfidence = false;
    auto suggestionResult = suggestionEngine.run(session, linguisticsConfig, request);
    state.analysis.suggestions = std::move(suggestionResult.suggestions);
    state.analysis.suggestionsReady = true;
    auto elapsed = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count());
    lspProfileSuggestionNs += elapsed;
    maybeLogLspProfileSample("suggest", elapsed, state.analysis.suggestions.size());
  }
  return state.analysis.suggestions;
}

const std::vector<starbytes::linguistics::CodeAction> &Server::getCodeActionsForDocument(const std::string &uri,
                                                                                          DocumentState &state,
                                                                                          bool safeOnly) {
  if (state.analysis.version != state.version || state.analysis.textHash != state.textHash) {
    state.analysis = {};
    state.analysis.version = state.version;
    state.analysis.textHash = state.textHash;
  }

  auto &ready = safeOnly ? state.analysis.safeActionsReady : state.analysis.allActionsReady;
  auto &actions = safeOnly ? state.analysis.safeActions : state.analysis.allActions;
  if (!ready) {
    auto start = std::chrono::steady_clock::now();
    const auto &findings = getLintFindingsForDocument(uri, state);
    const auto &suggestions = getSuggestionsForDocument(uri, state);
    starbytes::linguistics::CodeActionRequest request;
    request.safeOnly = safeOnly;
    auto actionResult = codeActionEngine.build(findings, suggestions, linguisticsConfig, request);
    actions = std::move(actionResult.actions);
    ready = true;
    auto elapsed = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count());
    lspProfileActionNs += elapsed;
    maybeLogLspProfileSample(safeOnly ? "actions.safe" : "actions.all", elapsed, actions.size());
  }
  return actions;
}

bool Server::getFormattedTextForDocument(const std::string &uri, DocumentState &state, std::string &formattedTextOut) {
  refreshAnalysisState(state);

  if(!state.analysis.formatReady) {
    auto start = std::chrono::steady_clock::now();
    starbytes::linguistics::LinguisticsSession session(uri, state.text);
    starbytes::linguistics::FormatRequest request;
    auto formatResult = formatterEngine.format(session, linguisticsConfig, request);
    state.analysis.formatOk = formatResult.ok;
    state.analysis.formattedText = std::move(formatResult.formattedText);
    state.analysis.formatReady = true;
    auto elapsed = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count());
    lspProfileFormatNs += elapsed;
    maybeLogLspProfileSample("format", elapsed, state.analysis.formattedText.empty() ? 0 : 1);
  }

  formattedTextOut = state.analysis.formattedText;
  return state.analysis.formatOk;
}

const BuiltinApiIndex *Server::getBuiltinsApiIndex() {
  std::string builtinsUri;
  std::string builtinsText;
  if (!getBuiltinsDocument(builtinsUri, builtinsText)) {
    return nullptr;
  }

  auto it = documents.find(builtinsUri);
  int version = 0;
  uint64_t textHash = hashText(builtinsText);
  if (it != documents.end()) {
    version = it->second.version;
    textHash = it->second.textHash;
  }

  if (builtinsIndexCache.valid && builtinsIndexCache.uri == builtinsUri &&
      builtinsIndexCache.version == version && builtinsIndexCache.textHash == textHash) {
    return &builtinsIndexCache.index;
  }

  builtinsIndexCache.valid = true;
  builtinsIndexCache.uri = builtinsUri;
  builtinsIndexCache.version = version;
  builtinsIndexCache.textHash = textHash;
  builtinsIndexCache.index = buildBuiltinApiIndex(builtinsUri, builtinsText);
  return &builtinsIndexCache.index;
}

std::vector<SymbolEntry> Server::collectSymbolsForUri(const std::string &uri, const std::string &text) {
  std::vector<SymbolEntry> symbols;
  if(symbolCache.tryGet(uri, text, symbols)) {
    return symbols;
  }
  symbols = collectSymbolsFromText(text);
  symbolCache.put(uri, text, symbols);
  return symbols;
}

void Server::invalidateSymbolCacheForUri(const std::string &uri) {
  symbolCache.invalidate(uri);
}

bool Server::findModuleDocumentByName(const std::string &moduleName,
                                      const std::string &anchorUri,
                                      std::string &uriOut,
                                      std::string &textOut) {
  auto scorePath = [&](const std::filesystem::path &fsPath) -> int {
    if (fsPath.stem() == moduleName && fsPath.extension() == ".starbint") {
      return 7;
    }
    if (fsPath.filename() == "main.starb" && fsPath.parent_path().filename() == moduleName) {
      return 6;
    }
    if (fsPath.stem() == moduleName && fsPath.extension() == ".starb") {
      return 5;
    }
    if (fsPath.extension() == ".starbint" && fsPath.parent_path().filename() == moduleName) {
      return 4;
    }
    if (fsPath.extension() == ".starb" && fsPath.parent_path().filename() == moduleName) {
      return 3;
    }
    return 0;
  };

  auto considerDocument = [&](const std::string &candidateUri, const std::string &candidateText, int &bestScore) {
    std::string path;
    if (!parseUriToPath(candidateUri, path)) {
      return;
    }
    int score = scorePath(std::filesystem::path(path));
    if (score > bestScore) {
      bestScore = score;
      uriOut = candidateUri;
      textOut = candidateText;
    }
  };

  int bestScore = 0;
  for (const auto &doc : documents) {
    considerDocument(doc.first, doc.second.text, bestScore);
  }
  if (bestScore >= 6) {
    return true;
  }

  std::vector<std::filesystem::path> searchRoots;
  std::unordered_set<std::string> seenRoots;
  auto addSearchBase = [&](const std::filesystem::path &base) {
    appendUniquePath(searchRoots, seenRoots, base);
    appendUniquePath(searchRoots, seenRoots, base / "modules");
    appendUniquePath(searchRoots, seenRoots, base / "stdlib");
    loadModulePathFileForLsp(base / ".starbmodpath", searchRoots, seenRoots);
  };
  auto addAncestors = [&](std::filesystem::path start) {
    if (start.empty()) {
      return;
    }
    start = std::filesystem::path(absolutePathString(start));
    for (unsigned depth = 0; depth < 8; ++depth) {
      addSearchBase(start);
      auto parent = start.parent_path();
      if (parent.empty() || parent == start) {
        break;
      }
      start = parent;
    }
  };

  std::filesystem::path anchorRoot;
  std::string anchorPath;
  if (!anchorUri.empty() && parseUriToPath(anchorUri, anchorPath)) {
    std::error_code ec;
    auto anchorFsPath = std::filesystem::path(anchorPath);
    anchorRoot = std::filesystem::is_directory(anchorFsPath, ec) && !ec ? anchorFsPath : anchorFsPath.parent_path();
    addAncestors(anchorRoot);
  }
  for (const auto &workspaceUri : workspaceRoots) {
    std::string workspacePath;
    if (!parseUriToPath(workspaceUri, workspacePath)) {
      continue;
    }
    addAncestors(std::filesystem::path(workspacePath));
  }
  addAncestors(std::filesystem::current_path());

  std::vector<std::filesystem::path> moduleDirs;
  std::unordered_set<std::string> seenModuleDirs;
  auto addModuleDir = [&](const std::filesystem::path &path) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec) || ec || !std::filesystem::is_directory(path, ec) || ec) {
      return;
    }
    appendUniquePath(moduleDirs, seenModuleDirs, path);
  };
  if (!anchorRoot.empty()) {
    addModuleDir(anchorRoot / moduleName);
    addModuleDir(anchorRoot.parent_path() / moduleName);
  }
  for (const auto &root : searchRoots) {
    addModuleDir(root / moduleName);
  }

  std::vector<std::filesystem::path> candidateFiles;
  std::unordered_set<std::string> seenFiles;
  auto addCandidateFile = [&](const std::filesystem::path &path) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec) || ec || !std::filesystem::is_regular_file(path, ec) || ec) {
      return;
    }
    appendUniquePath(candidateFiles, seenFiles, path);
  };

  for (const auto &root : searchRoots) {
    addCandidateFile(root / (moduleName + ".starbint"));
    addCandidateFile(root / (moduleName + ".starb"));
  }

  for (const auto &moduleDir : moduleDirs) {
    addCandidateFile(moduleDir / (moduleName + ".starbint"));
    addCandidateFile(moduleDir / "main.starb");
    addCandidateFile(moduleDir / (moduleName + ".starb"));

    std::error_code iterErr;
    for (std::filesystem::directory_iterator it(moduleDir, iterErr), end; it != end; it.increment(iterErr)) {
      if (iterErr) {
        break;
      }
      if (!it->is_regular_file()) {
        continue;
      }
      auto ext = it->path().extension().string();
      if (ext != ".starbint" && ext != ".starb") {
        continue;
      }
      addCandidateFile(it->path());
    }
  }

  for (const auto &candidatePath : candidateFiles) {
    auto candidateUri = pathToUri(candidatePath.string());
    std::string candidateText;
    if (!getDocumentTextByUri(candidateUri, candidateText) || candidateText.empty()) {
      continue;
    }
    considerDocument(candidateUri, candidateText, bestScore);
  }

  return bestScore > 0;
}

bool Server::resolveHoverSymbol(const std::string &uri,
                                const std::string &text,
                                unsigned line,
                                unsigned character,
                                const std::string &word,
                                size_t wordStart,
                                std::string &resolvedUriOut,
                                SymbolEntry &symbolOut) {
  auto lines = splitLinesCopy(text);

  auto findSymbolAtRegion = [&](const std::string &symbolUri,
                                const std::string &symbolText,
                                const Region &region) -> std::optional<HoverResolvedSymbol> {
    auto symbols = collectSymbolsForUri(symbolUri, symbolText);
    unsigned symbolLine = region.startLine > 0 ? region.startLine - 1 : 0;
    for (const auto &symbol : symbols) {
      if (symbol.line == symbolLine && symbol.start == region.startCol) {
        return HoverResolvedSymbol{symbolUri, symbol};
      }
    }
    const SymbolEntry *bestContainingSymbol = nullptr;
    for (const auto &symbol : symbols) {
      if (symbol.line != symbolLine || symbol.length == 0) {
        continue;
      }
      auto symbolEnd = symbol.start + symbol.length;
      if (region.startCol < symbol.start || region.startCol >= symbolEnd) {
        continue;
      }
      if (!bestContainingSymbol || symbol.length < bestContainingSymbol->length) {
        bestContainingSymbol = &symbol;
      }
    }
    if (bestContainingSymbol) {
      return HoverResolvedSymbol{symbolUri, *bestContainingSymbol};
    }
    return std::nullopt;
  };

  auto findSymbolByName = [&](const std::string &symbolUri,
                              const std::string &symbolText,
                              const std::string &name,
                              const std::string &containerName) -> std::optional<HoverResolvedSymbol> {
    auto symbols = collectSymbolsForUri(symbolUri, symbolText);
    for (const auto &symbol : symbols) {
      if (symbol.name != name) {
        continue;
      }
      if (!containerName.empty() && symbol.containerName != containerName) {
        continue;
      }
      if (containerName.empty() && symbol.isMember) {
        continue;
      }
      return HoverResolvedSymbol{symbolUri, symbol};
    }
    return std::nullopt;
  };

  auto findDeclaredSymbolOnLine = [&](const std::string &symbolName,
                                      const std::string &detail,
                                      unsigned lineNumber,
                                      unsigned startCol) -> std::optional<HoverResolvedSymbol> {
    auto symbols = collectSymbolsForUri(uri, text);
    const SymbolEntry *bestMatch = nullptr;
    for (const auto &symbol : symbols) {
      if (symbol.name != symbolName || symbol.detail != detail || symbol.line != lineNumber) {
        continue;
      }
      if (!bestMatch ||
          std::abs(static_cast<int>(symbol.start) - static_cast<int>(startCol)) <
              std::abs(static_cast<int>(bestMatch->start) - static_cast<int>(startCol))) {
        bestMatch = &symbol;
      }
    }
    if (!bestMatch) {
      return std::nullopt;
    }
    return HoverResolvedSymbol{uri, *bestMatch};
  };

  auto synthesizeSymbol = [&](ASTIdentifier *id,
                              ASTStmt *owner,
                              ASTType *typeOverride,
                              const std::string &displayName,
                              const std::string &detailOverride,
                              const std::string &typeTextOverride = std::string()) -> std::optional<HoverResolvedSymbol> {
    if (!id || !owner) {
      return std::nullopt;
    }
    if (auto existing = findSymbolAtRegion(uri, text, id->codeRegion)) {
      auto typeText = typeOverride ? hoverTypeToString(typeOverride) : typeTextOverride;
      if (owner->type == VAR_DECL) {
        auto *varDecl = static_cast<ASTVarDecl *>(owner);
        bool isConst = varDecl->isConst;
        existing->symbol.kind = isConst ? SYMBOL_KIND_CONSTANT : SYMBOL_KIND_VARIABLE;
        if (existing->symbol.detail.empty() || existing->symbol.detail == "variable" ||
            existing->symbol.detail == "constant") {
          existing->symbol.detail = detailOverride.empty() ? (isConst ? "constant" : "variable") : detailOverride;
        }
        if (!typeText.empty()) {
          existing->symbol.signature = hoverVarSignature(isConst, existing->symbol.name, typeText);
        }
      } else if (owner->type == SECURE_DECL) {
        existing->symbol.kind = SYMBOL_KIND_VARIABLE;
        if (existing->symbol.detail.empty()) {
          existing->symbol.detail = detailOverride.empty() ? "catch error" : detailOverride;
        }
        if (!typeText.empty()) {
          existing->symbol.signature = "catch " + existing->symbol.name + ":" + typeText;
        }
      } else if (owner->type == FUNC_DECL || owner->type == CLASS_CTOR_DECL) {
        existing->symbol.kind = SYMBOL_KIND_VARIABLE;
        if (existing->symbol.detail.empty()) {
          existing->symbol.detail = detailOverride.empty() ? "parameter" : detailOverride;
        }
        if (!typeText.empty()) {
          existing->symbol.signature = hoverVarSignature(false, existing->symbol.name, typeText, "param");
        }
      }
      return existing;
    }

    SymbolEntry symbol;
    symbol.name = displayName.empty() ? sourceSliceForRegion(lines, id->codeRegion) : displayName;
    symbol.line = id->codeRegion.startLine > 0 ? id->codeRegion.startLine - 1 : 0;
    symbol.start = id->codeRegion.startCol;
    symbol.length = id->codeRegion.endCol > id->codeRegion.startCol ? id->codeRegion.endCol - id->codeRegion.startCol
                                                                     : static_cast<unsigned>(symbol.name.size());
    symbol.documentation = hoverDocCommentForLine(lines, symbol.line);

    if (owner->type == VAR_DECL) {
      auto *varDecl = static_cast<ASTVarDecl *>(owner);
      bool isConst = varDecl->isConst;
      symbol.kind = isConst ? SYMBOL_KIND_CONSTANT : SYMBOL_KIND_VARIABLE;
      symbol.detail = detailOverride.empty() ? (isConst ? "constant" : "variable") : detailOverride;
      symbol.signature = hoverVarSignature(isConst,
                                           symbol.name,
                                           typeOverride ? hoverTypeToString(typeOverride) : typeTextOverride);
      return HoverResolvedSymbol{uri, symbol};
    }

    if (owner->type == SECURE_DECL) {
      symbol.kind = SYMBOL_KIND_VARIABLE;
      symbol.detail = detailOverride.empty() ? "catch error" : detailOverride;
      symbol.signature = "catch " + symbol.name;
      if (typeOverride) {
        symbol.signature += ":";
        symbol.signature += hoverTypeToString(typeOverride);
      }
      return HoverResolvedSymbol{uri, symbol};
    }

    if (owner->type == FUNC_DECL || owner->type == CLASS_CTOR_DECL) {
      symbol.kind = SYMBOL_KIND_VARIABLE;
      symbol.detail = detailOverride.empty() ? "parameter" : detailOverride;
      symbol.signature = hoverVarSignature(false,
                                           symbol.name,
                                           typeOverride ? hoverTypeToString(typeOverride) : typeTextOverride,
                                           "param");
      return HoverResolvedSymbol{uri, symbol};
    }

    return std::nullopt;
  };

  auto stateIt = documents.find(uri);
  if (stateIt == documents.end()) {
    return false;
  }
  const auto *parsedDocument = getSemanticResolvedDocumentForUri(uri, stateIt->second);
  if (!parsedDocument || !parsedDocument->mainTable) {
    return false;
  }

  auto directImports = parsedDocument->imports;
  std::unordered_map<std::string, std::shared_ptr<Semantics::SymbolTable>> currentDeps;
  for (const auto &importName : directImports) {
    std::string depUri;
    std::string depText;
    if (!findModuleDocumentByName(importName, uri, depUri, depText)) {
      continue;
    }
    auto depIt = documents.find(depUri);
    if (depIt == documents.end()) {
      continue;
    }
    if (const auto *depDocument = getSemanticResolvedDocumentForUri(depUri, depIt->second);
        depDocument && depDocument->mainTable) {
      currentDeps[importName] = depDocument->mainTable;
    }
  }

  std::function<std::string(const std::string &)> inferCurrentDocumentVarTypeText;

  Semantics::STableContext resolutionContext;
  resolutionContext.mainBorrowed = parsedDocument->mainTable.get();
  appendImportedSymbolTablesForHover(resolutionContext, directImports, currentDeps);

  auto applySemanticEntrySymbolData = [&](SymbolEntry &symbol,
                                          Semantics::SymbolTable::Entry *entry) {
    if (!entry) {
      return;
    }
    if (entry->type == Semantics::SymbolTable::Entry::Var) {
      auto *var = static_cast<Semantics::SymbolTable::Var *>(entry->data);
      bool isConst = var && var->isReadonly;
      if (symbol.kind == SYMBOL_KIND_CONSTANT) {
        isConst = true;
      }
      symbol.kind = isConst ? SYMBOL_KIND_CONSTANT : SYMBOL_KIND_VARIABLE;
      if (symbol.detail.empty() || symbol.detail == "variable" || symbol.detail == "constant") {
        symbol.detail = isConst ? "constant" : "variable";
      }
      if (var) {
        symbol.signature = hoverVarSignature(isConst, symbol.name, var->type);
        symbol.isDeprecated = var->isDeprecated;
        symbol.deprecationMessage = var->deprecationMessage;
      }
    } else if (entry->type == Semantics::SymbolTable::Entry::Function) {
      auto *func = static_cast<Semantics::SymbolTable::Function *>(entry->data);
      if (func) {
        symbol.isDeprecated = func->isDeprecated;
        symbol.deprecationMessage = func->deprecationMessage;
      }
    } else if (entry->type == Semantics::SymbolTable::Entry::Class) {
      auto *klass = static_cast<Semantics::SymbolTable::Class *>(entry->data);
      if (klass) {
        symbol.isDeprecated = klass->isDeprecated;
        symbol.deprecationMessage = klass->deprecationMessage;
      }
    } else if (entry->type == Semantics::SymbolTable::Entry::Interface) {
      auto *interfaceData = static_cast<Semantics::SymbolTable::Interface *>(entry->data);
      if (interfaceData) {
        symbol.isDeprecated = interfaceData->isDeprecated;
        symbol.deprecationMessage = interfaceData->deprecationMessage;
      }
    } else if (entry->type == Semantics::SymbolTable::Entry::TypeAlias) {
      auto *aliasData = static_cast<Semantics::SymbolTable::TypeAlias *>(entry->data);
      if (aliasData) {
        symbol.isDeprecated = aliasData->isDeprecated;
        symbol.deprecationMessage = aliasData->deprecationMessage;
      }
    }
  };

  auto makeSymbolFromEntry = [&](Semantics::SymbolTable::Entry *entry,
                                 const std::string &moduleNameHint,
                                 const std::string &containerNameHint) -> std::optional<HoverResolvedSymbol> {
    if (!entry) {
      return std::nullopt;
    }
    if (!moduleNameHint.empty()) {
      std::string moduleUri;
      std::string moduleText;
      if (findModuleDocumentByName(moduleNameHint, uri, moduleUri, moduleText)) {
        if (containerNameHint == moduleNameHint) {
          if (auto found = findSymbolByName(moduleUri, moduleText, entry->name, std::string())) {
            applySemanticEntrySymbolData(found->symbol, entry);
            if (entry->type == Semantics::SymbolTable::Entry::Var &&
                found->symbol.signature.find(':') == std::string::npos) {
              auto inferredType = inferCurrentDocumentVarTypeText(entry->name);
              if (!inferredType.empty()) {
                bool isConst = found->symbol.kind == SYMBOL_KIND_CONSTANT;
                found->symbol.signature = hoverVarSignature(isConst, found->symbol.name, inferredType);
              }
            }
            return found;
          }
        }
        if (auto found = findSymbolByName(moduleUri, moduleText, entry->name, containerNameHint)) {
          applySemanticEntrySymbolData(found->symbol, entry);
          return found;
        }
      }
    }
    if (auto found = findSymbolByName(uri, text, entry->name, containerNameHint)) {
      applySemanticEntrySymbolData(found->symbol, entry);
      if (entry->type == Semantics::SymbolTable::Entry::Var &&
          found->symbol.signature.find(':') == std::string::npos) {
        auto inferredType = inferCurrentDocumentVarTypeText(entry->name);
        if (!inferredType.empty()) {
          bool isConst = found->symbol.kind == SYMBOL_KIND_CONSTANT;
          found->symbol.signature = hoverVarSignature(isConst, found->symbol.name, inferredType);
        }
      }
      return found;
    }
    return std::nullopt;
  };

  auto findLocalBinding = [&](const std::vector<HoverFrame> &frames,
                              std::shared_ptr<ASTScope> currentScope,
                              const std::string &lookupName,
                              const std::string &sourceName,
                              std::shared_ptr<ASTScope> &outerScopeStart) -> const HoverBinding * {
    outerScopeStart = nullptr;
    for (auto it = frames.rbegin(); it != frames.rend(); ++it) {
      for (auto bindingIt = it->bindings.rbegin(); bindingIt != it->bindings.rend(); ++bindingIt) {
        if (bindingIt->lookupName == lookupName || bindingIt->displayName == sourceName) {
          return &*bindingIt;
        }
      }
      if (it->functionBoundary) {
        outerScopeStart = it->scope ? it->scope->parentScope : nullptr;
        break;
      }
    }
    return nullptr;
  };

  std::function<ASTType *(ASTExpr *, std::vector<HoverFrame> &, ASTClassDecl *)> inferExprType;
  std::function<std::string(ASTExpr *, std::vector<HoverFrame> &, ASTClassDecl *)> inferExprTypeText;
  std::function<std::optional<HoverResolvedSymbol>(ASTExpr *, std::vector<HoverFrame> &, ASTClassDecl *)> resolveExprHover;
  std::function<bool(ASTStmt *, std::vector<HoverFrame> &, ASTClassDecl *)> walkStmt;
  std::function<bool(ASTBlockStmt *, std::vector<HoverFrame> &, ASTClassDecl *, bool)> walkBlock;

  auto findClassEntry = [&](ASTType *type, std::shared_ptr<ASTScope> scope) -> Semantics::SymbolTable::Entry * {
    if (!type) {
      return nullptr;
    }
    auto *entry = resolutionContext.findEntryNoDiag(type->getName(), scope);
    if (!entry) {
      entry = resolutionContext.findEntryByEmittedNoDiag(type->getName());
    }
    if (entry && entry->type == Semantics::SymbolTable::Entry::Class) {
      return entry;
    }
    return nullptr;
  };

  std::function<Semantics::SymbolTable::Var *(Semantics::SymbolTable::Class *, const std::string &, std::shared_ptr<ASTScope>)>
      findClassFieldRecursive = [&](Semantics::SymbolTable::Class *classData,
                                    const std::string &fieldName,
                                    std::shared_ptr<ASTScope> scope) -> Semantics::SymbolTable::Var * {
    if (!classData) {
      return nullptr;
    }
    for (auto *field : classData->fields) {
      if (field && field->name == fieldName) {
        return field;
      }
    }
    if (!classData->superClassType) {
      return nullptr;
    }
    auto *superEntry = findClassEntry(classData->superClassType, scope);
    if (!superEntry) {
      return nullptr;
    }
    return findClassFieldRecursive(static_cast<Semantics::SymbolTable::Class *>(superEntry->data), fieldName, scope);
  };

  std::function<Semantics::SymbolTable::Function *(Semantics::SymbolTable::Class *, const std::string &, std::shared_ptr<ASTScope>)>
      findClassMethodRecursive = [&](Semantics::SymbolTable::Class *classData,
                                     const std::string &methodName,
                                     std::shared_ptr<ASTScope> scope) -> Semantics::SymbolTable::Function * {
    if (!classData) {
      return nullptr;
    }
    for (auto *method : classData->instMethods) {
      if (method && method->name == methodName) {
        return method;
      }
    }
    if (!classData->superClassType) {
      return nullptr;
    }
    auto *superEntry = findClassEntry(classData->superClassType, scope);
    if (!superEntry) {
      return nullptr;
    }
    return findClassMethodRecursive(static_cast<Semantics::SymbolTable::Class *>(superEntry->data), methodName, scope);
  };

  auto resolveScopeAccessEntry = [&](ASTExpr *memberExpr) -> Semantics::SymbolTable::Entry * {
    if (!memberExpr || memberExpr->type != MEMBER_EXPR || !memberExpr->resolvedScope ||
        !memberExpr->rightExpr || !memberExpr->rightExpr->id) {
      return nullptr;
    }

    auto *entry = resolutionContext.findEntryInExactScopeNoDiag(memberExpr->rightExpr->id->val, memberExpr->resolvedScope);
    if (entry) {
      return entry;
    }

    if (memberExpr->leftExpr && memberExpr->leftExpr->type == ID_EXPR && memberExpr->leftExpr->id) {
      auto leftName = sourceSliceForRegion(lines, memberExpr->leftExpr->id->codeRegion);
      if (std::find(directImports.begin(), directImports.end(), leftName) != directImports.end()) {
        auto depTableIt = currentDeps.find(leftName);
        if (depTableIt != currentDeps.end() && depTableIt->second) {
          if (auto *depEntries =
                  depTableIt->second->findEntriesInExactScope(memberExpr->rightExpr->id->val, ASTScopeGlobal);
              depEntries && !depEntries->empty()) {
            return depEntries->front();
          }
        }
      }
    }

    return nullptr;
  };

  inferExprType = [&](ASTExpr *expr, std::vector<HoverFrame> &frames, ASTClassDecl *currentClass) -> ASTType * {
    if (!expr) {
      return nullptr;
    }
    switch (expr->type) {
      case STR_LITERAL:
        return STRING_TYPE;
      case REGEX_LITERAL:
        return REGEX_TYPE;
      case BOOL_LITERAL:
        return BOOL_TYPE;
      case NUM_LITERAL: {
        auto *literal = static_cast<ASTLiteralExpr *>(expr);
        if (literal->floatValue.has_value()) {
          return DOUBLE_TYPE;
        }
        return INT_TYPE;
      }
      case ARRAY_EXPR:
        return ARRAY_TYPE;
      case DICT_EXPR:
        return DICTIONARY_TYPE;
      case ID_EXPR: {
        auto *id = expr->id;
        if (!id) {
          return nullptr;
        }
        std::shared_ptr<ASTScope> outerScopeStart;
        if (const auto *binding =
                findLocalBinding(frames, frames.empty() ? ASTScopeGlobal : frames.back().scope, id->val, word, outerScopeStart)) {
          return binding->type;
        }
        auto lookupScope = outerScopeStart ? outerScopeStart : (frames.empty() ? ASTScopeGlobal : frames.back().scope);
        auto *entry = resolutionContext.findEntryNoDiag(id->val, lookupScope);
        if (!entry) {
          return nullptr;
        }
        if (entry->type == Semantics::SymbolTable::Entry::Var) {
          return static_cast<Semantics::SymbolTable::Var *>(entry->data)->type;
        }
        if (entry->type == Semantics::SymbolTable::Entry::Class) {
          return static_cast<Semantics::SymbolTable::Class *>(entry->data)->classType;
        }
        if (entry->type == Semantics::SymbolTable::Entry::Function) {
          return static_cast<Semantics::SymbolTable::Function *>(entry->data)->returnType;
        }
        return nullptr;
      }
      case MEMBER_EXPR: {
        if (!expr->rightExpr || !expr->rightExpr->id) {
          return nullptr;
        }
        if (expr->isScopeAccess && expr->resolvedScope) {
          auto *entry = resolveScopeAccessEntry(expr);
          if (!entry) {
            return nullptr;
          }
          if (entry->type == Semantics::SymbolTable::Entry::Var) {
            return static_cast<Semantics::SymbolTable::Var *>(entry->data)->type;
          }
          if (entry->type == Semantics::SymbolTable::Entry::Class) {
            return static_cast<Semantics::SymbolTable::Class *>(entry->data)->classType;
          }
          if (entry->type == Semantics::SymbolTable::Entry::Function) {
            return static_cast<Semantics::SymbolTable::Function *>(entry->data)->returnType;
          }
          return nullptr;
        }

        auto *leftType = inferExprType(expr->leftExpr, frames, currentClass);
        if (!leftType) {
          return nullptr;
        }
        auto memberName = expr->rightExpr->id->val;
        if (isStringTypeForHover(leftType)) {
          if (memberName == "length") {
            return INT_TYPE;
          }
          if (memberName == "trim" || memberName == "lower" || memberName == "upper" || memberName == "slice" ||
              memberName == "replace" || memberName == "repeat") {
            return STRING_TYPE;
          }
          return ANY_TYPE;
        }
        if (isArrayTypeForHover(leftType) || isDictTypeForHover(leftType) || isMapTypeForHover(leftType)) {
          if (memberName == "length") {
            return INT_TYPE;
          }
          return ANY_TYPE;
        }
        auto *classEntry = findClassEntry(leftType, frames.empty() ? ASTScopeGlobal : frames.back().scope);
        if (classEntry) {
          auto *classData = static_cast<Semantics::SymbolTable::Class *>(classEntry->data);
          if (auto *field = findClassFieldRecursive(classData, memberName, frames.empty() ? ASTScopeGlobal : frames.back().scope)) {
            return field->type;
          }
          if (auto *method =
                  findClassMethodRecursive(classData, memberName, frames.empty() ? ASTScopeGlobal : frames.back().scope)) {
            return method->returnType;
          }
        }
        return nullptr;
      }
      case IVKE_EXPR: {
        if (expr->isConstructorCall && expr->callee) {
          if (expr->callee->type == ID_EXPR && expr->callee->id) {
            auto *entry = resolutionContext.findEntryNoDiag(expr->callee->id->val,
                                                              frames.empty() ? ASTScopeGlobal : frames.back().scope);
            if (entry && entry->type == Semantics::SymbolTable::Entry::Class) {
              return static_cast<Semantics::SymbolTable::Class *>(entry->data)->classType;
            }
          } else if (expr->callee->type == MEMBER_EXPR && expr->callee->rightExpr && expr->callee->rightExpr->id &&
                     expr->callee->resolvedScope) {
            auto *entry = resolveScopeAccessEntry(expr->callee);
            if (entry && entry->type == Semantics::SymbolTable::Entry::Class) {
              return static_cast<Semantics::SymbolTable::Class *>(entry->data)->classType;
            }
          }
        }
        return inferExprType(expr->callee, frames, currentClass);
      }
      default:
        return nullptr;
    }
  };

  auto functionReturnTypeFromSignature = [&](const std::string &signature) -> std::string {
    auto closePos = signature.rfind(')');
    if (closePos == std::string::npos) {
      return {};
    }
    return trimCopy(signature.substr(closePos + 1));
  };

  auto importedModuleReturnTypeText = [&](ASTExpr *memberExpr) -> std::string {
    if (!memberExpr || memberExpr->type != MEMBER_EXPR || !memberExpr->leftExpr || !memberExpr->rightExpr ||
        !memberExpr->rightExpr->id || memberExpr->leftExpr->type != ID_EXPR || !memberExpr->leftExpr->id) {
      return {};
    }
    auto moduleName = memberExpr->leftExpr->id->val;
    if (moduleName.empty() ||
        std::find(directImports.begin(), directImports.end(), moduleName) == directImports.end()) {
      return {};
    }

    std::string moduleUri;
    std::string moduleText;
    if (!findModuleDocumentByName(moduleName, uri, moduleUri, moduleText)) {
      return {};
    }
    if (auto found = findSymbolByName(moduleUri, moduleText, memberExpr->rightExpr->id->val, std::string())) {
      return functionReturnTypeFromSignature(found->symbol.signature);
    }
    return {};
  };

  auto importedModuleCallTypeTextFromRegion = [&](const Region &region) -> std::string {
    if (region.startLine == 0 || region.startLine != region.endLine) {
      return {};
    }
    auto exprText = sourceSliceForRegion(lines, region);
    if (exprText.empty()) {
      return {};
    }

    static const std::regex moduleCallRegex(R"(^\s*([A-Za-z_][A-Za-z0-9_]*)\.([A-Za-z_][A-Za-z0-9_]*)\s*\()");
    std::smatch match;
    if (!std::regex_search(exprText, match, moduleCallRegex) || match.size() < 3) {
      return {};
    }

    auto moduleName = match[1].str();
    auto symbolName = match[2].str();
    if (std::find(directImports.begin(), directImports.end(), moduleName) == directImports.end()) {
      return {};
    }

    std::string moduleUri;
    std::string moduleText;
    if (!findModuleDocumentByName(moduleName, uri, moduleUri, moduleText)) {
      return {};
    }
    if (auto found = findSymbolByName(moduleUri, moduleText, symbolName, std::string())) {
      return functionReturnTypeFromSignature(found->symbol.signature);
    }
    return {};
  };

  inferCurrentDocumentVarTypeText = [&](const std::string &varName) -> std::string {
    for (auto *stmt : parsedDocument->statements) {
      if (!stmt || stmt->type != VAR_DECL) {
        continue;
      }
      auto *varDecl = static_cast<ASTVarDecl *>(static_cast<ASTDecl *>(stmt));
      for (const auto &spec : varDecl->specs) {
        if (!spec.id || spec.id->val != varName) {
          continue;
        }
        if (spec.type) {
          return hoverTypeToString(spec.type);
        }
        if (!spec.expr) {
          return {};
        }
        if (auto inferred = importedModuleCallTypeTextFromRegion(spec.expr->codeRegion); !inferred.empty()) {
          return inferred;
        }
        if (spec.expr->type == IVKE_EXPR && spec.expr->callee) {
          if (auto inferred = importedModuleCallTypeTextFromRegion(spec.expr->callee->codeRegion); !inferred.empty()) {
            return inferred;
          }
        }
        return {};
      }
    }
    return {};
  };

  inferExprTypeText = [&](ASTExpr *expr, std::vector<HoverFrame> &frames, ASTClassDecl *currentClass) -> std::string {
    if (!expr) {
      return {};
    }
    if (auto *type = inferExprType(expr, frames, currentClass)) {
      return hoverTypeToString(type);
    }

    switch (expr->type) {
      case ID_EXPR: {
        auto *id = expr->id;
        if (!id) {
          return {};
        }
        std::shared_ptr<ASTScope> outerScopeStart;
        if (const auto *binding =
                findLocalBinding(frames, frames.empty() ? ASTScopeGlobal : frames.back().scope, id->val, word, outerScopeStart)) {
          return binding->renderedType;
        }
        auto lookupScope = outerScopeStart ? outerScopeStart : (frames.empty() ? ASTScopeGlobal : frames.back().scope);
        auto *entry = resolutionContext.findEntryNoDiag(id->val, lookupScope);
        if (!entry) {
          return {};
        }
        if (entry->type == Semantics::SymbolTable::Entry::Function) {
          auto *function = static_cast<Semantics::SymbolTable::Function *>(entry->data);
          if (function && function->returnType) {
            return hoverTypeToString(function->returnType);
          }
        }
        return {};
      }
      case MEMBER_EXPR: {
        if (auto importedType = importedModuleReturnTypeText(expr); !importedType.empty()) {
          return importedType;
        }
        if (!(expr->isScopeAccess && expr->resolvedScope)) {
          return {};
        }
        auto *entry = resolveScopeAccessEntry(expr);
        if (!entry) {
          return {};
        }
        if (entry->type == Semantics::SymbolTable::Entry::Function) {
          auto *function = static_cast<Semantics::SymbolTable::Function *>(entry->data);
          if (function && function->returnType) {
            return hoverTypeToString(function->returnType);
          }
          std::string moduleHint;
          std::string containerHint = expr->resolvedScope ? expr->resolvedScope->name : std::string();
          if (expr->leftExpr && expr->leftExpr->type == ID_EXPR && expr->leftExpr->id) {
            auto leftName = sourceSliceForRegion(lines, expr->leftExpr->id->codeRegion);
            if (std::find(directImports.begin(), directImports.end(), leftName) != directImports.end()) {
              moduleHint = leftName;
            }
          }
          if (auto found = makeSymbolFromEntry(entry, moduleHint, containerHint)) {
            return functionReturnTypeFromSignature(found->symbol.signature);
          }
        }
        return {};
      }
      case IVKE_EXPR:
        if (expr->callee) {
          if (auto importedType = importedModuleReturnTypeText(expr->callee); !importedType.empty()) {
            return importedType;
          }
          if (auto importedType = importedModuleCallTypeTextFromRegion(expr->codeRegion); !importedType.empty()) {
            return importedType;
          }
          return inferExprTypeText(expr->callee, frames, currentClass);
        }
        return {};
      default:
        return {};
    }
  };

  auto inferBindingTypeText = [&](const HoverBinding *binding) -> std::string {
    if (!binding) {
      return {};
    }
    if (!binding->renderedType.empty()) {
      return binding->renderedType;
    }
    if (!binding->declOwner || binding->declOwner->type != VAR_DECL || !binding->declId) {
      return {};
    }

    auto *varDecl = static_cast<ASTVarDecl *>(binding->declOwner);
    for (const auto &spec : varDecl->specs) {
      if (!spec.id || spec.id != binding->declId) {
        continue;
      }
      if (spec.type) {
        return hoverTypeToString(spec.type);
      }
      if (!spec.expr) {
        return {};
      }
      if (auto inferred = importedModuleCallTypeTextFromRegion(spec.expr->codeRegion); !inferred.empty()) {
        return inferred;
      }
      if (spec.expr->type == IVKE_EXPR && spec.expr->callee) {
        if (auto inferred = importedModuleCallTypeTextFromRegion(spec.expr->callee->codeRegion); !inferred.empty()) {
          return inferred;
        }
      }
      return {};
    }
    return {};
  };

  resolveExprHover = [&](ASTExpr *expr,
                         std::vector<HoverFrame> &frames,
                         ASTClassDecl *currentClass) -> std::optional<HoverResolvedSymbol> {
    if (!expr) {
      return std::nullopt;
    }

    if (expr->type == ID_EXPR && expr->id && regionContainsPosition(expr->id->codeRegion, line + 1, character)) {
      std::shared_ptr<ASTScope> outerScopeStart;
      if (const auto *binding =
              findLocalBinding(frames, frames.empty() ? ASTScopeGlobal : frames.back().scope, expr->id->val, word,
                               outerScopeStart)) {
        auto bindingTypeText = inferBindingTypeText(binding);
        return synthesizeSymbol(binding->declId,
                                binding->declOwner,
                                binding->type,
                                binding->displayName,
                                "",
                                bindingTypeText);
      }
      auto lookupScope = outerScopeStart ? outerScopeStart : (frames.empty() ? ASTScopeGlobal : frames.back().scope);
      auto *entry = resolutionContext.findEntryNoDiag(expr->id->val, lookupScope);
      if (entry) {
        return makeSymbolFromEntry(entry, std::string(), std::string());
      }
      return std::nullopt;
    }

    if (expr->type == MEMBER_EXPR && expr->rightExpr && expr->rightExpr->id &&
        regionContainsPosition(expr->rightExpr->id->codeRegion, line + 1, character)) {
      if (expr->isScopeAccess && expr->resolvedScope) {
        auto *entry = resolveScopeAccessEntry(expr);
        std::string moduleHint;
        std::string containerHint = expr->resolvedScope->name;
        if (!entry) {
          return std::nullopt;
        }
        if (expr->leftExpr && expr->leftExpr->type == ID_EXPR && expr->leftExpr->id) {
          auto leftName = sourceSliceForRegion(lines, expr->leftExpr->id->codeRegion);
          if (moduleHint.empty() && std::find(directImports.begin(), directImports.end(), leftName) != directImports.end()) {
            moduleHint = leftName;
          }
        }
        return makeSymbolFromEntry(entry, moduleHint, containerHint);
      }

      auto *leftType = inferExprType(expr->leftExpr, frames, currentClass);
      if (!leftType) {
        return std::nullopt;
      }

      auto memberName = sourceSliceForRegion(lines, expr->rightExpr->id->codeRegion);
      if (isStringTypeForHover(leftType) || isArrayTypeForHover(leftType) || isDictTypeForHover(leftType) ||
          isMapTypeForHover(leftType) || isRegexTypeForHover(leftType)) {
        return std::nullopt;
      }

      auto *classEntry = findClassEntry(leftType, frames.empty() ? ASTScopeGlobal : frames.back().scope);
      if (classEntry) {
        auto *classData = static_cast<Semantics::SymbolTable::Class *>(classEntry->data);
        auto *field = findClassFieldRecursive(classData, memberName, frames.empty() ? ASTScopeGlobal : frames.back().scope);
        if (field) {
          return findSymbolByName(uri, text, field->name, leftType->getName().str());
        }
        auto *method =
            findClassMethodRecursive(classData, memberName, frames.empty() ? ASTScopeGlobal : frames.back().scope);
        if (method) {
          return findSymbolByName(uri, text, method->name, leftType->getName().str());
        }
      }
      return std::nullopt;
    }

    if (expr->callee) {
      if (auto resolved = resolveExprHover(expr->callee, frames, currentClass)) {
        return resolved;
      }
    }
    if (expr->leftExpr) {
      if (auto resolved = resolveExprHover(expr->leftExpr, frames, currentClass)) {
        return resolved;
      }
    }
    if (expr->middleExpr) {
      if (auto resolved = resolveExprHover(expr->middleExpr, frames, currentClass)) {
        return resolved;
      }
    }
    if (expr->rightExpr) {
      if (auto resolved = resolveExprHover(expr->rightExpr, frames, currentClass)) {
        return resolved;
      }
    }
    for (auto *arg : expr->exprArrayData) {
      if (auto resolved = resolveExprHover(arg, frames, currentClass)) {
        return resolved;
      }
    }
    for (const auto &entry : expr->dictExpr) {
      if (auto resolved = resolveExprHover(entry.first, frames, currentClass)) {
        return resolved;
      }
      if (auto resolved = resolveExprHover(entry.second, frames, currentClass)) {
        return resolved;
      }
    }
    return std::nullopt;
  };

  walkBlock = [&](ASTBlockStmt *block,
                  std::vector<HoverFrame> &frames,
                  ASTClassDecl *currentClass,
                  bool functionBoundary) -> bool {
    if (!block) {
      return false;
    }
    frames.push_back(HoverFrame{block->parentScope, functionBoundary, {}});
    for (auto *stmt : block->body) {
      if (walkStmt(stmt, frames, currentClass)) {
        frames.pop_back();
        return true;
      }
    }
    frames.pop_back();
    return false;
  };

  walkStmt = [&](ASTStmt *stmt, std::vector<HoverFrame> &frames, ASTClassDecl *currentClass) -> bool {
    if (!stmt) {
      return false;
    }

    if (stmt->type & EXPR) {
      return resolveExprHover(static_cast<ASTExpr *>(stmt), frames, currentClass).has_value() &&
             ([&]() {
               auto resolved = resolveExprHover(static_cast<ASTExpr *>(stmt), frames, currentClass);
               if (!resolved.has_value()) {
                 return false;
               }
               resolvedUriOut = resolved->uri;
               symbolOut = resolved->symbol;
               return true;
             })();
    }

    if (!(stmt->type & DECL)) {
      return false;
    }

    auto *decl = static_cast<ASTDecl *>(stmt);
    switch (decl->type) {
      case VAR_DECL: {
        auto *varDecl = static_cast<ASTVarDecl *>(decl);
        for (auto &spec : varDecl->specs) {
          auto *resolvedType = spec.type ? spec.type : inferExprType(spec.expr, frames, currentClass);
          auto resolvedTypeText = resolvedType ? hoverTypeToString(resolvedType)
                                               : inferExprTypeText(spec.expr, frames, currentClass);
          if (spec.expr) {
            if (auto resolved = resolveExprHover(spec.expr, frames, currentClass)) {
              resolvedUriOut = resolved->uri;
              symbolOut = resolved->symbol;
              return true;
            }
          }
          if (spec.id && regionContainsPosition(spec.id->codeRegion, line + 1, character)) {
            if (auto resolved =
                    synthesizeSymbol(spec.id,
                                     decl,
                                     resolvedType,
                                     sourceSliceForRegion(lines, spec.id->codeRegion),
                                     "",
                                     resolvedTypeText)) {
              resolvedUriOut = resolved->uri;
              symbolOut = resolved->symbol;
              return true;
            }
          }
          if (!frames.empty() && spec.id) {
            frames.back().bindings.push_back(
                HoverBinding{spec.id, decl, resolvedType, varDecl->isConst, spec.id->val,
                             sourceSliceForRegion(lines, spec.id->codeRegion), resolvedTypeText});
          }
        }
        return false;
      }
      case FUNC_DECL: {
        auto *funcDecl = static_cast<ASTFuncDecl *>(decl);
        if (funcDecl->funcId && regionContainsPosition(funcDecl->funcId->codeRegion, line + 1, character)) {
          if (auto resolved = findSymbolAtRegion(uri, text, funcDecl->funcId->codeRegion)) {
            resolvedUriOut = resolved->uri;
            symbolOut = resolved->symbol;
            return true;
          }
        }
        if (!frames.empty() && funcDecl->funcId) {
          frames.back().bindings.push_back(
              HoverBinding{funcDecl->funcId, decl, funcDecl->returnType, false, funcDecl->funcId->val,
                           sourceSliceForRegion(lines, funcDecl->funcId->codeRegion)});
        }
        if (!funcDecl->blockStmt) {
          return false;
        }
        frames.push_back(HoverFrame{funcDecl->blockStmt->parentScope, true, {}});
        for (const auto &param : funcDecl->params) {
          if (!param.first) {
            continue;
          }
          if (regionContainsPosition(param.first->codeRegion, line + 1, character)) {
            if (auto resolved = synthesizeSymbol(param.first,
                                                 decl,
                                                 param.second,
                                                 sourceSliceForRegion(lines, param.first->codeRegion),
                                                 "parameter")) {
              resolvedUriOut = resolved->uri;
              symbolOut = resolved->symbol;
              frames.pop_back();
              return true;
            }
          }
          frames.back().bindings.push_back(
              HoverBinding{param.first, decl, param.second, false, param.first->val,
                           sourceSliceForRegion(lines, param.first->codeRegion)});
        }
        if (currentClass) {
          auto selfName = std::string("self");
          auto selfRegion = funcDecl->funcId ? funcDecl->funcId->codeRegion : Region{};
          auto *selfId = new ASTIdentifier();
          selfId->val = selfName;
          selfId->codeRegion = selfRegion;
          frames.back().bindings.push_back(HoverBinding{selfId, decl, currentClass->classType, false, selfName, selfName});
        }
        for (auto *innerStmt : funcDecl->blockStmt->body) {
          if (walkStmt(innerStmt, frames, currentClass)) {
            frames.pop_back();
            return true;
          }
        }
        frames.pop_back();
        return false;
      }
      case CLASS_DECL: {
        auto *classDecl = static_cast<ASTClassDecl *>(decl);
        if (classDecl->id && regionContainsPosition(classDecl->id->codeRegion, line + 1, character)) {
          if (auto resolved = findSymbolAtRegion(uri, text, classDecl->id->codeRegion)) {
            resolvedUriOut = resolved->uri;
            symbolOut = resolved->symbol;
            return true;
          }
        }
        if (!frames.empty() && classDecl->id) {
          frames.back().bindings.push_back(
              HoverBinding{classDecl->id, decl, classDecl->classType, false, classDecl->id->val,
                           sourceSliceForRegion(lines, classDecl->id->codeRegion)});
        }
        for (auto *field : classDecl->fields) {
          if (walkStmt(field, frames, classDecl)) {
            return true;
          }
        }
        for (auto *method : classDecl->methods) {
          if (walkStmt(method, frames, classDecl)) {
            return true;
          }
        }
        return false;
      }
      case SCOPE_DECL: {
        auto *scopeDecl = static_cast<ASTScopeDecl *>(decl);
        if (scopeDecl->scopeId && regionContainsPosition(scopeDecl->scopeId->codeRegion, line + 1, character)) {
          if (auto resolved = findSymbolAtRegion(uri, text, scopeDecl->scopeId->codeRegion)) {
            resolvedUriOut = resolved->uri;
            symbolOut = resolved->symbol;
            return true;
          }
        }
        if (!frames.empty() && scopeDecl->scopeId) {
          frames.back().bindings.push_back(
              HoverBinding{scopeDecl->scopeId, decl, nullptr, false, scopeDecl->scopeId->val,
                           sourceSliceForRegion(lines, scopeDecl->scopeId->codeRegion)});
        }
        if (!scopeDecl->blockStmt) {
          return false;
        }
        frames.push_back(HoverFrame{scopeDecl->blockStmt->parentScope, false, {}});
        for (auto *innerStmt : scopeDecl->blockStmt->body) {
          if (walkStmt(innerStmt, frames, currentClass)) {
            frames.pop_back();
            return true;
          }
        }
        frames.pop_back();
        return false;
      }
      case TYPE_ALIAS_DECL: {
        auto *aliasDecl = static_cast<ASTTypeAliasDecl *>(decl);
        if (aliasDecl->id && regionContainsPosition(aliasDecl->id->codeRegion, line + 1, character)) {
          if (auto resolved = findSymbolAtRegion(uri, text, aliasDecl->id->codeRegion)) {
            resolvedUriOut = resolved->uri;
            symbolOut = resolved->symbol;
            return true;
          }
          unsigned symbolLine = aliasDecl->id->codeRegion.startLine > 0 ? aliasDecl->id->codeRegion.startLine - 1 : 0;
          if (auto resolved =
                  findDeclaredSymbolOnLine(aliasDecl->id->val, "type alias", symbolLine, aliasDecl->id->codeRegion.startCol)) {
            resolvedUriOut = resolved->uri;
            symbolOut = resolved->symbol;
            return true;
          }
        }
        if (!frames.empty() && aliasDecl->id) {
          frames.back().bindings.push_back(
              HoverBinding{aliasDecl->id,
                           decl,
                           aliasDecl->aliasedType,
                           false,
                           aliasDecl->id->val,
                           sourceSliceForRegion(lines, aliasDecl->id->codeRegion),
                           aliasDecl->aliasedType ? hoverTypeToString(aliasDecl->aliasedType) : std::string()});
        }
        return false;
      }
      case SECURE_DECL: {
        auto *secureDecl = static_cast<ASTSecureDecl *>(decl);
        if (secureDecl->guardedDecl && walkStmt(secureDecl->guardedDecl, frames, currentClass)) {
          return true;
        }
        if (!secureDecl->catchBlock) {
          return false;
        }
        frames.push_back(HoverFrame{secureDecl->catchBlock->parentScope, false, {}});
        if (secureDecl->catchErrorId) {
          if (regionContainsPosition(secureDecl->catchErrorId->codeRegion, line + 1, character)) {
            if (auto resolved = synthesizeSymbol(secureDecl->catchErrorId,
                                                 decl,
                                                 secureDecl->catchErrorType,
                                                 sourceSliceForRegion(lines, secureDecl->catchErrorId->codeRegion),
                                                 "catch error")) {
              resolvedUriOut = resolved->uri;
              symbolOut = resolved->symbol;
              frames.pop_back();
              return true;
            }
          }
          frames.back().bindings.push_back(HoverBinding{secureDecl->catchErrorId,
                                                        decl,
                                                        secureDecl->catchErrorType,
                                                        false,
                                                        secureDecl->catchErrorId->val,
                                                        sourceSliceForRegion(lines, secureDecl->catchErrorId->codeRegion)});
        }
        for (auto *innerStmt : secureDecl->catchBlock->body) {
          if (walkStmt(innerStmt, frames, currentClass)) {
            frames.pop_back();
            return true;
          }
        }
        frames.pop_back();
        return false;
      }
      case COND_DECL: {
        auto *condDecl = static_cast<ASTConditionalDecl *>(decl);
        for (auto &spec : condDecl->specs) {
          if (spec.expr) {
            if (auto resolved = resolveExprHover(spec.expr, frames, currentClass)) {
              resolvedUriOut = resolved->uri;
              symbolOut = resolved->symbol;
              return true;
            }
          }
          if (walkBlock(spec.blockStmt, frames, currentClass, false)) {
            return true;
          }
        }
        return false;
      }
      case FOR_DECL: {
        auto *forDecl = static_cast<ASTForDecl *>(decl);
        if (forDecl->expr) {
          if (auto resolved = resolveExprHover(forDecl->expr, frames, currentClass)) {
            resolvedUriOut = resolved->uri;
            symbolOut = resolved->symbol;
            return true;
          }
        }
        return walkBlock(forDecl->blockStmt, frames, currentClass, false);
      }
      case WHILE_DECL: {
        auto *whileDecl = static_cast<ASTWhileDecl *>(decl);
        if (whileDecl->expr) {
          if (auto resolved = resolveExprHover(whileDecl->expr, frames, currentClass)) {
            resolvedUriOut = resolved->uri;
            symbolOut = resolved->symbol;
            return true;
          }
        }
        return walkBlock(whileDecl->blockStmt, frames, currentClass, false);
      }
      case RETURN_DECL: {
        auto *returnDecl = static_cast<ASTReturnDecl *>(decl);
        if (!returnDecl->expr) {
          return false;
        }
        if (auto resolved = resolveExprHover(returnDecl->expr, frames, currentClass)) {
          resolvedUriOut = resolved->uri;
          symbolOut = resolved->symbol;
          return true;
        }
        return false;
      }
      default:
        return false;
    }
  };

  std::vector<HoverFrame> frames;
  frames.push_back(HoverFrame{ASTScopeGlobal, false, {}});
  for (auto *stmt : parsedDocument->statements) {
    if (walkStmt(stmt, frames, nullptr)) {
      return true;
    }
  }
  return false;
}

bool Server::resolveSemanticSymbolTarget(const std::string &uri,
                                         const std::string &text,
                                         unsigned line,
                                         unsigned character,
                                         const std::string &word,
                                         size_t wordStart,
                                         const BuiltinApiIndex &builtinsIndex,
                                         std::string &resolvedUriOut,
                                         SymbolEntry &symbolOut) {
  auto symbols = collectSymbolsForUri(uri, text);
  const SymbolEntry *bestDeclaredSymbol = nullptr;
  for (const auto &symbol : symbols) {
    if (symbol.name != word || symbol.line != line || symbol.length == 0) {
      continue;
    }
    auto symbolEnd = symbol.start + symbol.length;
    if (character < symbol.start || character >= symbolEnd) {
      continue;
    }
    if (!bestDeclaredSymbol || symbol.length < bestDeclaredSymbol->length) {
      bestDeclaredSymbol = &symbol;
    }
  }
  if (bestDeclaredSymbol) {
    resolvedUriOut = uri;
    symbolOut = *bestDeclaredSymbol;
    return true;
  }

  std::string memberReceiver;
  bool memberContext = extractReceiverBeforeOffset(text, wordStart, memberReceiver);
  if (memberContext && !builtinsIndex.membersByType.empty()) {
    auto inferredType = inferBuiltinTypeForReceiver(text, memberReceiver, line, builtinsIndex);
    if (inferredType.has_value()) {
      auto typeMembers = builtinsIndex.membersByType.find(*inferredType);
      if (typeMembers != builtinsIndex.membersByType.end()) {
        auto memberIt = typeMembers->second.find(word);
        if (memberIt != typeMembers->second.end()) {
          resolvedUriOut = builtinsIndex.uri;
          symbolOut = memberIt->second;
          return true;
        }
      }
    }
  }

  if (resolveHoverSymbol(uri, text, line, character, word, wordStart, resolvedUriOut, symbolOut)) {
    return true;
  }

  if (!builtinsIndex.topLevelByName.empty()) {
    auto topLevel = builtinsIndex.topLevelByName.find(word);
    if (topLevel != builtinsIndex.topLevelByName.end()) {
      resolvedUriOut = builtinsIndex.uri;
      symbolOut = topLevel->second;
      return true;
    }
  }

  return false;
}

bool Server::resolveSemanticTypeTarget(const std::string &anchorUri,
                                       const std::string &typeName,
                                       const BuiltinApiIndex &builtinsIndex,
                                       const std::string &containerHint,
                                       std::string &resolvedUriOut,
                                       SymbolEntry &symbolOut) {
  auto normalizedTypeName = normalizeTypeTargetName(typeName);
  if (normalizedTypeName.empty()) {
    return false;
  }

  auto findTypeInDocument = [&](const std::string &candidateUri,
                                const std::string &candidateText,
                                const std::string &symbolName,
                                const std::string &containerName,
                                bool requireTopLevelOnly) -> bool {
    auto symbols = collectSymbolsForUri(candidateUri, candidateText);
    for (const auto &symbol : symbols) {
      if (symbol.name != symbolName || !isTypeDefinitionSymbol(symbol)) {
        continue;
      }
      if (!containerName.empty()) {
        if (symbol.containerName != containerName) {
          continue;
        }
      } else if (requireTopLevelOnly && symbol.isMember) {
        continue;
      }
      resolvedUriOut = candidateUri;
      symbolOut = symbol;
      return true;
    }
    return false;
  };

  auto searchCurrentDocument = [&](const std::string &symbolName) -> bool {
    std::string candidateText;
    if (!getDocumentTextByUri(anchorUri, candidateText) || candidateText.empty()) {
      return false;
    }
    return findTypeInDocument(anchorUri, candidateText, symbolName, std::string(), true);
  };

  auto searchContainerHint = [&](const std::string &symbolName) -> bool {
    if (containerHint.empty()) {
      return false;
    }
    std::string candidateText;
    if (getDocumentTextByUri(anchorUri, candidateText) &&
        findTypeInDocument(anchorUri, candidateText, symbolName, containerHint, false)) {
      return true;
    }
    for (const auto &doc : documents) {
      if (doc.first == anchorUri) {
        continue;
      }
      if (findTypeInDocument(doc.first, doc.second.text, symbolName, containerHint, false)) {
        return true;
      }
    }
    return false;
  };

  auto qualifiedParts = splitTopLevelTypeText(normalizedTypeName, '.');
  if (qualifiedParts.size() > 1) {
    const auto &localName = qualifiedParts.back();
    auto immediateContainer = qualifiedParts[qualifiedParts.size() - 2];

    std::string moduleUri;
    std::string moduleText;
    if (findModuleDocumentByName(qualifiedParts.front(), anchorUri, moduleUri, moduleText)) {
      if (qualifiedParts.size() == 2) {
        if (findTypeInDocument(moduleUri, moduleText, localName, std::string(), true)) {
          return true;
        }
      } else if (findTypeInDocument(moduleUri, moduleText, localName, immediateContainer, false)) {
        return true;
      }
    }

    std::string anchorText;
    if (getDocumentTextByUri(anchorUri, anchorText) &&
        findTypeInDocument(anchorUri, anchorText, localName, immediateContainer, false)) {
      return true;
    }

    for (const auto &doc : documents) {
      if (doc.first == anchorUri) {
        continue;
      }
      if (findTypeInDocument(doc.first, doc.second.text, localName, immediateContainer, false)) {
        return true;
      }
    }
    return false;
  }

  if (searchContainerHint(normalizedTypeName)) {
    return true;
  }

  if (searchCurrentDocument(normalizedTypeName)) {
    return true;
  }

  for (const auto &doc : documents) {
    if (doc.first == anchorUri) {
      continue;
    }
    if (findTypeInDocument(doc.first, doc.second.text, normalizedTypeName, std::string(), true)) {
      return true;
    }
  }

  if (!builtinsIndex.topLevelByName.empty()) {
    auto builtinsIt = builtinsIndex.topLevelByName.find(normalizedTypeName);
    if (builtinsIt != builtinsIndex.topLevelByName.end() && isTypeDefinitionSymbol(builtinsIt->second)) {
      resolvedUriOut = builtinsIndex.uri;
      symbolOut = builtinsIt->second;
      return true;
    }
  }

  return false;
}

bool Server::resolveSemanticSymbolAtPosition(const std::string &uri,
                                             const std::string &text,
                                             unsigned line,
                                             unsigned character,
                                             const BuiltinApiIndex &builtinsIndex,
                                             std::string &resolvedUriOut,
                                             SymbolEntry &symbolOut,
                                             size_t *wordStartOut,
                                             size_t *wordEndOut,
                                             std::string *wordOut) {
  std::string word;
  size_t wordStart = 0;
  size_t wordEnd = 0;
  if (!wordRangeAtPosition(text, line, character, word, wordStart, wordEnd)) {
    return false;
  }

  if (wordStartOut) {
    *wordStartOut = wordStart;
  }
  if (wordEndOut) {
    *wordEndOut = wordEnd;
  }
  if (wordOut) {
    *wordOut = word;
  }

  return resolveSemanticSymbolTarget(uri,
                                     text,
                                     line,
                                     character,
                                     word,
                                     wordStart,
                                     builtinsIndex,
                                     resolvedUriOut,
                                     symbolOut);
}

bool Server::symbolIdentityMatches(const std::string &leftUri,
                                   const SymbolEntry &left,
                                   const std::string &rightUri,
                                   const SymbolEntry &right) const {
  return leftUri == rightUri && left.line == right.line && left.start == right.start && left.length == right.length &&
         left.kind == right.kind;
}

bool Server::symbolIsRenameOwned(const std::string &resolvedUri,
                                 const SymbolEntry &resolvedSymbol,
                                 const BuiltinApiIndex &builtinsIndex) {
  std::string ownerText;
  if (!getDocumentTextByUri(resolvedUri, ownerText) || ownerText.empty()) {
    return false;
  }

  std::string ownerResolvedUri;
  SymbolEntry ownerResolvedSymbol;
  if (!resolveSemanticSymbolAtPosition(resolvedUri,
                                       ownerText,
                                       resolvedSymbol.line,
                                       resolvedSymbol.start,
                                       builtinsIndex,
                                       ownerResolvedUri,
                                       ownerResolvedSymbol)) {
    return false;
  }

  return symbolIdentityMatches(ownerResolvedUri, ownerResolvedSymbol, resolvedUri, resolvedSymbol);
}

void Server::setDocumentTextByUri(const std::string &uri, const std::string &text, int version, bool isOpen) {
  DocumentState state;
  state.text = text;
  state.version = version;
  state.isOpen = isOpen;
  state.textHash = hashText(text);
  documents[uri] = std::move(state);
  semanticSnapshots.erase(uri);
  invalidateSymbolCacheForUri(uri);
  if (isBuiltinsInterfaceUri(uri) || (builtinsIndexCache.valid && builtinsIndexCache.uri == uri)) {
    builtinsIndexCache.valid = false;
  }
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
      it->second.textHash = hashText(it->second.text);
      it->second.analysis = {};
      semanticSnapshots.erase(uri);
      invalidateSymbolCacheForUri(uri);
      if (isBuiltinsInterfaceUri(uri) || (builtinsIndexCache.valid && builtinsIndexCache.uri == uri)) {
        builtinsIndexCache.valid = false;
      }
      return;
    }
  }

  documents.erase(it);
  semanticSnapshots.erase(uri);
  invalidateSymbolCacheForUri(uri);
  if (isBuiltinsInterfaceUri(uri) || (builtinsIndexCache.valid && builtinsIndexCache.uri == uri)) {
    builtinsIndexCache.valid = false;
  }
}

void Server::appendLinguisticsDiagnosticsJson(const std::string &uri,
                                              const std::string &text,
                                              rapidjson::Value &diagnostics,
                                              rapidjson::Document::AllocatorType &alloc) {
  auto start = std::chrono::steady_clock::now();

  auto docIt = documents.find(uri);
  if(docIt != documents.end()) {
    const auto &findings = getLintFindingsForDocument(uri, docIt->second);
    for(const auto &finding : findings) {
      rapidjson::Value diag(rapidjson::kObjectType);
      appendLintDiagnosticJson(diag, finding, uri, alloc);
      diagnostics.PushBack(diag, alloc);
    }

    const auto &suggestions = getSuggestionsForDocument(uri, docIt->second);
    for(const auto &suggestion : suggestions) {
      rapidjson::Value diag(rapidjson::kObjectType);
      appendSuggestionDiagnosticJson(diag, suggestion, alloc);
      diagnostics.PushBack(diag, alloc);
    }
  }
  else {
    starbytes::linguistics::LinguisticsSession session(uri, text);
    auto lintResult = lintEngine.run(session, linguisticsConfig);
    for(const auto &finding : lintResult.findings) {
      rapidjson::Value diag(rapidjson::kObjectType);
      appendLintDiagnosticJson(diag, finding, uri, alloc);
      diagnostics.PushBack(diag, alloc);
    }

    starbytes::linguistics::SuggestionRequest suggestionRequest;
    suggestionRequest.includeLowConfidence = false;
    auto suggestionResult = suggestionEngine.run(session, linguisticsConfig, suggestionRequest);
    for(const auto &suggestion : suggestionResult.suggestions) {
      rapidjson::Value diag(rapidjson::kObjectType);
      appendSuggestionDiagnosticJson(diag, suggestion, alloc);
      diagnostics.PushBack(diag, alloc);
    }
  }

  auto elapsed = static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count());
  lspProfileDiagnosticsNs += elapsed;
  maybeLogLspProfileSample("diagnostics.append", elapsed, diagnostics.Size());
}

void Server::publishDiagnostics(const std::string &uri) {
  rapidjson::Document paramsDoc(rapidjson::kObjectType);
  auto &alloc = paramsDoc.GetAllocator();

  paramsDoc.AddMember("uri", rapidjson::Value(uri.c_str(), alloc), alloc);
  rapidjson::Value diagnostics(rapidjson::kArrayType);

  std::string text;
  if (getDocumentTextByUri(uri, text)) {
    auto docIt = documents.find(uri);
    if (docIt != documents.end()) {
      const auto &compilerDiagnostics = getCompilerDiagnosticsForDocument(uri, docIt->second);
      for (const auto &entry : compilerDiagnostics) {
        rapidjson::Value diag(rapidjson::kObjectType);
        appendCompilerDiagnosticJson(diag, entry, uri, alloc);
        diagnostics.PushBack(diag, alloc);
      }
      appendLinguisticsDiagnosticsJson(uri, docIt->second.text, diagnostics, alloc);
    }
    else {
      appendLinguisticsDiagnosticsJson(uri, text, diagnostics, alloc);
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

  configureSymbolCache();
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
  serverInfo.AddMember("version", rapidjson::Value("0.11.0", alloc), alloc);

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
  maybeLogLspProfileSummary();
  symbolCache.save();
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
  auto cursorOffset = offsetFromPosition(text, line, character);

  BuiltinApiIndex builtinsIndex;
  if (const auto *cachedBuiltins = getBuiltinsApiIndex()) {
    builtinsIndex = *cachedBuiltins;
  }

  std::string memberReceiver;
  std::string memberPrefix;
  bool memberContext = extractMemberCompletionContext(text, cursorOffset, memberReceiver, memberPrefix);
  if (memberContext) {
    loweredPrefix = toLower(memberPrefix);
  }
  std::optional<std::string> inferredMemberType;
  if (memberContext && !builtinsIndex.membersByType.empty()) {
    inferredMemberType = inferBuiltinTypeForReceiver(text, memberReceiver, line, builtinsIndex);
  }

  auto stateIt = documents.find(uri);
  const auto *parsedDocument = stateIt != documents.end() ? getSemanticResolvedDocumentForUri(uri, stateIt->second) : nullptr;
  auto lines = splitLinesCopy(text);
  const std::vector<std::string> emptyImports;
  const auto &directImports = parsedDocument ? parsedDocument->imports : emptyImports;

  auto builtinBaseTypeFromTypeText = [&](const std::string &typeText) -> std::optional<std::string> {
    if (typeText.empty()) {
      return std::nullopt;
    }
    auto baseType = completionDeclaredBaseTypeFromSignature("decl _:" + typeText);
    if (baseType.empty()) {
      return std::nullopt;
    }
    if (builtinsIndex.membersByType.find(baseType) == builtinsIndex.membersByType.end()) {
      return std::nullopt;
    }
    return baseType;
  };

  auto functionReturnTypeTextFromSignature = [&](const std::string &signature) -> std::string {
    auto closePos = signature.rfind(')');
    if (closePos == std::string::npos) {
      return {};
    }
    return trimCopy(signature.substr(closePos + 1));
  };

  auto importedModuleMemberTypeText = [&](ASTExpr *expr) -> std::string {
    ASTExpr *memberExpr = nullptr;
    if (!expr) {
      return {};
    }
    if (expr->type == IVKE_EXPR) {
      memberExpr = expr->callee;
    } else if (expr->type == MEMBER_EXPR) {
      memberExpr = expr;
    }
    if (!memberExpr || memberExpr->type != MEMBER_EXPR || !memberExpr->leftExpr || !memberExpr->rightExpr ||
        memberExpr->leftExpr->type != ID_EXPR || !memberExpr->leftExpr->id || !memberExpr->rightExpr->id) {
      return {};
    }

    const auto &moduleName = memberExpr->leftExpr->id->val;
    if (moduleName.empty() || std::find(directImports.begin(), directImports.end(), moduleName) == directImports.end()) {
      return {};
    }

    std::string moduleUri;
    std::string moduleText;
    if (!findModuleDocumentByName(moduleName, uri, moduleUri, moduleText)) {
      return {};
    }

    auto moduleSymbols = collectSymbolsForUri(moduleUri, moduleText);
    for (const auto &symbol : moduleSymbols) {
      if (symbol.name != memberExpr->rightExpr->id->val || symbol.isMember) {
        continue;
      }
      if (auto typeText = functionReturnTypeTextFromSignature(symbol.signature); !typeText.empty()) {
        return typeText;
      }
    }
    return {};
  };

  auto inferBuiltinReceiverTypeFromSemanticCache = [&]() -> std::optional<std::string> {
    if (!memberContext || memberReceiver.empty() || !parsedDocument) {
      return std::nullopt;
    }

    ASTType *bestType = nullptr;
    std::string bestTypeText;
    unsigned bestLine = 0;
    unsigned bestCol = 0;
    bool found = false;

    for (auto *stmt : parsedDocument->statements) {
      if (!stmt || stmt->type != VAR_DECL) {
        continue;
      }
      auto *varDecl = static_cast<ASTVarDecl *>(static_cast<ASTDecl *>(stmt));
      for (const auto &spec : varDecl->specs) {
        if (!spec.id || spec.id->val != memberReceiver || spec.id->codeRegion.startLine == 0) {
          continue;
        }
        unsigned specLine = spec.id->codeRegion.startLine - 1;
        unsigned specCol = spec.id->codeRegion.startCol;
        if (specLine > line || (specLine == line && specCol > character)) {
          continue;
        }
        if (found && (specLine < bestLine || (specLine == bestLine && specCol < bestCol))) {
          continue;
        }
        found = true;
        bestLine = specLine;
        bestCol = specCol;
        bestType = spec.type;
        bestTypeText.clear();
        if (!bestType && spec.expr) {
          bestTypeText = importedModuleMemberTypeText(spec.expr);
        }
      }
    }

    if (!found) {
      return std::nullopt;
    }
    if (bestType) {
      return builtinBaseTypeFromTypeText(hoverTypeToString(bestType));
    }
    return builtinBaseTypeFromTypeText(bestTypeText);
  };

  if (memberContext && !inferredMemberType.has_value() && !builtinsIndex.membersByType.empty() && !memberReceiver.empty()) {
    inferredMemberType = inferBuiltinReceiverTypeFromSemanticCache();
  }

  if (memberContext && !inferredMemberType.has_value() && !builtinsIndex.membersByType.empty() && !memberReceiver.empty()) {
    size_t memberStart = cursorOffset >= memberPrefix.size() ? cursorOffset - memberPrefix.size() : 0;
    if (memberStart > memberReceiver.size()) {
      size_t receiverStart = memberStart - memberReceiver.size() - 1;
      if (receiverStart + memberReceiver.size() <= text.size()) {
        unsigned receiverLine = 0;
        unsigned receiverChar = 0;
        positionFromOffset(text, receiverStart, receiverLine, receiverChar);
        std::string receiverUri;
        SymbolEntry receiverSymbol;
        if (resolveHoverSymbol(uri,
                               text,
                               receiverLine,
                               receiverChar,
                               memberReceiver,
                               receiverStart,
                               receiverUri,
                               receiverSymbol)) {
          auto declaredBase = completionDeclaredBaseTypeFromSignature(receiverSymbol.signature);
          if (!declaredBase.empty()) {
            inferredMemberType = declaredBase;
          }
        }
      }
    }
  }

  std::vector<CompletionEntry> entries;
  std::set<std::string> seen;
  auto pushEntry = [&](const std::string &label,
                       int kind,
                       const std::string &detail,
                       const SymbolEntry *resolvedSymbol = nullptr,
                       const std::string &resolvedUri = std::string()) {
    auto loweredLabel = toLower(label);
    if (!loweredPrefix.empty() && loweredLabel.rfind(loweredPrefix, 0) != 0) {
      return;
    }
    if (seen.insert(label).second) {
      CompletionEntry entry;
      entry.label = label;
      entry.kind = kind;
      entry.detail = detail;
      if (resolvedSymbol) {
        entry.hasResolvedSymbol = true;
        entry.resolvedUri = resolvedUri;
        entry.resolvedSymbol = *resolvedSymbol;
      }
      entries.push_back(std::move(entry));
    }
  };

  auto makeLocalSymbol = [&](ASTIdentifier *id,
                             ASTStmt *owner,
                             ASTType *typeOverride,
                             const std::string &displayName,
                             const std::string &detailOverride) -> std::optional<SymbolEntry> {
    if (!id || !owner) {
      return std::nullopt;
    }

    SymbolEntry symbol;
    symbol.name = displayName.empty() ? sourceSliceForRegion(lines, id->codeRegion) : displayName;
    symbol.line = id->codeRegion.startLine > 0 ? id->codeRegion.startLine - 1 : 0;
    symbol.start = id->codeRegion.startCol;
    symbol.length = id->codeRegion.endCol > id->codeRegion.startCol ? id->codeRegion.endCol - id->codeRegion.startCol
                                                                     : static_cast<unsigned>(symbol.name.size());
    symbol.documentation = hoverDocCommentForLine(lines, symbol.line);

    if (owner->type == VAR_DECL) {
      auto *varDecl = static_cast<ASTVarDecl *>(owner);
      bool isConst = varDecl->isConst;
      symbol.kind = isConst ? SYMBOL_KIND_CONSTANT : SYMBOL_KIND_VARIABLE;
      symbol.detail = detailOverride.empty() ? (isConst ? "constant" : "variable") : detailOverride;
      symbol.signature = hoverVarSignature(isConst, symbol.name, typeOverride);
      return symbol;
    }

    if (owner->type == SECURE_DECL) {
      symbol.kind = SYMBOL_KIND_VARIABLE;
      symbol.detail = detailOverride.empty() ? "catch error" : detailOverride;
      symbol.signature = "catch " + symbol.name;
      if (typeOverride) {
        symbol.signature += ":";
        symbol.signature += hoverTypeToString(typeOverride);
      }
      return symbol;
    }

    if (owner->type == FUNC_DECL || owner->type == CLASS_CTOR_DECL) {
      symbol.kind = SYMBOL_KIND_VARIABLE;
      symbol.detail = detailOverride.empty() ? "parameter" : detailOverride;
      symbol.signature = hoverVarSignature(false, symbol.name, typeOverride, "param");
      return symbol;
    }

    return std::nullopt;
  };

  if (!memberContext) {
    for (const auto &keyword : kKeywordCompletions) {
      pushEntry(keyword, COMPLETION_KIND_KEYWORD, "keyword");
    }
  }

  if (memberContext && inferredMemberType.has_value()) {
      auto memberIt = builtinsIndex.membersByType.find(*inferredMemberType);
      if (memberIt != builtinsIndex.membersByType.end()) {
        for (const auto &member : memberIt->second) {
          pushEntry(member.second.name,
                    completionKindFromSymbolKind(member.second.kind),
                    member.second.detail,
                    &member.second,
                    builtinsIndex.uri);
        }
      }
  }

  if (memberContext && parsedDocument) {
    bool handledScopedReceiver = false;
    if (std::find(parsedDocument->imports.begin(), parsedDocument->imports.end(), memberReceiver) != parsedDocument->imports.end()) {
      std::string moduleUri;
      std::string moduleText;
      if (findModuleDocumentByName(memberReceiver, uri, moduleUri, moduleText)) {
        auto symbols = collectSymbolsForUri(moduleUri, moduleText);
        for (const auto &symbol : symbols) {
          if (symbol.isMember) {
            continue;
          }
          pushEntry(symbol.name, completionKindFromSymbolKind(symbol.kind), symbol.detail, &symbol, moduleUri);
        }
        handledScopedReceiver = true;
      }
    }

    if (!handledScopedReceiver && !memberReceiver.empty()) {
      std::string receiverUri;
      SymbolEntry receiverSymbol;
      size_t memberStart = cursorOffset >= memberPrefix.size() ? cursorOffset - memberPrefix.size() : 0;
      if (memberStart > memberReceiver.size()) {
        size_t receiverStart = memberStart - memberReceiver.size() - 1;
        if (receiverStart + memberReceiver.size() <= text.size()) {
          unsigned receiverLine = 0;
          unsigned receiverChar = 0;
          positionFromOffset(text, receiverStart, receiverLine, receiverChar);
          if (resolveHoverSymbol(uri,
                                 text,
                                 receiverLine,
                                 receiverChar,
                                 memberReceiver,
                                 receiverStart,
                                 receiverUri,
                                 receiverSymbol)) {
            std::string receiverContainer;
            if (receiverSymbol.kind == SYMBOL_KIND_NAMESPACE || receiverSymbol.kind == SYMBOL_KIND_CLASS ||
                receiverSymbol.kind == SYMBOL_KIND_INTERFACE || receiverSymbol.kind == SYMBOL_KIND_STRUCT) {
              receiverContainer = receiverSymbol.name;
            } else {
              receiverContainer = completionDeclaredBaseTypeFromSignature(receiverSymbol.signature);
            }

            if (!receiverContainer.empty()) {
              for (const auto &doc : documents) {
                auto symbols = collectSymbolsForUri(doc.first, doc.second.text);
                for (const auto &symbol : symbols) {
                  if (!symbol.isMember || symbol.containerName != receiverContainer) {
                    continue;
                  }
                  pushEntry(symbol.name,
                            completionKindFromSymbolKind(symbol.kind),
                            symbol.detail,
                            &symbol,
                            doc.first);
                }
              }
              handledScopedReceiver = true;
            }
          }
        }
      }
    }
  }

  if (!memberContext && parsedDocument) {
    for (const auto &importName : parsedDocument->imports) {
      SymbolEntry importSymbol;
      importSymbol.name = importName;
      importSymbol.kind = SYMBOL_KIND_NAMESPACE;
      importSymbol.detail = "module";
      importSymbol.signature = "import " + importName;
      pushEntry(importName, COMPLETION_KIND_MODULE, "module", &importSymbol, uri);
    }

    auto cursorLineOneBased = line + 1;
    auto cursorBeforeRegion = [&](const Region &region) {
      if (region.startLine == 0) {
        return false;
      }
      if (cursorLineOneBased < region.startLine) {
        return true;
      }
      if (cursorLineOneBased == region.startLine && character < region.startCol) {
        return true;
      }
      return false;
    };
    auto regionEndsBeforeCursor = [&](const Region &region) {
      if (region.endLine == 0) {
        return false;
      }
      auto endCol = region.endCol > region.startCol ? region.endCol : (region.startCol + 1);
      if (cursorLineOneBased > region.endLine) {
        return true;
      }
      if (cursorLineOneBased == region.endLine && character >= endCol) {
        return true;
      }
      return false;
    };

    std::vector<HoverFrame> frames;
    frames.push_back(HoverFrame{ASTScopeGlobal, false, {}});

    std::function<bool(ASTStmt *, ASTClassDecl *)> collectStmtPath;
    std::function<bool(ASTBlockStmt *, ASTClassDecl *, bool)> collectBlockPath;

    auto registerVarBindings = [&](ASTVarDecl *varDecl, ASTDecl *decl) {
      if (!varDecl || frames.empty()) {
        return;
      }
      for (const auto &spec : varDecl->specs) {
        if (!spec.id) {
          continue;
        }
        frames.back().bindings.push_back(
            HoverBinding{spec.id, decl, spec.type, varDecl->isConst, spec.id->val,
                         sourceSliceForRegion(lines, spec.id->codeRegion)});
      }
    };

    collectBlockPath = [&](ASTBlockStmt *block, ASTClassDecl *currentClass, bool functionBoundary) -> bool {
      if (!block) {
        return false;
      }
      frames.push_back(HoverFrame{block->parentScope, functionBoundary, {}});
      for (auto *stmt : block->body) {
        if (!stmt) {
          continue;
        }
        if (cursorBeforeRegion(stmt->codeRegion)) {
          break;
        }
        if (collectStmtPath(stmt, currentClass)) {
          return true;
        }
      }
      return true;
    };

    collectStmtPath = [&](ASTStmt *stmt, ASTClassDecl *currentClass) -> bool {
      if (!stmt) {
        return false;
      }
      if (!regionContainsPosition(stmt->codeRegion, cursorLineOneBased, character)) {
        if (regionEndsBeforeCursor(stmt->codeRegion) && stmt->type == VAR_DECL) {
          auto *varDecl = static_cast<ASTVarDecl *>(static_cast<ASTDecl *>(stmt));
          registerVarBindings(varDecl, static_cast<ASTDecl *>(stmt));
        }
        return false;
      }

      if (!(stmt->type & DECL)) {
        return true;
      }

      auto *decl = static_cast<ASTDecl *>(stmt);
      switch (decl->type) {
        case VAR_DECL:
          return true;
        case FUNC_DECL: {
          auto *funcDecl = static_cast<ASTFuncDecl *>(decl);
          if (!funcDecl->blockStmt) {
            return true;
          }
          frames.push_back(HoverFrame{funcDecl->blockStmt->parentScope, true, {}});
          for (const auto &param : funcDecl->params) {
            if (!param.first) {
              continue;
            }
            frames.back().bindings.push_back(
                HoverBinding{param.first, decl, param.second, false, param.first->val,
                             sourceSliceForRegion(lines, param.first->codeRegion)});
          }
          if (currentClass) {
            auto selfName = std::string("self");
            auto *selfId = new ASTIdentifier();
            selfId->val = selfName;
            selfId->codeRegion = funcDecl->funcId ? funcDecl->funcId->codeRegion : Region{};
            frames.back().bindings.push_back(HoverBinding{selfId, decl, currentClass->classType, false, selfName, selfName});
          }
          if (collectBlockPath(funcDecl->blockStmt, currentClass, true)) {
            return true;
          }
          return true;
        }
        case CLASS_DECL: {
          auto *classDecl = static_cast<ASTClassDecl *>(decl);
          for (auto *field : classDecl->fields) {
            if (collectStmtPath(field, classDecl)) {
              return true;
            }
          }
          for (auto *method : classDecl->methods) {
            if (collectStmtPath(method, classDecl)) {
              return true;
            }
          }
          return true;
        }
        case SCOPE_DECL: {
          auto *scopeDecl = static_cast<ASTScopeDecl *>(decl);
          if (scopeDecl->blockStmt && collectBlockPath(scopeDecl->blockStmt, currentClass, false)) {
            return true;
          }
          return true;
        }
        case SECURE_DECL: {
          auto *secureDecl = static_cast<ASTSecureDecl *>(decl);
          if (secureDecl->guardedDecl && collectStmtPath(secureDecl->guardedDecl, currentClass)) {
            return true;
          }
          if (secureDecl->catchBlock) {
            frames.push_back(HoverFrame{secureDecl->catchBlock->parentScope, false, {}});
            if (secureDecl->catchErrorId) {
              frames.back().bindings.push_back(
                  HoverBinding{secureDecl->catchErrorId,
                               decl,
                               secureDecl->catchErrorType,
                               false,
                               secureDecl->catchErrorId->val,
                               sourceSliceForRegion(lines, secureDecl->catchErrorId->codeRegion)});
            }
            if (collectBlockPath(secureDecl->catchBlock, currentClass, false)) {
              return true;
            }
          }
          return true;
        }
        case COND_DECL: {
          auto *condDecl = static_cast<ASTConditionalDecl *>(decl);
          for (auto &spec : condDecl->specs) {
            if (spec.blockStmt && collectBlockPath(spec.blockStmt, currentClass, false)) {
              return true;
            }
          }
          return true;
        }
        case FOR_DECL: {
          auto *forDecl = static_cast<ASTForDecl *>(decl);
          if (forDecl->blockStmt && collectBlockPath(forDecl->blockStmt, currentClass, false)) {
            return true;
          }
          return true;
        }
        case WHILE_DECL: {
          auto *whileDecl = static_cast<ASTWhileDecl *>(decl);
          if (whileDecl->blockStmt && collectBlockPath(whileDecl->blockStmt, currentClass, false)) {
            return true;
          }
          return true;
        }
        default:
          return true;
      }
    };

    for (auto *stmt : parsedDocument->statements) {
      if (!stmt) {
        continue;
      }
      if (cursorBeforeRegion(stmt->codeRegion)) {
        break;
      }
      if (collectStmtPath(stmt, nullptr)) {
        break;
      }
    }

    for (const auto &frame : frames) {
      for (const auto &binding : frame.bindings) {
        auto localSymbol = makeLocalSymbol(binding.declId, binding.declOwner, binding.type, binding.displayName, "");
        if (!localSymbol.has_value()) {
          continue;
        }
        pushEntry(localSymbol->name,
                  completionKindFromSymbolKind(localSymbol->kind),
                  localSymbol->detail,
                  &*localSymbol,
                  uri);
      }
    }

    std::string resolvedWord;
    size_t resolvedWordStart = 0;
    size_t resolvedWordEnd = 0;
    SymbolEntry resolvedScopedSymbol;
    std::string resolvedScopedUri;
    if (wordRangeAtPosition(text, line, character, resolvedWord, resolvedWordStart, resolvedWordEnd) &&
        resolveHoverSymbol(uri,
                           text,
                           line,
                           character,
                           resolvedWord,
                           resolvedWordStart,
                           resolvedScopedUri,
                           resolvedScopedSymbol)) {
      pushEntry(resolvedScopedSymbol.name,
                completionKindFromSymbolKind(resolvedScopedSymbol.kind),
                resolvedScopedSymbol.detail,
                &resolvedScopedSymbol,
                resolvedScopedUri);
    }
  }

  for (const auto &doc : documents) {
    auto symbols = collectSymbolsForUri(doc.first, doc.second.text);
    for (const auto &symbol : symbols) {
      if (memberContext) {
        if (!symbol.isMember) {
          continue;
        }
        if (!inferredMemberType.has_value() || symbol.containerName != *inferredMemberType) {
          continue;
        }
      } else if (symbol.isMember) {
        continue;
      }
      pushEntry(symbol.name, completionKindFromSymbolKind(symbol.kind), symbol.detail, &symbol, doc.first);
    }
  }

  if (!memberContext) {
    for (const auto &topLevel : builtinsIndex.topLevelByName) {
      const auto &symbol = topLevel.second;
      if (symbol.isMember) {
        continue;
      }
      pushEntry(symbol.name, completionKindFromSymbolKind(symbol.kind), symbol.detail, &symbol, builtinsIndex.uri);
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
    if (entry.hasResolvedSymbol && entry.resolvedSymbol.isDeprecated) {
      appendDeprecatedTagArray(item, LSP_COMPLETION_ITEM_TAG_DEPRECATED, alloc);
    }
    if (entry.hasResolvedSymbol) {
      rapidjson::Value data(rapidjson::kObjectType);
      data.AddMember("uri", rapidjson::Value(entry.resolvedUri.c_str(), alloc), alloc);
      data.AddMember("symbolLine", entry.resolvedSymbol.line, alloc);
      data.AddMember("symbolStart", entry.resolvedSymbol.start, alloc);
      data.AddMember("symbolLength", entry.resolvedSymbol.length, alloc);
      data.AddMember("symbolKind", entry.resolvedSymbol.kind, alloc);
      data.AddMember("detail", rapidjson::Value(entry.resolvedSymbol.detail.c_str(), alloc), alloc);
      data.AddMember("signature", rapidjson::Value(entry.resolvedSymbol.signature.c_str(), alloc), alloc);
      data.AddMember("documentation", rapidjson::Value(entry.resolvedSymbol.documentation.c_str(), alloc), alloc);
      data.AddMember("containerName", rapidjson::Value(entry.resolvedSymbol.containerName.c_str(), alloc), alloc);
      data.AddMember("isMember", entry.resolvedSymbol.isMember, alloc);
      data.AddMember("isDeprecated", entry.resolvedSymbol.isDeprecated, alloc);
      data.AddMember("deprecationMessage", rapidjson::Value(entry.resolvedSymbol.deprecationMessage.c_str(), alloc), alloc);
      item.AddMember("data", data, alloc);
    }
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

  std::string label;
  if(item.HasMember("label") && item["label"].IsString()) {
    label = item["label"].GetString();
  }

  std::optional<SymbolEntry> resolvedSymbol;
  std::string resolvedUri;
  BuiltinApiIndex builtinsIndex;
  if(const auto *cachedBuiltins = getBuiltinsApiIndex()) {
    builtinsIndex = *cachedBuiltins;
  }
  if (item.HasMember("data") && item["data"].IsObject()) {
    const auto &data = item["data"];
    if (data.HasMember("uri") && data["uri"].IsString()) {
      resolvedUri = data["uri"].GetString();
    }
    SymbolEntry exact;
    bool exactOk = false;
    if (data.HasMember("symbolLine") && data["symbolLine"].IsUint() &&
        data.HasMember("symbolStart") && data["symbolStart"].IsUint() &&
        data.HasMember("symbolLength") && data["symbolLength"].IsUint()) {
      exact.line = data["symbolLine"].GetUint();
      exact.start = data["symbolStart"].GetUint();
      exact.length = data["symbolLength"].GetUint();
      exactOk = true;
    }
    exact.name = label;
    if (data.HasMember("symbolKind") && data["symbolKind"].IsInt()) {
      exact.kind = data["symbolKind"].GetInt();
    }
    if (data.HasMember("detail") && data["detail"].IsString()) {
      exact.detail = data["detail"].GetString();
    }
    if (data.HasMember("signature") && data["signature"].IsString()) {
      exact.signature = data["signature"].GetString();
    }
    if (data.HasMember("documentation") && data["documentation"].IsString()) {
      exact.documentation = data["documentation"].GetString();
    }
    if (data.HasMember("containerName") && data["containerName"].IsString()) {
      exact.containerName = data["containerName"].GetString();
    }
    if (data.HasMember("isMember") && data["isMember"].IsBool()) {
      exact.isMember = data["isMember"].GetBool();
    }
    if (data.HasMember("isDeprecated") && data["isDeprecated"].IsBool()) {
      exact.isDeprecated = data["isDeprecated"].GetBool();
    }
    if (data.HasMember("deprecationMessage") && data["deprecationMessage"].IsString()) {
      exact.deprecationMessage = data["deprecationMessage"].GetString();
    }
    if (exactOk) {
      resolvedSymbol = exact;
    }
  }
  if(!label.empty()) {
    if(!resolvedSymbol.has_value()) {
      auto top = builtinsIndex.topLevelByName.find(label);
      if(top != builtinsIndex.topLevelByName.end()) {
        resolvedSymbol = top->second;
        resolvedUri = builtinsIndex.uri;
      }
    }
    if(!resolvedSymbol.has_value()) {
      for(const auto &memberType : builtinsIndex.membersByType) {
        auto m = memberType.second.find(label);
        if(m != memberType.second.end()) {
          resolvedSymbol = m->second;
          resolvedUri = builtinsIndex.uri;
          break;
        }
      }
    }
    if(!resolvedSymbol.has_value()) {
      for (const auto &doc : documents) {
        auto symbols = collectSymbolsForUri(doc.first, doc.second.text);
        auto it = std::find_if(symbols.begin(), symbols.end(), [&](const SymbolEntry &symbol) {
          return symbol.name == label;
        });
        if(it != symbols.end()) {
          resolvedSymbol = *it;
          resolvedUri = doc.first;
          break;
        }
      }
    }
  }

  if(resolvedSymbol.has_value()) {
    if (resolvedSymbol->isDeprecated && !item.HasMember("tags")) {
      appendDeprecatedTagArray(item, LSP_COMPLETION_ITEM_TAG_DEPRECATED, alloc);
    }
    if(!item.HasMember("detail")) {
      item.AddMember("detail", rapidjson::Value(resolvedSymbol->detail.c_str(), alloc), alloc);
    }
    std::string renderedDoc = DoxygenDoc::parse(resolvedSymbol->documentation).renderMarkdown();
    if(renderedDoc.empty()) {
      renderedDoc = resolvedSymbol->documentation;
    }
    if (resolvedSymbol->isDeprecated) {
      std::string deprecationNote = "> Deprecated";
      if (!resolvedSymbol->deprecationMessage.empty()) {
        deprecationNote += ": " + resolvedSymbol->deprecationMessage;
      }
      if (!renderedDoc.empty()) {
        renderedDoc = deprecationNote + "\n\n" + renderedDoc;
      } else {
        renderedDoc = deprecationNote;
      }
    }
    if(!resolvedSymbol->signature.empty()) {
      if(!renderedDoc.empty()) {
        renderedDoc = "```starbytes\n" + resolvedSymbol->signature + "\n```\n\n" + renderedDoc;
      }
      else {
        renderedDoc = "```starbytes\n" + resolvedSymbol->signature + "\n```";
      }
    }
    if(!renderedDoc.empty()) {
      if(item.HasMember("documentation")) {
        item["documentation"].SetString(renderedDoc.c_str(), alloc);
      }
      else {
        item.AddMember("documentation", rapidjson::Value(renderedDoc.c_str(), alloc), alloc);
      }
    }
    if (!resolvedUri.empty() && !item.HasMember("data")) {
      rapidjson::Value data(rapidjson::kObjectType);
      data.AddMember("uri", rapidjson::Value(resolvedUri.c_str(), alloc), alloc);
      data.AddMember("symbolLine", resolvedSymbol->line, alloc);
      data.AddMember("symbolStart", resolvedSymbol->start, alloc);
      data.AddMember("symbolLength", resolvedSymbol->length, alloc);
      item.AddMember("data", data, alloc);
    }
  }
  else if (!item.HasMember("documentation")) {
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

  BuiltinApiIndex builtinsIndex;
  if (const auto *cachedBuiltins = getBuiltinsApiIndex()) {
    builtinsIndex = *cachedBuiltins;
  }

  std::optional<SymbolEntry> bestSymbol;
  std::string bestUri = uri;

  std::string memberReceiver;
  bool memberContext = extractReceiverBeforeOffset(text, wordStart, memberReceiver);
  if (memberContext && !builtinsIndex.membersByType.empty()) {
    auto inferredType =
        inferBuiltinTypeForReceiver(text, memberReceiver, params["position"]["line"].GetUint(), builtinsIndex);
    if (inferredType.has_value()) {
      auto typeMembers = builtinsIndex.membersByType.find(*inferredType);
      if (typeMembers != builtinsIndex.membersByType.end()) {
        auto memberIt = typeMembers->second.find(word);
        if (memberIt != typeMembers->second.end()) {
          bestSymbol = memberIt->second;
          bestUri = builtinsIndex.uri;
        }
      }
    }
  }

  auto scanDefinitions = [&](const std::string &candidateUri, const std::string &candidateText) {
    auto symbols = collectSymbolsForUri(candidateUri, candidateText);
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

  if (!bestSymbol.has_value()) {
    SymbolEntry resolvedScopedSymbol;
    std::string resolvedScopedUri;
    if (resolveHoverSymbol(uri,
                           text,
                           params["position"]["line"].GetUint(),
                           params["position"]["character"].GetUint(),
                           word,
                           wordStart,
                           resolvedScopedUri,
                           resolvedScopedSymbol)) {
      bestSymbol = resolvedScopedSymbol;
      bestUri = resolvedScopedUri;
    }
  }

  if (!bestSymbol.has_value()) {
    for (const auto &doc : documents) {
      scanDefinitions(doc.first, doc.second.text);
    }
  }

  if (!bestSymbol.has_value() && !builtinsIndex.topLevelByName.empty()) {
    auto topLevel = builtinsIndex.topLevelByName.find(word);
    if (topLevel != builtinsIndex.topLevelByName.end()) {
      bestSymbol = topLevel->second;
      bestUri = builtinsIndex.uri;
    }
  }

  rapidjson::Document payloadDoc(rapidjson::kObjectType);
  auto &alloc = payloadDoc.GetAllocator();

  rapidjson::Value result(rapidjson::kObjectType);
  rapidjson::Value contents(rapidjson::kObjectType);
  contents.AddMember("kind", rapidjson::Value("markdown", alloc), alloc);

  std::string markdown = "**" + word + "**";
  if (bestSymbol.has_value()) {
    auto titleKind = bestSymbol->detail.empty() ? "symbol" : bestSymbol->detail;
    if (!titleKind.empty()) {
      titleKind[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(titleKind[0])));
    }
    markdown = "### " + titleKind + " `" + bestSymbol->name + "`";

    std::string signatureBlock = bestSymbol->signature;
    if (!signatureBlock.empty() && bestSymbol->isMember && !bestSymbol->containerName.empty()) {
      if (bestSymbol->kind == SYMBOL_KIND_METHOD || bestSymbol->kind == SYMBOL_KIND_FIELD ||
          bestSymbol->kind == SYMBOL_KIND_CONSTANT || bestSymbol->kind == SYMBOL_KIND_VARIABLE) {
        signatureBlock = "class " + bestSymbol->containerName + " {\n    " + bestSymbol->signature + "\n}";
      }
    }
    if (!signatureBlock.empty()) {
      markdown += "\n\n```starbytes\n" + signatureBlock + "\n```";
    }
    if (bestSymbol->isDeprecated) {
      markdown += "\n\n> Deprecated";
      if (!bestSymbol->deprecationMessage.empty()) {
        markdown += ": " + bestSymbol->deprecationMessage;
      }
    }
    appendInheritanceMarkdown(markdown, *bestSymbol);

    std::string symbolText;
    if (getDocumentTextByUri(bestUri, symbolText)) {
      CodeView codeView(bestUri, symbolText);
      Region region;
      region.startLine = bestSymbol->line + 1;
      region.endLine = bestSymbol->line + 1;
      region.startCol = bestSymbol->start;
      region.endCol = bestSymbol->start + std::max<unsigned>(1, bestSymbol->length);
      auto rendered = codeView.renderRegion(region, "", 1);
      if (!rendered.empty()) {
        markdown += "\n\n```text\n" + rendered + "\n```";
      }
    }

    auto parsedDoc = DoxygenDoc::parse(bestSymbol->documentation);
    auto renderedDoc = parsedDoc.renderMarkdown();
    if (!renderedDoc.empty()) {
      markdown += "\n\n" + renderedDoc;
    } else if (!bestSymbol->documentation.empty()) {
      markdown += "\n\n" + bestSymbol->documentation;
    }

    if (bestSymbol->isMember && !bestSymbol->containerName.empty()) {
      markdown += "\n\nParent scope: `" + bestSymbol->containerName + "`";
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

  BuiltinApiIndex builtinsIndex;
  if (const auto *cachedBuiltins = getBuiltinsApiIndex()) {
    builtinsIndex = *cachedBuiltins;
  }

  rapidjson::Document payloadDoc(rapidjson::kObjectType);
  auto &alloc = payloadDoc.GetAllocator();
  rapidjson::Value locations(rapidjson::kArrayType);
  std::set<std::string> seenLocations;

  auto appendLocation = [&](const std::string &defUri, const SymbolEntry &symbol) {
    auto key = defUri + ":" + std::to_string(symbol.line) + ":" + std::to_string(symbol.start);
    if (!seenLocations.insert(key).second) {
      return;
    }
    rapidjson::Value location(rapidjson::kObjectType);
    location.AddMember("uri", rapidjson::Value(defUri.c_str(), alloc), alloc);

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
  };

  std::string memberReceiver;
  bool memberContext = extractReceiverBeforeOffset(text, start, memberReceiver);
  if (memberContext && !builtinsIndex.membersByType.empty()) {
    auto inferredType = inferBuiltinTypeForReceiver(text, memberReceiver, params["position"]["line"].GetUint(), builtinsIndex);
    if (inferredType.has_value()) {
      auto typeMembers = builtinsIndex.membersByType.find(*inferredType);
      if (typeMembers != builtinsIndex.membersByType.end()) {
        auto memberIt = typeMembers->second.find(word);
        if (memberIt != typeMembers->second.end()) {
          appendLocation(builtinsIndex.uri, memberIt->second);
          writeResult(request["id"], locations);
          return;
        }
      }
    }
  }

  SymbolEntry resolvedSymbol;
  std::string resolvedUri;
  if (resolveHoverSymbol(uri,
                         text,
                         params["position"]["line"].GetUint(),
                         params["position"]["character"].GetUint(),
                         word,
                         start,
                         resolvedUri,
                         resolvedSymbol)) {
    appendLocation(resolvedUri, resolvedSymbol);
    writeResult(request["id"], locations);
    return;
  }

  for (const auto &doc : documents) {
    auto symbols = collectSymbolsForUri(doc.first, doc.second.text);
    for (const auto &symbol : symbols) {
      if (symbol.name != word) {
        continue;
      }
      appendLocation(doc.first, symbol);
    }
  }

  if (locations.Empty() && !builtinsIndex.topLevelByName.empty()) {
    auto topLevel = builtinsIndex.topLevelByName.find(word);
    if (topLevel != builtinsIndex.topLevelByName.end()) {
      appendLocation(builtinsIndex.uri, topLevel->second);
    }
  }

  writeResult(request["id"], locations);
}

void Server::handleDeclaration(rapidjson::Document &request) {
  if (!request.HasMember("id")) {
    return;
  }
  if (!request.HasMember("params") || !request["params"].IsObject()) {
    writeError(request["id"], JSONRPC_INVALID_PARAMS, "Missing declaration params");
    return;
  }

  auto &params = request["params"];
  if (!params.HasMember("textDocument") || !params["textDocument"].IsObject() ||
      !params["textDocument"].HasMember("uri") || !params["textDocument"]["uri"].IsString() ||
      !params.HasMember("position") || !params["position"].IsObject() || !params["position"].HasMember("line") ||
      !params["position"].HasMember("character") || !params["position"]["line"].IsUint() ||
      !params["position"]["character"].IsUint()) {
    writeError(request["id"], JSONRPC_INVALID_PARAMS, "Invalid declaration params");
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

  BuiltinApiIndex builtinsIndex;
  if (const auto *cachedBuiltins = getBuiltinsApiIndex()) {
    builtinsIndex = *cachedBuiltins;
  }

  rapidjson::Document payloadDoc(rapidjson::kObjectType);
  auto &alloc = payloadDoc.GetAllocator();
  rapidjson::Value locations(rapidjson::kArrayType);

  auto appendLocation = [&](const std::string &defUri, const SymbolEntry &symbol) {
    rapidjson::Value location(rapidjson::kObjectType);
    location.AddMember("uri", rapidjson::Value(defUri.c_str(), alloc), alloc);

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
  };

  SymbolEntry resolvedSymbol;
  std::string resolvedUri;
  if (resolveSemanticSymbolTarget(uri,
                                  text,
                                  params["position"]["line"].GetUint(),
                                  params["position"]["character"].GetUint(),
                                  word,
                                  start,
                                  builtinsIndex,
                                  resolvedUri,
                                  resolvedSymbol)) {
    appendLocation(resolvedUri, resolvedSymbol);
    writeResult(request["id"], locations);
    return;
  }

  for (const auto &doc : documents) {
    auto symbols = collectSymbolsForUri(doc.first, doc.second.text);
    for (const auto &symbol : symbols) {
      if (symbol.name != word) {
        continue;
      }
      appendLocation(doc.first, symbol);
    }
  }

  writeResult(request["id"], locations);
}

void Server::handleTypeDefinition(rapidjson::Document &request) {
  if (!request.HasMember("id")) {
    return;
  }
  if (!request.HasMember("params") || !request["params"].IsObject()) {
    writeError(request["id"], JSONRPC_INVALID_PARAMS, "Missing type definition params");
    return;
  }

  auto &params = request["params"];
  if (!params.HasMember("textDocument") || !params["textDocument"].IsObject() ||
      !params["textDocument"].HasMember("uri") || !params["textDocument"]["uri"].IsString() ||
      !params.HasMember("position") || !params["position"].IsObject() || !params["position"].HasMember("line") ||
      !params["position"].HasMember("character") || !params["position"]["line"].IsUint() ||
      !params["position"]["character"].IsUint()) {
    writeError(request["id"], JSONRPC_INVALID_PARAMS, "Invalid type definition params");
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

  BuiltinApiIndex builtinsIndex;
  if (const auto *cachedBuiltins = getBuiltinsApiIndex()) {
    builtinsIndex = *cachedBuiltins;
  }

  rapidjson::Document payloadDoc(rapidjson::kObjectType);
  auto &alloc = payloadDoc.GetAllocator();
  rapidjson::Value locations(rapidjson::kArrayType);

  auto appendLocation = [&](const std::string &defUri, const SymbolEntry &symbol) {
    rapidjson::Value location(rapidjson::kObjectType);
    location.AddMember("uri", rapidjson::Value(defUri.c_str(), alloc), alloc);

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
  };

  SymbolEntry resolvedSymbol;
  std::string resolvedUri;
  if (!resolveSemanticSymbolTarget(uri,
                                   text,
                                   params["position"]["line"].GetUint(),
                                   params["position"]["character"].GetUint(),
                                   word,
                                   start,
                                   builtinsIndex,
                                   resolvedUri,
                                   resolvedSymbol)) {
    writeResult(request["id"], locations);
    return;
  }

  std::unordered_set<std::string> visitedTypeTexts;
  std::unordered_set<std::string> visitedAliasSymbols;
  std::function<void(const std::string &, const std::string &, const std::string &, unsigned)> collectTypeTargets =
      [&](const std::string &anchorForType, const std::string &rawTypeText, const std::string &containerHint, unsigned depth) {
        if (depth > 12) {
          return;
        }

        auto trimmedTypeText = trimCopy(rawTypeText);
        if (trimmedTypeText.empty()) {
          return;
        }

        auto visitKey = anchorForType + "::" + containerHint + "::" + trimmedTypeText;
        if (!visitedTypeTexts.insert(visitKey).second) {
          return;
        }

        std::vector<std::string> functionParamTypes;
        std::string functionReturnType;
        if (extractFunctionTypeParts(trimmedTypeText, functionParamTypes, functionReturnType)) {
          for (const auto &paramType : functionParamTypes) {
            collectTypeTargets(anchorForType, paramType, containerHint, depth + 1);
          }
          collectTypeTargets(anchorForType, functionReturnType, containerHint, depth + 1);
          return;
        }

        SymbolEntry typeTargetSymbol;
        std::string typeTargetUri;
        if (!resolveSemanticTypeTarget(anchorForType,
                                       trimmedTypeText,
                                       builtinsIndex,
                                       containerHint,
                                       typeTargetUri,
                                       typeTargetSymbol)) {
          return;
        }

        appendLocation(typeTargetUri, typeTargetSymbol);
        if (typeTargetSymbol.detail != "type alias") {
          return;
        }

        auto aliasKey = typeTargetUri + ":" + std::to_string(typeTargetSymbol.line) + ":" + std::to_string(typeTargetSymbol.start);
        if (!visitedAliasSymbols.insert(aliasKey).second) {
          return;
        }

        auto aliasTarget = aliasTargetTypeText(typeTargetSymbol);
        if (!aliasTarget.empty()) {
          collectTypeTargets(typeTargetUri, aliasTarget, typeTargetSymbol.containerName, depth + 1);
        }
      };

  if (resolvedSymbol.detail == "type alias") {
    auto aliasTarget = aliasTargetTypeText(resolvedSymbol);
    if (!aliasTarget.empty()) {
      collectTypeTargets(resolvedUri.empty() ? uri : resolvedUri, aliasTarget, resolvedSymbol.containerName, 0);
    }
  } else {
    auto typeTargetName = typeTargetNameFromSymbol(resolvedSymbol);
    if (!typeTargetName.empty()) {
      collectTypeTargets(resolvedUri.empty() ? uri : resolvedUri, typeTargetName, resolvedSymbol.containerName, 0);
    }
  }

  writeResult(request["id"], locations);
}

void Server::handleImplementation(rapidjson::Document &request) {
  if (!request.HasMember("id")) {
    return;
  }
  if (!request.HasMember("params") || !request["params"].IsObject()) {
    writeError(request["id"], JSONRPC_INVALID_PARAMS, "Missing implementation params");
    return;
  }

  auto &params = request["params"];
  if (!params.HasMember("textDocument") || !params["textDocument"].IsObject() ||
      !params["textDocument"].HasMember("uri") || !params["textDocument"]["uri"].IsString() ||
      !params.HasMember("position") || !params["position"].IsObject() || !params["position"].HasMember("line") ||
      !params["position"].HasMember("character") || !params["position"]["line"].IsUint() ||
      !params["position"]["character"].IsUint()) {
    writeError(request["id"], JSONRPC_INVALID_PARAMS, "Invalid implementation params");
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

  SymbolEntry targetSymbol;
  std::string targetUri;
  if (!resolveHoverSymbol(uri,
                          text,
                          params["position"]["line"].GetUint(),
                          params["position"]["character"].GetUint(),
                          word,
                          start,
                          targetUri,
                          targetSymbol)) {
    handleDefinition(request);
    return;
  }

  rapidjson::Document payloadDoc(rapidjson::kObjectType);
  auto &alloc = payloadDoc.GetAllocator();
  rapidjson::Value locations(rapidjson::kArrayType);
  std::set<std::string> seen;

  auto appendLocation = [&](const std::string &defUri, const SymbolEntry &symbol) {
    auto key = defUri + ":" + std::to_string(symbol.line) + ":" + std::to_string(symbol.start);
    if (!seen.insert(key).second) {
      return;
    }
    rapidjson::Value location(rapidjson::kObjectType);
    location.AddMember("uri", rapidjson::Value(defUri.c_str(), alloc), alloc);

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
  };

  auto inheritanceMatches = [&](const SymbolEntry &containerSymbol) -> bool {
    auto colonPos = containerSymbol.signature.find(':');
    if (colonPos == std::string::npos) {
      return false;
    }
    auto bases = splitCommaList(trimCopy(containerSymbol.signature.substr(colonPos + 1)));
    if (bases.empty()) {
      return false;
    }
    if (targetSymbol.kind == SYMBOL_KIND_CLASS) {
      return bases.front() == targetSymbol.name;
    }
    if (targetSymbol.kind == SYMBOL_KIND_INTERFACE) {
      return std::find(bases.begin(), bases.end(), targetSymbol.name) != bases.end();
    }
    return false;
  };

  auto containerImplementsTarget = [&](const std::string &candidateUri,
                                       const std::vector<SymbolEntry> &symbols,
                                       const std::string &containerName) -> bool {
    for (const auto &symbol : symbols) {
      if (symbol.name != containerName || symbol.isMember) {
        continue;
      }
      if (symbol.kind != SYMBOL_KIND_CLASS && symbol.kind != SYMBOL_KIND_STRUCT && symbol.kind != SYMBOL_KIND_INTERFACE) {
        continue;
      }
      return inheritanceMatches(symbol);
    }
    (void)candidateUri;
    return false;
  };

  for (const auto &doc : documents) {
    auto symbols = collectSymbolsForUri(doc.first, doc.second.text);
    for (const auto &symbol : symbols) {
      if (targetSymbol.kind == SYMBOL_KIND_CLASS || targetSymbol.kind == SYMBOL_KIND_INTERFACE) {
        if (symbol.isMember) {
          continue;
        }
        if (symbol.kind != SYMBOL_KIND_CLASS && symbol.kind != SYMBOL_KIND_STRUCT && symbol.kind != SYMBOL_KIND_INTERFACE) {
          continue;
        }
        if (inheritanceMatches(symbol)) {
          appendLocation(doc.first, symbol);
        }
        continue;
      }

      if (targetSymbol.kind == SYMBOL_KIND_METHOD && !targetSymbol.containerName.empty()) {
        if (symbol.kind != SYMBOL_KIND_METHOD || symbol.name != targetSymbol.name || symbol.containerName.empty()) {
          continue;
        }
        if (symbol.containerName == targetSymbol.containerName) {
          continue;
        }
        if (containerImplementsTarget(doc.first, symbols, symbol.containerName)) {
          appendLocation(doc.first, symbol);
        }
      }
    }
  }

  if (locations.Empty()) {
    appendLocation(targetUri, targetSymbol);
  }

  writeResult(request["id"], locations);
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
  std::set<std::string> seen;

  auto appendReferenceLocation = [&](const std::string &refUri, unsigned startLine, unsigned startChar, unsigned endLine,
                                     unsigned endChar) {
    auto key = refUri + ":" + std::to_string(startLine) + ":" + std::to_string(startChar);
    if (!seen.insert(key).second) {
      return;
    }
    rapidjson::Value location(rapidjson::kObjectType);
    location.AddMember("uri", rapidjson::Value(refUri.c_str(), alloc), alloc);

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
  };

  BuiltinApiIndex builtinsIndex;
  if (const auto *cachedBuiltins = getBuiltinsApiIndex()) {
    builtinsIndex = *cachedBuiltins;
  }

  SymbolEntry targetSymbol;
  std::string targetUri;
  bool targetResolved = false;

  std::string memberReceiver;
  bool memberContext = extractReceiverBeforeOffset(text, wordStart, memberReceiver);
  if (memberContext && !builtinsIndex.membersByType.empty()) {
    auto inferredType = inferBuiltinTypeForReceiver(text, memberReceiver, params["position"]["line"].GetUint(), builtinsIndex);
    if (inferredType.has_value()) {
      auto typeMembers = builtinsIndex.membersByType.find(*inferredType);
      if (typeMembers != builtinsIndex.membersByType.end()) {
        auto memberIt = typeMembers->second.find(word);
        if (memberIt != typeMembers->second.end()) {
          targetResolved = true;
          targetSymbol = memberIt->second;
          targetUri = builtinsIndex.uri;
        }
      }
    }
  }

  if (!targetResolved && resolveHoverSymbol(uri,
                                            text,
                                            params["position"]["line"].GetUint(),
                                            params["position"]["character"].GetUint(),
                                            word,
                                            wordStart,
                                            targetUri,
                                            targetSymbol)) {
    targetResolved = true;
  }

  if (!targetResolved && !builtinsIndex.topLevelByName.empty()) {
    auto topLevel = builtinsIndex.topLevelByName.find(word);
    if (topLevel != builtinsIndex.topLevelByName.end()) {
      targetResolved = true;
      targetSymbol = topLevel->second;
      targetUri = builtinsIndex.uri;
    }
  }

  if (targetResolved) {
    for (const auto &doc : documents) {
      std::vector<size_t> occurrences;
      appendOccurrences(doc.second.text, word, occurrences);
      for (size_t offset : occurrences) {
        unsigned startLine = 0;
        unsigned startChar = 0;
        unsigned endLine = 0;
        unsigned endChar = 0;
        positionFromOffset(doc.second.text, offset, startLine, startChar);
        positionFromOffset(doc.second.text, offset + word.size(), endLine, endChar);

        SymbolEntry occurrenceSymbol;
        std::string occurrenceUri;
        bool occurrenceResolved = false;

        std::string occurrenceReceiver;
        if (extractReceiverBeforeOffset(doc.second.text, offset, occurrenceReceiver) && !builtinsIndex.membersByType.empty()) {
          auto inferredType = inferBuiltinTypeForReceiver(doc.second.text, occurrenceReceiver, startLine, builtinsIndex);
          if (inferredType.has_value()) {
            auto typeMembers = builtinsIndex.membersByType.find(*inferredType);
            if (typeMembers != builtinsIndex.membersByType.end()) {
              auto memberIt = typeMembers->second.find(word);
              if (memberIt != typeMembers->second.end()) {
                occurrenceResolved = true;
                occurrenceSymbol = memberIt->second;
                occurrenceUri = builtinsIndex.uri;
              }
            }
          }
        }

        if (!occurrenceResolved &&
            resolveHoverSymbol(doc.first, doc.second.text, startLine, startChar, word, offset, occurrenceUri, occurrenceSymbol)) {
          occurrenceResolved = true;
        }

        if (!occurrenceResolved && !builtinsIndex.topLevelByName.empty()) {
          auto topLevel = builtinsIndex.topLevelByName.find(word);
          if (topLevel != builtinsIndex.topLevelByName.end()) {
            occurrenceResolved = true;
            occurrenceSymbol = topLevel->second;
            occurrenceUri = builtinsIndex.uri;
          }
        }

        if (!occurrenceResolved) {
          continue;
        }
        if (occurrenceUri != targetUri || occurrenceSymbol.line != targetSymbol.line ||
            occurrenceSymbol.start != targetSymbol.start || occurrenceSymbol.length != targetSymbol.length) {
          continue;
        }
        if (!includeDeclaration && doc.first == targetUri && startLine == targetSymbol.line &&
            startChar == targetSymbol.start) {
          continue;
        }
        appendReferenceLocation(doc.first, startLine, startChar, endLine, endChar);
      }
    }

    writeResult(request["id"], refs);
    return;
  }

  for (const auto &doc : documents) {
    std::vector<size_t> occurrences;
    appendOccurrences(doc.second.text, word, occurrences);

    std::set<size_t> declarationOffsets;
    if (!includeDeclaration) {
      auto symbols = collectSymbolsForUri(doc.first, doc.second.text);
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

      appendReferenceLocation(doc.first, startLine, startChar, endLine, endChar);
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
  auto symbols = collectSymbolsForUri(uri, text);

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
    if (symbol.isDeprecated) {
      appendDeprecatedTagArray(docSymbol, LSP_SYMBOL_TAG_DEPRECATED, alloc);
    }

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
    auto symbols = collectSymbolsForUri(doc.first, doc.second.text);
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
      if (symbol.isDeprecated) {
        appendDeprecatedTagArray(symbolInfo, LSP_SYMBOL_TAG_DEPRECATED, alloc);
      }
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

  BuiltinApiIndex builtinsIndex;
  if (const auto *cachedBuiltins = getBuiltinsApiIndex()) {
    builtinsIndex = *cachedBuiltins;
  }

  std::string word;
  size_t startOffset = 0;
  size_t endOffset = 0;
  std::string resolvedUri;
  SymbolEntry resolvedSymbol;
  if (!resolveSemanticSymbolAtPosition(uri,
                                       text,
                                       params["position"]["line"].GetUint(),
                                       params["position"]["character"].GetUint(),
                                       builtinsIndex,
                                       resolvedUri,
                                       resolvedSymbol,
                                       &startOffset,
                                       &endOffset,
                                       &word)) {
    rapidjson::Value nullResult;
    nullResult.SetNull();
    writeResult(request["id"], nullResult);
    return;
  }

  if (!symbolIsRenameOwned(resolvedUri, resolvedSymbol, builtinsIndex)) {
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

  BuiltinApiIndex builtinsIndex;
  if (const auto *cachedBuiltins = getBuiltinsApiIndex()) {
    builtinsIndex = *cachedBuiltins;
  }

  std::string oldName;
  size_t startOffset = 0;
  size_t endOffset = 0;
  std::string targetUri;
  SymbolEntry targetSymbol;
  if (!resolveSemanticSymbolAtPosition(uri,
                                       text,
                                       params["position"]["line"].GetUint(),
                                       params["position"]["character"].GetUint(),
                                       builtinsIndex,
                                       targetUri,
                                       targetSymbol,
                                       &startOffset,
                                       &endOffset,
                                       &oldName)) {
    rapidjson::Value nullResult;
    nullResult.SetNull();
    writeResult(request["id"], nullResult);
    return;
  }

  if (!symbolIsRenameOwned(targetUri, targetSymbol, builtinsIndex)) {
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

      std::string occurrenceResolvedUri;
      SymbolEntry occurrenceResolvedSymbol;
      if (!resolveSemanticSymbolAtPosition(doc.first,
                                           doc.second.text,
                                           sLine,
                                           sChar,
                                           builtinsIndex,
                                           occurrenceResolvedUri,
                                           occurrenceResolvedSymbol)) {
        continue;
      }

      if (!symbolIdentityMatches(occurrenceResolvedUri, occurrenceResolvedSymbol, targetUri, targetSymbol)) {
        continue;
      }

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

    if (!edits.Empty()) {
      changes.AddMember(rapidjson::Value(doc.first.c_str(), alloc), edits, alloc);
    }
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

  BuiltinApiIndex builtinsIndex;
  if (const auto *cachedBuiltins = getBuiltinsApiIndex()) {
    builtinsIndex = *cachedBuiltins;
  }

  std::string signatureParams;
  std::string signatureReturn;
  std::string signatureName = functionName;
  bool found = false;

  if (!builtinsIndex.topLevelByName.empty()) {
    std::string memberReceiver;
    bool memberContext = extractReceiverBeforeOffset(text, nameStart, memberReceiver);
    if (memberContext && !builtinsIndex.membersByType.empty()) {
      auto inferredType = inferBuiltinTypeForReceiver(text, memberReceiver, params["position"]["line"].GetUint(), builtinsIndex);
      if (inferredType.has_value()) {
        auto typeMembers = builtinsIndex.membersByType.find(*inferredType);
        if (typeMembers != builtinsIndex.membersByType.end()) {
          auto member = typeMembers->second.find(functionName);
          if (member != typeMembers->second.end() &&
              signaturePartsFromSignature(member->second.signature, signatureName, signatureParams, signatureReturn)) {
            found = true;
          }
        }
      }
    }
    if (!found) {
      auto builtinTop = builtinsIndex.topLevelByName.find(functionName);
      if (builtinTop != builtinsIndex.topLevelByName.end() && builtinTop->second.kind == SYMBOL_KIND_FUNCTION &&
          signaturePartsFromSignature(builtinTop->second.signature, signatureName, signatureParams, signatureReturn)) {
        found = true;
      }
    }
  }

  for (const auto &doc : documents) {
    if (found) {
      break;
    }
    if (findFunctionSignatureInText(doc.second.text, functionName, signatureName, signatureParams, signatureReturn)) {
      found = true;
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

  std::string label = signatureName + "(" + signatureParams + ")";
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
  auto handlerStart = std::chrono::steady_clock::now();
  if (!request.HasMember("params") || !request["params"].IsObject() ||
      !request["params"].HasMember("textDocument") || !request["params"]["textDocument"].IsObject() ||
      !request["params"]["textDocument"].HasMember("uri") || !request["params"]["textDocument"]["uri"].IsString()) {
    writeError(request["id"], JSONRPC_INVALID_PARAMS, "Invalid formatting params");
    return;
  }

  auto uri = std::string(request["params"]["textDocument"]["uri"].GetString());
  std::string text;
  if(!getDocumentTextByUri(uri, text)) {
    rapidjson::Value edits(rapidjson::kArrayType);
    writeResult(request["id"], edits);
    return;
  }

  rapidjson::Document payloadDoc(rapidjson::kObjectType);
  auto &alloc = payloadDoc.GetAllocator();
  rapidjson::Value edits(rapidjson::kArrayType);

  std::string formattedText;
  bool formattedOk = false;
  auto docIt = documents.find(uri);
  if(docIt != documents.end()) {
    formattedOk = getFormattedTextForDocument(uri, docIt->second, formattedText);
  }
  else {
    starbytes::linguistics::LinguisticsSession session(uri, text);
    starbytes::linguistics::FormatRequest formatRequest;
    auto formatResult = formatterEngine.format(session, linguisticsConfig, formatRequest);
    formattedOk = formatResult.ok;
    formattedText = std::move(formatResult.formattedText);
  }

  if(formattedOk && formattedText != text) {
    unsigned endLine = 0;
    unsigned endCharacter = 0;
    positionFromOffset(text, text.size(), endLine, endCharacter);

    starbytes::linguistics::TextSpan fullSpan;
    fullSpan.start = {0, 0};
    fullSpan.end = {endLine, endCharacter};

    starbytes::linguistics::TextEdit edit;
    edit.span = fullSpan;
    edit.replacement = formattedText;
    edits.PushBack(buildLspTextEdit(edit, alloc), alloc);
  }

  auto elapsed = static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - handlerStart).count());
  lspProfileFormattingNs += elapsed;
  maybeLogLspProfileSample("handler.formatting", elapsed, edits.Size());

  writeResult(request["id"], edits);
}

void Server::handleRangeFormatting(rapidjson::Document &request) {
  if (!request.HasMember("id")) {
    return;
  }
  auto handlerStart = std::chrono::steady_clock::now();
  if (!request.HasMember("params") || !request["params"].IsObject() ||
      !request["params"].HasMember("textDocument") || !request["params"]["textDocument"].IsObject() ||
      !request["params"]["textDocument"].HasMember("uri") || !request["params"]["textDocument"]["uri"].IsString() ||
      !request["params"].HasMember("range")) {
    writeError(request["id"], JSONRPC_INVALID_PARAMS, "Invalid range formatting params");
    return;
  }

  starbytes::linguistics::TextSpan requestedRange;
  if(!parseLspRange(request["params"]["range"], requestedRange)) {
    writeError(request["id"], JSONRPC_INVALID_PARAMS, "Invalid range formatting range");
    return;
  }

  auto uri = std::string(request["params"]["textDocument"]["uri"].GetString());
  std::string text;
  if(!getDocumentTextByUri(uri, text)) {
    rapidjson::Value edits(rapidjson::kArrayType);
    writeResult(request["id"], edits);
    return;
  }

  rapidjson::Document payloadDoc(rapidjson::kObjectType);
  auto &alloc = payloadDoc.GetAllocator();
  rapidjson::Value edits(rapidjson::kArrayType);

  std::string formattedText;
  bool formattedOk = false;
  auto docIt = documents.find(uri);
  if(docIt != documents.end()) {
    formattedOk = getFormattedTextForDocument(uri, docIt->second, formattedText);
  }
  else {
    starbytes::linguistics::LinguisticsSession session(uri, text);
    starbytes::linguistics::FormatRequest formatRequest;
    auto formatResult = formatterEngine.format(session, linguisticsConfig, formatRequest);
    formattedOk = formatResult.ok;
    formattedText = std::move(formatResult.formattedText);
  }

  if(formattedOk && formattedText != text) {
    // Current formatter emits full-document canonical output.
    unsigned endLine = 0;
    unsigned endCharacter = 0;
    positionFromOffset(text, text.size(), endLine, endCharacter);
    starbytes::linguistics::TextSpan fullSpan;
    fullSpan.start = {0, 0};
    fullSpan.end = {endLine, endCharacter};

    if(spansIntersect(fullSpan, requestedRange) || (requestedRange.start.line == 0 && requestedRange.start.character == 0)) {
      starbytes::linguistics::TextEdit edit;
      edit.span = fullSpan;
      edit.replacement = formattedText;
      edits.PushBack(buildLspTextEdit(edit, alloc), alloc);
    }
  }

  auto elapsed = static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - handlerStart).count());
  lspProfileFormattingNs += elapsed;
  maybeLogLspProfileSample("handler.rangeFormatting", elapsed, edits.Size());

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
  auto handlerStart = std::chrono::steady_clock::now();
  if (!request.HasMember("params") || !request["params"].IsObject() ||
      !request["params"].HasMember("textDocument") || !request["params"]["textDocument"].IsObject() ||
      !request["params"]["textDocument"].HasMember("uri") || !request["params"]["textDocument"]["uri"].IsString()) {
    writeError(request["id"], JSONRPC_INVALID_PARAMS, "Invalid code action params");
    return;
  }

  std::optional<starbytes::linguistics::TextSpan> requestedRange;
  if(request["params"].HasMember("range")) {
    starbytes::linguistics::TextSpan parsedRange;
    if(!parseLspRange(request["params"]["range"], parsedRange)) {
      writeError(request["id"], JSONRPC_INVALID_PARAMS, "Invalid code action range");
      return;
    }
    requestedRange = parsedRange;
  }

  auto uri = std::string(request["params"]["textDocument"]["uri"].GetString());
  std::string text;
  if(!getDocumentTextByUri(uri, text)) {
    rapidjson::Value actions(rapidjson::kArrayType);
    writeResult(request["id"], actions);
    return;
  }

  std::vector<starbytes::linguistics::LintFinding> lintFindings;
  std::vector<starbytes::linguistics::CodeAction> builtActions;

  auto docIt = documents.find(uri);
  if(docIt != documents.end()) {
    lintFindings = getLintFindingsForDocument(uri, docIt->second);
    auto safeOnly = linguisticsConfig.actions.preferSafeActions;
    builtActions = getCodeActionsForDocument(uri, docIt->second, safeOnly);
  }
  else {
    starbytes::linguistics::LinguisticsSession session(uri, text);
    auto lintResult = lintEngine.run(session, linguisticsConfig);
    lintFindings = lintResult.findings;
    starbytes::linguistics::SuggestionRequest suggestionRequest;
    suggestionRequest.includeLowConfidence = false;
    auto suggestionResult = suggestionEngine.run(session, linguisticsConfig, suggestionRequest);
    starbytes::linguistics::CodeActionRequest actionRequest;
    actionRequest.safeOnly = linguisticsConfig.actions.preferSafeActions;
    auto actionsResult = codeActionEngine.build(lintResult.findings, suggestionResult.suggestions, linguisticsConfig, actionRequest);
    builtActions = std::move(actionsResult.actions);
  }

  rapidjson::Document payloadDoc(rapidjson::kObjectType);
  auto &alloc = payloadDoc.GetAllocator();
  rapidjson::Value actions(rapidjson::kArrayType);

  auto appendDiagnosticsForRefs = [&](rapidjson::Value &diagnosticsOut,
                                      const std::vector<std::string> &refs) {
    for(const auto &ref : refs) {
      for(const auto &finding : lintFindings) {
        if(ref != finding.id && ref != finding.code) {
          continue;
        }
        rapidjson::Value diag(rapidjson::kObjectType);
        appendLintDiagnosticJson(diag, finding, uri, alloc);
        diagnosticsOut.PushBack(diag, alloc);
        break;
      }
    }
  };

  for(const auto &action : builtActions) {
    if(action.edits.empty()) {
      continue;
    }
    if(requestedRange.has_value() && !actionTouchesRange(action, *requestedRange)) {
      continue;
    }

    rapidjson::Value item(rapidjson::kObjectType);
    item.AddMember("title", rapidjson::Value(action.title.c_str(), alloc), alloc);
    item.AddMember("kind", rapidjson::Value(action.kind.c_str(), alloc), alloc);
    item.AddMember("isPreferred", action.preferred, alloc);

    rapidjson::Value diagnostics(rapidjson::kArrayType);
    appendDiagnosticsForRefs(diagnostics, action.diagnosticRefs);
    if(!diagnostics.Empty()) {
      item.AddMember("diagnostics", diagnostics, alloc);
    }

    rapidjson::Value workspaceEdit(rapidjson::kObjectType);
    rapidjson::Value changes(rapidjson::kObjectType);
    rapidjson::Value textEdits(rapidjson::kArrayType);
    for(const auto &edit : action.edits) {
      textEdits.PushBack(buildLspTextEdit(edit, alloc), alloc);
    }
    changes.AddMember(rapidjson::Value(uri.c_str(), alloc), textEdits, alloc);
    workspaceEdit.AddMember("changes", changes, alloc);
    item.AddMember("edit", workspaceEdit, alloc);

    rapidjson::Value data(rapidjson::kObjectType);
    data.AddMember("id", rapidjson::Value(action.id.c_str(), alloc), alloc);
    data.AddMember("isSafe", action.isSafe, alloc);
    item.AddMember("data", data, alloc);

    actions.PushBack(item, alloc);
  }

  auto elapsed = static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - handlerStart).count());
  lspProfileCodeActionNs += elapsed;
  maybeLogLspProfileSample("handler.codeAction", elapsed, actions.Size());

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

  std::vector<unsigned> encoded;
  auto docIt = documents.find(uri);
  if (docIt != documents.end()) {
    const auto &entries = getSemanticTokensForDocument(uri, docIt->second);
    encoded = encodeSemanticTokens(entries);
  } else {
    auto entries = collectSemanticTokenEntries(text, 0, 0, false);
    encoded = encodeSemanticTokens(entries);
  }

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
  std::vector<unsigned> encoded;
  auto docIt = documents.find(uri);
  if (docIt != documents.end()) {
    const auto &fullEntries = getSemanticTokensForDocument(uri, docIt->second);
    auto rangedEntries = filterSemanticTokensByLineRange(fullEntries, startLine, endLine);
    encoded = encodeSemanticTokens(rangedEntries);
  } else {
    auto entries = collectSemanticTokenEntries(text, startLine, endLine, true);
    encoded = encodeSemanticTokens(entries);
  }

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
  std::vector<unsigned> encoded;
  auto docIt = documents.find(uri);
  if (docIt != documents.end()) {
    const auto &entries = getSemanticTokensForDocument(uri, docIt->second);
    encoded = encodeSemanticTokens(entries);
  } else {
    auto entries = collectSemanticTokenEntries(text, 0, 0, false);
    encoded = encodeSemanticTokens(entries);
  }

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
  if (method == "textDocument/semanticTokens" || method == "textDocument/semanticTokens/full") {
    handleSemanticTokens(request);
    return;
  }
  if (method == "textDocument/semanticTokens/range") {
    handleSemanticTokensRange(request);
    return;
  }
  if (method == "textDocument/semanticTokens/edits" || method == "textDocument/semanticTokens/full/delta") {
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
    if (!request.HasMember("params") || !request["params"].IsObject() ||
        !request["params"].HasMember("textDocument") || !request["params"]["textDocument"].IsObject() ||
        !request["params"]["textDocument"].HasMember("uri") || !request["params"]["textDocument"]["uri"].IsString()) {
      writeError(request["id"], JSONRPC_INVALID_PARAMS, "Invalid textDocument/diagnostic params");
      return;
    }

    auto uri = std::string(request["params"]["textDocument"]["uri"].GetString());
    std::string text;
    getDocumentTextByUri(uri, text);
    std::vector<CompilerDiagnosticEntry> compilerDiagnostics;
    auto docIt = documents.find(uri);
    if (docIt != documents.end()) {
      compilerDiagnostics = getCompilerDiagnosticsForDocument(uri, docIt->second);
      text = docIt->second.text;
    }

    rapidjson::Document payloadDoc(rapidjson::kObjectType);
    auto &alloc = payloadDoc.GetAllocator();
    rapidjson::Value result(rapidjson::kObjectType);
    result.AddMember("kind", rapidjson::Value("full", alloc), alloc);
    rapidjson::Value items(rapidjson::kArrayType);
    for (const auto &entry : compilerDiagnostics) {
      rapidjson::Value diag(rapidjson::kObjectType);
      appendCompilerDiagnosticJson(diag, entry, uri, alloc);
      items.PushBack(diag, alloc);
    }
    appendLinguisticsDiagnosticsJson(uri, text, items, alloc);
    result.AddMember("items", items, alloc);
    result.AddMember("resultId", rapidjson::Value("0", alloc), alloc);
    writeResult(request["id"], result);
    return;
  }

  if (method == "workspace/diagnostic") {
    rapidjson::Document payloadDoc(rapidjson::kObjectType);
    auto &alloc = payloadDoc.GetAllocator();
    rapidjson::Value result(rapidjson::kObjectType);
    rapidjson::Value items(rapidjson::kArrayType);
    for (auto &doc : documents) {
      auto compilerDiagnostics = getCompilerDiagnosticsForDocument(doc.first, doc.second);
      rapidjson::Value report(rapidjson::kObjectType);
      report.AddMember("uri", rapidjson::Value(doc.first.c_str(), alloc), alloc);
      report.AddMember("kind", rapidjson::Value("full", alloc), alloc);
      rapidjson::Value diags(rapidjson::kArrayType);
      for (const auto &entry : compilerDiagnostics) {
        rapidjson::Value diag(rapidjson::kObjectType);
        appendCompilerDiagnosticJson(diag, entry, doc.first, alloc);
        diags.PushBack(diag, alloc);
      }
      appendLinguisticsDiagnosticsJson(doc.first, doc.second.text, diags, alloc);
      report.AddMember("items", diags, alloc);
      report.AddMember("resultId", rapidjson::Value("0", alloc), alloc);
      items.PushBack(report, alloc);
    }
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
  symbolCache.save();
}

} // namespace starbytes::lsp
