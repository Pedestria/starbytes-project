#include "JSONOutput.h"
#include "LSPProtocol.h"
#include <cctype>
#include <cstdio>
#include <iostream>
#include <sstream>
#include <string>
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

    typedef pair<string,JSONNode *> JSONObjEntry;

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
            string * generate(){
                generateJSONObject(tree);
                
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
            JSONParser(string code) : code(code),currentIndex(code.find("{")){};
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
                    return parseResult;
                    // for(auto entries : parseResult->entries){
                    //     cout << "{Key:" << entries.first << ",NodeType:" << int(entries.second->type) << "}";
                    // }
                }
                catch (string message){
                    cout << message << "\n";
                    return nullptr;
                }

            }
    };
    //Translates From JSON to LSPServerObject and Back.
    class JSONTranslator{
        private:
            void translateLSPWorkspaceFolder(JSONObject *obj,LSPWorkspaceFolder *node){
                node->type = WorkspaceFolder;
                for(auto ent : obj->entries){
                    if(ent.first == "uri"){
                        node->uri = ((JSONString *)ent.second)->value;
                    } else if(ent.first == "name"){
                        node->name = ((JSONString *)ent.second)->value;
                    }
                }
            };
            void translateJSONObject(JSONObject *node,LSPServerObject *cntr,string& methodToInvoke){
                if(methodToInvoke == "initialize"){
                    LSPIntializeParams *Node = new LSPIntializeParams();
                    Node->type = IntializeParams;
                    for(auto ent : node->entries){
                        if(ent.first == "processId"){
                            Node->process_id = ((JSONString*)ent.second)->value;
                        } else if(ent.first == "clientInfo"){
                            Node->hasClientInfo = true;
                            LSPClientInfo *info = new LSPClientInfo();
                            info->type = ClientInfo;
                            for(auto ent2 : ((JSONObject*)ent.second)->entries){
                                if(ent2.first == "name"){
                                    info->name = ((JSONString *)ent2.second)->value;
                                } else if(ent2.first == "version"){
                                    info->hasVersion = true;
                                    info->version = ((JSONString*)ent2.second)->value;
                                }
                            }
                            Node->client_info = info;
                        } else if(ent.first == "rootPath"){
                            Node->hasRootPath = true;
                            Node->root_path = ((JSONString *)ent.second)->value;
                        } else if(ent.first == "rootUri"){
                            if(ent.second->type == JSONNodeType::String){
                                Node->hasRootUri = true;
                                Node->root_uri = ((JSONString *)ent.second)->value;
                            }
                        } else if(ent.first == "capabilities"){
                            LSPClientCapabilities *clientcap = new LSPClientCapabilities();
                            for(auto ent2 : ((JSONObject *)ent.second)->entries){
                                if(ent2.first == "workspace"){
                                    clientcap->hasWorkspace = true;
                                    LSPClientCapabilitiesWorkspace *wrkspce = new LSPClientCapabilitiesWorkspace();
                                    
                                }
                            }
                        } else if(ent.first == "trace"){
                            Node->hasTrace = true;
                            JSONString * s = (JSONString*)ent.second;
                            if(s->value == "off"){
                                Node->trace = LSPTraceSetting::Off;
                            } else if(s->value == "messages"){
                                Node->trace = LSPTraceSetting::Messages;
                            } else if(s->value == "verbose"){
                                Node->trace = LSPTraceSetting::Verbose;
                            }
                        } else if(ent.first == "workspaceFolders"){
                            Node->hasWorkspacefolders = true;
                            if(ent.second->type == JSONNodeType::Array){
                                for(auto obj : ((JSONArray *)ent.second)->objects){
                                    LSPWorkspaceFolder *folder = new LSPWorkspaceFolder();
                                    translateLSPWorkspaceFolder((JSONObject *)obj,folder);
                                    Node->workspace_folders.push_back(folder);
                                }
                            }else {
                                Node->hasWorkspacefolders = false;
                            }
                        }
                    }
                }
            };
            void translateJSONArray(JSONArray *node,std::vector<LSPServerObject *> *resloc,string& methodToInvoke){
                for(auto NODE : node->objects){
                    if(NODE->type == JSONNodeType::Object){
                        LSPServerObject *cntr;
                        translateJSONObject((JSONObject *)NODE,cntr,methodToInvoke);
                        resloc->push_back(cntr);
                    }
                }
            };
        public:
            JSONTranslator(){};
            void translateFromJSON(LSPServerMessage *msgcntr,string &message){
                JSONObject *messagec = JSONParser(message).parse();
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
                                translateJSONObject((JSONObject *)entry.second,cntr,req->method);
                                req->params_object = cntr;
                            } else if(Type == JSONNodeType::Array){
                                translateJSONArray((JSONArray *)entry.second,&req->params_array,req->method);
                            }
                        }
                    }
                }
                else {

                }
            };
            void compileToJSON(LSPServerReply *msgtosnd,string *msgout){
                JSONGenerator g = JSONGenerator(translateToJSON(msgtosnd));
                msgout = g.generate();
            }
            JSONObject * translateLSPObjectToJSON(LSPServerObject *obj){
                JSONObject *result;
                if(obj->type == ReplyError){
                    LSPReplyError *err = (LSPReplyError *)obj;
                    result = createJSONObject({
                        JSONObjEntry ("code",createJSONNumber(to_string(err->code))),
                        JSONObjEntry ("message",createJSONString(true,err->message))
                    });
                } else if(obj->type == CompletionOptions){
                    LSPCompletionOptions *re = (LSPCompletionOptions *)obj;
                    result = createJSONObject({
                        JSONObjEntry ("triggerCharacters",convertStringArray(&re->trigger_characters)),
                        JSONObjEntry ("allCommitCharacters",convertStringArray(&re->all_commit_characters)),
                        JSONObjEntry ("resolveProvider",createJSONBoolean(re->resolve_provider))
                    });
                }
                return result;
            }
            JSONObject * translateToJSON(LSPServerReply *msgtosnd){
                JSONObject *result = createJSONObject({
                    JSONObjEntry ("id",createJSONNumber(to_string(msgtosnd->id))),
                });
                if(msgtosnd->error != nullptr){
                    result->entries.insert(JSONObjEntry("error",translateLSPObjectToJSON(msgtosnd->error)));
                }
                else if(msgtosnd->object_result != nullptr){
                    result->entries.insert(JSONObjEntry ("result",translateLSPObjectToJSON(msgtosnd->object_result)));
                }
                return result;
            };
    };


};


