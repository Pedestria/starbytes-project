#include "JSONOutput.h"
#include "LSPProtocol.h"
#include <cctype>
#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>
#include <map>
#include <array>
#include <vector>

using namespace std;

namespace Starbytes {
namespace LSP {

namespace JSON {
    enum class JSONNodeType:int {
        Object,Array,String,Number,Null,Boolean
    };
    struct JSONNode {
        JSONNodeType type;
    };

    struct JSONNull : JSONNode {};

    struct JSONBoolean : JSONNode {
        bool value;
    };
    struct JSONNumber : JSONNode {
        string value;
    };

    struct JSONString : JSONNode {
        string value;
    };
    struct JSONObject : JSONNode {
        map<string,JSONNode*> entries;
    };
    struct JSONArray : JSONNode {
        vector<JSONNode*> objects;
    };

    enum class JSONTokType:int {
        CloseBrace,CloseBracket,OpenBrace,OpenBracket,Numeric,String,Colon,Comma,Boolean,Null
    };

    string StarbytesLSPJsonError(string message){
        return "JSONParser Error: \n"+message;
    }

    bool isNumber(string subject){
        for(auto c : subject){
            if(isalpha(c)){
                return false;
            } else if(isnumber(c)){
                continue;
            } else {
                return false;
            }
        }
        return true;
    }

    bool isBoolean(string subject){
        return subject == "true" || subject == "false";
    }

    bool isNull (string subject){
        return subject == "null";
    }
    struct JSONToken {
        string content;
        JSONTokType type;
    };

    class JSONGenerator{
        private:
            ostringstream result;
            string *ptr;
            JSONObject *tree;
            void generateJSONNull(JSONNull *node){
                result << "null";
                delete node;
            }
            void generateJSONNumber(JSONNumber *node){
                result << node->value;
                delete node;
            }
            void generateJSONBoolean(JSONBoolean * node){
                if(node->value){
                    result << "true";
                }else{
                    result << "false";
                }
                delete node;
            }
            void generateJSONString(JSONString * node){
                result << node->value;
                delete node;
            }
            void generateJSONArray(JSONArray * node){
                result << "[";
                int count = 0;
                for(auto obj : node->objects){
                    if(count > 0){
                        result << ",";
                    }
                    switch (obj->type) {
                    case JSONNodeType::Object:
                        generateJSONObject((JSONObject *)obj);
                        break;
                    case JSONNodeType::Array:
                        generateJSONArray((JSONArray *)obj);
                        break;
                    case JSONNodeType::String:
                        generateJSONString((JSONString *)obj);
                        break;
                    case JSONNodeType::Boolean:
                        generateJSONBoolean((JSONBoolean *)obj);
                        break;
                    case JSONNodeType::Number:
                        generateJSONNumber((JSONNumber *)obj);
                        break;
                    case JSONNodeType::Null:
                        generateJSONNull((JSONNull *)obj);
                        break;
                    default:
                        break;
                    }
                    ++count;
                }
                result << "]";
                delete node;
            }
            void generateJSONObject(JSONObject *object){
                result << "{";
                int count = 0;
                for (auto entry : object->entries){
                    if(count > 0){
                        result << ",";
                    }
                    result << entry.first;
                    result << ":";
                    switch (entry.second->type) {
                        case JSONNodeType::Object:
                            generateJSONObject((JSONObject *)entry.second);
                            break;
                        case JSONNodeType::Array:
                            generateJSONArray((JSONArray *) entry.second);
                            break;
                        case JSONNodeType::String:
                            generateJSONString((JSONString *)entry.second);
                            break;
                        case JSONNodeType::Boolean:
                            generateJSONBoolean((JSONBoolean *)entry.second);
                            break;
                        case JSONNodeType::Number:
                            generateJSONNumber((JSONNumber *)entry.second);
                            break;
                        case JSONNodeType::Null:
                            generateJSONNull((JSONNull *)entry.second);
                            break;
                        default:
                            break;
                    }
                    ++count;                
                }
                result << "}";
                delete object;
            }
        public:
            JSONGenerator(JSONObject *_tree):tree(_tree){}
            void generate(string *_resultc){
                generateJSONObject(tree);
                _resultc = new string();
                *_resultc = result.str();
                ptr = _resultc;
            };
            void freeString(){
                delete ptr;
            }
    };

