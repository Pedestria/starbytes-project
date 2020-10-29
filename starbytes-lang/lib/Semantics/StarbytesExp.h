#include "starbytes/AST/AST.h"
#include "starbytes/Semantics/Scope.h"
#include "TypeCheck.h"

#ifndef SEMANTICS_STARBYTESEXP_H
#define SEMANTICS_STARBYTESEXP_H

STARBYTES_SEMANTICS_NAMESPACE

class SemanticA;

using namespace AST;

#define AST_EXPRESSION_EVALUATOR(type) SemanticSymbol* evaluate##type(type * node_ty,SemanticA *& sem)
#define AST_VISITOR(name,node_to_visit) class name { SemanticA *sem; using NODE = AST::node_to_visit;public:name(SemanticA *s);~name();void visit(NODE *node); }


AST_EXPRESSION_EVALUATOR(ASTCallExpression);

AST_EXPRESSION_EVALUATOR(ASTMemberExpression);

AST_EXPRESSION_EVALUATOR(ASTArrayExpression);

AST_EXPRESSION_EVALUATOR(ASTStringLiteral);

AST_EXPRESSION_EVALUATOR(ASTBooleanLiteral);

AST_EXPRESSION_EVALUATOR(ASTNumericLiteral);

AST_EXPRESSION_EVALUATOR(ASTExpression);

AST_VISITOR(ExprStatementVisitor,ASTExpressionStatement);


#undef AST_EXPRESSION_EVALUATOR


NAMESPACE_END

#endif