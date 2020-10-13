#include "starbytes/ByteCode/BCEncodeDecodeFactory.h"

STARBYTES_BYTECODE_NAMESPACE

char & encodeCharacter(char &c){
    c += 50;
    return c;
};

unsigned & encodeDigit(unsigned &i){
    i = i << 1;
    return i;
};

char & decodeCharacter(char &c){
    c -= 50;
    return c;
};

unsigned & decodeDigit(unsigned &i){
    i = i >> 1;
    return i;
};

NAMESPACE_END
