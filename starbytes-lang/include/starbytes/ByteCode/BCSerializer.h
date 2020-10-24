#include "BCDef.h"
#include <sstream>
#include <fstream>

#ifndef BYTECODE_BCSERIALIZER_H
#define BYTECODE_BCSERIALIZER_H

STARBYTES_BYTECODE_NAMESPACE

void serializeBCProgram(std::ostringstream & output_stream,BCProgram *program);

void deserializeBCProgram(std::ifstream & input,BCProgram *& program_loc);

NAMESPACE_END

#endif