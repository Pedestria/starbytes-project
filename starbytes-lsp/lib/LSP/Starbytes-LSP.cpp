#include "LSP/Starbytes-LSP.h"
#include "LSP/LSPProtocol.h"
#include "starbytes/Base/Base.h"
// #include "Parser/Lexer.h"
// #include "Parser/Parser.h"
#include <array>
#include <cctype>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

// #ifdef HAS_IO_H
// #include <io.h>
// #endif

// #ifdef HAS_UNISTD_H
// #include <unistd.h>
// #endif

STARBYTES_STD_NAMESPACE
using namespace std;

namespace LSP {


void LSPQueue::queueMessage(LSPServerMessage *msg) { messages.push_back(msg); };

LSPServerMessage *LSPQueue::getLatest() {
  LSPServerMessage *rc = messages.back();
  messages.pop_back();
  return rc;
};

void StarbytesLSPServer::init() {
  getMessageFromStdin();
  LSPServerMessage *INITAL = queue.getLatest();
  if (INITAL->type == Request) {
    LSPServerRequest *RQ = (LSPServerRequest *)INITAL;
    if (RQ->method == INITIALIZE) {
      json_transit.sendIntializeMessage(RQ->id);
    }
  }
};

void StarbytesLSPServer::getMessageFromStdin() {
  LSP::LSPServerMessage *cntr = json_transit.read();
  queue.queueMessage(cntr);
};

void StarbytesLSPServer::addFile(StarbytesLSPServerTextDocument &file_ref) {
  files.push_back(file_ref);
};

bool StarbytesLSPServer::fileExists(LSPTextDocumentIdentifier &id_ref) {
  for (auto &txt_dcmt : files) {
    if (txt_dcmt.id == id_ref) {
      return true;
    }
  }
  return false;
};

LSPTextDocumentItem NULL_ITEM;
/*If file does not exist, will return null `textDocumentItem`*/
LSPTextDocumentItem &
StarbytesLSPServer::getTextDocumentOfFile(LSPTextDocumentIdentifier &id_ref) {
  LSPTextDocumentItem &nulldoc = NULL_ITEM;
  for (auto &txt_dcmt : files) {
    if (txt_dcmt.id == id_ref) {
      return txt_dcmt.content;
    }
  }
  return nulldoc;
};

void StarbytesLSPServer::replaceFile(LSPTextDocumentIdentifier &id_ref,
                                     LSPTextDocumentItem &item_ref) {
  for (auto &txt_dcmt : files) {
    if (txt_dcmt.id == id_ref) {
      txt_dcmt.content = item_ref;
      break;
    }
  }
};

bool StarbytesLSPServer::deallocFile(LSPTextDocumentIdentifier & id_ref){
  bool returncode = false;
  for(int i = 0;i < files.size();++i){
    auto & txt_dcmt = files[i];
    if(txt_dcmt.id == id_ref){
      files.erase(files.begin()+i);
      returncode = true;
      break;
    }
  }
  return returncode;
};

}; // namespace LSP

NAMESPACE_END