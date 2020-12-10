#include "starbytes/Project/Project.h"
#include "starbytes/Parser/Lookup.h"
#include "starbytes/Base/Document.h"
#include "starbytes/Base/Base.h"
#include "starbytes/Parser/Parser.h"
#include "starbytes/Parser/Lexer.h"
#include "starbytes/Semantics/Main.h"
#include "starbytes/Gen/Gen.h"
#include "DependencyTree.h"
#include <fstream>
#include <iostream>
#include <cctype>

STARBYTES_STD_NAMESPACE

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
        std::string name;
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
            std::string entry;
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
                PFToken *tok1 = nextToken();
                if(tok1->type == PFTokenType::Identifier){
                        ptr->name = tok1->content;
                        tok1 = aheadToken();
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
            }
            void parseToConfig(){
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
            void buildDependencyTree(DependencyTree * tree){
                //Create TGs!
                for(auto & node : modules){
                    TargetDependency dep {};
                    dep.name = node->name;
                    dep.type = node->type;
                    dep.source_dir = node->directory;
                    tree->add(dep);
                }
                //Figure out each TG's dependents!
                for(auto & mod : modules){
                    for(auto & dep_name : mod->dependencies){
                        Foundation::Unsafe<TargetDependency> tg_ref = tree->getDependencyByName(dep_name);
                        if(tg_ref.hasError()){
                            //TODO: Log Error!
                        }
                        else
                            tg_ref.getResult().dependents.push_back(mod->name); 
                    }
                }

                int idx = 0;
                for(auto & dump : dumps){
                    if(!dump->all){
                        for(auto & t_name : dump->targets_to_dump){
                            Foundation::Unsafe<TargetDependency> t_dep = tree->getDependencyByName(t_name);
                            if(t_dep.hasError()){
                                //TODO: Log Error!
                            }
                            else 
                                t_dep.getResult().dump_loc = dump->dump_dest;
                            
                        }
                    }
                    else {
                        tree->foreach([&dump](TargetDependency &dep){
                            dep.dump_loc = dump->dump_dest;
                        });
                    }
                    delete dump;
                    dumps.erase(dumps.begin()+idx);
                    ++idx;
                }
            }
        public:
            ProjectFileParser(std::string _file):file(_file){};
            DependencyTree * constructDependencyTreeFromParsing(){

                DependencyTree *tree;
                tokenize();
                try{
                    parseToConfig();
                    tree = new DependencyTree(entry);
                    buildDependencyTree(tree);

                }
                catch(std::string message){
                    std::cerr << "\x1b[31m" << message << "\x1b[0m";
                    exit(1);
                }
                return tree;
            };
    };
//ByteCode::BCProgram *
    Semantics::ScopeStore compileTargetDep(TargetDependency &tg){
        std::vector<AbstractSyntaxTree *> source_trees;

        #ifdef HAS_FILESYSTEM_H

        using DIRENT = std::filesystem::directory_entry;

        Foundation::foreachInDirectory(tg.source_dir,[&source_trees](DIRENT dirent_ref){
            if(dirent_ref.is_regular_file()){
                std::string file = dirent_ref.path().generic_string();
                std::string *code_ptr = Foundation::readFile(file);
                std::vector<Token> tokens;
                Lexer(*code_ptr,tokens).tokenize();
                std::vector<AbstractSyntaxTree *> module_sources;
                AbstractSyntaxTree *file_ast = new AbstractSyntaxTree();
                file_ast->filename = file;
                try {
                    Parser(tokens,file_ast).convertToAST();
                    //TODO: Return ScopeStore when running SemanticA on any AST!
                    
                }
                catch (std::string error){
                    std::cerr << error << std::endl;
                }
                source_trees.push_back(file_ast);
            }
        });

        #endif

        Semantics::SemanticASettings settings;

        Semantics::SemanticA sem(settings);
        
        sem.initialize();
        for(auto & ast : source_trees){
            sem.analyzeFileForModule(ast);
        }
        return sem.finishAndDumpStore();
        // return CodeGen::generateToBCProgram(source_trees);
    };

    void compileFromTree(DependencyTree *tree,std::vector<StarbytesCompiledModule> *vector_ptr){
        // tree->foreach([&tree](TargetDependency &tg){
        //     if(tg.name == tree->entry_point_target){
        //         StarbytesCompiledModule mod;
        //         mod.name = tg.name;
        //         mod.program = compileTargetDep(tg);
        //     }
        // });
    }

    ProjectCompileResult buildTreeFromConfig(std::string & module_config_file){
        auto config_file_buf = Foundation::readFile(module_config_file);
        if(config_file_buf->empty() == false){
            auto first_result = ProjectFileParser(*config_file_buf).constructDependencyTreeFromParsing();
            std::vector<StarbytesCompiledModule> compiled_mods;
            compileFromTree(first_result,&compiled_mods);
            ProjectCompileResult result (compiled_mods,first_result);
            return result;
        }
        else{
            exit(1);
        }
    }

NAMESPACE_END