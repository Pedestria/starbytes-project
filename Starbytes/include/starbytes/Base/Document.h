#include "starbytes/Base/Base.h"

#ifndef AST_DOCUMENT_H
#define AST_DOCUMENT_H

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