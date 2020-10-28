#include "STDObjects.h"
#include "starbytes/Base/Base.h"
#include "starbytes/ByteCode/BCDef.h"
#include <any>
#include <optional>
#include <string>
#include <vector>

#ifndef INTERPRETER_RUNTIME_ENGINE_H
#define INTERPRETER_RUNTIME_ENGINE_H

STARBYTES_INTERPRETER_NAMESPACE
namespace Engine {
TYPED_ENUM PtrType : int{CreateFunc,  InvokeFunc,         CreateVariable,
                         SetVariable, CreateFunctionArgs, ReturnFunc};
template <typename ARGTYPE = std::any> struct InternalFuncPtr : RuntimeObject {
  PtrType type;
  ARGTYPE args;
};

template <typename _ValType = StarbytesObject *> struct StoredVariable {
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
  StarbytesClassInstance(std::string _name, std::string _type)
      : StarbytesObject(SBObjectType::ClassInstance), name(_name),
        type(_type){};
  std::string name;
  std::string type;
  std::vector<StarbytesClassProperty *> properties;
  std::vector<StarbytesClassMethod *> methods;
};

// Data Types/Data Type Instance Methods Implementations

// CORE RUNTIME ENGINE!!!

struct CreateFunctionArgs {
  std::string name;
  std::vector<StoredVariable<> *> args;
  std::initializer_list<InternalFuncPtr<> *> body;
};

struct InvokeFunctionArgs {
  std::string name;
  std::vector<StarbytesObject **> args;
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

ReturnFuncArgs *create_return_func_args(StarbytesObject *__obj);

CreateVariableArgs *create_create_variable_args(std::string _name);

SetVariableArgs *create_set_variable_args(std::string _name,
                                          StarbytesObject *_value);

InvokeFunctionArgs *
create_invoke_function_args(std::string _name,
                            std::initializer_list<StarbytesObject **> _args);

class Scope {
private:
  std::string name;

public:
  std::vector<StoredFunction *> stored_functions;
  std::vector<StoredVariable<> *> stored_variables;
  std::vector<StarbytesClass *> stored_classes;
  std::string &getName() { return name; }
  Scope(std::string &_name) : name(_name){};
  ~Scope(){};
};
/*Passes Program to BCEngine*/
void _internal_exec_bc_program(ByteCode::BCProgram *& executable);

}; // namespace Engine
NAMESPACE_END

#endif
