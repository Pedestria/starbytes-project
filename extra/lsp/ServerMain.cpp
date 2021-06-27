#include <iostream>
#include <llvm/Support/JSON.h>
#include <llvm/Support/Regex.h>

#include "ServerMain.h"
#include "LSPProtocol.h"

namespace starbytes::lsp {


llvm::json::Object json 
({
  {"id",1},
  {"method",Init},
  {"result",{
    {"capabilities",{
      {"hoverProvider",true}
    }},
    {"serverInfo",{
      {"name","Starbytes LSP"},
      {"version","0.1"}
    }}
  }}  
});

Server::Server(starbytes::lsp::ServerOptions &options)
    : in(options.in),out(options.os) {

}

InMessage *Server::getMessage() {
  std::string first;
  std::getline(in,first);
  llvm::StringRef m = first;
  int content_length;
  auto ok =llvm::to_integer(m.substr(16),content_length);
  if(!ok){
    return nullptr;
  };
  // std::cout << content_length << std::endl;
  std::string buffer;
  buffer.resize(content_length);
  in.read(buffer.data(),content_length);

  auto json = llvm::json::parse(buffer);
  
  auto msg = new InMessage();

  if(!json){
    return nullptr;
    // std::cout << json.takeError() << std::endl;
  }
  else {
    auto object = json->getAsObject();
    auto id = object->get("id")->getAsString();
    if(!id.hasValue()){
      msg->isNotification = true;
    };
    auto m = object->get("method")->getAsString();
  };

}

void Server::run() {
  while(true){
      auto message = getMessage();

      if(message->method == Init) {
        
      }
      else if(message->method == Exit){
        break;
      };



  }
}


}
