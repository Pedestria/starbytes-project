#include "Module/Module.h"
#include "Parser/Lookup.h"
#include <fstream>
#include <iostream>
#include <cctype>

namespace Starbytes {

    std::string * readFile(std::string & File){
        std::string * buffer = new std::string();
        std::ifstream input (File);
        if(input.is_open()){
            std::ostringstream file;
            file << input.rdbuf();
            *buffer = file.str();
            input.close();
        }
        else{
            *buffer = "";
            std::cerr << "Error: Cannot Locate File: " << File;
        }
        return buffer;
        
    }

    enum class PFTokenType:int {
        Identifier,Keyword,Colon,String,Asterisk,OpenBracket,CloseBracket,Number
    };

    LookupArray<std::string> keywords = {"module","dump","to","get"};

    bool isColon(char & c){
        if(c == ':'){
            return true;
        }
        else{
            return false;
        }
    }

    bool isAsterisk(char & c){
        if(c == '*'){
            return true;
        }
        else{
            return false;
        }
    }

    bool isQuote(char & c){
        if(c == '"'){
            return true;
        }
        else {
            return false;
        }
    }

    bool isBracket(char & c){
        if(c == '[' || c == ']'){
            return true;
        }
        else{
            return false;
        }
    }

    bool isKeyword(std::string & subject){
        return keywords.lookup(subject);
    }

    bool isNumber(std::string & subject){
        for(auto c : subject){
            if(!std::isdigit(c)){
                return false;
            }
        }
        return true;
    }

    struct PFToken {
        PFTokenType type;
        std::string content;
    };

    class ProjectFileParser {
        private:
            std::string file;
            unsigned int currentIndex;
            std::vector<PFToken *> tokens;
            std::vector<StarbytesModule *> * result;
            char TokenBuffer[500];
            char *tokptr;
            char *start;
            char nextChar(){
                return file[++currentIndex];
            }
            char aheadChar(){
                return file[currentIndex+1];
            }
            void clearCache(PFTokenType type = PFTokenType::Identifier){
                if(tokptr == TokenBuffer){
                    return;
                }
                else{
                    auto size = tokptr - start;
                    std::string result = std::string(TokenBuffer,size);
                    if(isKeyword(result)){
                        type = PFTokenType::Keyword;
                    }
                    PFToken *tok = new PFToken();
                    tok->type = type;
                    tok->content = result;
                    tokens.push_back(tok);
                }
            }
            void tokenize(){
                using namespace std;
                tokptr = TokenBuffer;
                start = tokptr;
                currentIndex = 0;
                char c = file[currentIndex];
                while(true){
                    if(isalnum(c)){
                        *tokptr = c;
                        ++tokptr;
                    }
                    else if(isColon(c)){
                        *tokptr = c;
                        ++tokptr;
                        clearCache(PFTokenType::Colon);
                    }
                    else if(isQuote(c)){
                        *tokptr = c;
                        ++tokptr;
                        while(true){
                            if(c == '"'){
                                *tokptr = c;
                                ++tokptr;
                                clearCache(PFTokenType::String);
                                break;
                            }
                            else{
                                *tokptr = c;
                                ++tokptr;
                            }
                            c = nextChar();
                        }
                    }
                    else if(isBracket(c)){
                        *tokptr = c;
                        ++tokptr;
                        PFTokenType T;
                        if(c == '['){
                            T = PFTokenType::OpenBracket;
                        }
                        else{
                            T = PFTokenType::CloseBracket;
                        }
                        clearCache(T);
                    }
                    else if(isAsterisk(c)){
                        *tokptr = c;
                        ++tokptr;
                        clearCache(PFTokenType::Asterisk);
                    }
                    else if(isspace(c)){
                        clearCache();
                    }
                    else if(c == '\0'){
                        clearCache();
                        break;
                    }
                    c = nextChar();
                }
            };
            void parseToModules(){
                
            }
        public:
            ProjectFileParser(std::string _file):file(_file){};
            std::vector<StarbytesModule *> * parseFile(){

                std::vector<StarbytesModule *> * _result = new std::vector<StarbytesModule *>();
                result = _result;
                tokenize();
                try{
                    parseToModules();
                }
                catch(std::string message){
                    std::cerr << "\x1b[31m" << message << "\x1b[0m";
                    exit(1);
                }
                return _result;
            };
    };

    std::vector<StarbytesCompiledModule *> * constructAndCompileModulesFromConfig(std::string & module_config_file){
        auto config_file_buf = readFile(module_config_file);
        if(config_file_buf->empty() == false){
            auto first_result = ProjectFileParser(*config_file_buf).parseFile();
        }
        else{
            exit(1);
        }
        auto * result = new std::vector<StarbytesCompiledModule *>();
        return result;
    }
}