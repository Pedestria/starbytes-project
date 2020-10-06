#include "starbytes/Base/Base.h"
#include "starbytes/AST/AST.h"
#include "starbytes/Semantics/Scope.h"

#ifndef SEMANTICS_VISITORS_STARBYTESDECL_H
#define SEMANTICS_VISITORS_STARBYTESDECL_H

STARBYTES_SEMANTICS_NAMESPACE

void visitVariableDecl(AST::ASTVariableDeclaration *node,ScopeStore *store);

NAMESPACE_END

#endif

