#include "starbytes/compiler/RTCode.h"
#include "starbytes/base/ADT.h"

#include <string>
#include <vector>

#ifndef STARBYTES_RUNTIME_STDLIBMATH_H
#define STARBYTES_RUNTIME_STDLIBMATH_H

namespace starbytes {
namespace Runtime::stdlib {

void addMathBuiltinTemplates(std::vector<RTFuncTemplate> &functions);
bool isMathBuiltinFunction(string_ref funcName);
StarbytesObject invokeMathBuiltinFunction(string_ref funcName,
                                          ArrayRef<StarbytesObject> args,
                                          std::string &lastRuntimeError);

}
}

#endif
