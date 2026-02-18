#include "starbytes/compiler/RTCode.h"
#include "starbytes/base/ADT.h"

namespace starbytes::Runtime {


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
        else if(code == CODE_RTSECURE_DECL){
            RTID targetVar;
            in >> &targetVar;
            out << "CODE_RTSECURE_DECL " << targetVar.len << " ";
            out.write(targetVar.value,sizeof(char) * targetVar.len);
            bool hasCatchBinding = false;
            in.read((char *)&hasCatchBinding,sizeof(hasCatchBinding));
            out << " " << std::boolalpha << hasCatchBinding << std::noboolalpha;
            if(hasCatchBinding){
                RTID catchBinding;
                in >> &catchBinding;
                out << " " << catchBinding.len << " ";
                out.write(catchBinding.value,sizeof(char) * catchBinding.len);
            }
            bool hasCatchType = false;
            in.read((char *)&hasCatchType,sizeof(hasCatchType));
            out << " " << std::boolalpha << hasCatchType << std::noboolalpha;
            if(hasCatchType){
                RTID catchType;
                in >> &catchType;
                out << " " << catchType.len << " ";
                out.write(catchType.value,sizeof(char) * catchType.len);
            }
            in.read((char *)&code,sizeof(RTCode));
            out << " ";
            _disasm_expr(code);
            in.read((char *)&code,sizeof(RTCode));
            out << " CODE_RTBLOCK_BEGIN" << std::endl;
            while(code != CODE_RTBLOCK_END){
                _disasm_expr(code);
                in.read((char *)&code,sizeof(RTCode));
                out << std::endl;
            }
            out << "CODE_RTBLOCK_END";
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
            out << "CODE_RTIVKFUNC ";
            RTCode calleeCode = CODE_MODULE_END;
            in.read((char *)&calleeCode,sizeof(calleeCode));
            _disasm_expr(calleeCode);
            unsigned arg_count;
            in.read((char *)&arg_count,sizeof(arg_count));
            out << " " << arg_count << " ";
            for(unsigned i = 0;i < arg_count;i++){
                in.read((char *)&code,sizeof(RTCode));
                _disasm_expr(code);
                out << " ";
            };
        }
        else if(code == CODE_UNARY_OPERATOR){
            RTCode unaryCode = UNARY_OP_NOT;
            in.read((char *)&unaryCode,sizeof(unaryCode));
            out << "CODE_UNARY_OPERATOR " << (unsigned)unaryCode << " ";
            in.read((char *)&code,sizeof(RTCode));
            _disasm_expr(code);
        }
        else if(code == CODE_BINARY_OPERATOR){
            RTCode binaryCode = BINARY_OP_PLUS;
            in.read((char *)&binaryCode,sizeof(binaryCode));
            out << "CODE_BINARY_OPERATOR " << (unsigned)binaryCode << " ";
            in.read((char *)&code,sizeof(RTCode));
            _disasm_expr(code);
            out << " ";
            in.read((char *)&code,sizeof(RTCode));
            _disasm_expr(code);
        }
        else if(code == CODE_RTOBJCREATE){
            
        }
        else if(code == CODE_RTREGEX_LITERAL){
            RTID patternId;
            RTID flagsId;
            in >> &patternId;
            in >> &flagsId;
            out << "CODE_RTREGEX_LITERAL " << patternId.len << " ";
            out.write(patternId.value,sizeof(char) * patternId.len);
            out << " " << flagsId.len << " ";
            out.write(flagsId.value,sizeof(char) * flagsId.len);
        }
        else if (code == CODE_RTVAR_REF){
            RTID id;
            in >> &id;
            out << "CODE_RTVAR_REF " << id.len << " ";
            out.write(id.value,sizeof(char) * id.len);
        }
        else if(code == CODE_RTVAR_SET){
            RTID id;
            in >> &id;
            out << "CODE_RTVAR_SET " << id.len << " ";
            out.write(id.value,sizeof(char) * id.len);
            out << " ";
            in.read((char *)&code,sizeof(RTCode));
            _disasm_expr(code);
        }
        else if(code == CODE_RTINDEX_GET){
            out << "CODE_RTINDEX_GET ";
            in.read((char *)&code,sizeof(RTCode));
            _disasm_expr(code);
            out << " ";
            in.read((char *)&code,sizeof(RTCode));
            _disasm_expr(code);
        }
        else if(code == CODE_RTINDEX_SET){
            out << "CODE_RTINDEX_SET ";
            in.read((char *)&code,sizeof(RTCode));
            _disasm_expr(code);
            out << " ";
            in.read((char *)&code,sizeof(RTCode));
            _disasm_expr(code);
            out << " ";
            in.read((char *)&code,sizeof(RTCode));
            _disasm_expr(code);
        }
        else if(code == CODE_RTDICT_LITERAL){
            unsigned pairCount = 0;
            in.read((char *)&pairCount,sizeof(pairCount));
            out << "CODE_RTDICT_LITERAL " << pairCount << " ";
            while(pairCount > 0){
                in.read((char *)&code,sizeof(RTCode));
                _disasm_expr(code);
                out << " ";
                in.read((char *)&code,sizeof(RTCode));
                _disasm_expr(code);
                out << " ";
                --pairCount;
            }
        }
        else if(code == CODE_RTTYPECHECK){
            out << "CODE_RTTYPECHECK ";
            in.read((char *)&code,sizeof(RTCode));
            _disasm_expr(code);
            RTID typeId;
            in >> &typeId;
            out << " " << typeId.len << " ";
            out.write(typeId.value,sizeof(char) * typeId.len);
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
            if(code == CODE_RTIVKFUNC || code == CODE_CONDITIONAL || code == CODE_RTREGEX_LITERAL
               || code == CODE_UNARY_OPERATOR || code == CODE_BINARY_OPERATOR
               || code == CODE_RTVAR_SET || code == CODE_RTINDEX_GET || code == CODE_RTINDEX_SET
               || code == CODE_RTDICT_LITERAL || code == CODE_RTTYPECHECK){
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

void runDisasm(std::istream & in,std::ostream &out){
    Disassembler(in,out).disasm();
}



};
