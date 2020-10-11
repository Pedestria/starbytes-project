#include "starbytes/Base/Base.h"

#ifndef BYTECODE_BCDEF_H
#define BYTECODE_BCDEF_H

STARBYTES_BYTECODE_NAMESPACE

struct BCProgram {};

TYPED_ENUM BCType:int {
    Identifier,Digit
};

struct BCUnit {};

struct BCIdentifier : BCUnit {
    static BCType type;
};

struct BCDigit : BCUnit {
    static BCType type;
};

NAMESPACE_END

#endif