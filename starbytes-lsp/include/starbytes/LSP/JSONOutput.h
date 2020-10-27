#include <optional>
#include <string>
#include <vector>
#include <map>
#include "starbytes/Base/Base.h"

#ifndef STARBYTES_LSP_JSON_OUTPUT_H
#define STARBYTES_LSP_JSON_OUTPUT_H

STARBYTES_STD_NAMESPACE

    

    template <typename T >using Optional = std::optional<T>;

    namespace LSP {
        namespace JSON {
            TYPED_ENUM JSONNodeType:int {
        Object,Array,String,Number,Null,Boolean,NullString
    };
            struct JSONNode {
                JSONNodeType type;
            };

            struct JSONNull : JSONNode {};
            struct JSONNullString : JSONNode {};
            struct JSONBoolean : JSONNode {
                bool value;
            };
            struct JSONNumber : JSONNode {
                std::string value;
            };

            struct JSONString : JSONNode {
                std::string value;
            };
            typedef std::pair<std::string,JSONNode *> JSONObjEntry;
            struct JSONObject : JSONNode {
                std::map<std::string,JSONNode*> entries;
            };
            struct JSONArray : JSONNode {
                std::vector<JSONNode*> objects;
            };

            TYPED_ENUM JSONTokType:int {
                CloseBrace,CloseBracket,OpenBrace,OpenBracket,Numeric,String,Colon,Comma,Boolean,Null,NullString
            };

        };

    enum LSPObjectType:int {
        CancellParams,IntializeParams,ClientInfo,ClientCapabilities,WorkspaceFolder,InitializeResult,CompletionOptions,HoverOptions,ReplyError
    };
    enum LSPMessageType:int {
        Request,Notification
    };
    //The Language Server Protocol Object that all objects extend!
    struct LSPServerObject {
        LSPObjectType type;
    };

    struct LSReplyErrorData : LSPServerObject {
        std::string str_data;
        int int_data;
        bool bool_data;
        std::string *array_data;
        LSPServerObject *object_data;
    };

    enum LSPErrorCode:int {
        ParseError = -32700,InvalidRequest = -32600,MethodNotFound = -32601,InvalidParams = -32602,InternalError = -32603, serverErrorStart = -32099,serverErrorEnd = -32000, ServerNotIntialized = -32002,UnknownErrorCode = -32001, RequestCancelled = -32800,ContentModified = -32801
    };

    struct LSPReplyError : LSPServerObject {
        LSPErrorCode code;
        std::string message;
        LSReplyErrorData data;
    };
    struct LSPServerReply {
        int id;
        std::string str_result;
        int int_result;
        bool bool_result;
        LSPServerObject * object_result;
        LSPReplyError * error;
    };
    struct LSPServerMessage { LSPMessageType type;};
    struct LSPServerRequest : LSPServerMessage {
        std::string id;
        std::string method;
        //Unparsed Args!
        Optional<JSON::JSONArray *> params_array;
        //Unparsed Args!
        Optional<JSON::JSONObject *> params_object;
    };

    typedef std::string ProgressToken;
    template<class TokVal>
    struct LSPServerProgressParams : LSPServerObject {
        ProgressToken token;
        TokVal value;
    };

    struct LSPServerCancellationParams : LSPServerObject {
        int int_id;
        std::string string_id;
    };
    struct LSPServerNotification : LSPServerMessage {
        std::string method;
        Optional<JSON::JSONArray *> params_array;
        Optional<JSON::JSONObject *> params_object;
    };

    class Messenger {
        private:

        public:
            void sendIntializeMessage(std::string & id);
            void reply(LSPServerReply *result);
            LSPServerMessage * read();
            Messenger(){};
    };

    std::string wrapQuotes(std::string &subject);
    std::string unwrapQuotes(std::string &subject);
};

NAMESPACE_END

#endif


