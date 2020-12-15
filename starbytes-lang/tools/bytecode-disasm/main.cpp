#include "starbytes/Base/Base.h"
#include "starbytes/ByteCode/BCDef.h"
#include <fstream>
#include <optional>

STARBYTES_STD_NAMESPACE

class BCDisasm {
    std::ifstream & input;
    template<typename _T>
    void print_c(_T item){
        std::cout << item << " ";
    };
    void _disam_expr(){
        ByteCode::BC code1;
        input.read((char *)&code1,sizeof(code1));
        print_c(code1);
        if(code1 == CRT_STB_STR){
            ByteCode::BCId id;
            input.read((char*)&id,sizeof(id));
            std::string str;
            ByteCode::bcid_to_cpp_str(id,str);
            print_c(str);
        }
        else if(code1 == CRT_STB_NUM){
            ByteCode::BCId id;
            input.read((char *)&id,sizeof(id));
            std::string str;
            ByteCode::bcid_to_cpp_str(id,str);
            print_c(str);
        };
    };
    public:
    BCDisasm(std::ifstream & _input):input(_input){};
    void disassemble(){
        if(input.is_open()) {
            unsigned idx = 0;
            while(idx < 2) {
                ByteCode::BC code0;
                input.read((char *)&code0,sizeof(code0));
                print_c(code0);
                if(code0 == PROG_END){
                    break;
                }
                else if(code0 == CRTVR){
                    ByteCode::BCId id;
                    input.read((char *)&id,sizeof(id));
                    std::string str;
                    ByteCode::bcid_to_cpp_str(id,str);
                    print_c(str);
                }
                else if(code0 == STVR){
                    ByteCode::BCId id;
                    input.read((char *)&id,sizeof(id));
                    std::string str;
                    ByteCode::bcid_to_cpp_str(id,str);
                    print_c(str);
                    _disam_expr();
                };
                ++idx;
            };
        }
        input.close();
    };
};

NAMESPACE_END

using namespace Starbytes;

std::optional<std::string> file;

Foundation::CommandInput __file{"file","f",[](std::string & input){
    file = input;
}};

int main(int argc,char * argv[]){
    Foundation::parseCmdArgs(argc,argv,{},{&__file},[](){
        std::cout << "Bytecode Disassembler" << std::endl;
    });
    if(file.has_value()){
        std::ifstream in(file.value(),std::ios::in);
        BCDisasm disasm(in);
        disasm.disassemble();
    }
    else
    std::cerr << ERROR_ANSI_ESC << "No input file!\nExiting..." << ANSI_ESC_RESET << std::endl;
};
