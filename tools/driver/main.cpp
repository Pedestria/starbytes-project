
#include "starbytes/base/FileExt.def"
#include "starbytes/compiler/Parser.h"
#include "starbytes/compiler/Gen.h"
#include "starbytes/runtime/RTEngine.h"
#include <filesystem>
#include <system_error>
#include <iostream>
#include <sstream>

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
    std::string moduleName;
    std::string script;

    for(int i = 1; i < argc; ++i){
        std::string arg = argv[i];
        if(arg == "compile"){
            continue;
        }
        if(arg == "--modulename" || arg == "-m"){
            if(i + 1 >= argc){
                std::cerr << "Missing value for " << arg << std::endl;
                return 1;
            }
            moduleName = argv[++i];
            continue;
        }
        const std::string moduleNamePrefix = "--modulename=";
        if(arg.rfind(moduleNamePrefix,0) == 0){
            moduleName = arg.substr(moduleNamePrefix.size());
            continue;
        }
        if(!arg.empty() && arg[0] != '-'){
            script = arg;
            continue;
        }
        std::cerr << "Unknown option: " << arg << std::endl;
        return 1;
    }

    if(script.empty()){
        std::cerr << "Usage: starbytes <script." << STARBYTES_SRCFILE_EXT
                  << "> [--modulename <name>]" << std::endl;
        return 1;
    }

    std::filesystem::path scriptPath(script);
    if(!std::filesystem::exists(scriptPath)){
        std::cerr << "Input file not found: " << script << std::endl;
        return 1;
    }

    std::ifstream in(scriptPath,std::ios::in);
    if(!in.is_open()){
        std::cerr << "Failed to open script: " << script << std::endl;
        return 1;
    }

    if(moduleName.empty()){
        moduleName = scriptPath.stem().string();
    }

    std::filesystem::path compileModPath = std::filesystem::current_path() / ".starbytes";
    std::filesystem::create_directories(compileModPath);

    std::ostringstream ss;
    ss << moduleName << '.' << STARBYTES_COMPILEDFILE_EXT;
    std::filesystem::path compiledModulePath = compileModPath / ss.str();

    std::ofstream moduleOut(compiledModulePath,std::ios::out | std::ios::binary);

    starbytes::Gen gen;
    auto moduleGenContext = starbytes::ModuleGenContext::Create(moduleName,moduleOut,compileModPath);
    auto parseContext = starbytes::ModuleParseContext::Create(moduleName);
    gen.setContext(&moduleGenContext);

    starbytes::Parser parser(gen);
    parser.parseFromStream(in,parseContext);
    if(!parser.finish()){
        moduleOut.close();
        std::error_code removeErr;
        std::filesystem::remove(compiledModulePath,removeErr);
        if(removeErr){
            std::cerr << "Warning: failed to remove '" << compiledModulePath
                      << "': " << removeErr.message() << std::endl;
        }
        return 1;
    }

    gen.finish();
    moduleOut.close();

    std::ifstream rtcode_in(compiledModulePath,std::ios::in | std::ios::binary);
    if(!rtcode_in.is_open()){
        std::cerr << "Failed to open compiled module: " << compiledModulePath << std::endl;
        return 1;
    }

    auto interp = starbytes::Runtime::Interp::Create();
    interp->exec(rtcode_in);

    starbytes::stdDiagnosticHandler->logAll();
    
    return 0;
}
