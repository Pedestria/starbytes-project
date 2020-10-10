#include "starbytes/Base/Base.h"
#include "starbytes/ByteCode/BCDef.h"

#ifndef INTERPRETER_BCREADER_H
#define INTERPRETER_BCREADER_H

STARBYTES_INTERPRETER_NAMESPACE

void execBCProgram(ByteCode::BCProgram *program);

NAMESPACE_END

#endif