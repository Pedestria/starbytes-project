#include "starbytes/runtime/RTEngine.h"
#include "starbytes/compiler/RTCode.h"
#include "RTStdlib.h"
#include "starbytes/base/ADT.h"

#include <iostream>
#include <iomanip>
#include <cstring>
#include <cassert>

#include "starbytes/interop.h"

namespace starbytes::Runtime {

struct RTScope {
    std::string name;
    RTScope *parent = nullptr;
};

RTScope *RTSCOPE_GLOBAL = new RTScope({"__GLOBAL__"});

static std::string rtidToString(const RTID &id){
    return std::string(id.value,id.len);
}

static std::string mangleClassMethodName(const std::string &className,const std::string &methodName){
    return "__" + className + "__" + methodName;
}



class RTAllocator {
    
   string_map<string_map<StarbytesObject>> all_var_objects;
    
public:
    std::string currentScope;

    RTAllocator():all_var_objects(),currentScope(RTSCOPE_GLOBAL->name){
        
    };

    string_map<StarbytesObject> & getObjectRegistryAtScope(string_ref name){
        return all_var_objects[name];
    };

    void setScope(string_ref scope_name){
        currentScope.assign(scope_name.getBuffer(),scope_name.size());
    };
    void setScope(const std::string &scope_name){
        currentScope = scope_name;
    };
    void allocVariable(string_ref name,StarbytesObject obj){
        auto found = all_var_objects.find(currentScope);
        if(found == all_var_objects.end()){
            string_map<StarbytesObject> vars;
            vars.insert(std::make_pair(name,obj));
            all_var_objects.insert(std::make_pair(currentScope,std::move(vars)));
        }
        else {
            auto & var_map = found->second;
            var_map.insert(std::make_pair(name,obj));
        };
    };
    void allocVariable(string_ref name,StarbytesObject obj,string_ref scope){
        auto found = all_var_objects.find(scope);
        if(found == all_var_objects.end()){
            string_map<StarbytesObject> vars;
            vars.insert(std::make_pair(name,obj));
            all_var_objects.insert(std::make_pair(scope,std::move(vars)));
        }
        else {
            auto & var_map = found->second;
            var_map.insert(std::make_pair(name,obj));
        };
    };

