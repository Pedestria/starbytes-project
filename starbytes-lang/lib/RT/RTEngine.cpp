#include "starbytes/RT/RTEngine.h"
#include "starbytes/RT/RTCode.h"

namespace starbytes::Runtime {


class InterpImpl : public Interp {
    
    std::vector<RTClassTemplate> classes;
    
    std::vector<RTFuncTemplate> functions;
    
    void exec(std::istream &in);
};

void Interp::exec(std::istream & in){
    RTCode code;
    in.read((char *)&code,sizeof(RTCode));
    while(code != CODE_MODULE_END){
        
        switch (code) {
            case CODE_RTVAR:{
                
                break;
            }
            case CODE_RTFUNC: {
                
                break;
            }
            case CODE_RTCLASS : {
                
                break;
            }
            default: {
                break;
            }
        }
        
        
        in.read((char *)&code,sizeof(RTCode));
    };
};

std::shared_ptr<Interp> Interp::Create(){
    return std::make_shared<InterpImpl>();
};

};
