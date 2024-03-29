#include "starbytes/Parser/Parser.h"
#include "starbytes/Gen/Gen.h"
#include <filesystem>
#include <llvm/Support/InitLLVM.h>
#include <llvm/Support/FileSystem.h>
// #include <llvm/Support/CommandLine.h>

int main(int argc,char *argv[]){
    llvm::InitLLVM init(argc,argv);
    

    // llvm::cl::ParseCommandLineOptions(argc,argv);

    std::string module_name = "Test";

    std::ofstream out("./test.stbxm",std::ios::out | std::ios::binary);
    auto currentDir = std::filesystem::current_path();

    starbytes::Gen gen;
    auto genContext = starbytes::ModuleGenContext::Create(module_name,out,currentDir);
    gen.setContext(&genContext);

    starbytes::Parser parser(gen);
    std::ifstream in("./test.stb");
    auto parseContext = starbytes::ModuleParseContext::Create(module_name);
    parser.parseFromStream(in,parseContext);
    if(!parser.finish()){
        out.close();
        llvm::sys::fs::remove("./test.stbxm");
        return 1;
    }
    else {
        gen.finish();
        out.close();
        return 0;
    };
  
};
