#include "BCDef.h"
#include "starbytes/AST/AST.h"

#ifndef BYTECODE_BCGENERATOR_H
#define BYTECODE_BCGENERATOR_H

STARBYTES_BYTECODE_NAMESPACE

BCProgram * generateToBCProgram(AST::AbstractSyntaxTree *ast);

NAMESPACE_END

#endif