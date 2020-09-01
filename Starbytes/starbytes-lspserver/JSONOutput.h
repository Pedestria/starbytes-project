#pragma once
#include <string>
#include <vector>


namespace LSP {
    struct LSPServerObject {

    };

    struct LSReplyErrorData {
        std::string str_data;
        int int_data;
        bool bool_data;
        std::string *array_data;
        LSPServerObject *object_data;
    };

    enum LSPErrorCode:int {
        ParseError = -32700,InvalidRequest = -32600,MethodNotFound = -32601,InvalidParams = -32602,InternalError = -32603, serverErrorStart = -32099,serverErrorEnd = -32000, ServerNotIntialized = -32002,UnknownErrorCode = -32001, RequestCancelled = -32800,ContentModified = -32801
    };

    struct LSPReplyError {
        LSPErrorCode code;
        std::string message;
        LSReplyErrorData data;
    };
    struct LSPServerReply {
        std::string str_result;
        int int_result;
        bool bool_result;
        LSPServerObject object_result;
        LSPReplyError error;
    };
    struct LSPServerRequest {
        std::string method;
        std::vector<std::string> params_array;
        LSPServerObject params_object;
    };
    namespace Messenger {
        LSPServerRequest getRequest();
        void reply(LSPServerReply result,int id);
    };
};
