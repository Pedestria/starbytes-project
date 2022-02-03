#include "starbytes/Parser/Parser.h"
#include "starbytes/Gen/Gen.h"
#include "starbytes/RT/RTEngine.h"
#include <filesystem>
#include <llvm/Support/InitLLVM.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/FileSystem.h>

int main(int argc,char *argv[]){
    llvm::InitLLVM init(argc,argv);
    

    llvm::cl::ParseCommandLineOptions(argc,argv);

    std::string module_name = "Test";

    std::ofstream out("./test.stbxm",std::ios::out | std::ios::binary);

    auto currentdir = std::filesystem::current_path();

    starbytes::Gen gen;
    auto genContext = starbytes::ModuleGenContext::Create(module_name,out,currentdir);
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
        
        std::ifstream module_in("./test.stbxm",std::ios::in | std::ios::binary);
        auto interp = starbytes::Runtime::Interp::Create();
        if(module_in.is_open()){
            interp->exec(module_in);
        }
        
        return 0;
    };
    
    
};
