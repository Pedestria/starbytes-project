#include "starbytes/Base/Base.h"
#include <iostream>

using namespace Starbytes;

#ifdef _WIN32
 WINDOWS_CONSOLE_INIT
#endif

int main(int argc,char * argv[]){
    #ifdef _WIN32
     setupConsole();
    #endif
    if(argc > 1) {
        std::string arg_ref = argv[1];
        std::string process_args;
        for(int i = 2;i < argc;++i){
            process_args.append(" ").append(argv[i]);
        }
        std::string process_to_execute;
        if(arg_ref == "build")
            process_to_execute = "starbytes-b";
        else if(arg_ref == "run")
            process_to_execute = "starbytes-r";
        std::cout << "Starting Starbytes Command:" << arg_ref << std::endl;
        Foundation::execute_child_process(process_to_execute,process_args,true);
    }
    else if(argc == 1)
      std::cerr << ERROR_ANSI_ESC << "Error: No subcommand provided!\nExiting..." << ANSI_ESC_RESET << std::endl;

    return 0;
};