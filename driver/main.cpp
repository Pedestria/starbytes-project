#include <llvm/Support/InitLLVM.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/Program.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/FileSystem.h>
#include "starbytes/Base/FileExt.def"

#include "starbytes/Parser/Parser.h"
#include "starbytes/Gen/Gen.h"
#include "starbytes/RT/RTEngine.h"
#include "llvm/Support/Errc.h"

namespace starbytes {
    class Driver {

    };
}

using namespace llvm;
auto desc = (llvm::Twine("<input script ") + STARBYTES_SRCFILE_EXT + ">").str();
namespace {

llvm::cl::SubCommand compile("compile","");

llvm::cl::SubCommand run("run","");

llvm::cl::opt<std::string> script(llvm::cl::Positional,
                                  llvm::cl::desc(desc),
                                  llvm::cl::Required,
                                  llvm::cl::sub(run));


llvm::cl::opt<std::string> moduleName("module-name",
                                      llvm::cl::desc(""),
                                      llvm::cl::sub(compile));

llvm::cl::opt<std::string> src_dir("src",llvm::cl::desc(""),
                                    llvm::cl::sub(compile));

llvm::cl::opt<bool> single_file("single",llvm::cl::desc(""),
                                    llvm::cl::sub(compile));

llvm::cl::alias s("s",llvm::cl::desc(""),llvm::cl::aliasopt(single_file));


}



int main(int argc,const char *argv[]){
    
    llvm::InitLLVM _llvm(argc,argv);
    
    llvm::cl::ParseCommandLineOptions(argc,argv);
    
    if(run){
        std::ifstream in(script,std::ios::in);


        auto script_name = llvm::sys::path::filename(script);

        auto code = llvm::sys::fs::create_directory("./.starbytes");

        starbytes::Gen gen;
       


        std::ofstream module_out(("./.starbytes/" + script_name + "." + STARBYTES_COMPILEDFILE_EXT).str(),std::ios::out | std::ios::binary);

        auto moduleGenContext = starbytes::ModuleGenContext::Create(script_name.str(),module_out);

        auto parseContext = starbytes::ModuleParseContext::Create(script_name.str());
        
        gen.setContext(&moduleGenContext);

        starbytes::Parser parser(gen);

        parser.parseFromStream(in,parseContext);
        if(!parser.finish()){
            module_out.close();
            llvm::sys::fs::remove("./.starbytes/" + script_name + "." + STARBYTES_COMPILEDFILE_EXT);
            return -1;
        }
        else {
            gen.finish();
            module_out.close();

            std::ifstream rtcode_in(("./.starbytes/" + script_name + "." + STARBYTES_COMPILEDFILE_EXT).str(),std::ios::in | std::ios::binary);

            auto interp = starbytes::Runtime::Interp::Create();
            interp->exec(rtcode_in);
        };
    };
    
    
    return 0;
}
