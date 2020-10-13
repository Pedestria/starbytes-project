#include "starbytes/ByteCode/BCEncodeDecodeFactory.h"
#include <sstream>
#include <iostream>

using namespace Starbytes::ByteCode;

int main(){
    std::string test = "PROGRAM Test\nSTART -->\nstore a = crt_fnc_args(\"x\",\"y\")\ncrt_fnc(\"Foo\",a,{\nreturn(ivk_inst_metd(refer_var(\"x\"),\"add\",{refer_var(\"y\")}))})\nivk_fnc(\"Foo\",{&crt_stb_num(1),&crt_stb_num(2)})\n<-- END";
    std::ostringstream result;
    std::ostringstream result2;
    for(auto & c : test){
       result << encodeCharacter(c);
    }
    std::string str = result.str();
    std::cout << "Encoded:" << str << "\n";
    for(auto & c : str){
        result2 << decodeCharacter(c);
    }
    std::cout << "Decoded:" << result2.str();
    
};