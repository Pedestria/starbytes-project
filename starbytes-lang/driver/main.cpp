#include <llvm/Support/InitLLVM.h>
#include <llvm/Support/CommandLine.h>

int main(int argc,const char *argv[]){
    llvm::InitLLVM _llvm(argc,argv);

    llvm::cl::opt<bool> opt("hello",llvm::cl::desc("A random Opt"));

    llvm::cl::AddLiteralOption(opt,"hello");
    
    llvm::cl::ParseCommandLineOptions(argc,argv);

    return 0;
};