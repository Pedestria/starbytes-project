#include "Runtime/RuntimeEngine.h"
#include "Runtime/RuntimeLib.h"
#include <string>
#include <vector>
#include <initializer_list>
#include <iostream>
#include <ctime>
#include <sstream>
#include <any>
#include <memory>

namespace Starbytes::Runtime::Engine {

    using namespace Lib;

    enum class PtrType:int {
        CreateFunc,InvokeFunc
    };
    template<typename ARGTYPE = std::any>
    struct InternalFuncPtr : RuntimeObject {
        PtrType type;
        ARGTYPE args;
    };

    template <typename _ValType = StarbytesObject *>
    struct StoredVariable {
        std::string name;
        _ValType val;
        bool is_reference;
    };

    struct StoredFunction {
        std::string name;
        std::vector<StoredVariable<> *> args;
        std::initializer_list<InternalFuncPtr<> *> body;
    };

    struct CreateFunctionArgs {
        std::string name;
        std::vector<StoredVariable<> *> args;
        std::initializer_list<InternalFuncPtr<> *> body;
    };

    struct InvokeFunctionArgs {
        std::string name;
        std::vector<StarbytesObject **>  args;
        std::vector<StoredVariable<> *> storage;
    };

    std::vector<StoredFunction *> stored_functions;
    std::vector<StoredVariable<> *> stored_variables;

    StarbytesString * create_starbytes_string(std::string value){
        StarbytesString * rc = new StarbytesString(value);
        return rc;
    }

    std::string starbytes_object_to_string(StarbytesObject *obj){
        std::ostringstream out;
        if(obj->isType(Lib::SBObjectType::String)){
            out << ((StarbytesString *)obj)->__get_interal();
        }

        return out.str();
    }

    void starbytes_log(std::string message){
        tm buf;
        time_t t = time(NULL);
        char RESULT [50];
        gmtime_s(&buf,&t);
        asctime_s(RESULT,sizeof (RESULT),&buf);
        std::string R (RESULT);
        size_t found = R.find("\n");
        if(found != std::string::npos){
            R.erase(R.begin()+found);
        }
        std::cout << "[" << R << "] - " << message << "\n";
    }
     //Dynamic Allocates `FuncPtr` object
    template<typename _Type>
    InternalFuncPtr <> * create_func_ptr(PtrType _type,_Type args){
        InternalFuncPtr <_Type> *rc = new InternalFuncPtr<_Type>();
        rc->type = _type;
        rc->args = args;
        return reinterpret_cast<InternalFuncPtr <> *>(rc);
    }
    void create_variable(std::string name){
        StoredVariable<> * val = new StoredVariable<>;
        val->name = name;
        stored_variables.push_back(val);
    }
    template<typename _Type>
    void set_variable(std::string name,_Type * value){
        for(auto v : stored_variables){
            if(v->name == name){
                v->val = value;
                break;
            }
        }
    }
    StarbytesObject ** get_address_of_starbytes_object (StarbytesObject * t){
        StarbytesObject ** ptr = &t;
        return ptr;
    }

    StarbytesObject ** refer_variable(std::string name){
        for(auto v : stored_variables){
            if(v->name == name){
                return &v->val;
            }
        }
        return nullptr;
    }

    StarbytesObject ** refer_arg (std::vector<StoredVariable<> *> * func_args,std::string name){
        for(auto v : *func_args){
            if(v->name == name){
                return &v->val;
                break;
            }
        }
        return nullptr;
    }

    std::vector<StoredVariable<> *> * create_function_args(std::initializer_list<std::string> arg_list){
        std::vector<StoredVariable<> *> * args = new std::vector<StoredVariable<> *>;
        for(auto arg : arg_list){
            StoredVariable<> *v = new StoredVariable<>();
            v->name = arg;
            args->push_back(v);
        }
        return args;
    }

    void set_function_args(std::vector<StoredVariable<> *> & var_storage,std::vector<StarbytesObject *> vals){
        for(int i = 0;i < var_storage.size();++i){
            var_storage[i]->val = vals[i];
        }
    }

    void create_function(std::string name,std::vector<StoredVariable<> *> args,std::initializer_list<InternalFuncPtr<> *> body){
        StoredFunction *func = new StoredFunction();
        func->name = name;
        func->body = body;
        func->args = args;
        stored_functions.push_back(func);
    }

    void invoke_function(std::string name,std::vector<StarbytesObject **> & args){
        if(name == "Log"){
            starbytes_log(starbytes_object_to_string(*args[0]));
        }
        else{
            for(auto stfnc : stored_functions){
                if(stfnc->name == name){
                    for(int i = 0;i < stfnc->args.size();++i){
                        stfnc->args[i]->val = *args[i];
                    }
                    //
                    for(auto fnc : stfnc->body){
                        if(fnc->type == PtrType::CreateFunc){
                            InternalFuncPtr<CreateFunctionArgs *> * new_ptr = reinterpret_cast<InternalFuncPtr<CreateFunctionArgs *> *>(fnc);
                            create_function(new_ptr->args->name,new_ptr->args->args,new_ptr->args->body);
                        }
                        else if(fnc->type == PtrType::InvokeFunc){
                            InternalFuncPtr<InvokeFunctionArgs *> * new_ptr = reinterpret_cast<InternalFuncPtr<InvokeFunctionArgs *> *>(fnc);
                            invoke_function(new_ptr->args->name,new_ptr->args->args);
                        }
                    }
                }
            }
        }
    }

    void Program(){
        /* 
            decl Var = "Hello! Advanced Starbytes!"
            func TestFunc(message:String) {
                Log(message)
            }
            TestFunc(Var)
        */
        create_variable("Var");
        set_variable("Var",create_starbytes_string("Hello! Advanced Starbytes!"));
        std::vector<StarbytesObject **> args {refer_variable("Var")};
        StarbytesObject * s = create_starbytes_string("Another Message!");
        // create_variable("Var2");
        // set_variable("Var2",create_starbytes_string("Another Message!"));
        std::vector<StarbytesObject **> args0 { &s };
        std::vector<StoredVariable<> *> * func_args = create_function_args({"Message"});
        InvokeFunctionArgs *aargs = new InvokeFunctionArgs;
        aargs->name = "Log";
        aargs->args = {refer_arg(func_args,"Message")};
        create_function("TestFunc",*func_args,{create_func_ptr(PtrType::InvokeFunc,aargs)});
        invoke_function("TestFunc",args);
        invoke_function("TestFunc",args0);
    }
    

};
