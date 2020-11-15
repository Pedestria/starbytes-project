#include "RuntimeEngine.h"
#include "STDObjects.h"
#include "starbytes/ByteCode/BCDef.h"
#include <any>
#include <ctime>
#include <future>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>


STARBYTES_INTERPRETER_NAMESPACE
namespace Engine {

ReturnFuncArgs *create_return_func_args(StarbytesObject *__obj) {
  ReturnFuncArgs *ar = new ReturnFuncArgs;
  ar->obj_to_return = __obj;
  return ar;
}

CreateVariableArgs *create_create_variable_args(std::string _name) {
  CreateVariableArgs *ar = new CreateVariableArgs;
  ar->name = _name;
  return ar;
}

SetVariableArgs *create_set_variable_args(std::string _name,
                                          StarbytesObject *_value) {
  SetVariableArgs *ar = new SetVariableArgs;
  ar->name = _name;
  ar->value = _value;
  return ar;
}

InvokeFunctionArgs *
create_invoke_function_args(std::string _name,
                            std::initializer_list<StarbytesObject **> _args) {
  InvokeFunctionArgs *rc = new InvokeFunctionArgs;
  rc->name = _name;
  rc->args = _args;
  return rc;
}

StarbytesString *create_starbytes_string(std::string value) {
  StarbytesString *rc = new StarbytesString(value);
  return rc;
}

bool is_starbytes_string(StarbytesObject *&ptr) {
  if (ptr->isType(SBObjectType::String)) {
    return true;
  } else {
    return false;
  }
}

StarbytesArray<StarbytesObject *> *
create_starbytes_array(std::initializer_list<StarbytesObject *> ilist) {
  StarbytesArray<StarbytesObject *> *rc =
      new StarbytesArray<StarbytesObject *>(ilist);
  return rc;
}

bool is_starbytes_array(StarbytesObject *&ptr) {
  if (ptr->isType(SBObjectType::Array)) {
    return true;
  } else {
    return false;
  }
}

template <typename _NumT>
StarbytesNumber*create_starbytes_number(_NumT _num,int ty) {
  StarbytesNumber *num = new StarbytesNumber(_num,ty);
  return num;
}

bool is_starbytes_number(StarbytesObject *&ptr) {
  if (ptr->isType(SBObjectType::Number)) {
    return true;
  } else {
    return false;
  }
}

template <typename KEY, typename VALUE>
StarbytesDictionary<KEY, VALUE> *create_starbytes_dictionary(
    std::initializer_list<std::pair<KEY, VALUE>> _ilist) {
  StarbytesDictionary<KEY, VALUE> *dict =
      new StarbytesDictionary<KEY, VALUE>(_ilist);
  return dict;
};

bool is_starbytes_dictionary(StarbytesObject *&ptr) {
  if (ptr->isType(SBObjectType::Dictionary)) {
    return true;
  } else {
    return false;
  }
}

typedef std::pair<StarbytesString *, StarbytesObject *> StarbytesDictEntry;

StarbytesBoolean *create_starbytes_boolean(bool __val) {
  StarbytesBoolean *boolean = new StarbytesBoolean(__val);
  return boolean;
}

bool is_starbytes_boolean(StarbytesObject *&ptr) {
  if (ptr->isType(SBObjectType::Boolean)) {
    return true;
  } else {
    return false;
  }
}

std::string starbytes_object_to_string(StarbytesObject *obj) {
  std::ostringstream out;
  if (is_starbytes_string(obj)) {
    out << "\x1b[38;5;214m\"" << ((StarbytesString *)obj)->__get_interal()
        << "\"\x1b[0m";
  } else if (is_starbytes_array(obj)) {
    out << "[";
    std::vector<StarbytesObject *> &v =
        ((StarbytesArray<StarbytesObject *> *)obj)->getIterator();
    for (auto i = 0; i < v.size(); ++i) {
      if (i > 0) {
        out << ",";
      }
      out << starbytes_object_to_string(v[i]);
    }
    out << "]";
  } else if (is_starbytes_number(obj)) {
    out << "\x1b[38;5;210m" << *((char *)((StarbytesNumber *)obj)->__get_interal())
        << "\x1b[0m";
  } else if (is_starbytes_dictionary(obj)) {
    out << "{";
    std::vector<std::pair<StarbytesString *, StarbytesObject *>> &_vector_map =
        ((StarbytesDictionary<StarbytesString *, StarbytesObject *> *)obj)
            ->getIterator();
    for (auto i = 0; i < _vector_map.size(); ++i) {
      if (i > 0) {
        out << ",";
      }
      out << starbytes_object_to_string(_vector_map[i].first) << ":"
          << starbytes_object_to_string(_vector_map[i].second);
    }
    out << "}";
  } else if (is_starbytes_boolean(obj)) {
    out << "\x1b[38;5;161m";
    StarbytesBoolean *b = (StarbytesBoolean *)obj;
    if (b->__get_interal() == true) {
      out << "true";
    } else {
      out << "false";
    }
    out << "\x1b[0m";
  }

  return out.str();
}

void starbytes_log(std::string message) {
  tm buf;
  time_t t = time(NULL);
  char RESULT[50];
#ifdef __APPLE__
  gmtime_r(&t, &buf);
  asctime_r(&buf, RESULT);
#elif _WIN32
  gmtime_s(&buf, &t);
  asctime_s(RESULT, sizeof(RESULT), &buf);
#endif

  std::string R(RESULT);
  size_t found = R.find("\n");
  if (found != std::string::npos) {
    R.erase(R.begin() + found);
  }
  std::cout << "[" << R << "] - " << message << "\n";
}
// Dynamic Allocates `FuncPtr` object
template <typename _Type>
InternalFuncPtr<> *create_func_ptr(PtrType _type, _Type args) {
  InternalFuncPtr<_Type> *rc = new InternalFuncPtr<_Type>();
  rc->type = _type;
  rc->args = args;
  return reinterpret_cast<InternalFuncPtr<> *>(rc);
}
void create_variable(std::string name, Scope *current_scope) {
  StoredVariable<> *val = new StoredVariable<>;
  val->name = name;
  current_scope->stored_variables.push_back(val);
}
template <typename _Type>
void set_variable(std::string name, _Type *value, Scope *&current_scope) {
  for (auto &v : current_scope->stored_variables) {
    if (v->name == name) {
      v->val = value;
      break;
    }
  }
}
bool dealloc_variable(std::string name, Scope *&current_scope) {
  for (auto var : current_scope->stored_variables) {
    if (var->name == name) {
      delete var->val;
      delete var;
      return true;
    }
  }
  return false;
}

StarbytesObject **get_address_of_starbytes_object(StarbytesObject *t) {
  StarbytesObject **ptr = &t;
  return ptr;
}
// Returns `address` of pointer to variable!
StarbytesObject **refer_variable_a(std::string name, Scope *current_scope) {
  for (auto &v : current_scope->stored_variables) {
    if (v->name == name) {
      return &v->val;
    }
  }
  return nullptr;
}
// Returns pointer to variable!
StarbytesObject *refer_variable_d(std::string name, Scope *current_scope) {
  for (auto &v : current_scope->stored_variables) {
    if (v->name == name) {
      return v->val;
    }
  }
  return nullptr;
}

StarbytesObject **refer_arg(std::vector<StoredVariable<> *> *func_args,
                            std::string name) {
  for (auto &v : *func_args) {
    if (v->name == name) {
      return &v->val;
      break;
    }
  }
  return nullptr;
}

std::vector<StoredVariable<> *> *
create_function_args(std::initializer_list<std::string> arg_list) {
  std::vector<StoredVariable<> *> *args = new std::vector<StoredVariable<> *>;
  for (auto arg : arg_list) {
    StoredVariable<> *v = new StoredVariable<>();
    v->name = arg;
    args->push_back(v);
  }
  return args;
}

#define FUNC_PTR(type, ptr)                                                    \
  InternalFuncPtr<type> *new_ptr =                                             \
      reinterpret_cast<InternalFuncPtr<type> *>(ptr)

void create_function(std::string name, std::vector<StoredVariable<> *> args,
                     std::initializer_list<InternalFuncPtr<> *> body,
                     Scope *&current_scope, bool lazy = false) {
  StoredFunction *func = new StoredFunction();
  func->name = name;
  func->body = body;
  func->args = args;
  func->lazy = lazy;
  current_scope->stored_functions.push_back(func);
}

void invoke_function(std::string name, std::vector<StarbytesObject **> & args,
                     Scope *current_scope) {
  if (name == "Log") {
    starbytes_log(starbytes_object_to_string(*args[0]));
  } else {
    for (auto &stfnc : current_scope->stored_functions) {
      if (stfnc->name == name && stfnc->lazy == false) {
        for (int i = 0; i < stfnc->args.size(); ++i) {
          stfnc->args[i]->val = *args[i];
        }
        //
        for (auto &fnc : stfnc->body) {
          if (fnc->type == PtrType::CreateFunc) {
            FUNC_PTR(CreateFunctionArgs *, fnc);
            create_function(new_ptr->args->name, new_ptr->args->args,
                            new_ptr->args->body, current_scope);
          } else if (fnc->type == PtrType::InvokeFunc) {
            FUNC_PTR(InvokeFunctionArgs *, fnc);
            invoke_function(new_ptr->args->name, new_ptr->args->args,
                            current_scope);
          } else if (fnc->type == PtrType::CreateVariable) {
            FUNC_PTR(CreateVariableArgs *, fnc);
            create_variable(new_ptr->args->name, current_scope);
          } else if (fnc->type == PtrType::SetVariable) {
            FUNC_PTR(SetVariableArgs *, fnc);
            set_variable(new_ptr->args->name, new_ptr->args->value,
                         current_scope);
          }
        }
      }
    }
  }
}

// void invoke_lazy_function(std::string name,std::vector<StarbytesObject **> &
// args){
//     std::thread th1 (invoke_function,name,args);
//     th1.join();
// }

StarbytesObject *
invoke_function_return_guaranteed(std::string name,
                                  std::vector<StarbytesObject **> args,
                                  Scope *current_scope) {
  StarbytesObject *return_ptr;
  for (auto &stfnc : current_scope->stored_functions) {
    if (stfnc->name == name) {
      for (int i = 0; i < stfnc->args.size(); ++i) {
        stfnc->args[i]->val = *args[i];
      }
      //
      for (auto &fnc : stfnc->body) {
        if (fnc->type == PtrType::CreateFunc) {
          FUNC_PTR(CreateFunctionArgs *, fnc);
          create_function(new_ptr->args->name, new_ptr->args->args,
                          new_ptr->args->body, current_scope);
        } else if (fnc->type == PtrType::InvokeFunc) {
          FUNC_PTR(InvokeFunctionArgs *, fnc);
          invoke_function(new_ptr->args->name, new_ptr->args->args,
                          current_scope);
        } else if (fnc->type == PtrType::CreateVariable) {
          FUNC_PTR(CreateVariableArgs *, fnc);
          create_variable(new_ptr->args->name, current_scope);
        } else if (fnc->type == PtrType::SetVariable) {
          FUNC_PTR(SetVariableArgs *, fnc);
          set_variable(new_ptr->args->name, new_ptr->args->value,
                       current_scope);
        } else if (fnc->type == PtrType::ReturnFunc) {
          FUNC_PTR(ReturnFuncArgs *, fnc);
          return_ptr = new_ptr->args->obj_to_return;
        }
      }
    }
  }
  return return_ptr;
}

// StarbytesObject * invoke_lazy_function_return_guaranteed(std::string
// name,std::vector<StarbytesObject **> & args){
//     std::promise<StarbytesObject *> promise;
//     std::future future_result = promise.get_future();
//     std::thread thread1 ([&promise,&name,&args]{
//         StarbytesObject *return_ptr;
//         for(auto & stfnc : current_scope->stored_functions){
//             if(stfnc->name == name){
//                 for(int i = 0;i < stfnc->args.size();++i){
//                     stfnc->args[i]->val = *args[i];
//                 }
//                 //
//                 for(auto & fnc : stfnc->body){
//                     if(fnc->type == PtrType::CreateFunc){
//                         InternalFuncPtr<CreateFunctionArgs *> * new_ptr =
//                         reinterpret_cast<InternalFuncPtr<CreateFunctionArgs
//                         *> *>(fnc);
//                         create_function(new_ptr->args->name,new_ptr->args->args,new_ptr->args->body);
//                     }
//                     else if(fnc->type == PtrType::InvokeFunc){
//                         InternalFuncPtr<InvokeFunctionArgs *> * new_ptr =
//                         reinterpret_cast<InternalFuncPtr<InvokeFunctionArgs
//                         *> *>(fnc);
//                         invoke_function(new_ptr->args->name,new_ptr->args->args);
//                     }
//                     else if(fnc->type == PtrType::CreateVariable){
//                         InternalFuncPtr<CreateVariableArgs *> * new_ptr =
//                         reinterpret_cast<InternalFuncPtr<CreateVariableArgs
//                         *> *>(fnc); create_variable(new_ptr->args->name);
//                     }
//                     else if(fnc->type == PtrType::SetVariable){
//                         InternalFuncPtr<SetVariableArgs *> * new_ptr =
//                         reinterpret_cast<InternalFuncPtr<SetVariableArgs *>
//                         *>(fnc);
//                         set_variable(new_ptr->args->name,new_ptr->args->value);
//                     }
//                     else if(fnc->type == PtrType::ReturnFunc){
//                         InternalFuncPtr<ReturnFuncArgs *> * new_ptr =
//                         reinterpret_cast<InternalFuncPtr<ReturnFuncArgs *>
//                         *>(fnc); return_ptr = new_ptr->args->obj_to_return;
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

void create_class(std::string name,
                  std::initializer_list<StarbytesClassMethod *> _methods,
                  std::initializer_list<StarbytesClassProperty *> _props,
                  Scope *&current_scope) {
  StarbytesClass *cl = new StarbytesClass();
  cl->name = name;
  cl->methods = _methods;
  cl->props = _props;
  current_scope->stored_classes.push_back(cl);
}

StarbytesClassMethod *
create_class_method(std::string name, std::vector<StoredVariable<> *> args,
                    std::initializer_list<InternalFuncPtr<> *> body) {
  StarbytesClassMethod *mthd = new StarbytesClassMethod;
  StoredFunction *func = new StoredFunction;
  func->name = name;
  func->args = args;
  func->body = body;
  mthd->func = func;
  return mthd;
}

StarbytesClassProperty *create_class_property(std::string name, bool immutable,
                                              bool loose,
                                              bool has_inital_val = false,
                                              bool is_constructible = false,
                                              StarbytesObject *val = nullptr) {
  StarbytesClassProperty *prop = new StarbytesClassProperty;
  prop->name = name;
  prop->immutable = immutable;
  prop->loose = loose;
  prop->not_constructible = has_inital_val;
  if (has_inital_val) {
    prop->value = val;
  }
  return prop;
}

StarbytesObject *refer_class_property(std::string name,
                                      StarbytesClassInstance *class_inst) {
  for (auto &p : class_inst->properties) {
    if (p->name == name) {
      return p->value;
    }
  }
  return nullptr;
}

void release_class_property(std::string name,
                            StarbytesClassInstance *class_inst) {
  for (int i = 0; i < class_inst->properties.size(); ++i) {
    auto &p = class_inst->properties[i];
    if (p->name == name && p->loose == true) {
      delete p->value;
      class_inst->properties.erase(class_inst->properties.begin() + i);
    }
  }
}

void invoke_instance_method(StarbytesObject *_instance_refer, std::string name,
                            std::vector<StarbytesObject **> args,
                            Scope *current_scope) {
  if (_instance_refer->isType(SBObjectType::ClassInstance)) {
    StarbytesClassInstance *ptr = (StarbytesClassInstance *)_instance_refer;
    for (auto &m : ptr->methods) {
      if (m->func->name == name) {
        // Assign Args!
        for (int i = 0; i < m->func->args.size(); ++i) {
          m->func->args[i]->val = *args[i];
        }
        //
        for (auto fnc : m->func->body) {
          if (fnc->type == PtrType::CreateFunc) {
            FUNC_PTR(CreateFunctionArgs *, fnc);
            create_function(new_ptr->args->name, new_ptr->args->args,
                            new_ptr->args->body, current_scope);
          } else if (fnc->type == PtrType::InvokeFunc) {
            FUNC_PTR(InvokeFunctionArgs *, fnc);
            invoke_function(new_ptr->args->name, new_ptr->args->args,
                            current_scope);
          } else if (fnc->type == PtrType::CreateVariable) {
            FUNC_PTR(CreateVariableArgs *, fnc);
            create_variable(new_ptr->args->name, current_scope);
          } else if (fnc->type == PtrType::SetVariable) {
            FUNC_PTR(SetVariableArgs *, fnc);
            set_variable(new_ptr->args->name, new_ptr->args->value,
                         current_scope);
          }
        }
      }
      break;
    }
  } else if (is_starbytes_string(_instance_refer)) {
    StarbytesString *str = (StarbytesString *)_instance_refer;
    // String Class Methods
    if (name == "append") {
      if (is_starbytes_string(*args[0])) {
        str->append((StarbytesString *)*args[0]);
      }
    }
  } else if (is_starbytes_number(_instance_refer)) {
    StarbytesNumber *num = (StarbytesNumber*)_instance_refer;
    if (name == "add") {
      if (is_starbytes_number(*args[0])) {
        num->add((StarbytesNumber *)*args[0]);
      }
    } else if (name == "subtract") {
      if (is_starbytes_number(*args[0])) {
        num->subtract((StarbytesNumber *)*args[0]);
      }
    }
  } else if (is_starbytes_array(_instance_refer)) {
    StarbytesArray<StarbytesObject *> *arr =
        (StarbytesArray<StarbytesObject *> *)_instance_refer;
    if (name == "push") {
      arr->push(*args[0]);
    }
  }
  // else if(is_starbytes_dictionary(_instance_refer)){
  //     StarbytesDictionary<StarbytesString *,StarbytesObject *> *dict =
  //     (StarbytesDictionary<StarbytesString *,StarbytesObject *>
  //     *)_instance_refer;
  //     //Make new function!!
  //     if(name == "set"){
  //         dict->set((StarbytesString *) *args[0],*args[1]);
  //     }
  // }
}

StarbytesObject *invoke_instance_method_return_guaranteed(
    StarbytesObject *_instance_refer, std::string name,
    std::vector<StarbytesObject **> args, Scope *&current_scope) {
  StarbytesObject *return_ptr = nullptr;
  if (_instance_refer->isType(SBObjectType::ClassInstance)) {
    StarbytesClassInstance *ptr = (StarbytesClassInstance *)_instance_refer;
    for (auto &m : ptr->methods) {
      if (m->func->name == name) {
        // Assign Args!
        for (int i = 0; i < m->func->args.size(); ++i) {
          m->func->args[i]->val = *args[i];
        }
        //
        for (auto fnc : m->func->body) {
          if (fnc->type == PtrType::CreateFunc) {
            FUNC_PTR(CreateFunctionArgs *, fnc);
            create_function(new_ptr->args->name, new_ptr->args->args,
                            new_ptr->args->body, current_scope);
          } else if (fnc->type == PtrType::InvokeFunc) {
            FUNC_PTR(InvokeFunctionArgs *, fnc);
            invoke_function(new_ptr->args->name, new_ptr->args->args,
                            current_scope);
          } else if (fnc->type == PtrType::CreateVariable) {
            FUNC_PTR(CreateVariableArgs *, fnc);
            create_variable(new_ptr->args->name, current_scope);
          } else if (fnc->type == PtrType::SetVariable) {
            FUNC_PTR(SetVariableArgs *, fnc);
            set_variable(new_ptr->args->name, new_ptr->args->value,
                         current_scope);
          } else if (fnc->type == PtrType::ReturnFunc) {
            FUNC_PTR(ReturnFuncArgs *, fnc);
            return_ptr = new_ptr->args->obj_to_return;
          }
        }
      }
      break;
    }
  } else if (is_starbytes_dictionary(_instance_refer)) {
    StarbytesDictionary<StarbytesString *, StarbytesObject *> *dict =
        (StarbytesDictionary<StarbytesString *, StarbytesObject *> *)
            _instance_refer;
    // Make new function!!
    if (name == "set") {
      return_ptr = create_starbytes_boolean(
          dict->set((StarbytesString *)*args[0], *args[1]));
    }
  }
  return return_ptr;
}

StarbytesObject *clone_object(StarbytesObject *_obj) {
  StarbytesObject *ptr = nullptr;
  if (_obj->isType(SBObjectType::String)) {
    ptr = create_starbytes_string(((StarbytesString *)_obj)->__get_interal());
  } else if (_obj->isType(SBObjectType::Array)) {
    std::vector<StarbytesObject *> vals;
    for (auto &it :
         ((StarbytesArray<StarbytesObject *> *)_obj)->getIterator()) {
      vals.push_back(clone_object(it));
    }
    ptr = new StarbytesArray(vals);
  } else if (_obj->isType(SBObjectType::Number)) {
    StarbytesNumber * ptr = (StarbytesNumber *)_obj;
    int num_ty = ptr->get_dyn_num_type();
    if(num_ty == 0){
      int * val = (int *)ptr->__get_interal();
      ptr = create_starbytes_number(*val, num_ty);
    }
    else if(num_ty == 1){
      float * val = (float *)ptr->__get_interal();
      ptr = create_starbytes_number(*val,num_ty);
    }
    else if(num_ty == 2){
      long * val = (long *)ptr->__get_interal();
      ptr = create_starbytes_number(*val,num_ty);
    }
    else if(num_ty == 3){
      double * val = (double *)ptr->__get_interal();
      ptr = create_starbytes_number(*val,num_ty);
    }
    
  }
  return ptr;
}

// StoredFunction * clone_function(StoredFunction * _func){

// }

// StarbytesClassInstance * create_instance_of_class(std::string
// type,std::string name,std::initializer_list<StarbytesObject *> args){
//     StarbytesClassInstance *cl_inst = new StarbytesClassInstance(name,type);
//     std::vector a (args);
//     for(auto & cl : current_scope->stored_classes){
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

void internal_if(StarbytesBoolean *_bool, std::string _func_to_invoke,
                 std::vector<StarbytesObject **> args, Scope *&current_scope) {
  if (_bool->__get_interal()) {
    invoke_function(_func_to_invoke, args, current_scope);
  } else {
    return;
  }
}

// StarbytesBoolean * convert_conditional_to_bool(){

// };
using namespace ByteCode;

class BCEngine {
private:
  std::vector<Engine::Scope *> scopes;
  std::vector<std::string> scope_heirarchy;

