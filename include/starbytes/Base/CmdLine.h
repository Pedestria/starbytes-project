#include <initializer_list>
#include <string>
#include <vector>
#include <memory>

#ifndef STARBYTES_BASE_CMDLINE_H
#define STARBYTES_BASE_CMDLINE_H

namespace starbytes{
    
    namespace cl {

        struct Command;
        struct Flag;

        typedef enum : int{
            None,
            String,
            Bool,
            Int
        } FlagType;

        typedef char FlagTypeNull;
        #define NULL_FLAG_VAL '\0'

        class FlagBuilder;

        #define FLAG_BUILDER_FUNC FlagBuilder flag(std::string name,char ch,CommandBuilder & parentCommand);

        class CommandBuilder {
            Command *ptr = nullptr;
            friend  CommandBuilder command(std::string name);
            friend FLAG_BUILDER_FUNC;
        };

        static CommandBuilder NULL_COMMAND= {};

        class FlagBuilder {
            Flag * ptr;
            friend FLAG_BUILDER_FUNC;
            friend class CommandBuilder;
        public:
            FlagBuilder & mapVar(bool t);
            FlagBuilder & mapVar(std::string & t);
        };
        
        CommandBuilder command(std::string name);
        template<class T = FlagTypeNull>
        FlagBuilder flag(std::string name,char ch = '\0',FlagType type = FlagType::None,T val = NULL_FLAG_VAL,CommandBuilder & parentCommand = cl::NULL_COMMAND);
        
       
       
        
        /**
        Only Run This Option Once
        */
        void parse(char **args,int argc);
    }
    
};

#endif