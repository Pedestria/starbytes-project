
#include "LSPProtocol.h"
#include <fstream>

#include <starbytes/Parser/Parser.h>

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
    
  std::shared_ptr<InMessage> getMessage();
  void sendError(OutError &err,MessageInfo & info);
  void sendMessage(std::shared_ptr<OutMessage> & outmessage,MessageInfo & info);
  void consumeNotification(std::shared_ptr<InMessage> & inmessage);
  void buildCancellableRequest(std::shared_ptr<InMessage> & initialMessage);
  void replyToRequest(std::shared_ptr<InMessage> & inmessage);
public:
  explicit Server(starbytes::lsp::ServerOptions &options);
  void run();
};

};

};

#endif // STARBYTES_LSP_SERVERMAIN_H
