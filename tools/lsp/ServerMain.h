
#include "LSPProtocol.h"
#include <fstream>
#include <future>

#include <starbytes/compiler/Parser.h>

#ifndef STARBYTES_LSP_SERVERMAIN_H
#define STARBYTES_LSP_SERVERMAIN_H


namespace starbytes {

namespace lsp {

struct ServerOptions {
 std::istream & in;
 std::ostream & os;
};

/**
 * */
class Server {
    
  std::istream & in;
    
  std::ostream & out;

  std::shared_ptr<ThreadedServerContext> serverContext;

  WorkspaceManager workspaceManager;

  MessageIO messageIO;

  std::vector<std::thread> threadsInFlight;

  std::mutex mutex;

  bool serverOn = true;
    
  std::shared_ptr<InMessage> getMessage();
  void sendError(OutError &err,std::shared_ptr<OutMessage> & out);
  void sendMessage(std::shared_ptr<OutMessage> & outmessage,MessageInfo & info);
  void consumeNotification(std::shared_ptr<InMessage> & inmessage);

  void buildCancellableRequest(std::shared_ptr<InMessage> & initialMessage);
  void replyToRequest(std::shared_ptr<InMessage> & inmessage);
  void processMessage(std::shared_ptr<InMessage> & inmessage);
  void cycle();
public:
  explicit Server(starbytes::lsp::ServerOptions &options);
  void run();
};

};

};

#endif // STARBYTES_LSP_SERVERMAIN_H
