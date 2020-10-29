#include "starbytes/Base/Base.h"
#include "starbytes/AST/AST.h"
#include "starbytes/Semantics/Scope.h"

#ifndef SEMANTICS_STARBYTESDECL_H
#define SEMANTICS_STARBYTESDECL_H

STARBYTES_SEMANTICS_NAMESPACE

class SemanticA;

#define AST_VISITOR(name,node_to_visit) class name { SemanticA *sem; using NODE = AST::node_to_visit;public:name(SemanticA *s);~name();void visit(NODE *node); }

AST_VISITOR(ImportDeclVisitor,ASTImportDeclaration);



AST_VISITOR(ScopeDeclVisitor,ASTScopeDeclaration);

AST_VISITOR(VariableDeclVisitor,ASTVariableDeclaration);

AST_VISITOR(ConstantDeclVistior,ASTConstantDeclaration);

class ReturnDeclVisitor {
    SemanticA *sem;
    using NODE = AST::ASTReturnDeclaration;
    public:
    ReturnDeclVisitor(SemanticA *s);
    ~ReturnDeclVisitor();
    void visit(NODE *node,ASTNode *&func_ptr);
};

AST_VISITOR(FunctionDeclVisitor,ASTFunctionDeclaration);

AST_VISITOR(ClassDeclVisitor,ASTClassDeclaration);

#undef AST_VISITOR



NAMESPACE_END

#endif

