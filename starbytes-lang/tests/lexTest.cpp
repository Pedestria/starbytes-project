#include "starbytes/Parser/Parser.h"
#include "starbytes/Syntax/Lexer.h"
#include <fstream>
#include <iostream>
#include <llvm/Support/InitLLVM.h>
#include <llvm/Support/FormatVariadic.h>

//using namespace starbytes;

int main(int argc,char * argv[]){
    
    llvm::InitLLVM init(argc,argv);

    starbytes::DiagnosticBufferedLogger errStream;
    starbytes::Syntax::Lexer lex(errStream);
    std::vector<starbytes::Syntax::Tok> tokenStream;
    std::ifstream in("./build/tests/test.stb");
    starbytes::CodeViewSrc src;
    if(in.is_open()){
        lex.tokenizeFromIStream(in,tokenStream,src);
    }
    else {
        std::cout << "Failed to Read File: ./test.stb" << std::endl;
        return 1;
    }
    in.close();
    std::cout << "CODE:" << src.code << std::endl;
        for(auto & t : tokenStream){
            std::cout << t.content << std::endl;
//            std::cout << llvm::formatv("{0}",t).str() << std::endl;
//
        };
    // }

    return 0;
};
