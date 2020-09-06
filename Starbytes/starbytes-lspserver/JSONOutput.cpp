#include "JSONOutput.h"
#include <cctype>
#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>
#include <map>
#include <array>
#include <vector>


using namespace Starbytes;
using namespace LSP;

using namespace std;

std::string ErrorToString(LSPReplyError error){
    ostringstream output;
    output << "{\n\t\t\"code\":" << int(error.code) << "\n\t\t\"message\":" << error.message;
    if(error.data.bool_data){
        output << "\n\t\t\"data\":" << error.data.bool_data;
    } else if(error.data.int_data){
        output << "\n\t\t\"data\":" << error.data.int_data;
    } else if(!error.data.str_data.empty()){
        output << "\n\t\t\"data\":\"" << error.data.str_data << "\"";
    }
    output << "\n\t}";
    return output.str();
}


void Messenger::reply(LSPServerReply result){
    ostringstream reply;
    reply << "{\n\t\"jsonrpc\":\"2.0\",";
    reply << "\n\t\"id\":" << result.id;
    if(result.bool_result){
        reply << "\n\t\"result\":" << result.bool_result;
    } else if(result.int_result){
        reply << "\n\t\"result\":" <<  result.int_result;
    } else if(!result.str_result.empty()){
        reply << "\n\t\"result\":\"" <<  result.str_result << "\"";
    } else {
        reply << "\n\t\"error\":" << ErrorToString(result.error);
    }
    reply << "\n}\n";
    std::string out = "Content-Length:"+to_string(reply.str().length())+"\r\nContent-Type:utf-8\r\n\r\n"+reply.str();
    // write(p[1],out.c_str(),out.length());
}

enum class JSONNodeType:int {
    Object,Array,String,Number
};
struct JSONNode {
    JSONNodeType type;
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
    CloseBrace,CloseBracket,OpenBrace,OpenBracket,Numeric,String,Colon,Comma
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

struct JSONToken {
    string content;
    JSONTokType type;
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
        void parse(LSPServerMessage *message){
            tokenize();
            // auto toks = tokens;
            // for(auto tok : toks){
            //     cout << "{Content:" << tok.content << "- Type:" << int(tok.type) << "}" << "\n";
            // }
            parseResult = new JSONObject();
            try {
                tokenIndex = 0;
                parseJSONObject(parseResult);
                
                 for(auto entries : parseResult->entries){
                    cout << "{Key:" << entries.first << ",NodeType:" << int(entries.second->type) << "}";
                }
            }
            catch (string message){
                cout << message << "\n";
            }

        }
};

void Messenger::read(){
    std::string message = "Content-Length:1231\r\nContent-Type:utf-8\r\n\r\n{\"jsonrpc\": \"2.0\",\"id\": 1,\"method\": \"textDocument/didOpen\",\"params\": {}}";
    // cin >> message;
    // if(!message.empty()){
        LSPServerMessage *messagee;
        JSONParser(message).parse(messagee);
    // }
}