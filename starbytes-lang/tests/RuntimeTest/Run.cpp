#include "starbytes/Interpreter/BCReader.h"
#include "starbytes/Core/Core.h"
#include "starbytes/Gen/Gen.h"
#include "starbytes/Base/Optional.h"
#include "starbytes/AST/TreePrinter.h"
#include <fstream>

using namespace Starbytes;

#ifdef _WIN32
 WINDOWS_CONSOLE_INIT
#endif

struct Input {
    Foundation::Optional<std::string> file;
} input;

Foundation::CommandInput test_file {"file","f",[](std::string & _input){
    input.file = _input;
}};

int main(int argc,char *argv[]){
    std::cout << argc << std::endl;
    #ifdef _WIN32
        setupConsole(); 
    #endif
    Foundation::parseCmdArgs(argc,argv,{},{&test_file},[](){
        std::cout << "Runtime Test!" << std::endl;
    });
    if(input.file.hasVal()) {
        std::string * fileBuf = Foundation::readFile(input.file.value());
        AbstractSyntaxTree * tree = parseCode(*fileBuf);
         TreePrinter().print(tree);
       std::ofstream out("./test.stbxm");
       std::vector<AbstractSyntaxTree *> srcs;
       srcs.push_back(tree);
       CodeGen::generateToBCProgram(srcs,out);
    }
    else {
        std::cerr << ERROR_ANSI_ESC << "No input file!\nExiting..." << ANSI_ESC_RESET << std::endl;
        exit(1);
    }
    // std::vector<BCUnit *> units;
    // std::string m = "ivkfn";
    // units.push_back(make_bc_code_begin(m));
    // units.push_back(make_bc_string)

    // BCProgram * prog = new BCProgram();
    // prog->program_name = "test";
    // prog->units = units;
    // execBCProgram(prog);
    return 0;
}