    class JSONParser {
        private:
            int currentIndex;
            char * bufptr;
            char * start;
            char TokenBuffer[200];
            string code;
            vector<JSONToken> tokens;
            char nextChar(){
                return code[++currentIndex];
            }
            char aheadChar(){
                return code[currentIndex + 1];
            }
            void clearCache(JSONTokType type = JSONTokType::String){
                auto length = bufptr-start;
                if(bufptr == TokenBuffer){
                    return;
                }
                else{
                    string result = string(TokenBuffer,length);
                    if(isNumber(result)){
                        type = JSONTokType::Numeric;
                    } else if(isBoolean(result)){
                        type = JSONTokType::Boolean;
                    } else if(isNull(result)){
                        type = JSONTokType::Null;
                    }
                    JSONToken tok;
                    tok.content = result;
                    tok.type = type;
                    tokens.push_back(tok);
                }
                bufptr = TokenBuffer;

            }
            void tokenize(){
                char c = code[currentIndex];
                bufptr = TokenBuffer;
                start = bufptr;
                while(true){
                    if(c == '{'){
                        clearCache();
                        *bufptr = c;
                        ++bufptr;
                        clearCache(JSONTokType::OpenBrace);
                    } else if(c == '}'){
                        clearCache();
                        *bufptr = c;
                        ++bufptr;
                        clearCache(JSONTokType::CloseBrace);
                    } else if(c == '['){
                        clearCache();
                        *bufptr = c;
                        ++bufptr;
                        clearCache(JSONTokType::OpenBracket);
                    } else if(c == ']'){
                        clearCache();
                        *bufptr = c;
                        ++bufptr;
                        clearCache(JSONTokType::CloseBracket);
                    
                    } else if(c == ':'){
                        clearCache();
                        *bufptr = c;
                        ++bufptr;
                        clearCache(JSONTokType::Colon);
                    }
                    else if(c == ','){
                        clearCache();
                        *bufptr = c;
                        ++bufptr;
                        clearCache(JSONTokType::Comma);
                    }
                    else if(isalpha(c)){
                        *bufptr = c;
                        ++bufptr;
                    }
                    else if(isnumber(c)){
                        *bufptr = c;
                        ++bufptr;
                    }
                    else if(c == '\0'){
                        clearCache();
                        break;
                    } else if(c == '\"'){
                        *bufptr = c;
                        ++bufptr;
                        c = nextChar();
                        while(true){
                            if(c == '\"'){
                                *bufptr = c;
                                ++bufptr;
                                clearCache();
                                break;
                            }else {
                                *bufptr = c;
                                ++bufptr;
                            }
                            c = nextChar();
                        }
                    }
                    c = nextChar();
                }
            
            }
            int tokenIndex;
            JSONObject *parseResult;
            JSONToken *nextToken(){
                return &tokens[++tokenIndex];
            }
            JSONToken *currentToken(){
                return &tokens[tokenIndex];
            }
            JSONToken *aheadToken(){
                return &tokens[tokenIndex + 1];
            }
            void parseJSONNumber(JSONNumber *node){
                node->type = JSONNodeType::Number;
                if(currentToken()->type == JSONTokType::Numeric){
                    node->value = currentToken()->content;
                }
            }
            void parserJSONNull(JSONNull *node){
                node->type = JSONNodeType::Null;
                if(currentToken()->type == JSONTokType::Null){
                    //No Error!
                }
            }
            void parseJSONBoolean (JSONBoolean * node){
                node->type = JSONNodeType::Boolean;
                if(currentToken()->type == JSONTokType::Boolean){
                    if(currentToken()->content == "true"){
                        node->value = true;
                    } else if(currentToken()->content == "false"){
                        node->value = false;
                    }
                }
            }
            void parseJSONString(JSONString *node){
                node->type = JSONNodeType::String;
                if(currentToken()->type == JSONTokType::String){
                    node->value = currentToken()->content;
                }
            }
            void parseJSONArray(JSONArray * Node){
                Node->type = JSONNodeType::Array;
                JSONToken *tok = currentToken();
                if(tok->type == JSONTokType::OpenBracket){
                    tok = nextToken();
                    JSONTokType *TYPE = &tok->type;
                    JSONNode *node;
                    if(*TYPE == JSONTokType::OpenBrace){
                        node = new JSONObject();
                        parseJSONObject((JSONObject *)node);
                    } else if(*TYPE == JSONTokType::OpenBracket){
                        node = new JSONArray();
                        parseJSONArray((JSONArray *)node);
                    } else if(*TYPE == JSONTokType::String){
                        node = new JSONString();
                        parseJSONString((JSONString *)node);
                    } else if(*TYPE == JSONTokType::Numeric){
                        node = new JSONNumber();
                        parseJSONNumber((JSONNumber *)node);
                    }else if(*TYPE == JSONTokType::Boolean){
                        node = new JSONBoolean();
                        parseJSONBoolean((JSONBoolean *)node);
                    } else if(*TYPE == JSONTokType::Null){
                        node = new JSONNull();
                        parserJSONNull((JSONNull *) node);
                    }
                    Node->objects.push_back(node);
                    tok = nextToken();
                    while(true){
                        if(tok->type == JSONTokType::Comma){
                            tok = nextToken();
                            JSONTokType *TYPE = &tok->type;
                            JSONNode *node;
                            if(*TYPE == JSONTokType::OpenBrace){
                                node = new JSONObject();
                                parseJSONObject((JSONObject *)node);
                            } else if(*TYPE == JSONTokType::OpenBracket){
                                node = new JSONArray();
                                parseJSONArray((JSONArray *)node);
                            } else if(*TYPE == JSONTokType::String){
                                node = new JSONString();
                                parseJSONString((JSONString *)node);
                            } else if(*TYPE == JSONTokType::Numeric){
                                node = new JSONNumber();
                                parseJSONNumber((JSONNumber *)node);
                            }else if(*TYPE == JSONTokType::Boolean){
                                    node = new JSONBoolean();
                                    parseJSONBoolean((JSONBoolean *)node);
                            } else if(*TYPE == JSONTokType::Null){
                                node = new JSONNull();
                                parserJSONNull((JSONNull *) node);
                            }
                            Node->objects.push_back(node);
                        }
                        else if(tok->type == JSONTokType::CloseBracket){
                            break;
                        }
                    }
                }
            }
            void parseJSONObject(JSONObject *obj){
                obj->type = JSONNodeType::Object;
                JSONToken *tok = currentToken();
                if(tok->type == JSONTokType::OpenBrace){
                    while(true){
                        tok = nextToken();
                        if(tok->type == JSONTokType::String){
                            string key = tok->content;
                            tok = nextToken();
                            if(tok->type == JSONTokType::Colon){
                                tok = nextToken();
                                JSONTokType *TYPE = &tok->type;
                                JSONNode *node;
                                if(*TYPE == JSONTokType::OpenBrace){
                                    node = new JSONObject();
                                    parseJSONObject((JSONObject *)node);
                                } else if(*TYPE == JSONTokType::OpenBracket){
                                    node = new JSONArray();
                                    parseJSONArray((JSONArray *)node);
                                } else if(*TYPE == JSONTokType::String){
                                    node = new JSONString();
                                    parseJSONString((JSONString *)node);
                                } else if(*TYPE == JSONTokType::Numeric){
                                    node = new JSONNumber();
                                    parseJSONNumber((JSONNumber *)node);
                                } else if(*TYPE == JSONTokType::Boolean){
                                    node = new JSONBoolean();
                                    parseJSONBoolean((JSONBoolean *)node);
                                } else if(*TYPE == JSONTokType::Null){
                                    node = new JSONNull();
                                    parserJSONNull((JSONNull *) node);
                                }
                                obj->entries.insert(pair<string,JSONNode *>(key,node));
                            }
                            else {
                                StarbytesLSPJsonError("Expected Colon");
                            }
                        }
                        else if(tok->type == JSONTokType::Comma){
                            continue;
                        }
                        else if(tok->type == JSONTokType::CloseBrace){
                            break;
                        }
                    }
                }
                else{
                    throw StarbytesLSPJsonError("Expected Open Brace!");
                }
            }
        public:
            JSONParser(string code) : code(code),currentIndex(code.find("{")){};
            void parse(JSONObject *loc){
                tokenize();
                // auto toks = tokens;
                // for(auto tok : toks){
                //     cout << "{Content:" << tok.content << "- Type:" << int(tok.type) << "}" << "\n";
                // }
                parseResult = new JSONObject();
                try {
                    tokenIndex = 0;
                    parseJSONObject(parseResult);
                    loc = parseResult;
                    // for(auto entries : parseResult->entries){
                    //     cout << "{Key:" << entries.first << ",NodeType:" << int(entries.second->type) << "}";
                    // }
                }
                catch (string message){
                    cout << message << "\n";
                }

            }
    };
    //Translates From JSON to LSPServerObject and Back.
    class JSONTranslator{
        private:
            void translateJSONObject(JSONObject *node,LSPServerObject *cntr){
                map<string,JSONNode *>::iterator it;
                // it = node->entries.find("");
                it = node->entries.find("processId");
                if(it != node->entries.end()){
                    LSPIntializeParams *Node = new LSPIntializeParams();
                    Node->type = IntializeParams;
                    for(auto ent : node->entries){
                        if(ent.first == "processId"){
                            Node->hasProcessId = true;
                            Node->process_id = ((JSONString *)ent.second)->value;
                        } else if(ent.first == "workDoneToken"){
                            Node->work_done_token = ((JSONString *)ent.second)->value;
                        }
                    }
                }

            };
            void translateJSONArray(JSONArray *node,std::vector<LSPServerObject *> *resloc){
                for(auto NODE : node->objects){
                    if(NODE->type == JSONNodeType::Object){
                        LSPServerObject *cntr;
                        translateJSONObject((JSONObject *)NODE,cntr);
                        resloc->push_back(cntr);
                    }
                }
            };
        public:
            JSONTranslator(){};
            void translateFromJSON(LSPServerMessage *msgcntr,string &message){
                JSONObject *messagec;
                JSONParser(message).parse(messagec);
                map<string,JSONNode*>::iterator it = messagec->entries.find("id");
                if(it != messagec->entries.end()){
                    LSPServerRequest *req = new LSPServerRequest();
                    msgcntr = req;
                    for (auto entry : messagec->entries){
                        if(entry.first == "id"){
                            req->id = ((JSONNumber *)entry.second)->value;
                        } else if(entry.first == "method"){
                            req->method = ((JSONString *)entry.second)->value;
                        } else if(entry.first == "params"){
                            JSONNodeType &Type = entry.second->type;
                            if(Type == JSONNodeType::Object){
                                LSPServerObject *cntr;
                                translateJSONObject((JSONObject *)entry.second,cntr);
                                req->params_object = cntr;
                            } else if(Type == JSONNodeType::Array){
                                translateJSONArray((JSONArray *)entry.second,&req->params_array);
                            }
                        }
                    }
                }
                else {

                }
            };
            void translateToJSON(LSPServerReply *msgtosnd,string *msgout){

            };
    };


};


void Messenger::read(LSPServerMessage *msgcntr){
    using namespace JSON;
    std::string message;
    cin >> message;
    if(!message.empty()){
        
    }
}

void Messenger::reply(LSPServerReply *result){
    using namespace JSON;
    JSONObject *obj;
    JSONGenerator gen = JSONGenerator(obj);
    string *r;
    gen.generate(r);
    cout << "Content-Length:" << r->length() << "\r\nContent-Type:utf-8\r\n\r\n" << *r;
    gen.freeString();
}


};
};


