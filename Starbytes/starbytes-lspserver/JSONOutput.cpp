#include "JSONOutput.h"
#include <iostream>
#include <sstream>
#include <string>
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
    write(p[1],out.c_str(),out.length());
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

LSPServerMessage Messenger::getMessage(){
    
};