  class TempAllocator {
      private:
        struct AllocEntry {
          StarbytesObject *& obj;
          std::string & temp_obj_name;
          AllocEntry(std::string & name,StarbytesObject *& obj_ptr):obj(obj_ptr),temp_obj_name(name){};
        };
        std::vector<AllocEntry *> objects;
      public:
        void alloc_object(std::string & name,StarbytesObject * obj_ptr){
          objects.push_back(new AllocEntry(name,obj_ptr));
        };
        StarbytesObject *& refer_object(std::string & name){
          for(auto & e : objects){
            if(e->temp_obj_name == name){
              return e->obj;
            }
          }
        };
        bool dealloc_object(std::string & name){
          bool returncode = false;
          for(int i = 0;i < objects.size();++i){
            AllocEntry * _a_entry = objects[i];
            if(_a_entry->temp_obj_name == name){
              delete _a_entry;
              objects.erase(objects.begin()+i);
              returncode = true;
            }
          }
          return returncode;
        };
        void dealloc_all_objects(){
          for(int i = 0;i < objects.size();++i){
            AllocEntry * _a_entry = objects[i];
            delete _a_entry;
          }
          objects.clear();
        };
  };
  std::vector<StarbytesObject **> args_alloc;

  BCProgram *& program;
  unsigned currentindex = 0;
  BCUnit *& nextUnit(){
      return program->units[++currentindex];
  };
  /*Gets ahead unit but does not move `currentindex`*/
  BCUnit *& aheadUnit(){
      return program->units[currentindex + 1];
  };
  BCUnit *& currentUnit(){
      return program->units[currentindex];
  };
  BCUnit *& behindUnit(){
      return program->units[currentindex-1];
  };


