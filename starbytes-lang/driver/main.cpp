#include <llvm/Support/InitLLVM.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Option/Option.h>
#include <llvm/Support/Program.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/FileSystem.h>

namespace {

llvm::cl::SubCommand compile("compile","");

llvm::cl::SubCommand run("run","");

llvm::cl::opt<std::string> moduleName("module-name",
                                      llvm::cl::desc(""),
                                      llvm::cl::sub(compile));

llvm::cl::opt<std::string> src_dir("src",llvm::cl::desc(""),
                                    llvm::cl::sub(compile));

llvm::cl::opt<bool> single_file("single",llvm::cl::desc(""),
                                    llvm::cl::sub(compile));

llvm::cl::alias s("s",llvm::cl::desc(""),llvm::cl::aliasopt(single_file));


};

int main(int argc,const char *argv[]){
    
    llvm::InitLLVM _llvm(argc,argv);
    
    
    llvm::cl::ParseCommandLineOptions(argc,argv);
    
    
    
    
    
    return 0;
};
