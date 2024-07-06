
#include "starbytes/Base/FileExt.def"
#include "starbytes/Base/CmdLine.h"

#include "starbytes/Parser/Parser.h"
#include "starbytes/Gen/Gen.h"
#include "starbytes/RT/RTEngine.h"
#include <filesystem>

namespace starbytes {
    class Driver {

    };
}



// auto desc = (Twine("<input script .") + STARBYTES_SRCFILE_EXT + ">").str();

// llvm::cl::SubCommand compile("compile","");

// llvm::cl::SubCommand run("run","");

// llvm::cl::opt<std::string> script(llvm::cl::Positional,
//                                   llvm::cl::desc(desc),
//                                   llvm::cl::Required,
//                                   llvm::cl::sub(run));


// llvm::cl::opt<std::string> moduleName("module-name",
//                                       llvm::cl::desc(""),
//                                       llvm::cl::sub(compile));

// llvm::cl::opt<std::string> src_dir("src",llvm::cl::desc(""),
//                                     llvm::cl::sub(compile));

// llvm::cl::opt<bool> single_file("single",llvm::cl::desc(""),
//                                     llvm::cl::sub(compile));

// llvm::cl::alias s("s",llvm::cl::desc(""),llvm::cl::aliasopt(single_file));




int main(int argc,const char *argv[]){

    std::string s;

    auto m = starbytes::cl::flag("modulename",'m').mapVar(s);

    auto compileCmd = starbytes::cl::command("compile");
    
    auto run = true;

    std::string script;
    
    if(run){
        std::ifstream in(script,std::ios::in);


        auto script_name = std::filesystem::path(script).filename().string();

        auto compile_mod_path = std::filesystem::current_path() += std::filesystem::path("/.starbytes");

        auto code = std::filesystem::create_directory(compile_mod_path);

        starbytes::Gen gen;
       


        std::ofstream module_out(("./.starbytes/" + script_name + "." + STARBYTES_COMPILEDFILE_EXT).str(),std::ios::out | std::ios::binary);

        auto moduleGenContext = starbytes::ModuleGenContext::Create(script_name,module_out,compile_mod_path);

        auto parseContext = starbytes::ModuleParseContext::Create(script_name);
        
        gen.setContext(&moduleGenContext);

        starbytes::Parser parser(gen);

        parser.parseFromStream(in,parseContext);
        if(!parser.finish()){
            module_out.close();
            std::filesystem::remove("./.starbytes/" + script_name + "." + STARBYTES_COMPILEDFILE_EXT);
            return -1;
        }
        else {
            gen.finish();
            module_out.close();

            std::ifstream rtcode_in("./.starbytes/" + script_name + "." + STARBYTES_COMPILEDFILE_EXT,std::ios::in | std::ios::binary);

            auto interp = starbytes::Runtime::Interp::Create();
            interp->exec(rtcode_in);
        };
    };
    
    
    return 0;
}
