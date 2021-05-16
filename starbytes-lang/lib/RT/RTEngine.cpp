#include "starbytes/RT/RTEngine.h"
#include "starbytes/RT/RTCode.h"

#include <llvm/ADT/StringRef.h>
#include <llvm/ADT/StringMap.h>

namespace starbytes::Runtime {

class RTAllocator {
    llvm::StringMap<llvm::StringMap<RTObject *>> all_var_objects;
    void swtichScope(llvm::StringRef scope_name);
    RTObject * allocVariable(llvm::StringRef name,RTObject *obj);
    void clearScope();
};


class InterpImpl : public Interp {

    std::unique_ptr<RTAllocator> allocator;
    
    std::vector<RTClassTemplate> classes;
    
    std::vector<RTFuncTemplate> functions;
    
    void exec(std::istream &in);
};

void InterpImpl::exec(std::istream & in){
    RTCode code;
    in.read((char *)&code,sizeof(RTCode));
    while(code != CODE_MODULE_END){
        
        switch (code) {
            case CODE_RTVAR:{
                
                break;
            }
            case CODE_RTIVKFUNC : {
                break;
            }
            case CODE_RTFUNC: {
                RTFuncTemplate temp;
                in >> temp;
                functions.push_back(std::move(temp));
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
