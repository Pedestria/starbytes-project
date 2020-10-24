#include "starbytes/Base/Base.h"
#include "LSP/Starbytes-LSP.h"
#include <string>

using namespace Starbytes;

#ifdef _WIN32
WINDOWS_CONSOLE_INIT
#endif

using namespace LSP;


std::string help(){
 return "\n \u001b[35m\u001b[4m The Starbytes LSP Implementation!\u001b[0m \n \n \u001b[4mFlags:\u001b[0m \n \n --help = Display help info. \n";
}

LSPServerSettings settings;

int main(int argc, char* argv[]) {
    #ifdef _WIN32
    setupConsole();
    #endif

    Foundation::parseCmdArgs(argc,argv,{},{},[]{
        std::cout << help();
        exit(0);
    });
    // StarbytesLSPServer server;
    // server.init(argc,argv);
    //Change

    // string test = "import mylib\nimport otherLibrary\ndecl hello = [\"One\",\"Two\"]\ndecl immutable hellop:Array = [\"One\",\"Two\"]\n";
    // parseStarbytesSource(test);
    
    LSP::StarbytesLSPServer server (settings);
    server.init();
    
    //If receives a textDocument via message, parse document!
    // cout << help();
    return 0;
}