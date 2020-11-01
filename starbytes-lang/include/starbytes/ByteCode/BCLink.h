
#include "starbytes/Base/Base.h"
#include "BCDef.h"
#include <vector>

STARBYTES_BYTECODE_NAMESPACE


void dyn_link_lib_modules(BCProgram *exec_or_lib_target,std::vector<BCProgram *> &modules);

NAMESPACE_END