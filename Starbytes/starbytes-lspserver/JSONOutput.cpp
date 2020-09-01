#include "JSONOutput.h"
#include <iostream>
#include <sstream>
#include <nlohmann/json.hpp>
#include <string>

using JSON = nlohmann::json;

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

void Messenger::reply(LSPServerReply result,int id){
    ostringstream reply;
    reply << "{\n\t\"jsonrpc\":\"2.0\",";
    reply << "\n\t\"id\":" << id;
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
    cout << "Content-Length:"<<reply.str().length()<<"\r\nContent-Type:utf-8\r\n\r\n" << reply.str();
}

// struct JSONToken {

// };

// class JSONParser {
//     private:
//         int currentIndex;
//         char * bufptr;
//         char TokenBuffer[200];
//         string code;
//         char nextChar(){
//             return code[++currentIndex];
//         }
//         char aheadChar(){
//             return code[currentIndex + 1];
//         }
//         void tokenize(){
//             char c = code[currentIndex];
//             while(true){

//             }
//         }
//     public:
//         JSONParser(string code) : code(code),currentIndex(0){};
//         void parse(){

//         }
// };

LSPServerRequest Messenger::getRequest(){
    string in;
    cin >> in;
    auto str = in.substr(in.find("{"));
    auto j = JSON::parse(str);
    LSPServerRequest request;
    request.method = j["method"];
    if(j.contains("params") && j["params"].is_array()){
        j.at("params").get_to(request.params_array);
    }
    
    return request;
};