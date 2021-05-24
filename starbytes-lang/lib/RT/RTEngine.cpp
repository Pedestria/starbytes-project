#include "starbytes/RT/RTEngine.h"
#include "starbytes/RT/RTCode.h"
#include "RTStdlib.h"

#include <llvm/ADT/StringRef.h>
#include <llvm/ADT/StringMap.h>
#include <iostream>
#include <iomanip>

namespace starbytes::Runtime {

void runtime_object_ref_inc(RTObject *obj){
    ++(obj->refCount);
};

void runtime_object_ref_dec(RTObject *obj){
    --(obj->refCount);
};
/// Garbage Collect
void runtime_object_gcollect(RTObject *obj){
    if(obj->refCount == 0){
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
                        runtime_object_gcollect(obj);
                    };
                    delete params;
                    break;
                }
                default:
                    break;
            }
        }
        else {
            
        };
    };
};

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
};


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
    
    void execNorm(RTCode &code,std::istream &in,llvm::StringMap<RTObject *> *block_args = nullptr);
    
    RTObject * invokeFunc(std::istream &in,llvm::StringRef & func_name,unsigned argCount);
    
    RTObject * evalExpr(std::istream &in,llvm::StringMap<RTObject *> *block_args = nullptr);
    
public:
    InterpImpl():allocator(std::make_unique<RTAllocator>()){
        
    };
    void exec(std::istream &in);
};

RTObject *InterpImpl::invokeFunc(std::istream & in,llvm::StringRef & func_name,unsigned argCount){
    for(auto & func_temp : functions){
        if(func_name == llvm::StringRef(func_temp.name.value,func_temp.name.len)){
            
            llvm::StringMap<RTObject *> args;
            
            auto func_param_it = func_temp.argsTemplate.begin();
            while(argCount > 0){
                RTObject * obj = evalExpr(in);
                args.insert(std::make_pair(llvm::StringRef(func_param_it->value,func_param_it->len),obj));
                ++func_param_it;
                --argCount;
            };
            auto prev_pos = in.tellg();
            /// Invoke
            in.seekg(func_temp.block_start_pos);
            
            
            
            RTCode code;
            in.read((char *)&code,sizeof(RTCode));
            while(code != CODE_RTBLOCK_END){
                execNorm(code,in,&args);
                in.read((char *)&code,sizeof(RTCode));
            };
            
            in.seekg(prev_pos);
            
            for(auto & arg : args){
                runtime_object_gcollect(arg.getValue());
            };
        };
    };
};

RTObject *InterpImpl::evalExpr(std::istream & in,llvm::StringMap<RTObject *> *block_args){
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
            if(!block_args){
                return allocator->referenceVariable(allocator->currentScope,var_name);
            }
            else {
                auto local_var = block_args->find(var_name);
                if(local_var != block_args->end()){
                    return local_var->getValue();
                }
                else {
                    return allocator->referenceVariable(allocator->currentScope,var_name);
                }
            };
            
            break;
        }
        default:
            break;
    }
};

void InterpImpl::execNorm(RTCode &code,std::istream &in,llvm::StringMap<RTObject *> *block_args){
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
            RTObject *object_to_print = evalExpr(in,block_args);
            stdlib::print(object_to_print);
        }
        else {
            invokeFunc(in,func_name,argCount);
        };
    }
};

void InterpImpl::exec(std::istream & in){
    std::cout << "Interp Starting" << std::endl;
    RTCode code;
    allocator->setScope("GLOBAL");
    in.read((char *)&code,sizeof(RTCode));
    while(code != CODE_MODULE_END){
        execNorm(code,in);
        in.read((char *)&code,sizeof(RTCode));
    };
    allocator->clearScope();
};

std::shared_ptr<Interp> Interp::Create(){
    return std::make_shared<InterpImpl>();
};

};
