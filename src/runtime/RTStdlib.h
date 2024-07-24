#include "starbytes/compiler/ASTNodes.def"
#include "starbytes/compiler/RTCode.h"

#include "starbytes/base/ADT.h"

#ifndef STARBYTES_RT_RTSTDLIB_H
#define STARBYTES_RT_RTSTDLIB_H

namespace starbytes {
namespace Runtime::stdlib {


void print(StarbytesObject object,map<StarbytesClassType,std::string> &reg);


}
}

#endif
