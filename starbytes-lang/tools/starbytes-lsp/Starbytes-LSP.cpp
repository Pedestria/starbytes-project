#include "Starbytes-LSP.h"
#include "LSPProtocol.h"
// #include "Parser/Lexer.h"
// #include "Parser/Parser.h"
#include <array>
#include <cctype>
#include <sstream>
#include <string>
#include <vector>

// #ifdef HAS_IO_H
// #include <io.h>
// #endif

// #ifdef HAS_UNISTD_H
// #include <unistd.h>
// #endif


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

using namespace LSP;

void LSPQueue::queueMessage(LSPServerMessage * msg){
    messages.push_back(msg);
};

LSPServerMessage * LSPQueue::getLatest(){
    LSPServerMessage * rc =  messages.back();
    messages.pop_back();
    return rc;
};

void StarbytesLSPServer::init(){
    getMessageFromStdin();
    LSPServerMessage *INITAL = queue.getLatest();
    if(INITAL->type == Request){
        LSPServerRequest *RQ = (LSPServerRequest *)INITAL;
        if(RQ->method == INITIALIZE){
            json_transit.sendIntializeMessage(RQ->id);
        }
    }
};

void StarbytesLSPServer::getMessageFromStdin(){
    LSP::LSPServerMessage *cntr = json_transit.read();
    queue.queueMessage(cntr);
};

// AbstractSyntaxTree * parseStarbytesSource(std::string & data){
//     auto toks = Lexer(data).tokenize();
//     AbstractSyntaxTree * tree = new AbstractSyntaxTree();
//     try {
//         Parser(toks,tree).convertToAST();
//         return tree;
//     }
//     catch (string message){
//         std::cerr << "Parser Error!";
//         return nullptr;
//     }

// }




int main(int argc, char* argv[]) {
    #ifdef _WIN32
    setupConsole();
    #endif
    using namespace LSP;
    // StarbytesLSPServer server;
    // server.init(argc,argv);
    //Change

    // string test = "import mylib\nimport otherLibrary\ndecl hello = [\"One\",\"Two\"]\ndecl immutable hellop:Array = [\"One\",\"Two\"]\n";
    // parseStarbytesSource(test);

    if(!parseArguments(argv,argc)){
        return 0;
    }
    else {
        LSP::StarbytesLSPServer server;
        server.init();
    }
    //If receives a textDocument via message, parse document!
    // cout << help();
}