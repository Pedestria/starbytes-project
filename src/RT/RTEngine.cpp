#include "starbytes/RT/RTEngine.h"
#include "starbytes/RT/RTCode.h"
#include "RTStdlib.h"

#include <llvm/ADT/StringRef.h>
#include <llvm/ADT/StringMap.h>
#include <iostream>
#include <iomanip>

namespace starbytes::Runtime {

struct RTScope {
    std::string name;
    RTScope *parent = nullptr;
};

RTScope *RTSCOPE_GLOBAL = new RTScope({"__GLOBAL__"});


void runtime_object_ref_inc(RTObject *obj){
    ++(obj->refCount);
}

void runtime_object_ref_dec(RTObject *obj){
    --(obj->refCount);
}

inline void runtime_object_delete(RTObject *obj){
    // std::cout << "OBJ:" << std::hex << size_t(obj) << "DELETE" << std::endl;
    if(obj->isInternal){
        RTInternalObject *_obj = (RTInternalObject *)obj;
        switch (_obj->type) {
            case RTINTOBJ_STR : {
                auto params = (RTInternalObject::StringParams *)_obj->data;
                delete params;
                break;
            }
            case RTINTOBJ_BOOL : {
                auto params = (RTInternalObject::BoolParams *)_obj->data;
                delete params;
                break;
            }
            case RTINTOBJ_ARRAY : {
                auto params = (RTInternalObject::ArrayParams *)_obj->data;
                for(auto & obj : params->data){
                    runtime_object_delete(obj);
                };
                delete params;
                break;
            }
            default:
                break;
        }
        delete _obj;
    }
    else {
        
    };
}
/// Garbage Collect
void runtime_object_collectg(RTObject *obj){
    if(obj->refCount == 0){
        runtime_object_delete(obj);
    };
}

RTObject *clone_runtime_object(RTObject *obj){
    if(obj->isInternal){
        RTInternalObject *internal_object = new RTInternalObject(),*_obj = (RTInternalObject *)obj;
        internal_object->type = _obj->type;
        if(_obj->type == RTINTOBJ_STR){
            auto *params = new RTInternalObject::StringParams();
            auto _params = (RTInternalObject::StringParams *)_obj->data;
            params->str = _params->str;
            internal_object->data = params;
        }
        else if(_obj->type == RTINTOBJ_BOOL){
            auto *params = new RTInternalObject::BoolParams();
            auto _params = (RTInternalObject::BoolParams *)_obj->data;
            params->value = _params->value;
            internal_object->data = params;
        }
        else if(_obj->type == RTINTOBJ_ARRAY){
            auto *params = new RTInternalObject::ArrayParams();
            auto _params = (RTInternalObject::ArrayParams *)_obj->data;
            for(auto & array_obj : params->data){
                _params->data.push_back(clone_runtime_object(array_obj));
            };
            internal_object->data = params;
        };
        return internal_object;
    }
    else {
        return nullptr;
    };
}


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
            all_var_objects.insert(std::make_pair(currentScope,std::move(vars)));
        }
        else {
            auto & var_map = found->second;
            var_map.insert(std::make_pair(name,obj));
        };
    };
    void allocVariable(llvm::StringRef name,RTObject * obj,llvm::StringRef scope){
        auto found = all_var_objects.find(scope);
        if(found == all_var_objects.end()){
            llvm::StringMap<RTObject *> vars;
            vars.insert(std::make_pair(name,obj));
            all_var_objects.insert(std::make_pair(scope,std::move(vars)));
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
//             std::cout << "Var NOT Found AND Scope NOT Found:" << name.data() << std::endl;
            return nullptr;
        }
        else {
            auto & var_map = found->getValue();
            for(auto & var : var_map){
                if(var.getKey() == name){
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
                runtime_object_delete(ent.second);
            };
            all_var_objects.erase(found);
        };
    };
    void clearScopeCollectG(){
        auto found = all_var_objects.find(currentScope);
        if(found != all_var_objects.end()){
            auto & map = found->second;
            for(auto & ent : map){
                runtime_object_collectg(ent.second);
            };
            all_var_objects.erase(found);
        }
    };
};


class InterpImpl final : public Interp {

    std::unique_ptr<RTAllocator> allocator;
    
    std::vector<RTClassTemplate> classes;
    
    std::vector<RTFuncTemplate> functions;
    
    void execNorm(RTCode &code,std::istream &in);
    
    RTObject * invokeFunc(std::istream &in,llvm::StringRef & func_name,unsigned argCount);
    
    RTObject * evalExpr(std::istream &in);
    
public:
    InterpImpl():allocator(std::make_unique<RTAllocator>()){
        
    };
    void exec(std::istream &in) override;
};

RTObject *InterpImpl::invokeFunc(std::istream & in,llvm::StringRef & func_name,unsigned argCount){
    for(auto & func_temp : functions){
        if(func_name == llvm::StringRef(func_temp.name.value,func_temp.name.len)){
            llvm::StringRef parentScope = allocator->currentScope;
            
            std::string funcScope = func_name.str() + std::to_string(func_temp.invocations);
            
            auto func_param_it = func_temp.argsTemplate.begin();
            while(argCount > 0){
                RTObject * obj = evalExpr(in);
                allocator->allocVariable(llvm::StringRef(func_param_it->value,func_param_it->len),obj,funcScope);
                ++func_param_it;
                --argCount;
            };
            allocator->setScope(funcScope);
            auto prev_pos = in.tellg();
            auto current_pos = func_temp.block_start_pos;
            /// Invoke
            in.seekg(current_pos);
            
            func_temp.invocations += 1;
            
            RTCode code;
            in.read((char *)&code,sizeof(RTCode));
            while(code != CODE_RTBLOCK_END){
                execNorm(code,in);
                in.read((char *)&code,sizeof(RTCode));
            };

         
            in.seekg(prev_pos);
            
            allocator->clearScopeCollectG();
            allocator->setScope(parentScope);

            return nullptr;
        };
    };
    return nullptr;
}

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
            auto object = new RTInternalObject();
            in >> object;
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
        default:
            break;
    }
    return nullptr;
}

void InterpImpl::execNorm(RTCode &code,std::istream &in){
    if(code == CODE_RTVAR){
        RTVar var;
        in >> &var;
        llvm::StringRef var_name (var.id.value,var.id.len);
//            std::cout << std::boolalpha << var.hasInitValue << std::endl;
//            std::cout << var_name.data() << std::endl;
        if(var.hasInitValue){
            auto val = evalExpr(in);
            runtime_object_ref_inc(val);
            allocator->allocVariable(var_name,val);
        }
    }
    else if(code == CODE_RTFUNC){
        RTFuncTemplate funcTemp;
        in >> &funcTemp;
        functions.push_back(funcTemp);
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
            runtime_object_ref_dec(object_to_print);
            runtime_object_collectg(object_to_print);
        }
        else {
            invokeFunc(in,func_name,argCount);
        };
    }
}

void InterpImpl::exec(std::istream & in){
    std::cout << "Interp Starting" << std::endl;
    RTCode code;
    std::string g = "GLOBAL";
    allocator->setScope(g);
    in.read((char *)&code,sizeof(RTCode));
    while(code != CODE_MODULE_END){
        execNorm(code,in);
        in.read((char *)&code,sizeof(RTCode));
    };
    allocator->clearScope();
}

std::shared_ptr<Interp> Interp::Create(){
    return std::make_shared<InterpImpl>();
}

}