  void set_current_scope(std::string &_scope_name) {
    scope_heirarchy.push_back(_scope_name);
  }

  void remove_current_scope() { scope_heirarchy.pop_back(); }

  std::string & get_current_scope() { return scope_heirarchy.front(); }

  void create_scope(std::string name) {
    Engine::Scope *c = new Engine::Scope(name);
    scopes.push_back(c);
  }

  Engine::Scope * get_scope(std::string name) {
    for (auto &s : scopes) {
      if (s->getName() == name) {
        return s;
      }
    }
    return nullptr;
  }

  void read_bc_crtvr(){
    BCString * bc_str = static_cast<BCString *>(nextUnit());
    create_variable(bc_str->value,get_scope(get_current_scope()));
    BCCodeEnd * end = static_cast<BCCodeEnd *>(nextUnit());
    if(end->code_node_name != "crtvr")
      throw "Runtime Engine Error: Bytecode does not match!";
  };

  void read_bc_stvr(){

  };

  void read_bc_ivkfn(){
    BCString * bc_str = static_cast<BCString *>(nextUnit());
    BCVectorBegin *bc_vec_bg = static_cast<BCVectorBegin *>(nextUnit());
    for(int i = 0; i < bc_vec_bg->length;++i){
        BCReference * ref = static_cast<BCReference *>(nextUnit());
        StarbytesObject ** ref_ptr;
        if(ref->ref_type == ByteCode::BCRefType::Indirect)
          ref_ptr = refer_variable_a(ref->var_name,get_scope(get_current_scope()));
        else if (ref->ref_type == ByteCode::BCRefType::Direct)
         throw "Runtime Engine Error: Can't use Direct Reference in this context!";
        args_alloc.push_back(ref_ptr);
      //TODO: Analyze args!
    };
    BCVectorEnd *bc_vec_ed = static_cast<BCVectorEnd *>(nextUnit());
    invoke_function(bc_str->value,args_alloc,get_scope(get_current_scope()));
    BCCodeEnd * end = static_cast<BCCodeEnd *>(nextUnit());
    if(end->code_node_name != "ivkfn")
      throw "Runtime Engine Error: Bytecode does not match!";
  };

