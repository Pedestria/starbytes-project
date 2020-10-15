#include "starbytes/AST/AST.h"
#include "starbytes/Base/Base.h"
#include "starbytes/Semantics/Scope.h"

#ifndef SEMANTICS_SEMANTICS_MAIN_H
#define SEMANTICS_SEMANTICS_MAIN_H

STARBYTES_SEMANTICS_NAMESPACE
        
    class SemanticA {
        using Tree = AST::AbstractSyntaxTree;

        private:
            Tree *& tree;
            ScopeStore store;
            // template<typename _Visitor>
            // void _visit_visitor(AST::ASTNode *&node){
            //     _Visitor().visit(this,node);
            // };
            void createScope(std::string & name);
            void registerSymbolInScope(std::string & scope,SemanticSymbol *symbol);
            void registerSymbolinExactCurrentScope(SemanticSymbol *symbol);
            friend class ScopeDeclVisitor;
            friend class VariableDeclVisitor;
            friend class FunctionDeclVisitor;
            friend class ClassDeclVisitor;
            
        public:
            void freeSymbolStores();
            void initialize();
            
            void visitNode(AST::ASTNode * node);
            SemanticA(Tree *& _tree):tree(_tree){};
            ~SemanticA(){};
    };

    inline std::string StarbytesSemanticsError(std::string message,DocumentPosition & pos){
        return "SemanticsError:\n"+message.append("\nThis Error Has Occured at {Line:"+std::to_string(pos.line)+",Start:"+std::to_string(pos.start)+",End:"+std::to_string(pos.end)+"}");
    };

NAMESPACE_END

#endif