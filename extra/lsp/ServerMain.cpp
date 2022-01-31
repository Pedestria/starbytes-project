#include <iostream>
#include <llvm/Support/JSON.h>
#include <llvm/Support/Regex.h>

#include "ServerMain.h"
#include "LSPProtocol.h"

namespace starbytes::lsp {


llvm::json::Object init_result
({
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
    : in(options.in),out(options.os),parser(std::make_unique<Parser>()) {

}

std::shared_ptr<InMessage> Server::getMessage() {
  std::string first;
  std::getline(in,first);
  llvm::StringRef m = first;
  int content_length;
  auto ok =llvm::to_integer(m.substr(16,m.size()-1),content_length);
  if(!ok){
    return nullptr;
  };
  // std::cout << content_length << std::endl;
  std::string buffer;
  buffer.resize(content_length);
  in.read(buffer.data(),content_length);

  auto json = llvm::json::parse(buffer);
  
  auto msg = std::make_shared<InMessage>();

  if(!json){
    return nullptr;
    // std::cout << json.takeError() << std::endl;
  }
  else {
    auto object = json->getAsObject();
    auto id = object->get("id")->getAsInteger();
    if(!id.hasValue()){
      msg->isNotification = true;
    }
    else {
        msg->info.id = id;
    }
      
    auto m = object->get("method")->getAsString();
    msg->info.method = m;
    
    return msg;
      
  };

}


void Server::sendMessage(std::shared_ptr<OutMessage> & o,MessageInfo & info){
    llvm::raw_string_ostream jout;
    llvm::json::OStream os(jout);
    os.objectBegin();
    os.attribute("id",info.id);
    os.attribute("method",info.method);
    if(o->result){
        os.attribute("result",o->result.getValue());
    }
    else {
        os.attribute("error",o->error.getValue());
    }
    os.objectEnd();
    auto j_str = jout.str();
    out << "Content-Length: " << j_str.size() << "\n\n" << j_str;
}

void Server::sendError(OutError &err,MessageInfo & info){
    auto msg = std::make_shared<OutMessage>();
    msg->error = llvm::json::Object({
        {"code",(int)err.code},
        {"message",err.message},
        {"data",err.data.getValue()}
    });
}

void Server::consumeNotification(std::shared_ptr<InMessage> & in){
    
}

void Server::buildCancellableRequest(std::shared_ptr<InMessage> & initialMessage){
    
}

void Server::replyToRequest(std::shared_ptr<InMessage> & in)
{
    if(in->info.method == Init){
        auto init_msg = std::make_shared<OutMessage>();
        init_msg->result = init_result;
        sendMessage(init_msg,in->info);
    }
    else {
        buildCancellableRequest(in);
    }
}

void Server::run() {
  while(true){
      auto message = getMessage();

      if(message->method == Exit){
        break;
      };
      
      if(!message->isNotification){
          consumeNotification(message);
      }
      else {
          replyToRequest(message);
      }

  }
}


}