void Messenger::read(LSPServerMessage *msgcntr){
    using namespace JSON;
    std::string message;
    cin >> message;
    if(!message.empty()){
        JSONTranslator t;
        LSPServerMessage *c;
        t.translateFromJSON(c,message);
    }
}

void Messenger::reply(LSPServerReply *result){
    using namespace JSON;
    JSONTranslator t;
    string *r;
    t.compileToJSON(result,r);
    cout << "Content-Length:" << r->length() << "\r\n\r\n" << *r;
}

void Messenger::sendIntializeMessage(){
    using namespace JSON;

    JSONObject *json_init = createJSONObject({
        JSONObjEntry ("jsonrpc",createJSONString(true,"2.0")),
        JSONObjEntry ("id",createJSONString(true,"initialize - (0)")),
        JSONObjEntry ("result",createJSONObject({
            JSONObjEntry ("capabilities",createJSONObject({
               
            })),
            JSONObjEntry ("serverInfo",createJSONObject({
                JSONObjEntry ("name",createJSONString(true,"StarbytesLSPServer"))
            }))
        }))
        });
    string *initMessage;
    JSONGenerator g = JSONGenerator(json_init);
    initMessage = g.generate();
    cout << "Content-Length:" << initMessage->length() << "\r\n\r\n" << *initMessage;

};


};
};


