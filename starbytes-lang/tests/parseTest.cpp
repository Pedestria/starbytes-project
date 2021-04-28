#include "starbytes/Parser/Parser.h"
#include <llvm/Support/InitLLVM.h>

int main(int argc,char * argv[]){
    llvm::InitLLVM init(argc,argv);

    starbytes::Parser parser;
    std::ifstream in("./build/tests/test.stb");
    starbytes::ModuleParseContext parseContext;
    parseContext.table = std::make_unique<starbytes::Semantics::SymbolTable>();

    parser.parseFromStream(in,parseContext);
    
};