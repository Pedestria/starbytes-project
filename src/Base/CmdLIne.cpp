#include "starbytes/Base/CmdLine.h"
#include <string>
#include "starbytes/Base/ADT.h"
#include <vector>
#include <iostream>
#include <cassert>
#include "starbytes/Base/Diagnostic.h"

namespace starbytes::cl {
   
     struct Command {
            std::string name;
            std::vector<Flag> opts;
        };

        struct Flag {
            std::string name;
            char ch;
            enum {
                NONE = 0,
                BOOL,
                STRING,
                INT
            } type = Flag::NONE;
            union {
                bool b;
                char * s;
                int i;
            };
        };

   


    static std::vector<Command> cmds;
     static std::vector<Flag> flags;

     CommandBuilder command(std::string name){

         cmds.push_back(Command());
         auto ptr = std::addressof(cmds[cmds.size() - 1]);
        
        CommandBuilder b;
        b.ptr->name = name;
        
        return b;
    };

    FlagBuilder flag(std::string name,char ch,CommandBuilder & b){
        FlagBuilder f;
        Flag fl{};

        fl.name = name;
        fl.ch = ch;
        // Check if there's a parent func.
        if(b.ptr){
            b.ptr->opts.push_back(fl);
        }
        else{
            flags.push_back(fl);
        }
        return f;
    }
    
    FlagBuilder & FlagBuilder::mapVar(bool b){
        assert(ptr->type == Flag::NONE && "CANNOT MAP ANOTHER VARIALE TYPE ALREADY EXITING TYPE");
        ptr->type = Flag::BOOL;
        ptr->b = b;
        return *this;
    }

     FlagBuilder & FlagBuilder::mapVar(std::string & str){
        assert(ptr->type == Flag::NONE && "CANNOT MAP ANOTHER VARIALE TYPE ALREADY EXITING TYPE");
        ptr->type = Flag::STRING;
        ptr->s = str;
        return *this;
    }
    

     void parse(char **args,int argc){

         auto checkForCmd = cmds.size() > 0;
         int startIndex = checkForCmd? 1 : 0;

         auto noCmdFound = true;

        if(checkForCmd){
            string_ref cmd = args[startIndex];
            
            for(auto & _cmd: cmds){
                if(_cmd.name.compare(cmd.getBuffer()) == 0){
                    noCmdFound = false;
                }
            }

            if(noCmdFound){
                stdDiagnosticHandler << StandardDiagnostic::createError("Unknown Command:");
                
            }

            ++startIndex;
        }

         for(unsigned i = startIndex;i++;argc > i){
             string_ref current_arg(args[i]);

            if(current_arg.substr_ref(0, 2).equals("--")){
                
            }
            // Search for the flag on the subcommand specified 
            if(!noCmdFound){

            }
            else {

                auto found = std::find_if(flags.begin(),flags.end(),[&](cl::Flag & fl){
                    return current_arg.equals(fl.name);
                });

                if(found == flags.end()){
                     std::cout << "Unknown Command:" << current_arg.getBuffer();
                }
            }
             
         }
     }
}