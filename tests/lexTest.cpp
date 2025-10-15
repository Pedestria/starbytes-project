#include "starbytes/compiler/Parser.h"
#include "starbytes/compiler/Lexer.h"
#include <fstream>
#include <iostream>

//using namespace starbytes;

int main(int argc,char * argv[]){
    
   
    auto & errStream = *starbytes::stdDiagnosticHandler;
    starbytes::Syntax::Lexer lex(errStream);
    std::vector<starbytes::Syntax::Tok> tokenStream;
    std::ifstream in("./tests/test.stb");
    // starbytes::Document src;
    if(in.is_open()){
        lex.tokenizeFromIStream(in,tokenStream);
    }
    else {
        std::cout << "Failed to Read File: ./test.stb" << std::endl;
        return 1;
    }
    in.close();
        std::cout << "CODE:" << src.code << std::endl;
        for(auto & t : tokenStream){
            std::cout << int(t.type) << std::endl;
            std::cout << t.content << std::endl;
//            std::cout << llvm::formatv("{0}",t).str() << std::endl;
//
        };
    // }

    return 0;
};
