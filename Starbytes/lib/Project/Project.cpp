#include "starbytes/Project/Project.h"
#include "starbytes/Parser/Lookup.h"
#include "starbytes/Base/Document.h"
#include "starbytes/Base/Base.h"
#include <fstream>
#include <iostream>
#include <cctype>

// #define UNIX_FS HAS_FILESYSTEM_H

// #ifdef HAS_DIRENT_H
// #include <dirent.h>
// #endif

// #ifdef HAS_FILESYSTEM_H 
// #endif

STARBYTES_FOUNDATION_NAMESPACE

    inline std::string & unwrapQuotes(std::string & subject){
        subject.erase(subject.begin());
        subject.erase(subject.end());
        return subject;
    }

    std::string ProjectFileParseError(std::string message,DocumentPosition * pos){
        return "\x1b[31mSyntax Error: \n "+message+"\x1b[0m" + "\n This Error has Occurred at:{" +"\n Start:"+std::to_string(pos->start)+"\n End:"+std::to_string(pos->end)+"\n Line:"+std::to_string(pos->line)+"\n}";
    }

    TYPED_ENUM PFTokenType:int {
        Identifier,Keyword,Colon,String,Asterisk,OpenBracket,CloseBracket,Number,Comma
    };

    TYPED_ENUM PFKeywordType:int {
        Module,Dump,To,Get
    };

    LookupArray<std::string> keywords = {"module","dump","to","get"};

    PFKeywordType matchKeyword(std::string & subject){
        PFKeywordType Type;
        if(subject == "module"){
            Type = PFKeywordType::Module;
        }
        else if(subject == "dump"){
            Type = PFKeywordType::Dump;
        }
        else if(subject == "to"){
            Type = PFKeywordType::To;
        } 
        else if(subject == "get"){
            Type = PFKeywordType::Get;
        }
        return Type;
    }
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
        for(auto & c : subject){
            if(!std::isdigit(c)){
                return false;
            }
        }
        return true;
    }

    struct PFToken {
        PFTokenType type;
        std::string content;
        DocumentPosition *pos;
    };

    struct GetDeclaration {
        std::string lib_name;
    };

    struct DumpDeclaration {
        bool all;
        std::vector<std::string> targets_to_dump;
        std::string dump_dest;
    };
    TYPED_ENUM ModuleType:int{
        Executable,Library
    };

    struct ModuleDeclaration{
        ModuleType type;
        bool hasType = false;
        bool hasDirectory = false;
        bool hasEntry = false;
        bool hasDependencies = false;
        std::string directory;
        std::string entry;
        std::vector<std::string> dependencies;
    };

    TYPED_ENUM ModuleDeclPropTypes:int {
        Directory,Entry,Type,Dependencies
    };

    ModuleDeclPropTypes matchPropType(std::string & subject){
        ModuleDeclPropTypes result;
        if(subject == "type"){
            result = ModuleDeclPropTypes::Type;
        }
        else if(subject == "entry"){
            result = ModuleDeclPropTypes::Entry;
        } else if(subject == "directory"){
            result = ModuleDeclPropTypes::Directory;
        } else if(subject == "dependencies"){
            result = ModuleDeclPropTypes::Dependencies;
        }
        return result;
    }

    class ProjectFileParser {
        private:
            std::string file;
            unsigned int currentIndex;
            unsigned int currentLine;
            std::vector<PFToken *> tokens;
            std::vector<StarbytesModule *> * result;
            std::vector<ModuleDeclaration *> modules;
            std::vector<GetDeclaration *> libs_to_get;
            std::vector<DumpDeclaration *> dumps;
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
                    DocumentPosition *pos = new DocumentPosition();
                    pos->line = currentLine;
                    pos->start = currentIndex - size + 1;
                    pos->end = currentIndex + 1;
                    PFToken *tok = new PFToken();
                    tok->type = type;
                    tok->content = result;
                    tok->pos = pos;
                    tokens.push_back(tok);
                }
            }
            PFToken *nextToken(){
                return tokens[++currentIndex];
            }
            PFToken * aheadToken(){
                return tokens[currentIndex + 1];
            }
            void goToNextToken(){
                ++currentIndex;
            }
            void tokenize(){
                using namespace std;
                tokptr = TokenBuffer;
                start = tokptr;
                currentLine = 1;
                currentIndex = 0;
                char c = file[currentIndex];
                while(true){
                    if(isalnum(c)){
                        *tokptr = c;
                        ++tokptr;
                    }
                    else if(c == ','){
                        *tokptr = c;
                        ++tokptr;
                        clearCache(PFTokenType::Comma);
                    }
                    else if(c == '\n'){
                        ++currentLine;
                        currentIndex = 0;
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
            void parseGetDecl(GetDeclaration *ptr){

            }
            void parseDumpDecl(DumpDeclaration *ptr){
                PFToken *tok1 = nextToken();
                if(tok1->type == PFTokenType::Asterisk){
                    ptr->all = true;

                } else if(tok1->type == PFTokenType::OpenBracket){
                    PFToken *tok2 = nextToken();
                    while(true){
                        if(tok2->type == PFTokenType::String){
                            ptr->targets_to_dump.push_back(tok2->content);
                            tok2 = nextToken();
                            if(tok2->type == PFTokenType::Comma){
                                continue;
                            }
                            else if(tok2->type == PFTokenType::CloseBracket){
                                break;
                            }
                            else {
                                throw ProjectFileParseError("Expected Comma or Close Bracket!",tok2->pos);
                            }
                        }
                        else{
                            throw ProjectFileParseError("Expected String!",tok2->pos);
                        }
                        tok2 = nextToken();
                    }
                }
                else{
                    throw ProjectFileParseError("Expected Asterisk or Open Bracket!",tok1->pos);
                }

            }
            void resolveModulePropertyValue(ModuleDeclaration *ptr,ModuleDeclPropTypes & p_type){
                PFToken *tok3 = nextToken();
                if(p_type == ModuleDeclPropTypes::Type){
                    if(tok3->type == PFTokenType::Identifier){
                        if(tok3->content == "library"){
                            ptr->type = ModuleType::Library;
                            ptr->hasType = true;
                        }
                        else if(tok3->content == "executable"){
                            ptr->type = ModuleType::Executable;
                        }
                        else {
                            throw ProjectFileParseError("Expected values: `library` or `executable`",tok3->pos);
                        }
                    }
                    else{
                        throw ProjectFileParseError("Expected Identifier.",tok3->pos);
                    }
                }
                else if(p_type == ModuleDeclPropTypes::Entry){
                    if(tok3->type == PFTokenType::String){
                        ptr->entry = unwrapQuotes(tok3->content);
                        ptr->hasEntry = true;
                    }
                    else{
                        throw ProjectFileParseError("Expected String.",tok3->pos);
                    }
                } else if(p_type == ModuleDeclPropTypes::Dependencies){
                    if(tok3->type == PFTokenType::OpenBracket){
                        PFToken *tok4 = nextToken();
                        while(true){
                            if(tok4->type == PFTokenType::String){
                                ptr->dependencies.push_back(unwrapQuotes(tok4->content));
                                PFToken *tok5 = nextToken();
                                if(tok5->type == PFTokenType::Comma){
                                    continue;
                                }
                                else if(tok5->type == PFTokenType::CloseBracket){
                                    ptr->hasDependencies = true;
                                    break;
                                }
                                else {
                                    throw ProjectFileParseError("Expected Close Bracket or Comma!",tok5->pos);
                                }
                            }
                            else{
                                throw ProjectFileParseError("Expected String!",tok4->pos);
                            }
                            tok4 = nextToken();
                        }
                    }
                    else{
                        throw ProjectFileParseError("Expected Open Bracket!",tok3->pos);
                    }
                } else if(p_type == ModuleDeclPropTypes::Directory){
                    if(tok3->type == PFTokenType::String){
                        ptr->directory = unwrapQuotes(tok3->content);
                        ptr->hasDirectory = true;
                    }
                    else{
                        throw ProjectFileParseError("Expected String!",tok3->pos);
                    }
                }
            }
            void parseModuleDecl(ModuleDeclaration * ptr){
                PFToken *tok1 = aheadToken();
                while(true){
                    if(tok1->type == PFTokenType::Identifier){
                        ModuleDeclPropTypes prop_type = matchPropType(tok1->content);
                        goToNextToken();
                        PFToken *tok2 = nextToken();
                        if(tok2->type == PFTokenType::Colon){
                            resolveModulePropertyValue(ptr,prop_type);
                        }
                        else{
                            throw ProjectFileParseError("Expected Colon",tok2->pos);
                        }
                    } else if(tok1->type == PFTokenType::Keyword){
                        break;
                    }
                    tok1 = aheadToken();
                }
                
            }
            void parseToModules(){
                currentIndex = 0;
                PFToken *tok = tokens[0];
                while(true){
                    if(tok->type == PFTokenType::Keyword){
                        PFKeywordType type = matchKeyword(tok->content);
                        if(type == PFKeywordType::Module){
                            ModuleDeclaration *node = new ModuleDeclaration();
                            parseModuleDecl(node);
                            modules.push_back(node);
                        }
                        else if(type == PFKeywordType::Dump){
                            DumpDeclaration *node = new DumpDeclaration();
                            parseDumpDecl(node);
                            dumps.push_back(node);
                        } else if(type == PFKeywordType::Get){
                            GetDeclaration *node = new GetDeclaration();
                            parseGetDecl(node);
                            libs_to_get.push_back(node);
                        }
                    }
                    tok = nextToken();
                }
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

NAMESPACE_END