#include "SymbolCache.h"

#include <fstream>
#include <iomanip>
#include <sstream>

namespace starbytes::lsp {

namespace {

constexpr const char *kCacheHeader = "STARBYTES_LSP_SYMBOL_CACHE_V1";

}

uint64_t SymbolCache::hashText(const std::string &text) {
  uint64_t hash = 1469598103934665603ULL;
  for(unsigned char c : text) {
    hash ^= static_cast<uint64_t>(c);
    hash *= 1099511628211ULL;
  }
  return hash;
}

void SymbolCache::setCachePath(const std::filesystem::path &path) {
  if(cachePath == path) {
    return;
  }
  cachePath = path;
  entriesByUri.clear();
  loaded = false;
  dirty = false;
}

void SymbolCache::loadIfNeeded() {
  if(loaded) {
    return;
  }
  load();
}

void SymbolCache::load() {
  entriesByUri.clear();
  loaded = true;

  if(cachePath.empty()) {
    return;
  }

  std::ifstream in(cachePath, std::ios::in);
  if(!in.is_open()) {
    return;
  }

  std::string header;
  if(!std::getline(in, header)) {
    return;
  }
  if(header != kCacheHeader) {
    return;
  }

  while(in) {
    std::string tag;
    in >> tag;
    if(!in) {
      break;
    }
    if(tag != "ENTRY") {
      std::string discard;
      std::getline(in, discard);
      continue;
    }

    std::string uri;
    uint64_t textHash = 0;
    size_t symbolCount = 0;
    in >> std::quoted(uri) >> textHash >> symbolCount;
    if(!in) {
      entriesByUri.clear();
      return;
    }

    CacheEntry entry;
    entry.textHash = textHash;
    entry.symbols.reserve(symbolCount);
    for(size_t i = 0; i < symbolCount; ++i) {
      std::string symbolTag;
      in >> symbolTag;
      if(!in || symbolTag != "SYM") {
        entriesByUri.clear();
        return;
      }

      SymbolEntry symbol;
      in >> std::quoted(symbol.name)
         >> symbol.kind
         >> std::quoted(symbol.detail)
         >> std::quoted(symbol.signature)
         >> std::quoted(symbol.documentation)
         >> std::quoted(symbol.containerName)
         >> symbol.isMember
         >> symbol.line
         >> symbol.start
         >> symbol.length;
      if(!in) {
        entriesByUri.clear();
        return;
      }
      entry.symbols.push_back(std::move(symbol));
    }

    entriesByUri[uri] = std::move(entry);
  }
}

bool SymbolCache::tryGet(const std::string &uri, const std::string &text, std::vector<SymbolEntry> &symbolsOut) {
  loadIfNeeded();
  auto it = entriesByUri.find(uri);
  if(it == entriesByUri.end()) {
    return false;
  }
  auto textHash = hashText(text);
  if(it->second.textHash != textHash) {
    return false;
  }
  symbolsOut = it->second.symbols;
  return true;
}

void SymbolCache::put(const std::string &uri, const std::string &text, const std::vector<SymbolEntry> &symbols) {
  loadIfNeeded();
  auto textHash = hashText(text);
  auto it = entriesByUri.find(uri);
  if(it != entriesByUri.end() && it->second.textHash == textHash && it->second.symbols == symbols) {
    return;
  }
  entriesByUri[uri] = CacheEntry{textHash, symbols};
  dirty = true;
}

void SymbolCache::invalidate(const std::string &uri) {
  loadIfNeeded();
  auto erased = entriesByUri.erase(uri);
  if(erased > 0) {
    dirty = true;
  }
}

void SymbolCache::save() {
  loadIfNeeded();
  if(!dirty || cachePath.empty()) {
    return;
  }

  auto parent = cachePath.parent_path();
  if(!parent.empty()) {
    std::error_code ec;
    std::filesystem::create_directories(parent, ec);
    if(ec) {
      return;
    }
  }

  std::ofstream out(cachePath, std::ios::out | std::ios::trunc);
  if(!out.is_open()) {
    return;
  }

  out << kCacheHeader << "\n";
  for(const auto &entryPair : entriesByUri) {
    const auto &uri = entryPair.first;
    const auto &entry = entryPair.second;
    out << "ENTRY " << std::quoted(uri) << " " << entry.textHash << " " << entry.symbols.size() << "\n";
    for(const auto &symbol : entry.symbols) {
      out << "SYM "
          << std::quoted(symbol.name) << " "
          << symbol.kind << " "
          << std::quoted(symbol.detail) << " "
          << std::quoted(symbol.signature) << " "
          << std::quoted(symbol.documentation) << " "
          << std::quoted(symbol.containerName) << " "
          << symbol.isMember << " "
          << symbol.line << " "
          << symbol.start << " "
          << symbol.length << "\n";
    }
  }
  dirty = false;
}

}
