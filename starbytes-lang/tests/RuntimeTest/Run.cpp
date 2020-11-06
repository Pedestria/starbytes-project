#include "starbytes/Interpreter/BCReader.h"
#include "starbytes/ByteCode/BCDef.h"

using namespace Starbytes::Interpreter;
using namespace Starbytes::ByteCode;

#ifdef _WIN32
 WINDOWS_CONSOLE_INIT
#endif

BCString * make_bc_string(std::string & value){
    BCString * ptr = new BCString();
    ptr->value = value;
    return ptr;
};

BCCodeBegin * make_bc_code_begin(std::string & method_name){
    BCCodeBegin * ptr = new BCCodeBegin();
    ptr->code_node_name = method_name;
    return ptr;
};
BCCodeEnd * make_bc_code_end(std::string & method_name){
    BCCodeEnd * ptr = new BCCodeEnd();
    ptr->code_node_name = method_name;
    return ptr;
};



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