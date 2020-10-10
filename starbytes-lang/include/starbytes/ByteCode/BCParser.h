#include "BCDef.h"
#include <string>

#ifndef BYTECODE_BCPARSER_H
#define BYTECODE_BCPARSER_H

STARBYTES_BYTECODE_NAMESPACE

BCProgram * parseToProgram(std::string & file);

NAMESPACE_END

#endif