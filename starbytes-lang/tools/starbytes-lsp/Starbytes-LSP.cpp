#include "Starbytes-LSP.h"
#include "LSPProtocol.h"
#include "starbytes/Base/Base.h"
// #include "Parser/Lexer.h"
// #include "Parser/Parser.h"
#include <array>
#include <cctype>
#include <cstdlib>
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

LSPServerSettings settings;


Foundation::CommandInput completion_style {"completion-item-style","c",[](std::string arg){
    CompletionItemStyle style;
    if(arg == "detailed"){
        style = CompletionItemStyle::Detailed;
    }
    else if(arg == "brief"){
        style = CompletionItemStyle::Brief;
    }
    settings.completion_item_style = style;
}};




int main(int argc, char* argv[]) {
    #ifdef _WIN32
    setupConsole();
    #endif
    using namespace LSP;

    Foundation::parseCmdArgs(argc,argv,{},{&completion_style},[]{
        cout << help();
        exit(0);
    });
    // StarbytesLSPServer server;
    // server.init(argc,argv);
    //Change

    // string test = "import mylib\nimport otherLibrary\ndecl hello = [\"One\",\"Two\"]\ndecl immutable hellop:Array = [\"One\",\"Two\"]\n";
    // parseStarbytesSource(test);

    
    LSP::StarbytesLSPServer server(settings);
    server.init();
    
    //If receives a textDocument via message, parse document!
    // cout << help();
}