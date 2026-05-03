#ifndef STARBYTES_RT_RTOPCODEMETA_H
#define STARBYTES_RT_RTOPCODEMETA_H

#include "starbytes/compiler/RTCode.h"
#include "starbytes/runtime/RTEngine.h"

namespace starbytes::Runtime {

bool isExpressionOpcode(RTCode code);
bool siteKindForOpcode(RTCode code, RuntimeProfileSiteKind &kindOut);

}

#endif
