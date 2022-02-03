#include <chrono>
#include <exception>
#include <iostream>
#include <llvm/Support/JSON.h>

#include "ServerMain.h"
#include "LSPProtocol.h"
#include <future>
#include <mutex>

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
  });



Server::Server(starbytes::lsp::ServerOptions &options)
    : in(options.in),out(options.os) {

}

std::shared_ptr<InMessage> Server::getMessage() {
  std::lock_guard<std::mutex> lk(mutex);

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
        msg->info.id = (int)id.getValue();
    }
      
    auto m = object->get("method")->getAsString();
    msg->info.method = m.getValue();
    
    return msg;
      
  };

}


void Server::sendMessage(std::shared_ptr<OutMessage> & o,MessageInfo & info){
    std::lock_guard<std::mutex> lk(mutex);
    
    std::string j_str;
    llvm::raw_string_ostream jout(j_str);
    llvm::json::OStream os(jout);
    os.objectBegin();
    os.attribute("id",info.id);
    os.attribute("method",info.method);
    if(o->result){
        os.attribute("result",o->result.getValue());
    }
    else {
        os.attribute("error",llvm::json::Value(std::move(o->error.getValue())));
    }
    os.objectEnd();
    jout.flush();
    out << "Content-Length: " << j_str.size() << "\n\n" << j_str << std::flush;

}

void Server::sendError(OutError &err,std::shared_ptr<OutMessage> & out){
    out->error = llvm::json::Object({
        {"code",(int)err.code},
    });

    if(!err.message.empty()){
      out->error->insert({"message",err.message});
    }

    if(err.data){
      out->error->insert({"data",err.data.getValue()});
    }

}

void Server::consumeNotification(std::shared_ptr<InMessage> & in){
    
}

void Server::buildCancellableRequest(std::shared_ptr<InMessage> & initialMessage){
    /// Start Async Task!
   
    std::promise<std::shared_ptr<OutMessage>> out;
    auto msgInfo = initialMessage->info;
    auto fut = out.get_future();
    bool cancel = false;
    auto & t = threadsInFlight.emplace_back([&](std::shared_ptr<InMessage> initialMessage,std::promise<std::shared_ptr<OutMessage>> out){
       auto m = std::make_shared<OutMessage>();
       {
          llvm::StringRef method = initialMessage->info.method;
          if(method == completion){
          
          }
          else if(method == hover){

            std::string documentId,pTk,wdTk;
            SrcLoc loc{};

            parseTextDocumentPositionParams(initialMessage->params,loc, documentId);
            parsePartialResultToken(initialMessage->params,pTk);
            parseWorkDoneToken(initialMessage->params,wdTk);

            if(!workspaceManager.documentIsOpen(documentId)){
              OutError err {OutErrorCode::INVALID_PARAMS};
              sendError(err,m);
            }
            else {
              auto sym = workspaceManager.getSymbolInfoInDocument(documentId,loc);
            }
          }
          else if(method == definition){



          }
       }
       if(!cancel){
         out.set_value_at_thread_exit(m);
       }
    },std::move(initialMessage),std::move(out));

    bool cancelThisRequest = false;
    while(!cancelThisRequest){
      auto status = fut.wait_for(std::chrono::milliseconds(1));
      if(status == std::future_status::ready){
        auto m = fut.get();
        sendMessage(m,msgInfo);
        break;
      }
      auto next = getMessage();
      if(next->info.method == CancelRequest && next->info.id == msgInfo.id){
         t.detach();
         cancelThisRequest = true;
         cancel = true;
      }
      else {
        threadsInFlight.emplace_back([&](std::shared_ptr<InMessage> msg){
            processMessage(msg);
        },std::move(next));
        
      }
    }
    
}

void Server::replyToRequest(std::shared_ptr<InMessage> & in)
{
    if(in->info.method == Init){
        auto init_msg = std::make_shared<OutMessage>();
        init_msg->result = llvm::json::Value(std::move(init_result));
        sendMessage(init_msg,in->info);
    }
    else {
        buildCancellableRequest(in);
    }
}

void Server::processMessage(std::shared_ptr<InMessage> & message){
  if(message->info.method == Exit){
       std::lock_guard<std::mutex> lk(mutex);
       serverOn = false;
    };
    
    if(!message->isNotification){
        consumeNotification(message);
    }
    else {
        replyToRequest(message);
    }
}

void Server::cycle(){
   auto message = getMessage();
   processMessage(message);
}

void Server::run() {
  while(serverOn){
    cycle();
    std::lock_guard<std::mutex> lk(mutex);
  };
}


}
