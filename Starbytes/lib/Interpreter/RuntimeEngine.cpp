#include "starbytes/Interpreter/RuntimeEngine.h"
#include "starbytes/Interpreter/STDObjects.h"
#include <string>
#include <vector>
#include <initializer_list>
#include <iostream>
#include <ctime>
#include <sstream>
#include <any>
#include <memory>
#include <future>
#include <thread>
#include <type_traits>

STARBYTES_INTERPRETER_NAMESPACE
namespace Engine {
        enum class PtrType:int {
            CreateFunc,InvokeFunc,CreateVariable,SetVariable,CreateFunctionArgs,ReturnFunc
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
            bool lazy;
            std::vector<StoredVariable<> *> args;
            std::initializer_list<InternalFuncPtr<> *> body;
        };

            struct StarbytesClassProperty {
                std::string name;
                bool not_constructible;
                bool loose;
                bool immutable;
                StarbytesObject *value;
            };

            struct StarbytesClassMethod {
                bool lazy;
                Engine::StoredFunction *func;
            };
            
            struct StarbytesClass {
                std::string name;
                std::vector<StarbytesClassProperty *> props;
                std::vector<StarbytesClassMethod *> methods;
            };
            struct StarbytesClassInstance : public StarbytesObject {
                StarbytesClassInstance(std::string _name,std::string _type):StarbytesObject(SBObjectType::ClassInstance),name(_name),type(_type){};
                std::string name;
                std::string type;
                std::vector<StarbytesClassProperty *> properties;
                std::vector<StarbytesClassMethod *> methods;
            };

    //Data Types/Data Type Instance Methods Implementations

    //CORE RUNTIME ENGINE!!!

    struct CreateFunctionArgs {
        std::string name;
        std::vector<StoredVariable<> *> args;
        std::initializer_list<InternalFuncPtr<> *> body;
    };

    struct InvokeFunctionArgs {
        std::string name;
        std::vector<StarbytesObject **>  args;
    };
    
    struct CreateVariableArgs {
        std::string name;
    };

    struct SetVariableArgs {
        std::string name;
        StarbytesObject *value;
    };

    struct ReturnFuncArgs {
        StarbytesObject *obj_to_return;
    };

    ReturnFuncArgs * create_return_func_args(StarbytesObject *__obj){
        ReturnFuncArgs * ar = new ReturnFuncArgs;
        ar->obj_to_return = __obj;
        return ar;
    }

    CreateVariableArgs * create_create_variable_args(std::string _name){
        CreateVariableArgs *ar = new CreateVariableArgs;
        ar->name = _name;
        return ar;
    }

    SetVariableArgs * create_set_variable_args(std::string _name,StarbytesObject *_value){
        SetVariableArgs *ar = new SetVariableArgs;
        ar->name = _name;
        ar->value = _value;
        return ar;
    }

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

    std::vector<std::string> scope_heirarchy;

    void set_current_scope(std::string _scope_name){
        scope_heirarchy.push_back(_scope_name);
    }

    void remove_current_scope(){
        scope_heirarchy.pop_back();
    }

    std::string & get_current_scope(){
        return scope_heirarchy.front();
    }

    std::vector<Scope *> scopes;

    void create_scope(std::string name){
        Scope *c = new Scope(name);
        scopes.push_back(c);
    }

    Scope * get_scope(std::string name){
        for(auto & s : scopes){
            if(s->getName() == name){
                return s;
            }
        }
        return nullptr;
    }


    StarbytesString * create_starbytes_string(std::string value){
        StarbytesString * rc = new StarbytesString(value);
        return rc;
    }

    bool is_starbytes_string(StarbytesObject *& ptr){
        if(ptr->isType(SBObjectType::String)){
            return true;
        }
        else{
            return false;
        }
    }

    StarbytesArray<StarbytesObject *> * create_starbytes_array(std::initializer_list<StarbytesObject *> ilist){
        StarbytesArray<StarbytesObject *> * rc = new StarbytesArray<StarbytesObject *>(ilist);
        return rc;
    }

    bool is_starbytes_array(StarbytesObject *& ptr){
        if(ptr->isType(SBObjectType::Array)){
            return true;
        }
        else{
            return false;
        }
    }

    template<typename _NumT>
    StarbytesNumber<_NumT> * create_starbytes_number(_NumT _num){
        StarbytesNumber<_NumT> *num = new StarbytesNumber<_NumT>(_num);
        return num;
    }

