#include <llvm/ADT/StringRef.h>

#ifndef STARBYTES_LSP_LSPPROTOCOL_H
#define STARBYTES_LSP_LSPPROTOCOL_H

namespace starbytes::lsp {

#define LSP_METHOD llvm::StringRef 
#define DECL_LSP_METHOD extern LSP_METHOD

DECL_LSP_METHOD Init;
DECL_LSP_METHOD textDocOpen;
DECL_LSP_METHOD Exit;


struct InMessage {
  bool isNotification = false;
  llvm::StringRef method;
};


struct OutMessage {
  llvm::StringRef method;
  
};

};

#endif // STARBYTES_LSP_LSPPROTOCOL_H