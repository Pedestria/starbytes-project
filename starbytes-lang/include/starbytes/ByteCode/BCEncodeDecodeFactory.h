#include "starbytes/ByteCode/BCDef.h"

#ifndef BYTECODE_BCENCODE_H
#define BYTECODE_BCENCODE_H

STARBYTES_BYTECODE_NAMESPACE

char & encodeCharacter(char &c);
unsigned & encodeDigit(unsigned &i);

char & decodeCharacter(char &c);
unsigned & decodeDigit(unsigned &i);


NAMESPACE_END

#endif