#include "Main.h"
#include "starbytes/Base/CmdLine.h"
#include "starbytes/Core/Core.h"
#include "starbytes/Gen/Gen.h"
#include "starbytes/Semantics/Main.h"
#include "starbytes/Base/Optional.h"
#include <vector>
#include <fstream>
#include <iostream>

#ifdef _WIN32
    WINDOWS_CONSOLE_INIT
#endif

using namespace Starbytes;
using namespace Starbytes::Foundation;
struct StarbytesBuildSettings {
    std::vector<std::string> modules_to_link;
    Foundation::Optional<std::string> module_name;
    Foundation::Optional<std::string> source_dir;
    std::string out_dir;
    bool library_mode;
    bool exec_mode;
} settings;

CommandLine::CommandOption library_mode {"Lib","L",CommandLine::FlagDescription("Sets `starbytes-b` to library mode."),[](){
    if(!settings.exec_mode)
        settings.library_mode = true;
    else {
     std::cerr << "Cannot set build mode to lib if exec mode is already enabled!";
     exit(1);
    }
}};

CommandLine::CommandOption exec_mode {"Exec","E",CommandLine::FlagDescription("Sets `starbytes-b` to executable mode."),[](){
    if(!settings.library_mode)
        settings.exec_mode = true;
    else {
        std::cerr << "Cannot set build mode to exec if lib mode is already enabled!";
        exit(1);
    }
}};

CommandLine::CommandInput module_name {"module-name","n",CommandLine::FlagDescription("Provides a name for the module to be compiled from the given the sources."),[](std::string & input){
    if(settings.module_name.hasVal())
        settings.module_name = input;
}};

CommandLine::CommandInput src_dir {"src-dir","s",CommandLine::FlagDescription("Provides a source directory"),[](std::string & input){
    if(!input.empty())
        settings.source_dir = input;
}};

CommandLine::CommandInput out_dir {"out-dir","d",CommandLine::FlagDescription("Provides an output directory. (Where to output the compiled module to.)"),[](std::string & input){
    if(!settings.out_dir.empty())
        settings.out_dir = input;
}};

CommandLine::CommandInput link_module {"link-module","l",CommandLine::FlagDescription("A compiled module to link to new module."),[](std::string & input){
    settings.modules_to_link.push_back(input);
}};

std::string backup_module_name = "MyModule";

int main(int argc,char * argv[]){
    #ifdef _WIN32
        setupConsole();
    #endif
    CommandLine::parseCmdArgs(argc,argv,{&exec_mode,&library_mode},{&link_module,&module_name,&src_dir},"Starbytes Build");

    if(!settings.module_name.hasVal()){
        std::cerr << ERROR_ANSI_ESC << "No Module Name Provided!" << ANSI_ESC_RESET << std::endl;
        settings.module_name = backup_module_name;
    }

    if(!settings.source_dir.hasVal()){
        std::cerr << ERROR_ANSI_ESC << "No Module Source Directory Provided!\nExiting..." << ANSI_ESC_RESET << std::endl;
        exit(1);
    };

    return 0;
};

