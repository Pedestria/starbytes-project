#include "starbytes/RT/RTEngine.h"
#include "starbytes/RT/RTCode.h"
#include "RTStdlib.h"

#include <llvm/ADT/StringRef.h>
#include <llvm/ADT/StringMap.h>
#include <iostream>
#include <iomanip>
#include <cstring>

#include "starbytes/interop.h"

namespace starbytes::Runtime {

struct RTScope {
    std::string name;
    RTScope *parent = nullptr;
};

RTScope *RTSCOPE_GLOBAL = new RTScope({"__GLOBAL__"});



class RTAllocator {
    
    llvm::StringMap<llvm::StringMap<StarbytesObject>> all_var_objects;
    
public:
    llvm::StringRef currentScope;
    void setScope(llvm::StringRef scope_name){
        currentScope = scope_name;
    };
    void allocVariable(llvm::StringRef name,StarbytesObject obj){
        auto found = all_var_objects.find(currentScope);
        if(found == all_var_objects.end()){
            llvm::StringMap<StarbytesObject> vars;
            vars.insert(std::make_pair(name,obj));
            all_var_objects.insert(std::make_pair(currentScope,std::move(vars)));
        }
        else {
            auto & var_map = found->second;
            var_map.insert(std::make_pair(name,obj));
        };
    };
    void allocVariable(llvm::StringRef name,StarbytesObject obj,llvm::StringRef scope){
        auto found = all_var_objects.find(scope);
        if(found == all_var_objects.end()){
            llvm::StringMap<StarbytesObject> vars;
            vars.insert(std::make_pair(name,obj));
            all_var_objects.insert(std::make_pair(scope,std::move(vars)));
        }
        else {
            auto & var_map = found->second;
            var_map.insert(std::make_pair(name,obj));
        };
    };

    StarbytesObject referenceVariable(llvm::StringRef scope,llvm::StringRef name){
        auto found = all_var_objects.find(scope);
        if(found == all_var_objects.end()){
//             std::cout << "Var NOT Found AND Scope NOT Found:" << name.data() << std::endl;
            return nullptr;
        }
        else {
            auto & var_map = found->getValue();
            for(auto & var : var_map){
                if(var.getKey() == name){
                    /// Increase Reference count upon variable reference.
                    StarbytesObjectReference(var.getValue());
//                     std::cout << "Found Var:" << name.data() << std::endl;
                    return var.getValue();
                    break;
                };
            };
//             std::cout << "Var NOT FOUND:" << name.data() << std::endl;
            return nullptr;
        }
    };
    void clearScope(){
        auto found = all_var_objects.find(currentScope);
        if(found != all_var_objects.end()){
            auto & map = found->second;
            for(auto & ent : map){
                StarbytesObjectRelease(ent.second);
            };
            all_var_objects.erase(found);
        };
    };
};

StarbytesNativeModule * starbytes_native_mod_load(llvm::StringRef path);
StarbytesFuncCallback starbytes_native_mod_load_function(StarbytesNativeModule * mod,llvm::StringRef name);
void starbytes_native_mod_close(StarbytesNativeModule * mod);

class InterpImpl final : public Interp {

   void addExtension(llvm::StringRef path);

    std::unique_ptr<RTAllocator> allocator;
    
    std::vector<RTClass> classes;
    
    std::vector<RTFuncTemplate> functions;
    
    void execNorm(RTCode &code,std::istream &in,bool * willReturn,StarbytesObject *return_val);
    
    StarbytesObject invokeFunc(std::istream &in,RTFuncTemplate *func_temp,unsigned argCount);
    
