#include "Main.h"
#include "starbytes/Base/CmdLine.h"
#include <vector>
#include <iostream>

#ifdef _WIN32
    WINDOWS_CONSOLE_INIT
#endif

using namespace Starbytes;
struct StarbytesBuildSettings {
    std::vector<std::string> modules_to_link;
} settings;


Foundation::CommandInput link_module {"link-module","l",[](std::string input){
    settings.modules_to_link.push_back(input);
}};

int main(int argc,char * argv[]){
    #ifdef _WIN32
        setupConsole();
    #endif
    Foundation::parseCmdArgs(argc,argv,{},{&link_module},[]{
        std::cout << "\x1b[34mStarbytes Build\x1b[0m" << std::endl;
        exit(1);
    });


    return 0;
};

