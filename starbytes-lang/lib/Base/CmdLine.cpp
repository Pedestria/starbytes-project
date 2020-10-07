#include "starbytes/Base/CmdLine.h"
#include "starbytes/Base/Macros.h"
#include <vector>
#include <iostream>



STARBYTES_FOUNDATION_NAMESPACE

void parseCmdArgs(int arg_count, char *argv[], std::initializer_list<CommandOption *> options, std::initializer_list<CommandInput *> inputs,void (*help_opt_callback)()){
    std::vector<std::string> flags;
    for(int i = 1;i < arg_count;++i){
        flags.push_back(std::string(argv[i]));
    }

    bool is_flag;
    
    for(int i = 0;i < flags.size();++i){
        is_flag = false;
        if(flags[i] == "--help" || flags[i] == "-h"){
            is_flag = true;
            help_opt_callback();
        }

        for(auto & in : inputs){
            std::string & flag = flags[i];
            if(flag == "--"+in->first_flag_match || flag == "-"+in->second_flag_match){
                is_flag = true;
                in->func_ptr(flags[++i]);
            }
        }
        for(auto & opt : options){
            std::string & flag = flags[i];
            if(flag == "--"+opt->first_flag_match || flag == "-"+opt->second_flag_match){
                is_flag = true;
                opt->func_ptr();
            }
        }

        if(!is_flag){
            std::cerr << "\x1b[31m\x1b[2mCommand Line Parse Error:\x1b[0m \n Unknown flag: " << flags[i] << "\n";
        }
    }
};

NAMESPACE_END