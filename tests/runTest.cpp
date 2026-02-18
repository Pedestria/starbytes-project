#include "starbytes/compiler/Parser.h"
#include "starbytes/compiler/Gen.h"
#include "starbytes/runtime/RTEngine.h"
#include <filesystem>
#include <fstream>

int main(int argc,char *argv[]){
    std::string module_name = "Test";
    std::string output_file = "./module_test_output.stbxm";

    std::ofstream out(output_file,std::ios::out | std::ios::binary);

    auto currentdir = std::filesystem::current_path();

    starbytes::Gen gen;
    auto genContext = starbytes::ModuleGenContext::Create(module_name,out,currentdir);
    gen.setContext(&genContext);

    starbytes::Parser parser(gen);
    std::ifstream in("./test.starb");
    if(!in.is_open()){
        in.open("./tests/test.starb");
    }
    if(!in.is_open()){
        out.close();
        return 1;
    }
    auto parseContext = starbytes::ModuleParseContext::Create(module_name);
    parser.parseFromStream(in,parseContext);

    if(!parser.finish()){
        out.close();
        std::filesystem::remove(output_file);
        return 1;
    }
    else {
        gen.finish();
        out.close();
        
        std::ifstream module_in(output_file,std::ios::in | std::ios::binary);
        auto interp = starbytes::Runtime::Interp::Create();
        if(module_in.is_open()){
            interp->exec(module_in);
        }
        
        return 0;
    };
    
    
};
