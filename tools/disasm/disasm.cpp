

#include "starbytes/AST/ASTNodes.def"
#include "starbytes/RT/RTCode.h"
#include "starbytes/Base/ADT.h"
#include <iostream>
#include <fstream>
#include "starbytes/Base/Diagnostic.h"

namespace starbytes {

using namespace Runtime;

class Disassembler {
    std::istream & in;
    std::ostream & out;
    void _disasm_decl(RTCode & code){
        if(code == CODE_RTVAR) {
            RTVar var;
            in >> &var;
            out << "CODE_RTVAR " << var.id.len << " ";
            out.write(var.id.value,sizeof(char) * var.id.len);
            in.read((char *)&code,sizeof(RTCode));
            out << " ";
            _disasm_expr(code);
        }
        else if(code == CODE_RTFUNC){
            RTFuncTemplate funcTemp;
            in >> &funcTemp;
            out << "CODE_RTFUNC " << funcTemp.name.len << " ";
            out.write(funcTemp.name.value,sizeof(char) * funcTemp.name.len);
            out << " ";
            out << funcTemp.argsTemplate.size() << " ";
            for(auto arg : funcTemp.argsTemplate){
                string_ref arg_name (arg.value,arg.len);
                out << arg_name << " ";
            };
            out << std::endl;
            in.seekg(funcTemp.block_start_pos);
            RTCode code2;
            in.read((char *)&code2,sizeof(RTCode));
            out << "CODE_RTFUNCBLOCK_BEGIN" << std::endl;
            while(code2 != CODE_RTFUNCBLOCK_END){
//                _disasm_decl(code2);
                _disasm_expr(code2);
                in.read((char *)&code2,sizeof(RTCode));
                out << std::endl;
            };
            out << "CODE_RTFUNCBLOCK_END";
        }
    };
    void _disasm_expr(RTCode & code){
        if(code == CODE_RTINTOBJCREATE){
            out << "CODE_RTINTOBJCREATE " << std::flush;
            RTCode obj_type;
            in.read((char *)&obj_type,sizeof(RTCode));
            if(obj_type == RTINTOBJ_STR){
                RTID strVal;
                in >> &strVal;
                out << "RTINTOBJ_STR " << strVal.len << " ";
                out.write(strVal.value,sizeof(char) * strVal.len);
            }
            else if(obj_type == RTINTOBJ_BOOL){
                bool val;
                in.read((char *)&val,sizeof(bool));
                out << "RTINTOBJ_BOOL " << std::boolalpha << val << std::noboolalpha;
            }
            else if(obj_type == RTINTOBJ_ARRAY){
                unsigned size;
                in.read((char *)&size,sizeof(size));
                out << "RTINTOBJ_ARRAY " << size << " ";
                for(unsigned i = 0;i < size;i++){
                    RTCode code;
                    in.read((char *)&code,sizeof(RTCode));
                    _disasm_expr(code);
                    out << " ";
                };
            }
            else if(obj_type == RTINTOBJ_NUM){
                starbytes_float_t num;
                in.read((char *)&num,sizeof(num));
                out << "RTINTOBJ_NUM " << num << " ";
            }
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
        }
        else if(code == CODE_RTOBJCREATE){
            
        }
        else if (code == CODE_RTVAR_REF){
            RTID id;
            in >> &id;
            out << "CODE_RTVAR_REF " << id.len << " ";
            out.write(id.value,sizeof(char) * id.len);
        }
        else if(code == CODE_CONDITIONAL){
            unsigned count;
            in.read((char *)&count,sizeof(count));
            out << "CODE_CONDITIONAL " << count << std::endl;
            while(count > 0) {
                RTCode code2;
                in.read((char *)&code2,sizeof(RTCode));
                if(code2 == COND_TYPE_IF){
                    out << "COND_TYPE_IF " << std::endl;
                    in.read((char *)&code2,sizeof(RTCode));
                    _disasm_expr(code2);
                    out << " ";
                }
                else if(code2 == COND_TYPE_ELSE){
                    out << "COND_TYPE_ELSE ";
                };


                in.read((char *)&code2,sizeof(RTCode));
                out << "CODE_RTBLOCK_BEGIN" << std::endl;
                while(code2 != CODE_RTBLOCK_END){
    //                _disasm_decl(code2);
                    _disasm_expr(code2);
                    in.read((char *)&code2,sizeof(RTCode));
                    out << std::endl;
                };
                out << "CODE_RTBLOCK_END" << std::endl;
                --count;
            }
            in.read((char *)&code,sizeof(RTCode));
            out << "CODE_CONDITIONAL_END " << std::endl;
        };
    };
public:
    Disassembler(std::istream & in,std::ostream &out):in(in),out(out){
       
    };
    void disasm(){
        RTCode code;
        in.read((char *)&code,sizeof(RTCode));
        while (code != CODE_MODULE_END) {
            if(code == CODE_RTIVKFUNC || code == CODE_CONDITIONAL){
                _disasm_expr(code);
            }
            else {
                _disasm_decl(code);
            }
            out << std::endl;
            in.read((char *)&code,sizeof(RTCode));
        }
        out << "CODE_MODULE_END" << std::endl;
    };
};

};

// namespace {

// llvm::cl::opt<std::string> file("file",llvm::cl::desc("The Starbytes Module to disassemble."));

// llvm::cl::alias f("f",llvm::cl::aliasopt(file),llvm::cl::desc("An alias for the option --file"),llvm::cl::NotHidden);

};

int main(int argc,const char *argv[]){
    // llvm::InitLLVM _llvm(argc,argv);
        

    // auto okay = llvm::cl::ParseCommandLineOptions(argc,argv);

    if(!okay){
        return 1;
    };

    auto error_engine = starbytes::DiagnosticHandler::createDefault(std::cout);

    std::string file;
    
    std::ifstream in(file,std::ios::in | std::ios::binary);

    if(in.is_open()){

        starbytes::Disassembler disasm(in,std::cout);

        disasm.disasm();

        in.close();

    }
    else {
        error_engine->push(starbytes::StandardDiagnostic::createError("Failed to read file"));
    };

    if(error_engine->hasErrored()){
        error_engine->logAll();
        return 1;
    }

   return 0;
};
