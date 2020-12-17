#include "starbytes/Base/Base.h"
#include "LSP/Starbytes-LSP.h"
#include <string>

using namespace Starbytes;

#ifdef _WIN32
WINDOWS_CONSOLE_INIT
#endif

using namespace LSP;


LSPServerSettings settings;

int main(int argc, char* argv[]) {
    #ifdef _WIN32
    setupConsole();
    #endif

    Foundation::CommandLine::parseCmdArgs(argc,argv,{},{},"The Starbytes LSP Implementation!");
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