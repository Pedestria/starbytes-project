#include "BCDef.h"
#include "starbytes/AST/AST.h"
#include <vector>

#ifndef BYTECODE_BCGENERATOR_H
#define BYTECODE_BCGENERATOR_H

STARBYTES_BYTECODE_NAMESPACE



BCProgram * generateToBCProgram(std::vector<AST::AbstractSyntaxTree *> & module_sources);

NAMESPACE_END

#endif