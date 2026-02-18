#include <istream>
#include <ostream>
#include <string>
#include <unordered_map>

#include <rapidjson/document.h>

#ifndef STARBYTES_LSP_SERVERMAIN_H
#define STARBYTES_LSP_SERVERMAIN_H


namespace starbytes {

namespace lsp {

struct ServerOptions {
  std::istream &in;
  std::ostream &os;
};

class Server {
  std::istream &in;
  std::ostream &out;

  bool serverOn = true;
  bool shutdownRequested = false;

  std::unordered_map<std::string, std::string> openDocuments;

  bool readMessage(std::string &body);
  void writeMessage(rapidjson::Document &doc);
  void writeResult(const rapidjson::Value &id, rapidjson::Value &result);
  void writeError(const rapidjson::Value &id, int code, const char *message);

  static std::string trim(const std::string &in);
  static bool isIdentifierChar(char c);
  static std::string extractPrefixAtPosition(const std::string &text, unsigned line, unsigned character);

  void handleInitialize(rapidjson::Document &request);
  void handleShutdown(rapidjson::Document &request);
  void handleCompletion(rapidjson::Document &request);
  void handleSemanticTokens(rapidjson::Document &request);
  void handleDidOpen(rapidjson::Document &request);
  void handleDidChange(rapidjson::Document &request);
  void handleDidClose(rapidjson::Document &request);
  void processRequest(rapidjson::Document &request);
  void processNotification(rapidjson::Document &request);
public:
  explicit Server(starbytes::lsp::ServerOptions &options);
  void run();
};

};

};

#endif // STARBYTES_LSP_SERVERMAIN_H
