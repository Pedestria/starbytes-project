#include "starbytes/Parser/Parser.h"
#include <llvm/Support/InitLLVM.h>
#include <llvm/Support/CommandLine.h>

int main(int argc,char * argv[]){
    llvm::InitLLVM init(argc,argv);
    
    llvm::cl::ParseCommandLineOptions(argc,argv);

    starbytes::Parser parser;
    std::ifstream in("./build/tests/test.stb");
    auto parseContext = starbytes::ModuleParseContext::Create("Test");
    parser.parseFromStream(in,parseContext);
    return 0;
};
