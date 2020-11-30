#include "starbytes/Base/Base.h"
#include "starbytes/AST/AST.h"
#include "starbytes/AST/ASTTraveler.h"

#ifndef GEN_GENDECL_H
#define GEN_GENDECL_H

STARBYTES_GEN_NAMESPACE

using namespace AST;

class CodeGenR;

ASTVisitorResponse genVarDecl(ASTTravelContext & context,CodeGenR *ptr);

NAMESPACE_END

#endif