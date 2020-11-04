#include "starbytes/Base/Base.h"
#include "starbytes/AST/AST.h"


#ifndef GEN_GENEXP_H
#define GEN_GENEXP_H

STARBYTES_GEN_NAMESPACE

class CodeGenR;

#define AST_GEN_VISITOR(node_to_visit) using AST::node_to_visit; void visit##node_to_visit(node_to_visit * _node,CodeGenR * _gen)

AST_GEN_VISITOR(ASTExpression);

AST_GEN_VISITOR(ASTCallExpression);
AST_GEN_VISITOR(ASTArrayExpression);
AST_GEN_VISITOR(ASTAwaitExpression);
AST_GEN_VISITOR(ASTNewExpression);

AST_GEN_VISITOR(ASTExpressionStatement);


#undef AST_GEN_VISITOR

NAMESPACE_END

#endif