    bool is_starbytes_number(StarbytesObject *& ptr){
        if(ptr->isType(SBObjectType::Number)){
            return true;
        }
        else{
            return false;
        }
    }

    template<typename KEY,typename VALUE>
    StarbytesDictionary<KEY,VALUE> * create_starbytes_dictionary(std::initializer_list<std::pair<KEY,VALUE>> _ilist){
        StarbytesDictionary<KEY,VALUE> *dict = new StarbytesDictionary<KEY,VALUE>(_ilist);
        return dict;
    };

    bool is_starbytes_dictionary(StarbytesObject *& ptr){
        if(ptr->isType(SBObjectType::Dictionary)){
            return true;
        }
        else{
            return false;
        }
    }

    typedef std::pair<StarbytesString *,StarbytesObject *> StarbytesDictEntry;

    StarbytesBoolean * create_starbytes_boolean(bool __val){
        StarbytesBoolean *boolean = new StarbytesBoolean(__val);
        return boolean;
    }

    bool is_starbytes_boolean(StarbytesObject *& ptr){
        if(ptr->isType(SBObjectType::Boolean)){
            return true;
        }
        else {
            return false;
        }
    }

    std::string starbytes_object_to_string(StarbytesObject *obj){
        std::ostringstream out;
        if(is_starbytes_string(obj)){
            out <<  "\x1b[38;5;214m\""  << ((StarbytesString *)obj)->__get_interal() << "\"\x1b[0m";
        }
        else if(is_starbytes_array(obj)){
            out << "[";
            std::vector<StarbytesObject *> & v = ((StarbytesArray<StarbytesObject *> *)obj)->getIterator();
            for(auto i = 0;i < v.size();++i){
                if(i > 0){
                    out << ",";
                }
                out << starbytes_object_to_string(v[i]);
            }
            out << "]";
        } else if(is_starbytes_number(obj)){
            out << "\x1b[38;5;210m" << ((StarbytesNumber<int> *)obj)->__get_interal() << "\x1b[0m";
        } else if(is_starbytes_dictionary(obj)){
            out << "{";
            std::vector<std::pair<StarbytesString *,StarbytesObject *>> & _vector_map = ((StarbytesDictionary<StarbytesString *,StarbytesObject *> *)obj)->getIterator();
            for(auto i = 0;i < _vector_map.size();++i){
                if(i > 0){
                    out << ",";
                }
                out << starbytes_object_to_string(_vector_map[i].first) << ":" << starbytes_object_to_string(_vector_map[i].second);
            }
            out << "}";
        } else if(is_starbytes_boolean(obj)) {
            out << "\x1b[38;5;161m";
            StarbytesBoolean *b = (StarbytesBoolean *)obj;
            if(b->__get_interal() == true){
                out << "true";
            }
            else{
                out << "false";
            }
            out << "\x1b[0m";
        }

        return out.str();
    }

