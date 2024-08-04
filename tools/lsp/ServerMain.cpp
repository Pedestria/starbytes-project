#include <chrono>
#include <exception>
#include <iostream>
#include <rapidjson/document.h>

#include "ServerMain.h"
#include "LSPProtocol.h"
#include "rapidjson/rapidjson.h"
#include <future>
#include <mutex>
#include <type_traits>

namespace starbytes::lsp {


std::string output = "{{\"capabilities\",{{\"hoverProvider\",true}}},{\"serverInfo\",{{\"name\",\"Starbytes LSP\"},{\"version\",\"0.1\"}}}}";



Server::Server(starbytes::lsp::ServerOptions &options)
    : in(options.in),out(options.os),serverContext(new ThreadedServerContext()),messageIO(serverContext) {

}

std::shared_ptr<InMessage> Server::getMessage() {
  std::lock_guard<std::mutex> lk(mutex);

  std::string first;
  std::getline(in,first);
  string_ref m = first;
  int content_length;
  auto ok =m.substr_ref(16,m.size()-1).size();
  if(!ok){
    return nullptr;
  };
  content_length = ok;
  // std::cout << content_length << std::endl;
  std::string buffer;
  buffer.resize(content_length);
  in.read(buffer.data(),content_length);

 rapidjson::Document json;
 json.Parse(buffer.c_str(),content_length);


  
  auto msg = std::make_shared<InMessage>();
  
  if(json.HasParseError()){
    return nullptr;
    // std::cout << json.takeError() << std::endl;
  }
  else {
    auto object = json.GetObject();
    auto id = object.HasMember("id");
    if(!id){
      msg->isNotification = true;
    } 
    else {
        msg->info.id = object["id"].GetInt();
    }
      
    auto m = object["method"].GetString();
    auto m_len = object["method"].GetStringLength();
    msg->info.method = std::string(m,m_len);
    
    return msg;
      
  };

}


void Server::sendMessage(std::shared_ptr<OutMessage> & o,MessageInfo & info){
    std::lock_guard<std::mutex> lk(mutex);
    
    rapidjson::StringBuffer j_str;
    rapidjson::Writer<rapidjson::StringBuffer> os(j_str);
    rapidjson::Document & d = serverContext->outputJSON;
    d.StartObject();
    d.AddMember("id",info.id.value(),d.GetAllocator());
    d.AddMember("method",rapidjson::StringRef(info.method.c_str()),d.GetAllocator());
    if(o->result){
        d.AddMember("result",*o->result.value(),d.GetAllocator());
    }
    else {
        d.AddMember("error",*o->error.value(),d.GetAllocator());
    }
    d.EndObject(3);
    d.Accept(os);

    out << "Content-Length: " << j_str.GetSize() << "\n\n" << j_str.GetString() << std::flush;
    serverContext->resetOutputJson();
}

void Server::sendError(OutError &err,std::shared_ptr<OutMessage> & out){
    out->error = new rapidjson::Value(rapidjson::kObjectType);

     rapidjson::StringBuffer j_str;
    rapidjson::Writer<rapidjson::StringBuffer> os(j_str);
    rapidjson::Document & d = serverContext->outputJSON;
    d.StartObject();
    out->error.value()->AddMember("code",(int)err.code,serverContext->outputJSON.GetAllocator());
    unsigned mem_count = 1;
    if(!err.message.empty()){
      out->error.value()->AddMember("message",rapidjson::StringRef(err.message.c_str()),serverContext->outputJSON.GetAllocator());
      mem_count++;
    }

    if(err.data){
      out->error.value()->AddMember("data",err.data.value()->GetObject(),serverContext->outputJSON.GetAllocator());
       mem_count++;
    }

    d.EndObject(mem_count);
    d.Accept(os);

    this->out << "Content-Length: " << j_str.GetSize() << "\n\n" << j_str.GetString() << std::flush;
    serverContext->resetOutputJson();
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
          string_ref method = initialMessage->info.method;
          if(method == completion){
          
          }
          else if(method == hover){

            std::string documentId,pTk,wdTk;
            Region loc{};

            messageIO.parseTextDocumentPositionParams(*initialMessage.get()->params,loc, documentId);
            messageIO.parsePartialResultToken(initialMessage.get()->params,pTk);
            messageIO.parseWorkDoneToken(initialMessage.get()->params,wdTk);

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
        // init_msg->result = new rapidjson::Value(init_result);
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
