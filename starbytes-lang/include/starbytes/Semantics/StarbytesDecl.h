#include "starbytes/Base/Base.h"
#include "starbytes/AST/AST.h"
#include "starbytes/Semantics/Scope.h"

#ifndef SEMANTICS_VISITORS_STARBYTESDECL_H
#define SEMANTICS_VISITORS_STARBYTESDECL_H

STARBYTES_SEMANTICS_NAMESPACE

class SemanticA;

class VariableDeclVisitor {
    using NODE = AST::ASTVariableDeclaration;
    public:
    VariableDeclVisitor(){};
    ~VariableDeclVisitor(){};
    void visit(SemanticA *s,NODE *node);
};

NAMESPACE_END

#endif

