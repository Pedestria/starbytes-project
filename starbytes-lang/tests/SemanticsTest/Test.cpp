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

Foundation::CommandInput file_input {"test-file","f",[](std::string file){
    file_ = file;
}};

int main (int argc,char * argv[]){
    #ifdef _WIN32
    setupConsole();
    #endif

    Foundation::parseCmdArgs(argc,argv,{},{&file_input},[]{
        std::cout << "HELP! TEST!";
        exit(1);
    });

    std::string * testcode = Foundation::readFile(file_);
    std::vector<Token> toks;
    Lexer(*testcode,toks).tokenize();
    try {
        AbstractSyntaxTree * tree = new AbstractSyntaxTree();
        Parser(toks,tree).convertToAST();
        Semantics::SemanticA(tree).initialize();
    }
    catch(std::string error){
        std::cerr << "\x1b[31m" << error << "\x1b[0m" << std::endl;
        exit(1);
    }
    return 0;
    // an.initialize();
}