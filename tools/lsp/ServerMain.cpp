#include "ServerMain.h"
#include "DocumentAnalysis.h"
#include "SymbolTypes.h"
#include "starbytes/base/CodeView.h"
#include "starbytes/base/Diagnostic.h"
#include "starbytes/base/DoxygenDoc.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
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

struct CompletionEntry {
  std::string label;
  int kind = COMPLETION_KIND_TEXT;
  std::string detail;
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
  documents[uri] = {textOut, 0, false};
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

void Server::setDocumentTextByUri(const std::string &uri, const std::string &text, int version, bool isOpen) {
  documents[uri] = {text, version, isOpen};
  semanticSnapshots.erase(uri);
  invalidateSymbolCacheForUri(uri);
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
      invalidateSymbolCacheForUri(uri);
      return;
    }
  }

  documents.erase(it);
  semanticSnapshots.erase(uri);
  invalidateSymbolCacheForUri(uri);
}

void Server::publishDiagnostics(const std::string &uri) {
  rapidjson::Document paramsDoc(rapidjson::kObjectType);
  auto &alloc = paramsDoc.GetAllocator();

  paramsDoc.AddMember("uri", rapidjson::Value(uri.c_str(), alloc), alloc);
  rapidjson::Value diagnostics(rapidjson::kArrayType);

  std::string text;
  if (getDocumentTextByUri(uri, text)) {
    auto compilerDiagnostics = collectCompilerDiagnosticsForText(uri, text);
    for (const auto &entry : compilerDiagnostics) {
      rapidjson::Value diag(rapidjson::kObjectType);
      rapidjson::Value range(rapidjson::kObjectType);
      rapidjson::Value start(rapidjson::kObjectType);
      rapidjson::Value end(rapidjson::kObjectType);
      unsigned startLine = entry.region.startLine > 0 ? entry.region.startLine - 1 : 0;
      unsigned endLine = entry.region.endLine > 0 ? entry.region.endLine - 1 : startLine;
      unsigned startCol = entry.region.startCol;
      unsigned endCol = entry.region.endCol > entry.region.startCol ? entry.region.endCol : (entry.region.startCol + 1);
      start.AddMember("line", startLine, alloc);
      start.AddMember("character", startCol, alloc);
      end.AddMember("line", endLine, alloc);
      end.AddMember("character", endCol, alloc);
      range.AddMember("start", start, alloc);
      range.AddMember("end", end, alloc);
      diag.AddMember("range", range, alloc);
      diag.AddMember("severity", entry.severity, alloc);
      diag.AddMember("source", rapidjson::Value("starbytes-compiler", alloc), alloc);
      diag.AddMember("message", rapidjson::Value(entry.message.c_str(), alloc), alloc);
      diagnostics.PushBack(diag, alloc);
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
  serverInfo.AddMember("version", rapidjson::Value("0.7.0", alloc), alloc);

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

  std::string builtinsUri;
  std::string builtinsText;
  BuiltinApiIndex builtinsIndex;
  if (getBuiltinsDocument(builtinsUri, builtinsText)) {
    builtinsIndex = buildBuiltinApiIndex(builtinsUri, builtinsText);
  }

  std::string memberReceiver;
  std::string memberPrefix;
  bool memberContext =
      extractMemberCompletionContext(text, offsetFromPosition(text, line, character), memberReceiver, memberPrefix);
  if (memberContext) {
    loweredPrefix = toLower(memberPrefix);
  }
  std::optional<std::string> inferredMemberType;
  if (memberContext && !builtinsIndex.membersByType.empty()) {
    inferredMemberType = inferBuiltinTypeForReceiver(text, memberReceiver, line, builtinsIndex);
  }
  if (memberContext && !inferredMemberType.has_value()) {
    memberContext = false;
    loweredPrefix = toLower(prefix);
  }

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

  if (!memberContext) {
    for (const auto &keyword : kKeywordCompletions) {
      pushEntry(keyword, COMPLETION_KIND_KEYWORD, "keyword");
    }
  }

  if (memberContext && inferredMemberType.has_value()) {
      auto memberIt = builtinsIndex.membersByType.find(*inferredMemberType);
      if (memberIt != builtinsIndex.membersByType.end()) {
        for (const auto &member : memberIt->second) {
          pushEntry(member.second.name, completionKindFromSymbolKind(member.second.kind), member.second.detail);
        }
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
      pushEntry(symbol.name, completionKindFromSymbolKind(symbol.kind), symbol.detail);
    }
  }

  if (!memberContext) {
    for (const auto &topLevel : builtinsIndex.topLevelByName) {
      const auto &symbol = topLevel.second;
      if (symbol.isMember) {
        continue;
      }
      pushEntry(symbol.name, completionKindFromSymbolKind(symbol.kind), symbol.detail);
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

  std::string label;
  if(item.HasMember("label") && item["label"].IsString()) {
    label = item["label"].GetString();
  }

  std::optional<SymbolEntry> resolvedSymbol;
  std::string builtinsUri;
  std::string builtinsText;
  BuiltinApiIndex builtinsIndex;
  if(getBuiltinsDocument(builtinsUri, builtinsText)) {
    builtinsIndex = buildBuiltinApiIndex(builtinsUri, builtinsText);
  }
  if(!label.empty()) {
    auto top = builtinsIndex.topLevelByName.find(label);
    if(top != builtinsIndex.topLevelByName.end()) {
      resolvedSymbol = top->second;
    }
    if(!resolvedSymbol.has_value()) {
      for(const auto &memberType : builtinsIndex.membersByType) {
        auto m = memberType.second.find(label);
        if(m != memberType.second.end()) {
          resolvedSymbol = m->second;
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
          break;
        }
      }
    }
  }

  if(resolvedSymbol.has_value()) {
    if(!item.HasMember("detail")) {
      item.AddMember("detail", rapidjson::Value(resolvedSymbol->detail.c_str(), alloc), alloc);
    }
    std::string renderedDoc = DoxygenDoc::parse(resolvedSymbol->documentation).renderMarkdown();
    if(renderedDoc.empty()) {
      renderedDoc = resolvedSymbol->documentation;
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

  std::string builtinsUri;
  std::string builtinsText;
  BuiltinApiIndex builtinsIndex;
  if (getBuiltinsDocument(builtinsUri, builtinsText)) {
    builtinsIndex = buildBuiltinApiIndex(builtinsUri, builtinsText);
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

  std::string builtinsUri;
  std::string builtinsText;
  BuiltinApiIndex builtinsIndex;
  if (getBuiltinsDocument(builtinsUri, builtinsText)) {
    builtinsIndex = buildBuiltinApiIndex(builtinsUri, builtinsText);
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

  std::string builtinsUri;
  std::string builtinsText;
  BuiltinApiIndex builtinsIndex;
  if (getBuiltinsDocument(builtinsUri, builtinsText)) {
    builtinsIndex = buildBuiltinApiIndex(builtinsUri, builtinsText);
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
    if (!request.HasMember("params") || !request["params"].IsObject() ||
        !request["params"].HasMember("textDocument") || !request["params"]["textDocument"].IsObject() ||
        !request["params"]["textDocument"].HasMember("uri") || !request["params"]["textDocument"]["uri"].IsString()) {
      writeError(request["id"], JSONRPC_INVALID_PARAMS, "Invalid textDocument/diagnostic params");
      return;
    }

    auto uri = std::string(request["params"]["textDocument"]["uri"].GetString());
    std::string text;
    getDocumentTextByUri(uri, text);
    auto compilerDiagnostics = collectCompilerDiagnosticsForText(uri, text);

    rapidjson::Document payloadDoc(rapidjson::kObjectType);
    auto &alloc = payloadDoc.GetAllocator();
    rapidjson::Value result(rapidjson::kObjectType);
    result.AddMember("kind", rapidjson::Value("full", alloc), alloc);
    rapidjson::Value items(rapidjson::kArrayType);
    for (const auto &entry : compilerDiagnostics) {
      rapidjson::Value diag(rapidjson::kObjectType);
      rapidjson::Value range(rapidjson::kObjectType);
      rapidjson::Value start(rapidjson::kObjectType);
      rapidjson::Value end(rapidjson::kObjectType);
      unsigned startLine = entry.region.startLine > 0 ? entry.region.startLine - 1 : 0;
      unsigned endLine = entry.region.endLine > 0 ? entry.region.endLine - 1 : startLine;
      unsigned startCol = entry.region.startCol;
      unsigned endCol = entry.region.endCol > entry.region.startCol ? entry.region.endCol : (entry.region.startCol + 1);
      start.AddMember("line", startLine, alloc);
      start.AddMember("character", startCol, alloc);
      end.AddMember("line", endLine, alloc);
      end.AddMember("character", endCol, alloc);
      range.AddMember("start", start, alloc);
      range.AddMember("end", end, alloc);
      diag.AddMember("range", range, alloc);
      diag.AddMember("severity", entry.severity, alloc);
      diag.AddMember("source", rapidjson::Value("starbytes-compiler", alloc), alloc);
      diag.AddMember("message", rapidjson::Value(entry.message.c_str(), alloc), alloc);
      items.PushBack(diag, alloc);
    }
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
    for (const auto &doc : documents) {
      auto compilerDiagnostics = collectCompilerDiagnosticsForText(doc.first, doc.second.text);
      rapidjson::Value report(rapidjson::kObjectType);
      report.AddMember("uri", rapidjson::Value(doc.first.c_str(), alloc), alloc);
      report.AddMember("kind", rapidjson::Value("full", alloc), alloc);
      rapidjson::Value diags(rapidjson::kArrayType);
      for (const auto &entry : compilerDiagnostics) {
        rapidjson::Value diag(rapidjson::kObjectType);
        rapidjson::Value range(rapidjson::kObjectType);
        rapidjson::Value start(rapidjson::kObjectType);
        rapidjson::Value end(rapidjson::kObjectType);
        unsigned startLine = entry.region.startLine > 0 ? entry.region.startLine - 1 : 0;
        unsigned endLine = entry.region.endLine > 0 ? entry.region.endLine - 1 : startLine;
        unsigned startCol = entry.region.startCol;
        unsigned endCol = entry.region.endCol > entry.region.startCol ? entry.region.endCol : (entry.region.startCol + 1);
        start.AddMember("line", startLine, alloc);
        start.AddMember("character", startCol, alloc);
        end.AddMember("line", endLine, alloc);
        end.AddMember("character", endCol, alloc);
        range.AddMember("start", start, alloc);
        range.AddMember("end", end, alloc);
        diag.AddMember("range", range, alloc);
        diag.AddMember("severity", entry.severity, alloc);
        diag.AddMember("source", rapidjson::Value("starbytes-compiler", alloc), alloc);
        diag.AddMember("message", rapidjson::Value(entry.message.c_str(), alloc), alloc);
        diags.PushBack(diag, alloc);
      }
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
