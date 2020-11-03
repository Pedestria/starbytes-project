#include "starbytes/Base/Base.h"
#include "starbytes/AST/AST.h"


#ifndef GEN_GENDECL_H
#define GEN_GENDECL_H

STARBYTES_GEN_NAMESPACE

class CodeGenR;

#define AST_GEN_VISITOR(node_to_visit) using AST::node_to_visit; void visit##node_to_visit(node_to_visit * _node,CodeGenR * _gen)

AST_GEN_VISITOR(ASTClassDeclaration);
AST_GEN_VISITOR(ASTFunctionDeclaration);
AST_GEN_VISITOR(ASTVariableDeclaration);




NAMESPACE_END

#endif
