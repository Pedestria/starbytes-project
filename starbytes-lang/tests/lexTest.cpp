#include "starbytes/Parser/Parser.h"
#include <fstream>
#include <iostream>
#include <llvm/Support/InitLLVM.h>

using namespace starbytes;

int main(int argc,char * argv[]){
    llvm::InitLLVM init(argc,argv);

    DiagnosticBufferedLogger errStream;
    Syntax::Lexer lex(errStream);
    std::vector<Syntax::Tok> tokenStream;
    std::ifstream in("./build/tests/test.stb");
    if(in.is_open()){
        lex.tokenizeFromIStream(in,tokenStream);
    }
    else {
        std::cout << "Failed to Read File: ./test.stb" << std::endl;
        return 1;
    }
    in.close();
    // if(!errStream.empty()) {
    //     errStream.logAll();
    //     return 1;
    // }
    // else {
        for(auto & t : tokenStream){
            std::cout << llvm::formatv("{0}",t).str() << std::endl;
        };
    // }

    return 0;
};