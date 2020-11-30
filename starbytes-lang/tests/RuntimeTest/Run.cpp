#include "starbytes/Interpreter/BCReader.h"

using namespace Starbytes::Interpreter;

#ifdef _WIN32
 WINDOWS_CONSOLE_INIT
#endif


int main(){
    #ifdef _WIN32
        setupConsole(); 
    #endif
    // std::vector<BCUnit *> units;
    // std::string m = "ivkfn";
    // units.push_back(make_bc_code_begin(m));
    // units.push_back(make_bc_string)

    // BCProgram * prog = new BCProgram();
    // prog->program_name = "test";
    // prog->units = units;
    // execBCProgram(prog);
}