    StarbytesObject referenceVariable(string_ref scope,string_ref name){
        auto found = all_var_objects.find(scope);
        if(found == all_var_objects.end()){
//             std::cout << "Var NOT Found AND Scope NOT Found:" << name.data() << std::endl;
            return nullptr;
        }
        else {
            auto & var_map = found->second;
            for(auto & var : var_map){
                if(var.first.size() == name.size() &&
                   std::memcmp(var.first.data(),name.getBuffer(),name.size()) == 0){
                    /// Increase Reference count upon variable reference.
                    StarbytesObjectReference(var.second);
//                     std::cout << "Found Var:" << name.data() << std::endl;
                    return var.second;
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

StarbytesNativeModule * starbytes_native_mod_load(string_ref path);
StarbytesFuncCallback starbytes_native_mod_load_function(StarbytesNativeModule * mod,string_ref name);
void starbytes_native_mod_close(StarbytesNativeModule * mod);

class InterpImpl final : public Interp {

   void addExtension(string_ref path);

    std::unique_ptr<RTAllocator> allocator;
    
    std::vector<RTClass> classes;
    
    std::vector<RTFuncTemplate> functions;

    map<StarbytesClassType,std::string> runtimeClassRegistry;
    string_map<StarbytesClassType> classTypeByName;
    map<StarbytesClassType,size_t> classIndexByType;

    void execNorm(RTCode &code,std::istream &in,bool * willReturn,StarbytesObject *return_val);
    
    StarbytesObject invokeFunc(std::istream &in,RTFuncTemplate *func_temp,unsigned argCount,StarbytesObject boundSelf = nullptr);
    RTClass *findClassByName(string_ref className);
    RTClass *findClassByType(StarbytesClassType classType);
    RTFuncTemplate *findFunctionByName(string_ref functionName);
    void discardExprArgs(std::istream &in,unsigned argCount);
    
    StarbytesObject evalExpr(std::istream &in);
    
public:
    InterpImpl():allocator(std::make_unique<RTAllocator>()){
        RTFuncTemplate printTemplate;
        printTemplate.name = {strlen("print"),"print"};
        printTemplate.argsTemplate.push_back({strlen("object"),"object"});
        functions.push_back(std::move(printTemplate));
    };
    ~InterpImpl() override = default;
    void exec(std::istream &in) override;
};

RTClass *InterpImpl::findClassByName(string_ref className){
    auto foundType = classTypeByName.find(std::string(className));
    if(foundType == classTypeByName.end()){
        return nullptr;
    }
    auto foundClass = classIndexByType.find(foundType->second);
    if(foundClass == classIndexByType.end()){
        return nullptr;
    }
    return &classes[foundClass->second];
}

RTClass *InterpImpl::findClassByType(StarbytesClassType classType){
    auto foundClass = classIndexByType.find(classType);
    if(foundClass == classIndexByType.end()){
        return nullptr;
    }
    return &classes[foundClass->second];
}

RTFuncTemplate *InterpImpl::findFunctionByName(string_ref functionName){
    for(auto &funcTemplate : functions){
        string_ref tempName(funcTemplate.name.value,(uint32_t)funcTemplate.name.len);
        if(functionName == tempName){
            return &funcTemplate;
        }
    }
    return nullptr;
}

void InterpImpl::discardExprArgs(std::istream &in,unsigned argCount){
    while(argCount > 0){
        auto arg = evalExpr(in);
        if(arg){
            StarbytesObjectRelease(arg);
        }
        --argCount;
    }
}


StarbytesObject InterpImpl::invokeFunc(std::istream & in,RTFuncTemplate *func_temp,unsigned argCount,StarbytesObject boundSelf){
    if(!func_temp){
        discardExprArgs(in,argCount);
        return nullptr;
    }
    std::string parentScope = allocator->currentScope;
    string_ref func_name = {func_temp->name.value,(uint32_t)func_temp->name.len};
    
    if(func_name == "print"){
        if(argCount > 0){
            auto object_to_print = evalExpr(in);
            stdlib::print(object_to_print,runtimeClassRegistry);
            if(object_to_print){
                StarbytesObjectRelease(object_to_print);
            }
            --argCount;
        }
        if(argCount > 0){
            discardExprArgs(in,argCount);
        }
        return nullptr;
    }
    else {
    
        std::string funcScope = std::string(func_name) + std::to_string(func_temp->invocations);

        if(boundSelf){
            StarbytesObjectReference(boundSelf);
            allocator->allocVariable(string_ref("self"),boundSelf,funcScope);
        }
                
        auto callSitePos = in.tellg();
        auto func_param_it = func_temp->argsTemplate.begin();
        while(argCount > 0){
            StarbytesObject obj = evalExpr(in);
            if(func_param_it != func_temp->argsTemplate.end()){
                allocator->allocVariable(string_ref(func_param_it->value,func_param_it->len),obj,funcScope);
                ++func_param_it;
            }
            else if(obj){
                StarbytesObjectRelease(obj);
            }
            --argCount;
        };
        auto resumePos = in.tellg();
        allocator->setScope(funcScope);
        auto current_pos = func_temp->block_start_pos;
        /// Invoke
        in.seekg(current_pos);
        
        func_temp->invocations += 1;
                
        RTCode code = CODE_MODULE_END;
        if(!in.read((char *)&code,sizeof(RTCode))){
            in.clear();
            in.seekg(callSitePos);
            allocator->clearScope();
            allocator->setScope(parentScope);
            return nullptr;
        }
        bool willReturn = false;
        StarbytesObject return_val = nullptr;
        while(in.good() && code != CODE_RTFUNCBLOCK_END){
            if(!willReturn){
                execNorm(code,in,&willReturn,&return_val);
            }
            if(!in.read((char *)&code,sizeof(RTCode))){
                break;
            }
        };
        in.seekg(resumePos);
        
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
            string_ref var_name (var_id.value,var_id.len);
            return allocator->referenceVariable(allocator->currentScope,var_name);
            
            break;
        }
        case CODE_RTFUNC_REF : {
            RTID var_id;
            in >> &var_id;
            string_ref func_name (var_id.value,var_id.len);
            auto *funcTemplate = findFunctionByName(func_name);
            if(funcTemplate){
                return StarbytesFuncRefNew(funcTemplate);
            }
            break;
        }
        case CODE_RTIVKFUNC : {
            auto funcRefObject = (StarbytesFuncRef)evalExpr(in);
            unsigned argCount;
            in.read((char *)&argCount,sizeof(argCount));
            if(!funcRefObject){
                discardExprArgs(in,argCount);
                return nullptr;
            }
            auto returnValue = invokeFunc(in,StarbytesFuncRefGetPtr(funcRefObject),argCount);
            StarbytesObjectRelease(funcRefObject);
            return returnValue;
        }
        case CODE_RTNEWOBJ : {
            RTID classId;
            in >> &classId;
            std::string className = rtidToString(classId);

            unsigned argCount = 0;
            in.read((char *)&argCount,sizeof(argCount));

            auto foundClassType = classTypeByName.find(className);
            StarbytesClassType classType;
            if(foundClassType == classTypeByName.end()){
                classType = StarbytesMakeClass(className.c_str());
                classTypeByName.insert(std::make_pair(className,classType));
                runtimeClassRegistry.insert(std::make_pair(classType,className));
            }
            else {
                classType = foundClassType->second;
            }

            StarbytesObject instance = StarbytesClassObjectNew(classType);
            auto *classMeta = findClassByType(classType);
            if(classMeta){
                for(auto &field : classMeta->fields){
                    std::string fieldName = rtidToString(field.id);
                    StarbytesObjectAddProperty(instance,const_cast<char *>(fieldName.c_str()),StarbytesBoolNew(StarbytesBoolFalse));
                }

                if(classMeta->hasFieldInitFunc){
                    std::string fieldInitName = rtidToString(classMeta->fieldInitFuncName);
                    auto *fieldInitFunc = findFunctionByName(string_ref(fieldInitName));
                    if(fieldInitFunc){
                        auto initResult = invokeFunc(in,fieldInitFunc,0,instance);
                        if(initResult){
                            StarbytesObjectRelease(initResult);
                        }
                    }
                }

                if(classMeta->constructors.empty()){
                    if(argCount > 0){
                        discardExprArgs(in,argCount);
                    }
                    return instance;
                }

                RTFuncTemplate *ctorMeta = nullptr;
                for(auto &ctorTemplate : classMeta->constructors){
                    if(ctorTemplate.argsTemplate.size() == argCount){
                        ctorMeta = &ctorTemplate;
                        break;
                    }
                }
                if(!ctorMeta){
                    discardExprArgs(in,argCount);
                    return instance;
                }

                std::string ctorName = rtidToString(ctorMeta->name);
                std::string mangledCtorName = mangleClassMethodName(className,ctorName);
                auto *runtimeCtor = findFunctionByName(string_ref(mangledCtorName));
                if(!runtimeCtor){
                    discardExprArgs(in,argCount);
                    return instance;
                }

                auto ctorResult = invokeFunc(in,runtimeCtor,argCount,instance);
                if(ctorResult){
                    StarbytesObjectRelease(ctorResult);
                }
                return instance;
            }
            discardExprArgs(in,argCount);
            return instance;
        }
        case CODE_RTMEMBER_GET : {
            auto object = evalExpr(in);
            RTID memberId;
            in >> &memberId;
            std::string memberName = rtidToString(memberId);

            StarbytesObject value = nullptr;
            if(object){
                value = StarbytesObjectGetProperty(object,memberName.c_str());
                if(value){
                    StarbytesObjectReference(value);
                }
                StarbytesObjectRelease(object);
            }
            if(value){
                return value;
            }
            return StarbytesBoolNew(StarbytesBoolFalse);
        }
        case CODE_RTMEMBER_SET : {
            auto object = evalExpr(in);
            RTID memberId;
            in >> &memberId;
            auto value = evalExpr(in);
            if(!value){
                value = StarbytesBoolNew(StarbytesBoolFalse);
            }
            if(!object){
                StarbytesObjectRelease(value);
                return nullptr;
            }

            std::string memberName = rtidToString(memberId);
            bool found = false;
            unsigned propCount = StarbytesObjectGetPropertyCount(object);
            for(unsigned i = 0;i < propCount;i++){
                auto *prop = StarbytesObjectIndexProperty(object,i);
                if(std::strcmp(prop->name,memberName.c_str()) == 0){
                    if(prop->data){
                        StarbytesObjectRelease(prop->data);
                    }
                    StarbytesObjectReference(value);
                    prop->data = value;
                    found = true;
                    break;
                }
            }

            if(!found){
                StarbytesObjectReference(value);
                StarbytesObjectAddProperty(object,const_cast<char *>(memberName.c_str()),value);
            }
            StarbytesObjectRelease(object);
            return value;
        }
        case CODE_RTMEMBER_IVK : {
            auto object = evalExpr(in);
            RTID memberId;
            in >> &memberId;
            unsigned argCount = 0;
            in.read((char *)&argCount,sizeof(argCount));
            if(!object){
                discardExprArgs(in,argCount);
                return nullptr;
            }

            StarbytesClassType classType = StarbytesClassObjectGetClass(object);
            auto *classMeta = findClassByType(classType);
            if(!classMeta){
                StarbytesObjectRelease(object);
                discardExprArgs(in,argCount);
                return nullptr;
            }

            std::string methodName = rtidToString(memberId);
            bool methodExists = false;
            for(auto &methodMeta : classMeta->methods){
                if(rtidToString(methodMeta.name) == methodName){
                    methodExists = true;
                    break;
                }
            }
            if(!methodExists){
                StarbytesObjectRelease(object);
                discardExprArgs(in,argCount);
                return nullptr;
            }

            std::string className = rtidToString(classMeta->name);
            std::string mangledName = mangleClassMethodName(className,methodName);
            auto *runtimeMethod = findFunctionByName(string_ref(mangledName));
            if(!runtimeMethod){
                StarbytesObjectRelease(object);
                discardExprArgs(in,argCount);
                return nullptr;
            }

            auto result = invokeFunc(in,runtimeMethod,argCount,object);
            StarbytesObjectRelease(object);
            return result;
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
        string_ref var_name (var.id.value,var.id.len);
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
    else if(code == CODE_RTIVKFUNC
         || code == CODE_RTMEMBER_SET
         || code == CODE_RTMEMBER_IVK
         || code == CODE_RTMEMBER_GET
         || code == CODE_RTNEWOBJ){
        in.seekg(-static_cast<std::streamoff>(sizeof(RTCode)),std::ios_base::cur);
        auto result = evalExpr(in);
        if(result){
            StarbytesObjectRelease(result);
        }
    }
    else if(code == CODE_RTCLASS_DEF){
        RTClass _class;
        in >> &_class;
        auto className = rtidToString(_class.name);
        auto classType = StarbytesMakeClass(className.c_str());
        classTypeByName[className] = classType;
        runtimeClassRegistry[classType] = className;
        classIndexByType[classType] = classes.size();
        classes.emplace_back(std::move(_class));
    }
}

void InterpImpl::exec(std::istream & in){
    std::cout << "Interp Starting" << std::endl;
    RTCode code = CODE_MODULE_END;
    std::string g = "GLOBAL";
    allocator->setScope(g);
    if(!in.read((char *)&code,sizeof(RTCode))){
        return;
    }
    bool temp = false;
    StarbytesObject temp2 = nullptr;
    while(in.good() && code != CODE_MODULE_END){
        execNorm(code,in,&temp,&temp2);
        if(!in.read((char *)&code,sizeof(RTCode))){
            break;
        }
    };
    allocator->clearScope();
}


void InterpImpl::addExtension(string_ref path) {

}

std::shared_ptr<Interp> Interp::Create(){
    return std::make_shared<InterpImpl>();
}

}
