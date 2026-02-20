#ifndef STARBYTES_LSP_SYMBOLTYPES_H
#define STARBYTES_LSP_SYMBOLTYPES_H

#include <string>
#include <unordered_map>
#include <vector>

namespace starbytes::lsp {

inline constexpr int COMPLETION_KIND_TEXT = 1;
inline constexpr int COMPLETION_KIND_METHOD = 2;
inline constexpr int COMPLETION_KIND_FUNCTION = 3;
inline constexpr int COMPLETION_KIND_CONSTRUCTOR = 4;
inline constexpr int COMPLETION_KIND_FIELD = 5;
inline constexpr int COMPLETION_KIND_VARIABLE = 6;
inline constexpr int COMPLETION_KIND_CLASS = 7;
inline constexpr int COMPLETION_KIND_INTERFACE = 8;
inline constexpr int COMPLETION_KIND_MODULE = 9;
inline constexpr int COMPLETION_KIND_PROPERTY = 10;
inline constexpr int COMPLETION_KIND_UNIT = 11;
inline constexpr int COMPLETION_KIND_VALUE = 12;
inline constexpr int COMPLETION_KIND_ENUM = 13;
inline constexpr int COMPLETION_KIND_KEYWORD = 14;
inline constexpr int COMPLETION_KIND_SNIPPET = 15;
inline constexpr int COMPLETION_KIND_STRUCT = 22;

inline constexpr int SYMBOL_KIND_NAMESPACE = 3;
inline constexpr int SYMBOL_KIND_CLASS = 5;
inline constexpr int SYMBOL_KIND_METHOD = 6;
inline constexpr int SYMBOL_KIND_FIELD = 8;
inline constexpr int SYMBOL_KIND_ENUM = 10;
inline constexpr int SYMBOL_KIND_INTERFACE = 11;
inline constexpr int SYMBOL_KIND_FUNCTION = 12;
inline constexpr int SYMBOL_KIND_VARIABLE = 13;
inline constexpr int SYMBOL_KIND_CONSTANT = 14;
inline constexpr int SYMBOL_KIND_STRUCT = 23;

inline constexpr unsigned TOKEN_TYPE_NAMESPACE = 0;
inline constexpr unsigned TOKEN_TYPE_CLASS = 1;
inline constexpr unsigned TOKEN_TYPE_FUNCTION = 2;
inline constexpr unsigned TOKEN_TYPE_VARIABLE = 3;
inline constexpr unsigned TOKEN_TYPE_KEYWORD = 4;

struct SymbolEntry {
  std::string name;
  int kind = 13;
  std::string detail;
  std::string signature;
  std::string documentation;
  std::string containerName;
  bool isMember = false;
  unsigned line = 0;
  unsigned start = 0;
  unsigned length = 0;

  bool operator==(const SymbolEntry &other) const {
    return name == other.name &&
           kind == other.kind &&
           detail == other.detail &&
           signature == other.signature &&
           documentation == other.documentation &&
           containerName == other.containerName &&
           isMember == other.isMember &&
           line == other.line &&
           start == other.start &&
           length == other.length;
  }
};

struct SemanticTokenEntry {
  unsigned line = 0;
  unsigned start = 0;
  unsigned length = 0;
  unsigned type = 4;
};

}

#endif
