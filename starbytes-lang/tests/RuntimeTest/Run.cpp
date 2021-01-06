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

using namespace Foundation;

CommandLine::CommandInput test_file {"file","f",CommandLine::FlagDescription("A file to test!"),[](std::string & _input){
    input.file = _input;
}};

int main(int argc,char *argv[]){
    std::cout << argc << std::endl;
    #ifdef _WIN32
        setupConsole(); 
    #endif
    CommandLine::parseCmdArgs(argc,argv,{},{&test_file},"Full Compile Test!");
    if(input.file.hasVal()) {
        std::string * fileBuf = Foundation::readFile(input.file.value());
        
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
