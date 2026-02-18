#include "ServerMain.h"

#include <algorithm>
#include <cctype>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

namespace starbytes::lsp {

namespace {

constexpr int TEXT_DOCUMENT_SYNC_FULL = 1;

enum class CompletionKind : int {
  Function = 3,
  Variable = 6,
  Class = 7,
  Module = 9,
  Keyword = 14
};

enum class SemanticTokenType : unsigned {
  Namespace = 0,
  Class = 1,
  Function = 2,
  Variable = 3,
  Keyword = 4
};

struct CompletionEntry {
  std::string label;
  CompletionKind kind;
  std::string detail;
};

struct SemanticTokenEntry {
  unsigned line;
  unsigned start;
  unsigned length;
  SemanticTokenType type;
};

const std::vector<std::string> kKeywordCompletions = {
    "decl", "func", "class", "scope", "import", "if", "elif", "else", "return", "new", "true", "false"};

const std::regex kDeclRegex(R"(\bdecl\s+([A-Za-z_][A-Za-z0-9_]*))");
const std::regex kFuncRegex(R"(\bfunc\s+([A-Za-z_][A-Za-z0-9_]*))");
const std::regex kClassRegex(R"(\bclass\s+([A-Za-z_][A-Za-z0-9_]*))");
const std::regex kScopeRegex(R"(\bscope\s+([A-Za-z_][A-Za-z0-9_]*))");

const std::regex kKeywordRegex(
    R"(\b(decl|func|class|scope|import|if|elif|else|return|new|true|false)\b)");

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
  return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
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

