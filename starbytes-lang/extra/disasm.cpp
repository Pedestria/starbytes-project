#include <llvm/Support/InitLLVM.h>
#include <llvm/Support/CommandLine.h>

#include "starbytes/RT/RTCode.h"
#include <iostream>
#include <fstream>

namespace starbytes {

using namespace Runtime;

class Disassembler {
    std::istream & in;
    std::ostream & out;
    void _disasm_decl(RTCode & code){
        
    };
    void _disasm_expr(RTCode & code){
        if(code == CODE_RTINTOBJCREATE){
            out << "CODE_RTINITOBJCREATE " << std::flush;
            RTCode obj_type;
            in.read((char *)&obj_type,sizeof(RTCode));
            if(obj_type == RTINTOBJ_STR){
                RTID strVal;
                in >> &strVal;
                out << "RTINTOBJ_STR " << strVal.len << " ";
                out.write(strVal.value,sizeof(char) * strVal.len);
            }
        }
        else if(code == CODE_RTOBJCREATE){
            
        }
        else if (code == CODE_RTVAR_REF){
            RTID id;
            in >> &id;
            out << "CODE_RTVAR_REF " << id.len << " ";
            out.write(id.value,sizeof(char) * id.len);
        }
    };
public:
    Disassembler(std::istream & in,std::ostream &out):in(in),out(out){
       
    };
    void disasm(){
        RTCode code;
        in.read((char *)&code,sizeof(RTCode));
        while (code != CODE_MODULE_END) {
            if(code == CODE_RTVAR) {
                RTVar var;
                in >> &var;
                out << "CODE_RTVAR " << var.id.len << " ";
                out.write(var.id.value,sizeof(char) * var.id.len);
                in.read((char *)&code,sizeof(RTCode));
                _disasm_expr(code);
            }
            else if(code == CODE_RTIVKFUNC){
                RTID func_id;
                in >> &func_id;
                out << "CODE_RTIVKFUNC " << func_id.len << " ";
                out.write(func_id.value,sizeof(char) * func_id.len);
                unsigned arg_count;
                in.read((char *)&arg_count,sizeof(arg_count));
                out << " " << arg_count << " ";
                for(unsigned i = 0;i < arg_count;i++){
                    in.read((char *)&code,sizeof(RTCode));
                    _disasm_expr(code);
                    out << " ";
                };
            };
            out << "\n";
            in.read((char *)&code,sizeof(RTCode));
        }
        out << "CODE_MODULE_END" << std::endl;
    };
};

};

namespace {

llvm::cl::opt<std::string> file("file",llvm::cl::desc("The Starbytes Module to disassemble."));

llvm::cl::alias f("f",llvm::cl::aliasopt(file),llvm::cl::desc("An alias for the option --file"),llvm::cl::NotHidden);

}

int main(int argc,const char *argv[]){
    llvm::InitLLVM _llvm(argc,argv);
        

    auto okay = llvm::cl::ParseCommandLineOptions(argc,argv);

    if(!okay){
        return 1;
    };
    
    std::ifstream in(file,std::ios::in | std::ios::binary);

    if(in.is_open()){

        starbytes::Disassembler disasm(in,std::cout);

        disasm.disasm();

        in.close();

    }
    else {
        llvm::errs() << "Failed to Read File: " << file;
    };

//    return 0;
};
