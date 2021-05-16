#include <llvm/Support/InitLLVM.h>
#include <llvm/Support/CommandLine.h>

#include "starbytes/RT/RTCode.h"
#include <iostream>
#include <fstream>

namespace starbytes {

using namespace Runtime;

class Disassembler {
    std::ifstream & in;
    std::ostream & out;
public:
    Disassembler(std::ifstream & in,std::ostream &out):in(in),out(out){
       
    };
    void disasm(){
        RTCode code;
        in.read((char *)&code,sizeof(RTCode));
        while (code != CODE_MODULE_END) {
        
            switch (code) {
                case CODE_RTVAR : {
                    RTVar var;
                    in >> var;
                    out << "CODE_RTVAR " << var.id.len << " ";
                    out.write(var.id.value,sizeof(char) * var.id.len);
                    break;
                }
                default : {
                    break;
                }
            }

            in.read((char *)&code,sizeof(RTCode));
        };
        out << " CODE_MODULE_END";
    };

};

};


int main(int argc,const char *argv[]){
    llvm::InitLLVM _llvm(argc,argv);

    llvm::cl::opt<std::string> file("file",llvm::cl::desc("The Starbytes Module to disassemble."));

    auto okay = llvm::cl::ParseCommandLineOptions(argc,argv);

    if(!okay){
        return 1;
    };
    
    std::ifstream in(file.getValue(),std::ios::binary);

    if(in.is_open()){

        starbytes::Disassembler disasm(in,std::cout);

        disasm.disasm();

        in.close();

    }
    else {
        llvm::errs() << "Failed to Read File: " << file;
    };

    return 0;
};