  void read_bc_code_unit(BCCodeBegin * unit_begin){
      std::string & subject = unit_begin->code_node_name;
      if(subject == "crtvr"){
          read_bc_crtvr();
      }
      else if(subject == "stvr"){
          read_bc_stvr();
      }
      else if(subject == "ivkfn"){
          read_bc_ivkfn();
      }
  };

  void read_bc_vector_unit(){

  };

  void read_starting_bc_unit(BCUnit *& unit){
      if(BC_UNIT_IS(unit,BCCodeBegin)){
          read_bc_code_unit(ASSERT_BC_UNIT(unit,BCCodeBegin));
      }
  };

public:
  BCEngine(BCProgram *& executable):program(executable){};
  ~BCEngine(){};
  void read(){
      std::cout << "STARBYTES_RUNTIME_ENGINE -->\x1b[33mStarting Executable: " << program->program_name << "\x1b[0m" << std::endl;
      auto & current_bc_unit = currentUnit();
      while(true){
          current_bc_unit = nextUnit();
          read_starting_bc_unit(current_bc_unit);

      }
  };
};

void _internal_exec_bc_program(BCProgram *&executable) {
  BCEngine(executable).read();
};

// void Program(){
//     create_scope("GLOBAL");
//     set_current_scope("GLOBAL");
//     /*
//         decl Var = "Hello! Advanced Starbytes!"
//         decl Some = "Extension!"
//         Var.append(Some)
//         func TestFunc(message:Printable) {
//             Log(Color.red(message))
//         }
//         TestFunc(Var)
//         TestFunc(["Another Message!","Other Message!"])
//     */
//     create_variable("Var");
//     set_variable("Var",create_starbytes_string("Hello! Advanced
//     Starbytes!")); create_variable("Some");
//     set_variable("Some",create_starbytes_string(" + Some Extension!"));
//     invoke_instance_method(refer_variable_d("Var"),"append",{refer_variable_a("Some")});
//     std::vector<StoredVariable<> *> * func_args =
//     create_function_args({"Message"});
//     create_function("TestFunc",*func_args,{
//         create_func_ptr(PtrType::InvokeFunc,create_invoke_function_args("Log",{refer_arg(func_args,"Message")}))
//         });
//     invoke_function("TestFunc",{refer_variable_a("Var")});
//     create_variable("Array");
//     set_variable("Array",create_starbytes_array({create_starbytes_string("Another
//     Message!"),create_starbytes_string("Other Message!")}));
//     invoke_function("TestFunc",{refer_variable_a("Array")});
//     StarbytesObject *n = create_starbytes_number(10);
//     invoke_instance_method(refer_variable_d("Array"),"push",{ &n });
//     invoke_function("TestFunc",{refer_variable_a("Array")});
//     create_variable("Map");
//     set_variable("Map",create_starbytes_dictionary({StarbytesDictEntry(create_starbytes_string("test"),create_starbytes_number(123))}));
//     invoke_function("TestFunc",{refer_variable_a("Map")});
//     StarbytesObject *a = create_starbytes_string("otherKey");
//     StarbytesObject *b = create_starbytes_boolean(false);
//     StarbytesObject *temp =
//     invoke_instance_method_return_guaranteed(refer_variable_d("Map"),"set",{
//     &a,&b}); internal_if((StarbytesBoolean *)temp,"TestFunc",{
//     refer_variable_a("Map")}); //invoke_function("TestFunc",{
//     refer_variable_a("Map")});
//     // std::vector<StoredVariable<> *> * func_args0 =
//     create_function_args({});
//     //
//     create_function("SomeOtherFunc",*func_args0,{create_func_ptr(PtrType::ReturnFunc,create_return_func_args(create_starbytes_number(3)))});
//     // create_variable("test");
//     // std::vector<StarbytesObject **> args2;
//     //
//     set_variable("test",invoke_function_return_guaranteed("SomeOtherFunc",args2));
//     // std::vector<StarbytesObject **> args3 {refer_variable_a("test")};
//     // invoke_function("TestFunc",args3);
// }

}; // namespace Engine

NAMESPACE_END