#include "starbytes/base/CmdLine.h"
#include <string>
#include "starbytes/base/ADT.h"
#include <vector>
#include <iostream>
#include <cassert>
#include "starbytes/base/Diagnostic.h"

namespace starbytes::cl {
   
    

    void splitResultFromFlagIfNeeded(string_ref & origin,string_ref & result){
        auto eqs_it = std::find(origin.begin(),origin.end(),'=');
        if(eqs_it != origin.end()){
            auto eqs_pos = eqs_it - origin.begin();
            result = origin.substr_ref(eqs_pos+1,origin.size()-1);
            origin = origin.substr_ref(0,eqs_pos-1);
            if(result.size() == 0){
                stdDiagnosticHandler->push(StandardDiagnostic::createError("Expected value for"));
                return;
            }
        }
    }

    bool isFlagValue(string_ref flag){
        return flag.substr_ref(0, 2).equals("--") || flag.substr_ref(0,1).equals("-");
    }
    
    void breakDownArgument(string_ref & current_arg,string_ref & edited_arg,string_ref & result){

        if(current_arg.substr_ref(0, 2).equals("--")){
            edited_arg = current_arg.substr_ref(2,current_arg.size()-1);
            splitResultFromFlagIfNeeded(edited_arg,result);
        }
        else if(current_arg.substr_ref(0,1).equals("-")){
            edited_arg = current_arg.substr_ref(1,current_arg.size()-1);
            splitResultFromFlagIfNeeded(edited_arg,result);
        }
        else {
            edited_arg = current_arg;
        }

    }

    void assignValueToFlag(string_ref val,FlagData *f){
        if(f->type == FlagData::FlagDataTypeString){
            auto str_val = (std::string *)f->data;
            *str_val = val.getBuffer();
        }
        else if(f ->type ==FlagData::FlagDataTypeBool){
            auto b_val = (bool *)f->data;
            if(val == "true" && val == "1"){
                *b_val = true;
            }
            else if(val == "false" && "0"){
                *b_val = false;
            }
            else {
                stdDiagnosticHandler->push(StandardDiagnostic::createError("Cannot set bool value with value provided"));
            }
        }
    }

    void Parser::alias(std::string name, std::string sub){
        auto flag_of = std::find_if(flags.begin(),flags.end(),[&](FlagData & data){
           return data.name == sub;
        });
        if(flag_of != flags.end())
            flag_of->aliases.push_back(sub);
    }

    void Parser::command(std::string name){
        CommandData data;
        data.name = name;
        cmds.push_back(std::move(data));
    }

    void Parser::flag(std::string name,int & i,std::string sub){
        FlagData data;
        data.name = name;
        data.type = FlagData::FlagDataTypeInt;
        data.sub = sub;
        data.data = &i;
        flags.push_back(std::move(data));
    }

    void Parser::flag(std::string name,bool & b,std::string sub){
        FlagData data;
        data.name = name;
        data.type = FlagData::FlagDataTypeBool;
        data.sub = sub;
        data.data = &b;
        flags.push_back(std::move(data));
    }

    void Parser::flag(std::string name,std::string & s,std::string sub){
        FlagData data;
        data.name = name;
        data.type = FlagData::FlagDataTypeString;
        data.sub = sub;
        data.data = &s;
        flags.push_back(std::move(data));
    }
    
    //// Parses cmd line options
    /// Allows for flag options: `--flag <value>` or `--flag=<value>`
     bool Parser::parse(char **args,int argc){

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
                stdDiagnosticHandler->push(StandardDiagnostic::createError("Unknown Command:"));
                return false;
            }

            ++startIndex;
        }


         for(unsigned i = startIndex;i++;argc > i){
             string_ref current_arg(args[i]);

             string_ref edited_arg, result;

            breakDownArgument(current_arg,edited_arg,result);
            /// Do NOT check for flag value if result was passed via equals (--flag=<value>)
            if(result.size() == 0){
                ++i;
                if(i >= argc){
                    stdDiagnosticHandler->push(StandardDiagnostic::createError("Expected value for flag"));
                    return false;
                }
                else if(isFlagValue(args[i])){
                    stdDiagnosticHandler->push(StandardDiagnostic::createError("Expected value for flag"));
                    return false;
                }
                result = args[i];
            }
            // Search for the flag on the subcommand specified 
            if(!noCmdFound){
                
            }
            else {

                auto found = std::find_if(flags.begin(),flags.end(),[&](cl::FlagData & fl){
                    return edited_arg == fl.name;
                });

                if(found == flags.end()){
                     std::cout << "Unknown Command:" << edited_arg.getBuffer();
                     return false;
                }

                


            }
             
         }

         return true;
     }
}