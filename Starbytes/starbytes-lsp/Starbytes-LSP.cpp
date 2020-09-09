#include "Starbytes-LSP.h"
#include "JSONOutput.h"
#include <array>
#include <cctype>
#include <sstream>
#include <string>
#include <array>
#include <vector>

#ifdef _WIN32
#include <Windows.h>
static HANDLE stdoutHandle;
static DWORD outModeInit;

void setupConsole(void){
    DWORD outMode = 0;
    stdoutHandle = GetStdHandle(STD_OUTPUT_HANDLE);

    if(stdoutHandle == INVALID_HANDLE_VALUE) {
		exit(GetLastError());
	}

    if(!GetConsoleMode(stdoutHandle, &outMode)) {
	    exit(GetLastError());
	}

    outModeInit = outMode;

    outMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;

    if(!SetConsoleMode(stdoutHandle, outMode)) {
		exit(GetLastError());
	}
}
#endif

using namespace Starbytes;
using namespace std;

string help(){
 return "\n \u001b[35m\u001b[4m The Starbytes LSP Implementation!\u001b[0m \n \n \u001b[4mFlags:\u001b[0m \n \n --help = Display help info. \n";
}

bool parseArguments(char * arguments[],int count){
    char ** flags = arguments;
    ++flags;
    vector<string> FLAGS;
    for(int i = 1;i < count;++i){
        FLAGS.push_back(string(*flags));
        ++flags;
    }
    int index = 0;
    while(index < FLAGS.size()){

        if(FLAGS[index] == "--help"){
            cout << help();
            return false;
        }
        // else if(FLAGS[index] == "--pipe"){
        //     ++index;
        //     pipe_id = stoi(FLAGS[index]);
        // }
        ++index;
    }
    return true;

}

// using namespace LSP;

// void StarbytesLSPServer::init(int argc, char* argv[]){
//     if(parseArguments(argv,argc)){
//         LSP::LSPServerMessage *cntr;
//         json_transit.read(cntr);
//        if(cntr->type == Request){
//            if(((LSPServerRequest *)cntr)->method == "intialize" ){

//            }
//        }
//     }
// };




int main(int argc, char* argv[]) {
    #ifdef _WIN32
    setupConsole();
    #endif

    // StarbytesLSPServer server;
    // server.init(argc,argv);

    if(!parseArguments(argv,argc)){
        return 0;
    }
    else {
        LSP::Messenger json_transit;
        LSP::LSPServerRequest * msg;
        json_transit.read(msg);
        json_transit.sendIntializeMessage();
    }
    // cout << help();
}