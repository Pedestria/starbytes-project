#include <initializer_list>
#include <string>
#include <vector>
#include <memory>
#include "ADT.h"

#ifndef STARBYTES_BASE_CMDLINE_H
#define STARBYTES_BASE_CMDLINE_H

namespace starbytes{
    
    namespace cl {

        struct CommandData {
            std::string name;
            std::string desc;
        };
        struct FlagData {
            std::string name;
            std::vector<std::string> aliases;
            std::string desc;
            std::string sub;
            typedef enum : int {
                FlagDataTypeInt,
                FlagDataTypeString,
                FlagDataTypeBool
            } T;
            T type;
            void *data;
        };

        class Parser {
            std::vector<CommandData> cmds;
            std::vector<FlagData> flags;
        public:
            

            void command(std::string name);

            void option(std::string name,std::string sub);

            void flag(std::string name,int & i,std::string sub);
            void flag(std::string name,bool & b,std::string sub);
            void flag(std::string name,std::string & s,std::string sub);

            void alias(std::string name,std::string sub);
            
            /**
            Only Run This Option Once
            */
            bool parse(char **args,int argc);

        };
    }
    
};

#endif