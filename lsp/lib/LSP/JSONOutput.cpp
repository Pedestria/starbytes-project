#include "LSP/JSONOutput.h"
#include "LSP/LSPProtocol.h"
#include "starbytes/Base/Macros.h"
#include <cctype>
#include <cstdio>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <streambuf>
#include <string>
#include <map>
#include <array>
#include <vector>

using namespace std;

STARBYTES_STD_NAMESPACE
namespace LSP {

namespace JSON {

    string StarbytesLSPJsonError(string message){
        return "\u001b[31mJSONParser Error: \n"+message + "\u001b[0m";
    }
    
    bool isNumber(string subject){
        for(auto c : subject){
            if(isalpha(c)){
                return false;
            } else if(isdigit(c)){
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

    JSONObject * createJSONObject(initializer_list<pair<string,JSONNode *>> ilist){
        JSONObject *obj = new JSONObject();
        obj->type = JSONNodeType::Object;
        for(auto ent : ilist){
            ent.first = "\""+ent.first+"\"";
            obj->entries.insert(ent);
        }
        return obj;
    }

    JSONArray * createJSONArray(initializer_list<JSONNode *> ilist){
        JSONArray *array = new JSONArray();
        array->type = JSONNodeType::Array;
        array->objects = vector(ilist);
        return array;
    }

    JSONString * createJSONString(bool wrapQuotes,string value){
        JSONString *str = new JSONString();
        str->type = JSONNodeType::String;
        if(wrapQuotes){
            value = "\""+value+"\"";
        }
        str->value = value;
        return str;
    }

    JSONNumber * createJSONNumber(string value){
        JSONNumber *num = new JSONNumber();
        num->type = JSONNodeType::Number;
        num->value = value;
        return num;
    };

    JSONBoolean * createJSONBoolean(bool value){
        JSONBoolean *bol =  new JSONBoolean();
        bol->type = JSONNodeType::Boolean;
        bol->value = value;
        return bol;
    }

    JSONNull * createJSONNull (string value){
        JSONNull *nil = new JSONNull();
        nil->type = JSONNodeType::Null;
        return nil;
    }

    JSONArray * convertStringArray(std::vector<std::string> *c){
        JSONArray * ar = createJSONArray({});
        for(auto str : *c){
            ar->objects.push_back(createJSONString(true,str));
        }
        return ar;
    }

    string wrapQuotes(string &subject){
        return "\""+subject+"\"";
    }
    string unwrapQuotes(string &subject){
        subject.erase(subject.begin());
        subject.erase(subject.end());
        return subject;
    }



    class JSONGenerator{
        private:
            ostringstream result;
            bool pretty;
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
            void generateJSONArray(JSONArray * node,int level){
                result << "[";
                if(pretty){
                    result << "\n";
                }
                int count = 0;
                for(auto obj : node->objects){
                    if(count > 0){
                        result << ",";
                        if(pretty){
                            result << "\n";
                        }
                    }
                    if(pretty){
                        for(int i = 0;i < level;++i){
                            result << "\t";
                        }
                    }
                    switch (obj->type) {
                    case JSONNodeType::Object:
                        generateJSONObject((JSONObject *)obj,level+1);
                        break;
                    case JSONNodeType::Array:
                        generateJSONArray((JSONArray *)obj,level+1);
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
                if(pretty){
                    result << "\n";
                    for(int i = 1;i < level;++i){
                            result << "\t";
                    }
                }
                result << "]";
                delete node;
            }
            void generateJSONObject(JSONObject *object,int level){
                result << "{";
                if(pretty){
                    result << "\n";
                }
                int count = 0;
                for (auto entry : object->entries){
                    if(count > 0){
                        result << ",";
                        if(pretty){
                            result << "\n";
                        }
                    }
                    if(pretty){
                        for(int i = 0;i < level;++i){
                            result << "\t";
                        }
                    }
                    result << entry.first;
                    result << ":";
                    switch (entry.second->type) {
                        case JSONNodeType::Object:
                            generateJSONObject((JSONObject *)entry.second,level+1);
                            break;
                        case JSONNodeType::Array:
                            generateJSONArray((JSONArray *) entry.second,level+1);
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
                if(pretty){
                    result << "\n";
                    for(int i = 1;i < level;++i){
                            result << "\t";
                    }
                }
                result << "}";
                delete object;
            }
        public:
            JSONGenerator(JSONObject *_tree):tree(_tree){}
            string * generate(bool _pretty){
                pretty = _pretty;
                generateJSONObject(tree,1);
                string * _resultc = new string();
                *_resultc = result.str();
                ptr = _resultc;
                return _resultc;
            };
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
            char behindChar(){
                return code[currentIndex - 1];
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
                    else if(isdigit(c)){
                        *bufptr = c;
                        ++bufptr;
                    }
                    else if(c == '\0'){
                        clearCache();
                        break;
                    } else if(c == '\"'){
                        c = nextChar();
                        while(true){
                            if(c == '\"'){
                                if(behindChar() == '\"'){
                                    *bufptr = '\"';
                                    ++bufptr;
                                    *bufptr = '\"';
                                    ++bufptr;
                                    clearCache(JSONTokType::NullString);
                                    break;
                                }
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
                            tok = nextToken();
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
                                obj->entries.insert(JSONObjEntry(key,node));
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
            JSONParser(string& code,int index) : code(code),currentIndex(index){};
            JSONObject * parse(){
                tokenize();
                // auto toks = tokens;
                // for(auto tok : toks){
                //     cout << "{Content:" << tok.content << "- Type:" << int(tok.type) << "}" << "\n";
                // }
                parseResult = new JSONObject();
                try {
                    tokenIndex = 0;
                    parseJSONObject(parseResult);
                    
                    // for(auto entries : parseResult->entries){
                    //     cout << "{Key:" << entries.first << ",NodeType:" << int(entries.second->type) << "}";
                    // }
                    return parseResult;
                }
                catch (string message){
                    cout << message << "\n";
                    exit(1);
                    return nullptr;
                }

            }
    };
    //Translates From JSON to LSPServerObject and Back.
    class JSONTranslator{
        public:
            JSONTranslator(){};
            LSPServerMessage * translateFromJSON(string &message){
                LSPServerMessage *msgcntr;
                JSONObject *messagec = JSONParser(message,0).parse();
                // map<string,JSONNode*>::iterator it = messagec->entries.find("id");
                // if(it != messagec->entries.end()){
                LSPServerRequest *req = new LSPServerRequest();
                req->type = Request;
                msgcntr = req;
                for (auto entry : messagec->entries){
                    if(entry.first == "id"){
                        req->id = ((JSONNumber *)entry.second)->value;
                    } else if(entry.first == "method"){
                        req->method = ((JSONString *)entry.second)->value;
                    } else if(entry.first == "params"){
                        if(entry.second->type == JSONNodeType::Array){
                            req->params_array = ((JSONArray *)entry.second);
                        }
                        else if(entry.second->type == JSONNodeType::Object){
                            req->params_object = ((JSONObject *)entry.second);
                        }   
                    }
                }
                // }
                // else {

                // }
                return msgcntr;
            };
    };


};


LSPServerMessage * Messenger::read(){
    using namespace JSON;
    std::string header;
    std::string message;
    while(true){
        getline(cin,header);
        size_t found = header.find("Content-Length:");
        if(found == string::npos){
            cout << "Header Expected!" << std::flush;
            continue;
        }
        int num = stoi(header.substr(found+16));
        std::string ch;
        getline(cin,ch);
        std::string buffer;
        int idx = 0;
        while(true){
            if(idx == num){
                break;
            }
            else{
                getline(cin,buffer);
                idx += buffer.size();
                message.append(buffer);
                buffer.clear();
                std::cout << "SIZE OF CONTENT:" << idx << std::endl;
            }
            
        }

        if(!message.empty()){
            JSONTranslator t;
            LSPServerMessage *c;
            c = t.translateFromJSON(message);
            return c;
        }
    }
    
    return nullptr;
}

void Messenger::reply(LSPServerReply *result){
    using namespace JSON;
    JSONTranslator t;
    string *r;
    // t.compileToJSON(result,false);
    cout << "Content-Length: " << r->length() << "\r\n\r\n" << *r;
}

void Messenger::sendIntializeMessage(string & id){
    using namespace JSON;

    JSONObject *json_init = createJSONObject({
        JSONObjEntry ("jsonrpc",createJSONString(true,"2.0")),
        JSONObjEntry ("id",createJSONString(true,id)),
        JSONObjEntry ("result",createJSONObject({
            JSONObjEntry ("capabilities",createJSONObject({
                JSONObjEntry("textDocumentSync",createJSONObject({
                    JSONObjEntry("openClose",createJSONBoolean(true)),
                    JSONObjEntry("change",createJSONNumber("1"))
                })),
                JSONObjEntry("completionProvider",createJSONObject({
                    JSONObjEntry("workDoneProgress",createJSONBoolean(true)),
                    JSONObjEntry("triggerCharacters",createJSONArray({
                        createJSONString(true,".")
                    })),
                    JSONObjEntry("resolveProvider",createJSONBoolean(true))
                })),
                JSONObjEntry("hoverProvider",createJSONBoolean(true)),
               JSONObjEntry("workspace",createJSONObject({
                   JSONObjEntry("workspaceFolders",createJSONObject({
                       JSONObjEntry("supported",createJSONBoolean(true))
                   }))
               }))
            })),
            JSONObjEntry ("serverInfo",createJSONObject({
                JSONObjEntry ("name",createJSONString(true,"StarbytesLSP-Server")),
            }))
        }))
        });
    string *initMessage;
    JSONGenerator g = JSONGenerator(json_init);
    initMessage = g.generate(false);
    cout << "Content-Length: " << initMessage->length() << "\r\n\r\n" << *initMessage;

};


};
NAMESPACE_END


