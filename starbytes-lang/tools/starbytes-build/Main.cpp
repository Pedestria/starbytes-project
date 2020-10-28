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
    bool hasModuleName;
    std::string module_name;
    bool library_mode;
    bool exec_mode;
} settings;

Foundation::CommandOption library_mode {"Lib","L",[](){
    if(!settings.exec_mode)
        settings.library_mode = true;
    else {
     std::cerr << "Cannot set build mode to lib if exec mode is already enabled!";
     exit(1);
    }
}};

Foundation::CommandOption exec_mode {"Exec","E",[](){
    if(!settings.library_mode)
        settings.exec_mode = true;
    else {
        std::cerr << "Cannot set build mode to exec if lib mode is already enabled!";
        exit(1);
    }
}};

Foundation::CommandInput module_name {"module-name","n",[](std::string & input){
    if(settings.hasModuleName)
        settings.module_name = input;
}};

Foundation::CommandInput link_module {"link-module","l",[](std::string & input){
    settings.modules_to_link.push_back(input);
}};

int main(int argc,char * argv[]){
    #ifdef _WIN32
        setupConsole();
    #endif
    Foundation::parseCmdArgs(argc,argv,{&exec_mode,&library_mode},{&link_module,&module_name},[]{
        std::cout << "\x1b[34mStarbytes Build\x1b[0m" << std::endl;
        exit(1);
    });


    return 0;
};

