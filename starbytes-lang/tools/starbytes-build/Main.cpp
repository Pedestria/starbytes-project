#include "Main.h"
#include "starbytes/Base/CmdLine.h"
#include "starbytes/Core/Core.h"
#include "starbytes/Gen/Gen.h"
#include "starbytes/Semantics/Main.h"
#include <vector>
#include <fstream>
#include <iostream>

#ifdef _WIN32
    WINDOWS_CONSOLE_INIT
#endif

using namespace Starbytes;
struct StarbytesBuildSettings {
    std::vector<std::string> modules_to_link;
    bool hasModuleName;
    std::string module_name;
    std::string source_dir;
    std::string out_dir;
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

Foundation::CommandInput out_dir {"out-dir","d",[](std::string & input){
    if(!settings.out_dir.empty())
        settings.out_dir = input;
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
    
    std::vector<AST::AbstractSyntaxTree *> module_asts;

    #ifdef HAS_FILESYSTEM_H

    using FSEntry = std::filesystem::directory_entry;

    Foundation::foreachInDirectory(settings.source_dir,[&module_asts](FSEntry entry){
        if(entry.is_regular_file()){
            std::string file_path = entry.path().generic_string();
            std::string * file_buf = Foundation::readFile(file_path);
            AST::AbstractSyntaxTree * tree = parseCode(*file_buf);
            module_asts.push_back(tree);
        }
    });

    #endif

    Semantics::SemanticA sem;
    sem.initialize();
    for(auto & ast : module_asts){
        sem.analyzeFileForModule(ast);
    }
    sem.finish();
    
    std::string out = settings.out_dir+"/"+settings.module_name+".stbxm";
    std::ofstream module_stream (out,std::ios::out);
    CodeGen::generateToBCProgram(module_asts,module_stream);
    module_stream.close();

    return 0;
};

