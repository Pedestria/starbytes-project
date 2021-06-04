
#include "LSPProtocol.h"
#include <fstream>

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
  InMessage *getMessage();
public:
  explicit Server(starbytes::lsp::ServerOptions &options);
  void run();
};

};

};

#endif // STARBYTES_LSP_SERVERMAIN_H
