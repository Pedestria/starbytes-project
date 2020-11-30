#include "starbytes/Base/Base.h"
#include "starbytes/AST/AST.h"
#include "starbytes/AST/ASTTraveler.h"

#ifndef GEN_GENEXPR_H
#define GEN_GENEXPR_H

STARBYTES_GEN_NAMESPACE

using namespace AST;

class CodeGenR;

void genStrLiteral(ASTStringLiteral * node,CodeGenR *ptr);
void genBoolLiteral(ASTBooleanLiteral * node,CodeGenR *ptr);
void genExpr(ASTExpression *exp,CodeGenR * ptr);

NAMESPACE_END

#endif