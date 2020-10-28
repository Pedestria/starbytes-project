#include "starbytes/Interpreter/BCReader.h"
#include "RuntimeEngine.h"
#include "starbytes/Base/Macros.h"
#include <vector>

STARBYTES_INTERPRETER_NAMESPACE

using namespace ByteCode;

void execBCProgram(BCProgram *&program){
  Engine::_internal_exec_bc_program(program);
};

NAMESPACE_END