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

using namespace JSON;

void parse_document_uri(LSPDocumentURI & uri_obj,JSONString * str){
  if(str->type == JSON::JSONNodeType::String){
    uri_obj = str->value;
  }
  else{
    // TODO Parse Error!
  }
};

void parse_doc_position(LSPPosition & pos,JSONObject * obj){

};

void parse_text_document_id(LSPTextDocumentIdentifier & txt_id,JSONObject * obj){
  if(obj->type == JSON::JSONNodeType::Object){
    auto & json_str = obj->entries[0];
    parse_document_uri(txt_id.uri,(JSONString *)json_str);
  }
};

void parse_text_document_pos_params(LSPTextDocumentPositionParams & params_obj,JSONObject *& json_object_params){
  for(auto & entry : json_object_params->entries){
    if(entry.first == "textDocument"){
      LSPTextDocumentIdentifier &id = params_obj.text_document;
      parse_text_document_id(id,(JSONObject *)entry.second);
    }
    else if(entry.first == "position"){
      LSPPosition & pos = params_obj.position;
      parse_doc_position(pos,(JSONObject *)entry.second);
      
    }
  }
};

void parse_text_document_open_params(LSPDidOpenTextDocumentParams & params_obj,JSONObject *& json_object_params){
  LSPTextDocumentItem & doc_item = params_obj.text_document;
  auto & json_arg = json_object_params->entries[0];
  if(json_arg->type == JSON::JSONNodeType::Object){
    for(auto & ent : static_cast<JSONObject *>(json_arg)->entries){
      if(ent.first == "uri"){
        LSPDocumentURI doc_uri;
        parse_document_uri(doc_uri,(JSONString *)ent.second);
        doc_item.uri = doc_uri;
      }
      else if(ent.first == "languageId"){
        if(ent.second->type == JSON::JSONNodeType::String){
            doc_item.language_id = ((JSONString *)ent.second)->value;
        }
      }
      else if(ent.first == "version"){
        if(ent.second->type == JSON::JSONNodeType::Number){
          doc_item.version = ((JSONNumber *)ent.second)->value;
        }
      } 
      else if(ent.first == "text"){
        if(ent.second->type == JSON::JSONNodeType::String){
          doc_item.text = ((JSONString *)ent.second)->value;
        }
      }
    }
  };
};


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
    LSPServerRequest *RQ = static_cast<LSPServerRequest *>(INITAL);
    if (RQ->method == INITIALIZE) {
      json_transit.sendIntializeMessage(RQ->id);
      mainLoop();
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
    if (txt_dcmt.content.uri == id_ref.uri) {
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
    if (txt_dcmt.content.uri == id_ref.uri) {
      return txt_dcmt.content;
    }
  }
  return nulldoc;
};

void StarbytesLSPServer::replaceFile(LSPTextDocumentIdentifier &id_ref,
                                     LSPTextDocumentItem &item_ref) {
  for (auto &txt_dcmt : files) {
    if (txt_dcmt.content.uri == id_ref.uri) {
      txt_dcmt.content = item_ref;
      break;
    }
  }
};

bool StarbytesLSPServer::deallocFile(LSPTextDocumentIdentifier & id_ref){
  bool returncode = false;
  for(int i = 0;i < files.size();++i){
    auto & txt_dcmt = files[i];
    if(txt_dcmt.content.uri == id_ref.uri){
      files.erase(files.begin()+i);
      returncode = true;
      break;
    }
  }
  return returncode;
};

void StarbytesLSPServer::mainLoop(){
  while(true){
    getMessageFromStdin();
    LSPServerMessage * latest = queue.getLatest();
    if(latest->type == Request){
      LSPServerRequest * latest_req = static_cast<LSPServerRequest *>(latest);
      if(latest_req->method == DOC_COMPLETION){

      }
      else if(latest_req->method == DOC_HOVER){
        
      }
    }
    //No Reply For Notes!!!
    else if(latest->type == Notification){
      LSPServerNotification * latest_note = static_cast<LSPServerNotification *>(latest);
      if(latest_note->method == OPEN_TEXT_DOCUMENT){
        LSPDidOpenTextDocumentParams params;
        if(latest_note->params_object.has_value()){
          parse_text_document_open_params(params,latest_note->params_object.value());
        }
        else{
          //  TODO Parse Error!
        }
        StarbytesLSPServerTextDocument doc (params.text_document);
        addFile(doc);
      }
    }
  };
};

}; // namespace LSP

NAMESPACE_END