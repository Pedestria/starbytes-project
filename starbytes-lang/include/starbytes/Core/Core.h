#include "starbytes/Parser/Parser.h"
#include "starbytes/Base/Document.h"
#include "starbytes/ByteCode/BCDef.h"


#ifndef CORE_CORE_H
#define CORE_CORE_H

STARBYTES_STD_NAMESPACE

AbstractSyntaxTree * parseCode(std::string & code);
void runBC(ByteCode::BCProgram *& prog);


NAMESPACE_END

#endif