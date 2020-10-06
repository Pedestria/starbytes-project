#include "Macros.h"

#ifndef BASE_DOCUMENT_H
#define BASE_DOCUMENT_H

STARBYTES_STD_NAMESPACE
    /*A Position on A Starbytes Document. Used to determine Positions of Tokens/ASTNodes/etc.
    */
    struct DocumentPosition {
        unsigned int line;
        unsigned int start;
        unsigned int end;
        unsigned int raw_index;
    };
NAMESPACE_END

#endif