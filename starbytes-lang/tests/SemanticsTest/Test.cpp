#include "starbytes/Semantics/Main.h"
#include "starbytes/Parser/Lexer.h"
#include "starbytes/Parser/Parser.h"
#include "starbytes/AST/AST.h"

//Test

using namespace Starbytes;

#ifdef _WIN32
WINDOWS_CONSOLE_INIT
#endif

std::string file_;

using namespace Foundation;

CommandLine::CommandInput file_input {"test-file","f",CommandLine::FlagDescription("A file to test!"),[](std::string & file){
    file_ = file;
}};

int main (int argc,char * argv[]){
    #ifdef _WIN32
    setupConsole();
    #endif

    CommandLine::parseCmdArgs(argc,argv,{},{&file_input},"Semantics Test!");

    if(file_.empty()){
        std::cout << ERROR_ANSI_ESC  <<"No test file provided!\nExiting..." << ANSI_ESC_RESET << std::endl;
        exit(1);
    }

    std::string * testcode = Foundation::readFile(file_);
    std::vector<Token> toks;
    Lexer(*testcode,toks).tokenize();
    try {
        AbstractSyntaxTree * tree = new AbstractSyntaxTree();
        Parser(toks,tree).convertToAST();
        for(auto & node : tree->nodes){
            std::cout << "NODETYPE:" << int(node->type) << std::endl;
        }
        Semantics::SemanticASettings settings;
        Semantics::SemanticA sem(settings);
        sem.initialize();
        sem.analyzeFileForModule(tree);
        sem.finish();
    }
    catch(std::string error){
        std::cerr << "\x1b[31m" << error << "\x1b[0m" << std::endl;
        exit(1);
    }
    return 0;
    // an.initialize();
}