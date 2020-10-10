#include "BCDef.h"
#include <sstream>

#ifndef BYTECODE_BCSERIALIZER_H
#define BYTECODE_BCSERIALIZER_H

STARBYTES_BYTECODE_NAMESPACE

void serializeBCProgram(std::ostringstream & output_stream,BCProgram *program);

NAMESPACE_END

#endif