  unsigned cursor = std::min<unsigned>(character, current.size());
  unsigned start = cursor;
  while (start > 0 && isIdentifierChar(current[start - 1])) {
    --start;
  }
  return current.substr(start, cursor - start);
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
        contentLength = (size_t)std::stoul(value);
      } catch (...) {
        return false;
      }
    }
  }

  if (!sawHeader || contentLength == 0) {
    return false;
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

void Server::handleInitialize(rapidjson::Document &request) {
  if (!request.HasMember("id")) {
    return;
  }

  rapidjson::Document payloadDoc(rapidjson::kObjectType);
  auto &alloc = payloadDoc.GetAllocator();

  rapidjson::Value capabilities(rapidjson::kObjectType);
  rapidjson::Value textDocSync(rapidjson::kObjectType);
  textDocSync.AddMember("openClose", true, alloc);
  textDocSync.AddMember("change", TEXT_DOCUMENT_SYNC_FULL, alloc);
  capabilities.AddMember("textDocumentSync", textDocSync, alloc);

  rapidjson::Value completionProvider(rapidjson::kObjectType);
  completionProvider.AddMember("resolveProvider", false, alloc);
  rapidjson::Value triggerChars(rapidjson::kArrayType);
  triggerChars.PushBack(rapidjson::Value(".", alloc), alloc);
  completionProvider.AddMember("triggerCharacters", triggerChars, alloc);
  capabilities.AddMember("completionProvider", completionProvider, alloc);

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
  semanticProvider.AddMember("full", true, alloc);
  capabilities.AddMember("semanticTokensProvider", semanticProvider, alloc);

  rapidjson::Value serverInfo(rapidjson::kObjectType);
  serverInfo.AddMember("name", rapidjson::Value("Starbytes LSP", alloc), alloc);
  serverInfo.AddMember("version", rapidjson::Value("0.1", alloc), alloc);

  rapidjson::Value result(rapidjson::kObjectType);
  result.AddMember("capabilities", capabilities, alloc);
  result.AddMember("serverInfo", serverInfo, alloc);
  writeResult(request["id"], result);
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

void Server::handleDidOpen(rapidjson::Document &request) {
  if (!request.HasMember("params")) {
    return;
  }
  auto &params = request["params"];
  if (!params.IsObject() || !params.HasMember("textDocument")) {
    return;
  }
  auto &textDoc = params["textDocument"];
  if (!textDoc.IsObject() || !textDoc.HasMember("uri") || !textDoc.HasMember("text")) {
    return;
  }
  openDocuments[textDoc["uri"].GetString()] = textDoc["text"].GetString();
}

void Server::handleDidChange(rapidjson::Document &request) {
  if (!request.HasMember("params")) {
    return;
  }
  auto &params = request["params"];
  if (!params.IsObject() || !params.HasMember("textDocument") || !params.HasMember("contentChanges")) {
    return;
  }
  auto &textDoc = params["textDocument"];
  auto &changes = params["contentChanges"];
  if (!textDoc.IsObject() || !changes.IsArray() || changes.Empty() || !textDoc.HasMember("uri")) {
    return;
  }

  const char *uri = textDoc["uri"].GetString();
  auto &lastChange = changes[changes.Size() - 1];
  if (!lastChange.IsObject() || !lastChange.HasMember("text")) {
    return;
  }
  openDocuments[uri] = lastChange["text"].GetString();
}

void Server::handleDidClose(rapidjson::Document &request) {
  if (!request.HasMember("params")) {
    return;
  }
  auto &params = request["params"];
  if (!params.IsObject() || !params.HasMember("textDocument")) {
    return;
  }
  auto &textDoc = params["textDocument"];
  if (!textDoc.IsObject() || !textDoc.HasMember("uri")) {
    return;
  }
  openDocuments.erase(textDoc["uri"].GetString());
}

void Server::handleCompletion(rapidjson::Document &request) {
  if (!request.HasMember("id")) {
    return;
  }
  if (!request.HasMember("params") || !request["params"].IsObject()) {
    writeError(request["id"], -32602, "Missing completion params");
    return;
  }
  auto &params = request["params"];
  if (!params.HasMember("textDocument") || !params["textDocument"].IsObject() ||
      !params["textDocument"].HasMember("uri")) {
    writeError(request["id"], -32602, "Missing textDocument.uri");
    return;
  }

  std::string uri = params["textDocument"]["uri"].GetString();
  auto foundDoc = openDocuments.find(uri);
  std::string docText;
  if (foundDoc != openDocuments.end()) {
    docText = foundDoc->second;
  }

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
  auto prefix = extractPrefixAtPosition(docText, line, character);

  std::vector<CompletionEntry> completions;
  completions.reserve(128);
  std::set<std::string> seenLabels;

  auto addCompletion = [&](const std::string &label, CompletionKind kind, const std::string &detail) {
    if (!prefix.empty() && label.rfind(prefix, 0) != 0) {
      return;
    }
    if (seenLabels.insert(label).second) {
      completions.push_back({label, kind, detail});
    }
  };

  for (auto &keyword : kKeywordCompletions) {
    addCompletion(keyword, CompletionKind::Keyword, "keyword");
  }

  auto scanPattern = [&](const std::regex &pattern, CompletionKind kind, const std::string &detail) {
    for (std::sregex_iterator it(docText.begin(), docText.end(), pattern), end; it != end; ++it) {
      auto label = (*it)[1].str();
      addCompletion(label, kind, detail);
    }
  };
  scanPattern(kDeclRegex, CompletionKind::Variable, "variable");
  scanPattern(kFuncRegex, CompletionKind::Function, "function");
  scanPattern(kClassRegex, CompletionKind::Class, "class");
  scanPattern(kScopeRegex, CompletionKind::Module, "scope");

  rapidjson::Document payloadDoc(rapidjson::kObjectType);
  auto &alloc = payloadDoc.GetAllocator();
  rapidjson::Value items(rapidjson::kArrayType);
  for (const auto &entry : completions) {
    rapidjson::Value item(rapidjson::kObjectType);
    item.AddMember("label", rapidjson::Value(entry.label.c_str(), alloc), alloc);
    item.AddMember("kind", static_cast<int>(entry.kind), alloc);
    item.AddMember("detail", rapidjson::Value(entry.detail.c_str(), alloc), alloc);
    items.PushBack(item, alloc);
  }
  writeResult(request["id"], items);
}

void Server::handleSemanticTokens(rapidjson::Document &request) {
  if (!request.HasMember("id")) {
    return;
  }
  if (!request.HasMember("params") || !request["params"].IsObject()) {
    writeError(request["id"], -32602, "Missing semantic token params");
    return;
  }
  auto &params = request["params"];
  if (!params.HasMember("textDocument") || !params["textDocument"].IsObject() ||
      !params["textDocument"].HasMember("uri")) {
    writeError(request["id"], -32602, "Missing textDocument.uri");
    return;
  }

  std::string uri = params["textDocument"]["uri"].GetString();
  auto foundDoc = openDocuments.find(uri);
  std::string docText;
  if (foundDoc != openDocuments.end()) {
    docText = foundDoc->second;
  }

  std::vector<SemanticTokenEntry> tokens;
  std::istringstream textStream(docText);
  std::string lineText;
  unsigned lineIndex = 0;
  while (std::getline(textStream, lineText)) {
    for (std::sregex_iterator it(lineText.begin(), lineText.end(), kKeywordRegex), end; it != end; ++it) {
      unsigned start = (unsigned)(*it).position(0);
      unsigned length = (unsigned)(*it).length(0);
      tokens.push_back({lineIndex, start, length, SemanticTokenType::Keyword});
    }
    for (std::sregex_iterator it(lineText.begin(), lineText.end(), kClassRegex), end; it != end; ++it) {
      unsigned start = (unsigned)(*it).position(1);
      unsigned length = (unsigned)(*it).length(1);
      tokens.push_back({lineIndex, start, length, SemanticTokenType::Class});
    }
    for (std::sregex_iterator it(lineText.begin(), lineText.end(), kFuncRegex), end; it != end; ++it) {
      unsigned start = (unsigned)(*it).position(1);
      unsigned length = (unsigned)(*it).length(1);
      tokens.push_back({lineIndex, start, length, SemanticTokenType::Function});
    }
    for (std::sregex_iterator it(lineText.begin(), lineText.end(), kDeclRegex), end; it != end; ++it) {
      unsigned start = (unsigned)(*it).position(1);
      unsigned length = (unsigned)(*it).length(1);
      tokens.push_back({lineIndex, start, length, SemanticTokenType::Variable});
    }
    for (std::sregex_iterator it(lineText.begin(), lineText.end(), kScopeRegex), end; it != end; ++it) {
      unsigned start = (unsigned)(*it).position(1);
      unsigned length = (unsigned)(*it).length(1);
      tokens.push_back({lineIndex, start, length, SemanticTokenType::Namespace});
    }
    ++lineIndex;
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
    return (unsigned)lhs.type < (unsigned)rhs.type;
  });

  rapidjson::Document payloadDoc(rapidjson::kObjectType);
  auto &alloc = payloadDoc.GetAllocator();
  rapidjson::Value encodedData(rapidjson::kArrayType);
  unsigned prevLine = 0;
  unsigned prevStart = 0;
  bool first = true;
  for (const auto &token : tokens) {
    unsigned deltaLine = first ? token.line : (token.line - prevLine);
    unsigned deltaStart = first ? token.start : (deltaLine == 0 ? token.start - prevStart : token.start);
    encodedData.PushBack(deltaLine, alloc);
    encodedData.PushBack(deltaStart, alloc);
    encodedData.PushBack(token.length, alloc);
    encodedData.PushBack((unsigned)token.type, alloc);
    encodedData.PushBack(0, alloc);
    prevLine = token.line;
    prevStart = token.start;
    first = false;
  }

  rapidjson::Value result(rapidjson::kObjectType);
  result.AddMember("data", encodedData, alloc);
  writeResult(request["id"], result);
}

void Server::processRequest(rapidjson::Document &request) {
  if (!request.HasMember("method") || !request["method"].IsString()) {
    writeError(request["id"], -32600, "Invalid request");
    return;
  }

  std::string method = request["method"].GetString();
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
  if (method == "textDocument/semanticTokens/full") {
    handleSemanticTokens(request);
    return;
  }
  writeError(request["id"], -32601, "Method not found");
}

void Server::processNotification(rapidjson::Document &request) {
  if (!request.HasMember("method") || !request["method"].IsString()) {
    return;
  }
  std::string method = request["method"].GetString();
  if (method == "exit") {
    (void)shutdownRequested;
    serverOn = false;
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
  if (method == "textDocument/didClose") {
    handleDidClose(request);
    return;
  }
  if (method == "initialized") {
    return;
  }
}

void Server::run() {
  while (serverOn) {
    std::string body;
    if (!readMessage(body)) {
      break;
    }

    rapidjson::Document request;
    request.Parse(body.c_str(), body.size());
    if (request.HasParseError() || !request.IsObject()) {
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
