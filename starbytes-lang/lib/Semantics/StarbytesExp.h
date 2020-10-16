#include "starbytes/AST/AST.h"
#include "starbytes/Semantics/Scope.h"
#include "TypeCheck.h"

#ifndef SEMANTICS_STARBYTESEXP_H
#define SEMANTICS_STARBYTESEXP_H

STARBYTES_SEMANTICS_NAMESPACE

class SemanticA;

using namespace AST;

#define AST_EXPRESSION_EVALUATOR(type) STBType * evaluate##type(type * node_ty,SemanticA *sem)

AST_EXPRESSION_EVALUATOR(ASTExpressionStatement);

AST_EXPRESSION_EVALUATOR(ASTExpression);

AST_EXPRESSION_EVALUATOR(ASTCallExpression);

AST_EXPRESSION_EVALUATOR(ASTMemberExpression);

AST_EXPRESSION_EVALUATOR(ASTArrayExpression);


AST_EXPRESSION_EVALUATOR(ASTStringLiteral);

AST_EXPRESSION_EVALUATOR(ASTBooleanLiteral);

AST_EXPRESSION_EVALUATOR(ASTNumericLiteral);

#undef AST_EXPRESSION_EVALUATOR


NAMESPACE_END

#endif