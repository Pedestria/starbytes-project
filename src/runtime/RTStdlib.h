#include "starbytes/compiler/ASTNodes.def"
#include "starbytes/compiler/RTCode.h"

#include "starbytes/base/ADT.h"
#include <string>
#include <vector>

#ifndef STARBYTES_RT_RTSTDLIB_H
#define STARBYTES_RT_RTSTDLIB_H

namespace starbytes {
namespace Runtime::stdlib {


void print(StarbytesObject object,map<StarbytesClassType,std::string> &reg);
void addMathBuiltinTemplates(std::vector<RTFuncTemplate> &functions);
bool isMathBuiltinFunction(string_ref funcName);
StarbytesObject invokeMathBuiltinFunction(string_ref funcName,
                                          ArrayRef<StarbytesObject> args,
                                          std::string &lastRuntimeError);


}
}

#endif
