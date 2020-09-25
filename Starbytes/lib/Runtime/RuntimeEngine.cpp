#include "Runtime/RuntimeEngine.h"
#include <string>
#include <vector>
#include <initializer_list>
#include <iostream>
#include <ctime>
#include <sstream>
#include <any>
#include <memory>

namespace Starbytes::Runtime::Engine {

    struct CreateFunctionArgs {
        std::string name;
        std::vector<StoredVariable<> *> args;
        std::initializer_list<InternalFuncPtr<> *> body;
    };

    struct InvokeFunctionArgs {
        std::string name;
        std::vector<StarbytesObject **>  args;
    };

    InvokeFunctionArgs * create_invoke_function_args(std::string _name,std::initializer_list<StarbytesObject **> _args){
        InvokeFunctionArgs *rc = new InvokeFunctionArgs;
        rc->name = _name;
        rc->args = _args;
        return rc;
    }

    // std::vector<StoredFunction *> stored_functions;
    // std::vector<StoredVariable<> *> stored_variables;
    // std::vector<StarbytesClass *> stored_classes;

    class Scope {
        private:
            std::string name;
        public:
            std::vector<StoredFunction *> stored_functions;
            std::vector<StoredVariable<> *> stored_variables;
            std::vector<StarbytesClass *> stored_classes;
            std::string & getName(){
                return name;
            }
            Scope(std::string & _name):name(_name){};
            ~Scope(){};
    };

    std::string current_scope;
    std::vector<Scope *> scopes;

    void create_scope(std::string name){
        Scope *c = new Scope(name);
        scopes.push_back(c);
    }

    Scope * get_scope(std::string name){
        for(auto s : scopes){
            if(s->getName() == name){
                return s;
            }
        }
        return nullptr;
    }

    void add_function_to_scope(std::string name,StoredFunction * func){
        get_scope(name)->stored_functions.push_back(func);
    }

    void add_class_to_scope(std::string name,StarbytesClass * cl){
        get_scope(name)->stored_classes.push_back(cl);
    }

    void add_variable_to_scope(std::string name,StoredVariable<> *var){
        get_scope(name)->stored_variables.push_back(var);
    }


    StarbytesString * create_starbytes_string(std::string value){
        StarbytesString * rc = new StarbytesString(value);
        return rc;
    }
    StarbytesArray<StarbytesObject *> * create_starbytes_array(std::initializer_list<StarbytesObject *> ilist){
        StarbytesArray<StarbytesObject *> * rc = new StarbytesArray<StarbytesObject *>(ilist);
        return rc;
    }

    std::string starbytes_object_to_string(StarbytesObject *obj){
        std::ostringstream out;
        if(obj->isType(SBObjectType::String)){
            out << ((StarbytesString *)obj)->__get_interal();
        }
        else if(obj->isType(SBObjectType::Array)){
            out << "[";
            std::vector<StarbytesObject *> v = ((StarbytesArray<StarbytesObject *> *)obj)->getIterator();
            for(auto i = 0;i < v.size();++i){
                if(i > 0){
                    out << ",";
                }
                out << starbytes_object_to_string(v[i]);
            }
            out << "]";
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
        get_scope(current_scope)->stored_variables.push_back(val);
    }
    template<typename _Type>
    void set_variable(std::string name,_Type * value){
        for(auto v : get_scope(current_scope)->stored_variables){
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
    //Returns `address` of pointer to variable!
    StarbytesObject ** refer_variable_a(std::string name){
        for(auto v : get_scope(current_scope)->stored_variables){
            if(v->name == name){
                return &v->val;
            }
        }
        return nullptr;
    }
    //Returns pointer to variable!
    StarbytesObject * refer_variable_d(std::string name){
        for(auto v : get_scope(current_scope)->stored_variables){
            if(v->name == name){
                return v->val;
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

    void create_function(std::string name,std::vector<StoredVariable<> *> args,std::initializer_list<InternalFuncPtr<> *> body){
        StoredFunction *func = new StoredFunction();
        func->name = name;
        func->body = body;
        func->args = args;
        get_scope(current_scope)->stored_functions.push_back(func);
    }

    void invoke_function(std::string name,std::vector<StarbytesObject **> & args){
        if(name == "Log"){
            starbytes_log(starbytes_object_to_string(*args[0]));
        }
        else{
            for(auto stfnc : get_scope(current_scope)->stored_functions){
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

    void create_class(std::string name){
        StarbytesClass *cl = new StarbytesClass();
        cl->name = name;
        cl->methods = {};
        cl->props = {};
        get_scope(current_scope)->stored_classes.push_back(cl);
    }

    StarbytesClassInstance * create_instance_of_class(std::string type,std::string name){
        StarbytesClassInstance *cl_inst = new StarbytesClassInstance(name,type);
        return cl_inst;
    }

    void create_class_method(std::string _class,std::string name,std::vector<StoredVariable<> *> args,std::initializer_list<InternalFuncPtr<> *> body){
        StarbytesClassMethod *mthd = new StarbytesClassMethod;
        StoredFunction *func = new StoredFunction;
        func->name = name;
        func->args = args;
        func->body = body;
        mthd->func = func;
        for(auto c : get_scope(current_scope)->stored_classes){
            if(c->name == _class){
                c->methods.push_back(mthd);
            }
        }
    }

    void invoke_instance_method(StarbytesObject * _instance_refer,std::string name,std::vector<StarbytesObject **> & args){
        if(_instance_refer->isType(SBObjectType::ClassInstance)){
            StarbytesClassInstance * ptr = (StarbytesClassInstance *) _instance_refer;
            for(auto m : *ptr->method_ptrs){
                    if(m->func->name == name){
                        //Assign Args!
                        for(int i = 0;i < m->func->args.size();++i){
                            m->func->args[i]->val = *args[i];
                        }
                        //
                        for(auto fnc : m->func->body){
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
                    break;
                }
            }
        }

    void Program(){
        current_scope = "GLOBAL";
        create_scope(current_scope);
        /* 
            decl Var = "Hello! Advanced Starbytes!"
            func TestFunc(message:String) {
                Log(message)
            }
            TestFunc(Var)
        */
        create_variable("Var");
        set_variable("Var",create_starbytes_string("Hello! Advanced Starbytes!"));
        std::vector<StarbytesObject **> args {refer_variable_a("Var")};
        StarbytesObject * s = create_starbytes_array({create_starbytes_string("Another Message!"),create_starbytes_string("Other Message! Right Here")});
        std::vector<StarbytesObject **> args0 { &s };
        std::vector<StoredVariable<> *> * func_args = create_function_args({"Message"});
        create_function("TestFunc",*func_args,{create_func_ptr(PtrType::InvokeFunc,create_invoke_function_args("Log",{refer_arg(func_args,"Message")}))});
        invoke_function("TestFunc",args);
        invoke_function("TestFunc",args0);
    }
    

};
