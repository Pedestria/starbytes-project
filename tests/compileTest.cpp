#include "starbytes/compiler/Parser.h"
#include "starbytes/compiler/Gen.h"
#include <filesystem>
#include <fstream>

int main(int argc,char *argv[]){
    std::string module_name = "Test";
    std::string output_file = "./module_test_output.stbxm";

    std::ofstream out(output_file,std::ios::out | std::ios::binary);
    auto currentDir = std::filesystem::current_path();

    starbytes::Gen gen;
    auto genContext = starbytes::ModuleGenContext::Create(module_name,out,currentDir);
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
        return 0;
    };
  
};
