#include "starbytes/compiler/Parser.h"
#include "starbytes/compiler/ASTDumper.h"
#include <fstream>

int main(int argc,char * argv[]){
    auto consumer = starbytes::ASTDumper::CreateStdoutASTDumper();
    starbytes::Parser parser(*consumer);
    std::ifstream in("./test.starb");
    if(!in.is_open()){
        in.open("./tests/test.starb");
    }
    if(!in.is_open()){
        return 1;
    }
    auto parseContext = starbytes::ModuleParseContext::Create("Test");
    parser.parseFromStream(in,parseContext);
    if(!parser.finish()){
        return 1;
    };
    return 0;
}