    StarbytesObject evalExpr(std::istream &in);
    
public:
    InterpImpl():allocator(std::make_unique<RTAllocator>()){
#define STARBYTES_BUILTIN_ARG(name) {strlen(name),name}
#define STARBYTES_BUILTIN(name,args) functions.push_back({{strlen(name),name},0,args,NULL})
        STARBYTES_BUILTIN("print",{STARBYTES_BUILTIN_ARG("object")});
        
    };
    ~InterpImpl() override = default;
    void exec(std::istream &in) override;
};


StarbytesObject InterpImpl::invokeFunc(std::istream & in,RTFuncTemplate *func_temp,unsigned argCount){
    llvm::StringRef parentScope = allocator->currentScope;
    llvm::StringRef func_name = {func_temp->name.value,func_temp->name.len};
    
    if(func_name == "print"){
        auto object_to_print = evalExpr(in);
        stdlib::print(object_to_print);
        StarbytesObjectRelease(object_to_print);
        return nullptr;
    }
    else {
    
        std::string funcScope = func_name.str() + std::to_string(func_temp->invocations);
                
        auto func_param_it = func_temp->argsTemplate.begin();
        while(argCount > 0){
            StarbytesObject obj = evalExpr(in);
            allocator->allocVariable(llvm::StringRef(func_param_it->value,func_param_it->len),obj,funcScope);
            ++func_param_it;
            --argCount;
        };
        allocator->setScope(funcScope);
        auto prev_pos = in.tellg();
        auto current_pos = func_temp->block_start_pos;
        /// Invoke
        in.seekg(current_pos);
        
        func_temp->invocations += 1;
                
        RTCode code;
        in.read((char *)&code,sizeof(RTCode));
        bool willReturn = false;
        StarbytesObject return_val = nullptr;
        while(code != CODE_RTFUNCBLOCK_END){
            if(!willReturn)
                execNorm(code,in,&willReturn,&return_val);
            in.read((char *)&code,sizeof(RTCode));
        };

     
        in.seekg(prev_pos);
        
        allocator->clearScope();
        allocator->setScope(parentScope);
        
        return return_val;
        
    }
    
}

StarbytesObject InterpImpl::evalExpr(std::istream & in){
    RTCode code;
    in.read((char *)&code,sizeof(RTCode));
    switch (code) {
        case CODE_RTOBJCREATE:
        {
            
            break;
        }
        case CODE_RTINTOBJCREATE :
        {
            StarbytesObject object;
            in >> &object;
            return object;
            break;
        }
        case CODE_RTVAR_REF : {
            RTID var_id;
            in >> &var_id;
            llvm::StringRef var_name (var_id.value,var_id.len);
            return allocator->referenceVariable(allocator->currentScope,var_name);
            
            break;
        }
        case CODE_RTFUNC_REF : {
            RTID var_id;
            in >> &var_id;
            llvm::StringRef func_name (var_id.value,var_id.len);
            for(auto & funcTemp : functions){
                if(func_name == llvm::StringRef(funcTemp.name.value,funcTemp.name.len)){
                    return StarbytesFuncRefNew(&funcTemp);
                };
            };
            
            break;
        }
        case CODE_RTIVKFUNC : {
            auto funcRefObject = (StarbytesFuncRef)evalExpr(in);
            unsigned argCount;
            in.read((char *)&argCount,sizeof(argCount));
            return invokeFunc(in,StarbytesFuncRefGetPtr(funcRefObject),argCount);
            
            break;
        }
        default:
            break;
    }
    return nullptr;
}

void InterpImpl::execNorm(RTCode &code,std::istream &in,bool * willReturn,StarbytesObject *return_val){
    if(code == CODE_RTVAR){
        RTVar var;
        in >> &var;
        llvm::StringRef var_name (var.id.value,var.id.len);
//            std::cout << std::boolalpha << var.hasInitValue << std::endl;
//            std::cout << var_name.data() << std::endl;
        if(var.hasInitValue){
            auto val = evalExpr(in);
            allocator->allocVariable(var_name,val);
        }
    }
    else if(code == CODE_CONDITIONAL){
        unsigned cond_spec_count;
        in.read((char *)&cond_spec_count,sizeof(cond_spec_count));
        while(cond_spec_count > 0){
            RTCode spec_ty;
            in.read((char *)&spec_ty,sizeof(RTCode));
            if(spec_ty == COND_TYPE_IF){
                auto cond = evalExpr(in);
                assert(StarbytesObjectTypecheck(cond,StarbytesBoolType()));
                /// SemanticA Checks if Cond is a Boolean
                auto boolVal = (bool)StarbytesBoolValue(cond);
                // If true, eval Block
                if(boolVal){
                    RTCode code;
                    in.read((char *)&code,sizeof(RTCode));
                    while(code != CODE_RTBLOCK_END){
                        if(!*willReturn)
                            execNorm(code,in,willReturn,return_val);
                        in.read((char *)&code,sizeof(RTCode));
                    };

                    while(code != CODE_CONDITIONAL_END)
                        in.read((char *)&code,sizeof(RTCode));
                    break;
                }
                ///Else Skip to next Conditional
                else 
                    while(code != CODE_RTBLOCK_END)
                        in.read((char *)&code,sizeof(RTCode));

            }
            else if(spec_ty == COND_TYPE_ELSE){
                RTCode code;
                in.read((char *)&code,sizeof(RTCode));
                while(code != CODE_RTBLOCK_END){
                    if(!*willReturn)
                        execNorm(code,in,willReturn,return_val);
                    in.read((char *)&code,sizeof(RTCode));
                };

                while(code != CODE_CONDITIONAL_END)
                    in.read((char *)&code,sizeof(RTCode));
                break;
            };
            --cond_spec_count;
        };
    }
    else if(code == CODE_RTRETURN){
        bool hasValue;
        in.read((char *)&hasValue,sizeof(hasValue));
        if(hasValue) {
            auto object = evalExpr(in);
            *return_val = object;
        }
        else {
            *return_val = nullptr;
        }
        *willReturn = true;
         
    }
    else if(code == CODE_RTFUNC){
        RTFuncTemplate funcTemp;
        in >> &funcTemp;
        functions.push_back(funcTemp);
    }
    else if(code == CODE_RTIVKFUNC){
        auto funcTempRef = (StarbytesFuncRef)evalExpr(in);
        unsigned argCount;
        in.read((char *)&argCount,sizeof(argCount));
        invokeFunc(in,StarbytesFuncRefGetPtr(funcTempRef),argCount);
    }
}

void InterpImpl::exec(std::istream & in){
    std::cout << "Interp Starting" << std::endl;
    RTCode code;
    std::string g = "GLOBAL";
    allocator->setScope(g);
    in.read((char *)&code,sizeof(RTCode));
    bool temp;
    StarbytesObject temp2;
    while(code != CODE_MODULE_END){
        execNorm(code,in,&temp,&temp2);
        in.read((char *)&code,sizeof(RTCode));
    };
    allocator->clearScope();
}


void InterpImpl::addExtension(llvm::StringRef path) {

}

std::shared_ptr<Interp> Interp::Create(){
    return std::make_shared<InterpImpl>();
}

}
