#ifndef STARBYTES_LSP_SYMBOLCACHE_H
#define STARBYTES_LSP_SYMBOLCACHE_H

#include "SymbolTypes.h"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace starbytes::lsp {

class SymbolCache final {
  struct CacheEntry {
    uint64_t textHash = 0;
    std::vector<SymbolEntry> symbols;
  };

  std::filesystem::path cachePath;
  std::unordered_map<std::string, CacheEntry> entriesByUri;
  bool loaded = false;
  bool dirty = false;

  static uint64_t hashText(const std::string &text);
  void loadIfNeeded();
  void load();

public:
  void setCachePath(const std::filesystem::path &path);
  bool tryGet(const std::string &uri, const std::string &text, std::vector<SymbolEntry> &symbolsOut);
  void put(const std::string &uri, const std::string &text, const std::vector<SymbolEntry> &symbols);
  void invalidate(const std::string &uri);
  void save();
};

}

#endif
