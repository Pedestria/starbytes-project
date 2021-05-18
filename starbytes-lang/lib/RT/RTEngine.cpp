#include "starbytes/RT/RTEngine.h"
#include "starbytes/RT/RTCode.h"
#include "RTStdlib.h"

#include <llvm/ADT/StringRef.h>
#include <llvm/ADT/StringMap.h>
#include <iostream>

namespace starbytes::Runtime {

class RTAllocator {
    
    llvm::StringMap<llvm::StringMap<RTObject *>> all_var_objects;
    
public:
    llvm::StringRef currentScope;
    void setScope(llvm::StringRef scope_name){
        currentScope = scope_name;
    };
    void allocVariable(llvm::StringRef name,RTObject * obj){
        auto found = all_var_objects.find(currentScope);
        if(found == all_var_objects.end()){
            llvm::StringMap<RTObject *> vars;
            vars.insert(std::make_pair(name,obj));
            all_var_objects.insert(std::make_pair(currentScope,vars));
        }
        else {
            auto & var_map = found->second;
            var_map.insert(std::make_pair(name,obj));
        };
    };
//    void allocVariable(llvm::StringRef name){
//        auto found = all_var_objects.find(currentScope);
//        if(found == all_var_objects.end()){
//            llvm::StringMap<RTObject *> vars;
//            vars.insert(std::make_pair(name,nullptr));
//        }
//        else {
//            auto & var_map = found->second;
//            var_map.insert(std::make_pair(name,nullptr));
//        };
//    };

    RTObject *referenceVariable(llvm::StringRef scope,llvm::StringRef name){
        auto found = all_var_objects.find(scope);
        if(found == all_var_objects.end()){
            // std::cout << "Var NOT Found AND Scope NOT Found:" << name.data() << std::endl;
            return nullptr;
        }
        else {
            auto & var_map = found->getValue();
            for(auto & var : var_map){
                if(var.getKey() == name){
                    // std::cout << "Found Var:" << name.data() << std::endl;
                    return var.getValue();
                    break;
                };
            };
            // std::cout << "Var NOT FOUND:" << name.data() << std::endl;
            return nullptr;
        }
    };
    void clearScope(){
        auto found = all_var_objects.find(currentScope);
        auto map = found->second;
        for(auto & ent : map){
            delete ent.second;
        };
        all_var_objects.erase(found);
    };
    
};


class InterpImpl : public Interp {

    std::unique_ptr<RTAllocator> allocator;
    
    std::vector<RTClassTemplate> classes;
    
    std::vector<RTFuncTemplate> functions;
    
    RTObject * evalExpr(std::istream &in);
    
public:
    InterpImpl():allocator(std::make_unique<RTAllocator>()){
        
    };
    void exec(std::istream &in);
};

RTObject *InterpImpl::evalExpr(std::istream & in){
    RTCode code;
    in.read((char *)&code,sizeof(RTCode));
    switch (code) {
        case CODE_RTOBJCREATE:
        {
            
            break;
        }
        case CODE_RTINTOBJCREATE :
        {
            RTInternalObject object;
            in >> &object;
            return new RTInternalObject(std::move(object));
            break;
        }
        case CODE_RTVAR_REF : {
            RTID var_id;
            in >> &var_id;
            llvm::StringRef var_name (var_id.value,var_id.len);
            return allocator->referenceVariable(allocator->currentScope,var_name);
            break;
        }
        default:
            break;
    }
};

void InterpImpl::exec(std::istream & in){
    std::cout << "Interp Starting" << std::endl;
    RTCode code;
    allocator->setScope("GLOBAL");
    in.read((char *)&code,sizeof(RTCode));
    while(code != CODE_MODULE_END){
        if(code == CODE_RTVAR){
            RTVar var;
            in >> &var;
            llvm::StringRef var_name (var.id.value,var.id.len);
            if(var.hasInitValue){
                auto val = evalExpr(in);
                if(val->isInternal){
                    allocator->allocVariable(var_name,val);
                }
                else {
                    allocator->allocVariable(var_name,val);
                }
            }
        }
        else if(code == CODE_RTIVKFUNC){
            RTID func_id;
            in >> &func_id;
            llvm::StringRef func_name (func_id.value,func_id.len);
            unsigned argCount;
            in.read((char *)&argCount,sizeof(argCount));
            if(func_name == "print"){
                RTObject *object_to_print = evalExpr(in);
                stdlib::print(object_to_print);
            };
        }
        
        in.read((char *)&code,sizeof(RTCode));
    };
};

std::shared_ptr<Interp> Interp::Create(){
    return std::make_shared<InterpImpl>();
};

};