    void starbytes_log(std::string message){
        tm buf;
        time_t t = time(NULL);
        char RESULT [50];
        #ifdef __APPLE__
        gmtime_r(&t,&buf);
        asctime_r(&buf,RESULT);
        #elif _WIN32
        gmtime_s(&buf,&t);
        asctime_s(RESULT,sizeof (RESULT),&buf);
        #endif
        
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
        get_scope(get_current_scope())->stored_variables.push_back(val);
    }
    template<typename _Type>
    void set_variable(std::string name,_Type * value){
        for(auto & v : get_scope(get_current_scope())->stored_variables){
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
        for(auto & v : get_scope(get_current_scope())->stored_variables){
            if(v->name == name){
                return &v->val;
            }
        }
        return nullptr;
    }
    //Returns pointer to variable!
    StarbytesObject * refer_variable_d(std::string name){
        for(auto & v : get_scope(get_current_scope())->stored_variables){
            if(v->name == name){
                return v->val;
            }
        }
        return nullptr;
    }

    StarbytesObject ** refer_arg (std::vector<StoredVariable<> *> * func_args,std::string name){
        for(auto & v : *func_args){
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

    void create_function(std::string name,std::vector<StoredVariable<> *> args,std::initializer_list<InternalFuncPtr<> *> body,bool lazy = false){
        StoredFunction *func = new StoredFunction();
        func->name = name;
        func->body = body;
        func->args = args;
        func->lazy = lazy;
        get_scope(get_current_scope())->stored_functions.push_back(func);
    }

    void invoke_function(std::string name,std::vector<StarbytesObject **> args){
        if(name == "Log"){
            starbytes_log(starbytes_object_to_string(*args[0]));
        }
        else{
            for(auto & stfnc : get_scope(get_current_scope())->stored_functions){
                if(stfnc->name == name && stfnc->lazy == false){
                    for(int i = 0;i < stfnc->args.size();++i){
                        stfnc->args[i]->val = *args[i];
                    }
                    //
                    for(auto & fnc : stfnc->body){
                        if(fnc->type == PtrType::CreateFunc){
                            InternalFuncPtr<CreateFunctionArgs *> * new_ptr = reinterpret_cast<InternalFuncPtr<CreateFunctionArgs *> *>(fnc);
                            create_function(new_ptr->args->name,new_ptr->args->args,new_ptr->args->body);
                        }
                        else if(fnc->type == PtrType::InvokeFunc){
                            InternalFuncPtr<InvokeFunctionArgs *> * new_ptr = reinterpret_cast<InternalFuncPtr<InvokeFunctionArgs *> *>(fnc);
                            invoke_function(new_ptr->args->name,new_ptr->args->args);
                        }
                        else if(fnc->type == PtrType::CreateVariable){
                            InternalFuncPtr<CreateVariableArgs *> * new_ptr = reinterpret_cast<InternalFuncPtr<CreateVariableArgs *> *>(fnc);
                            create_variable(new_ptr->args->name);
                        }
                        else if(fnc->type == PtrType::SetVariable){
                            InternalFuncPtr<SetVariableArgs *> * new_ptr = reinterpret_cast<InternalFuncPtr<SetVariableArgs *> *>(fnc);
                            set_variable(new_ptr->args->name,new_ptr->args->value);
                        }  
                    }
                }
            }
        }
    }

    // void invoke_lazy_function(std::string name,std::vector<StarbytesObject **> & args){
    //     std::thread th1 (invoke_function,name,args);
    //     th1.join();
    // }

    StarbytesObject * invoke_function_return_guaranteed(std::string name,std::vector<StarbytesObject **> args){
            StarbytesObject *return_ptr;
            for(auto & stfnc : get_scope(get_current_scope())->stored_functions){
                if(stfnc->name == name){
                    for(int i = 0;i < stfnc->args.size();++i){
                        stfnc->args[i]->val = *args[i];
                    }
                    //
                    for(auto & fnc : stfnc->body){
                        if(fnc->type == PtrType::CreateFunc){
                            InternalFuncPtr<CreateFunctionArgs *> * new_ptr = reinterpret_cast<InternalFuncPtr<CreateFunctionArgs *> *>(fnc);
                            create_function(new_ptr->args->name,new_ptr->args->args,new_ptr->args->body);
                        }
                        else if(fnc->type == PtrType::InvokeFunc){
                            InternalFuncPtr<InvokeFunctionArgs *> * new_ptr = reinterpret_cast<InternalFuncPtr<InvokeFunctionArgs *> *>(fnc);
                            invoke_function(new_ptr->args->name,new_ptr->args->args);
                        }
                        else if(fnc->type == PtrType::CreateVariable){
                            InternalFuncPtr<CreateVariableArgs *> * new_ptr = reinterpret_cast<InternalFuncPtr<CreateVariableArgs *> *>(fnc);
                            create_variable(new_ptr->args->name);
                        }
                        else if(fnc->type == PtrType::SetVariable){
                            InternalFuncPtr<SetVariableArgs *> * new_ptr = reinterpret_cast<InternalFuncPtr<SetVariableArgs *> *>(fnc);
                            set_variable(new_ptr->args->name,new_ptr->args->value);
                        }  
                        else if(fnc->type == PtrType::ReturnFunc){
                            InternalFuncPtr<ReturnFuncArgs *> * new_ptr = reinterpret_cast<InternalFuncPtr<ReturnFuncArgs *> *>(fnc);
                            return_ptr = new_ptr->args->obj_to_return;
                        }
                    }
                }
            }
            return return_ptr;
    }

    // StarbytesObject * invoke_lazy_function_return_guaranteed(std::string name,std::vector<StarbytesObject **> & args){
    //     std::promise<StarbytesObject *> promise;
    //     std::future future_result = promise.get_future();
    //     std::thread thread1 ([&promise,&name,&args]{
    //         StarbytesObject *return_ptr;
    //         for(auto & stfnc : get_scope(get_current_scope())->stored_functions){
    //             if(stfnc->name == name){
    //                 for(int i = 0;i < stfnc->args.size();++i){
    //                     stfnc->args[i]->val = *args[i];
    //                 }
    //                 //
    //                 for(auto & fnc : stfnc->body){
    //                     if(fnc->type == PtrType::CreateFunc){
    //                         InternalFuncPtr<CreateFunctionArgs *> * new_ptr = reinterpret_cast<InternalFuncPtr<CreateFunctionArgs *> *>(fnc);
    //                         create_function(new_ptr->args->name,new_ptr->args->args,new_ptr->args->body);
    //                     }
    //                     else if(fnc->type == PtrType::InvokeFunc){
    //                         InternalFuncPtr<InvokeFunctionArgs *> * new_ptr = reinterpret_cast<InternalFuncPtr<InvokeFunctionArgs *> *>(fnc);
    //                         invoke_function(new_ptr->args->name,new_ptr->args->args);
    //                     }
    //                     else if(fnc->type == PtrType::CreateVariable){
    //                         InternalFuncPtr<CreateVariableArgs *> * new_ptr = reinterpret_cast<InternalFuncPtr<CreateVariableArgs *> *>(fnc);
    //                         create_variable(new_ptr->args->name);
    //                     }
    //                     else if(fnc->type == PtrType::SetVariable){
    //                         InternalFuncPtr<SetVariableArgs *> * new_ptr = reinterpret_cast<InternalFuncPtr<SetVariableArgs *> *>(fnc);
    //                         set_variable(new_ptr->args->name,new_ptr->args->value);
    //                     }  
    //                     else if(fnc->type == PtrType::ReturnFunc){
    //                         InternalFuncPtr<ReturnFuncArgs *> * new_ptr = reinterpret_cast<InternalFuncPtr<ReturnFuncArgs *> *>(fnc);
    //                         return_ptr = new_ptr->args->obj_to_return;
    //                     }
    //                 }
    //             }
    //         }
    //         promise.set_value_at_thread_exit(return_ptr);
    //     });
    //     StarbytesObject * RESULT = future_result.get();
    //     thread1.join();
    //     return RESULT;
    // }

    void create_class(std::string name,std::initializer_list<StarbytesClassMethod *> _methods,std::initializer_list<StarbytesClassProperty *> _props){
        StarbytesClass *cl = new StarbytesClass();
        cl->name = name;
        cl->methods = _methods;
        cl->props = _props;
        get_scope(get_current_scope())->stored_classes.push_back(cl);
    }



    StarbytesClassMethod * create_class_method(std::string name,std::vector<StoredVariable<> *> args,std::initializer_list<InternalFuncPtr<> *> body){
        StarbytesClassMethod *mthd = new StarbytesClassMethod;
        StoredFunction *func = new StoredFunction;
        func->name = name;
        func->args = args;
        func->body = body;
        mthd->func = func;
        return mthd;
    }

    StarbytesClassProperty * create_class_property(std::string name,bool immutable,bool loose,bool has_inital_val = false,bool is_constructible = false,StarbytesObject * val = nullptr){
        StarbytesClassProperty *prop = new StarbytesClassProperty;
        prop->name = name;
        prop->immutable = immutable;
        prop->loose = loose;
        prop->not_constructible = has_inital_val;
        if(has_inital_val){
            prop->value = val;
        }
        return prop;
    }

    StarbytesObject * refer_class_property(std::string name,StarbytesClassInstance * class_inst){
        for(auto & p : class_inst->properties){
            if(p->name == name){
                return p->value;
            }
        }
        return nullptr;
    }

    void release_class_property(std::string name,StarbytesClassInstance * class_inst){
        for(int i = 0;i < class_inst->properties.size();++i){
            auto &p = class_inst->properties[i];
            if(p->name == name && p->loose == true){
                delete p->value;
                class_inst->properties.erase(class_inst->properties.begin()+i);
            }
        }
    }

    void invoke_instance_method(StarbytesObject * _instance_refer,std::string name,std::vector<StarbytesObject **> args){
        if(_instance_refer->isType(SBObjectType::ClassInstance)){
            StarbytesClassInstance * ptr = (StarbytesClassInstance *) _instance_refer;
            for(auto & m : ptr->methods){
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
                            else if(fnc->type == PtrType::CreateVariable){
                                InternalFuncPtr<CreateVariableArgs *> * new_ptr = reinterpret_cast<InternalFuncPtr<CreateVariableArgs *> *>(fnc);
                                create_variable(new_ptr->args->name);
                            }
                            else if(fnc->type == PtrType::SetVariable){
                                InternalFuncPtr<SetVariableArgs *> * new_ptr = reinterpret_cast<InternalFuncPtr<SetVariableArgs *> *>(fnc);
                                set_variable(new_ptr->args->name,new_ptr->args->value);
                            }
                            
                        }
                    }
                break;
            }
        }
        else if(is_starbytes_string(_instance_refer)){
            StarbytesString *str = (StarbytesString *)_instance_refer;
            //String Class Methods
            if(name == "append"){
                if(is_starbytes_string(*args[0])){    
                    str->append((StarbytesString *)*args[0]);
                }
            }
        }
        else if(is_starbytes_number(_instance_refer)){
            StarbytesNumber<int> *num = (StarbytesNumber<int> *)_instance_refer;
            if(name == "add"){
                if(is_starbytes_number(*args[0])){
                    num->add((StarbytesNumber <int> *)*args[0]);
                }
            } else if(name == "subtract"){
                if(is_starbytes_number(*args[0])){
                    num->subtract((StarbytesNumber <int> *)*args[0]);
                }
            }
        } else if(is_starbytes_array(_instance_refer)){
            StarbytesArray<StarbytesObject *> *arr = (StarbytesArray<StarbytesObject *> *)_instance_refer;
            if(name == "push"){
                arr->push(*args[0]);
            }
        } 
        //else if(is_starbytes_dictionary(_instance_refer)){
        //     StarbytesDictionary<StarbytesString *,StarbytesObject *> *dict = (StarbytesDictionary<StarbytesString *,StarbytesObject *> *)_instance_refer;
        //     //Make new function!!
        //     if(name == "set"){
        //         dict->set((StarbytesString *) *args[0],*args[1]);
        //     }
        // }
    }

    StarbytesObject * invoke_instance_method_return_guaranteed(StarbytesObject * _instance_refer,std::string name,std::vector<StarbytesObject **> args){
        StarbytesObject *return_ptr;
        if(_instance_refer->isType(SBObjectType::ClassInstance)){
            StarbytesClassInstance * ptr = (StarbytesClassInstance *) _instance_refer;
            for(auto & m : ptr->methods){
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
                        else if(fnc->type == PtrType::CreateVariable){
                            InternalFuncPtr<CreateVariableArgs *> * new_ptr = reinterpret_cast<InternalFuncPtr<CreateVariableArgs *> *>(fnc);
                            create_variable(new_ptr->args->name);
                        }
                        else if(fnc->type == PtrType::SetVariable){
                            InternalFuncPtr<SetVariableArgs *> * new_ptr = reinterpret_cast<InternalFuncPtr<SetVariableArgs *> *>(fnc);
                            set_variable(new_ptr->args->name,new_ptr->args->value);
                        }
                        else if(fnc->type == PtrType::ReturnFunc){
                            InternalFuncPtr<ReturnFuncArgs *> * new_ptr = reinterpret_cast<InternalFuncPtr<ReturnFuncArgs *> *>(fnc);
                            return_ptr = new_ptr->args->obj_to_return;
                        }   
                    }
                }
                break;
            }
        }
        else if(is_starbytes_dictionary(_instance_refer)){
            StarbytesDictionary<StarbytesString *,StarbytesObject *> *dict = (StarbytesDictionary<StarbytesString *,StarbytesObject *> *)_instance_refer;
            //Make new function!!
            if(name == "set"){
                return_ptr = create_starbytes_boolean(dict->set((StarbytesString *) *args[0],*args[1]));
            }
        }
        return return_ptr;
    }

    StarbytesObject * clone_object(StarbytesObject *_obj){
        StarbytesObject *ptr;
        if(_obj->isType(SBObjectType::String)){
            ptr = create_starbytes_string(((StarbytesString *)_obj)->__get_interal());
        }
        else if(_obj->isType(SBObjectType::Array)){
            std::vector<StarbytesObject *> vals;
            for(auto & it : ((StarbytesArray<StarbytesObject *> *)_obj)->getIterator()){
                vals.push_back(clone_object(it));
            }
            ptr = new StarbytesArray(vals);
        } else if(_obj->isType(SBObjectType::Number)){
            ptr = create_starbytes_number(((StarbytesNumber<int> *)_obj)->__get_interal());
        }
        return ptr;
    }

    // StoredFunction * clone_function(StoredFunction * _func){

    // }

    // StarbytesClassInstance * create_instance_of_class(std::string type,std::string name,std::initializer_list<StarbytesObject *> args){
    //     StarbytesClassInstance *cl_inst = new StarbytesClassInstance(name,type);
    //     std::vector a (args);
    //     for(auto & cl : get_scope(get_current_scope())->stored_classes){
    //         if(cl->name == type){
    //            for(int i = 0; i < cl->props.size(); ++i){
    //                auto & p = cl->props[i];
    //                if(p->not_constructible){
    //                    StarbytesObject * property_val = clone_object(p->value);
    //                    cl_inst->properties.push_back(create_class_property(p->name,p->immutable,p->loose,true,property_val));
    //                }
    //                else{
    //                    cl_inst->properties.push_back(create_class_property(p->name,p->immutable,p->loose,false,true,a[i]));
    //                }
    //            }
    //            for (auto & m : cl->methods){
    //                StarbytesClassMethod *me = new StarbytesClassMethod;
    //                me->lazy = m->lazy;
    //                me->func = clone_function(m->func);
    //                cl_inst->methods.push_back(me);
    //            }
    //         }
    //     }
    //     return cl_inst;
    // }

    // enum class starbytes_conditional_operators:int{
    //     IsEqual,IsNotEqual,IsGreaterThan,IsLessThan
    // };


    void internal_if(StarbytesBoolean *_bool,std::string _func_to_invoke,std::vector<StarbytesObject **> args){
        if(_bool->__get_interal()){
            invoke_function(_func_to_invoke,args);
        }
        else{
            return;
        }
    }

    // StarbytesBoolean * convert_conditional_to_bool(){

    // };

    void Program(){
        create_scope("GLOBAL");
        set_current_scope("GLOBAL");
        /* 
            decl Var = "Hello! Advanced Starbytes!"
            decl Some = "Extension!"
            Var.append(Some)
            func TestFunc(message:Printable) {
                Log(Color.red(message))
            }
            TestFunc(Var)
            TestFunc(["Another Message!","Other Message!"])
        */
        create_variable("Var");
        set_variable("Var",create_starbytes_string("Hello! Advanced Starbytes!"));
        create_variable("Some");
        set_variable("Some",create_starbytes_string(" + Some Extension!"));
        invoke_instance_method(refer_variable_d("Var"),"append",{refer_variable_a("Some")});
        std::vector<StoredVariable<> *> * func_args = create_function_args({"Message"});
        create_function("TestFunc",*func_args,{
            create_func_ptr(PtrType::InvokeFunc,create_invoke_function_args("Log",{refer_arg(func_args,"Message")}))
            });
        invoke_function("TestFunc",{refer_variable_a("Var")});
        create_variable("Array");
        set_variable("Array",create_starbytes_array({create_starbytes_string("Another Message!"),create_starbytes_string("Other Message!")}));
        invoke_function("TestFunc",{refer_variable_a("Array")});
        StarbytesObject *n = create_starbytes_number(10);
        invoke_instance_method(refer_variable_d("Array"),"push",{ &n });
        invoke_function("TestFunc",{refer_variable_a("Array")});
        create_variable("Map");
        set_variable("Map",create_starbytes_dictionary({StarbytesDictEntry(create_starbytes_string("test"),create_starbytes_number(123))}));
        invoke_function("TestFunc",{refer_variable_a("Map")});
        StarbytesObject *a = create_starbytes_string("otherKey");
        StarbytesObject *b = create_starbytes_boolean(false);
        StarbytesObject *temp = invoke_instance_method_return_guaranteed(refer_variable_d("Map"),"set",{ &a,&b});
        internal_if((StarbytesBoolean *)temp,"TestFunc",{ refer_variable_a("Map")}); //invoke_function("TestFunc",{ refer_variable_a("Map")});
        // std::vector<StoredVariable<> *> * func_args0 = create_function_args({});
        // create_function("SomeOtherFunc",*func_args0,{create_func_ptr(PtrType::ReturnFunc,create_return_func_args(create_starbytes_number(3)))});
        // create_variable("test");
        // std::vector<StarbytesObject **> args2;
        // set_variable("test",invoke_function_return_guaranteed("SomeOtherFunc",args2));
        // std::vector<StarbytesObject **> args3 {refer_variable_a("test")};
        // invoke_function("TestFunc",args3);
    }
    

};

NAMESPACE_END