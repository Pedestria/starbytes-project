#include "starbytes/AST/AST.h"
#include "starbytes/Base/Base.h"
#include "Scope.h"

#ifndef SEMANTICS_SEMANTICS_MAIN_H
#define SEMANTICS_SEMANTICS_MAIN_H

STARBYTES_SEMANTICS_NAMESPACE

using namespace AST;

#define FRIEND_AST_EVALUATOR(type) friend SemanticSymbol * evaluate##type(type * node_ty,SemanticA * sem)
        
    class SemanticA {
        using Tree = AbstractSyntaxTree;

        private:
            Tree *& tree;
            ScopeStore store;
            std::vector<std::string> modules;
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
            friend class ImportDeclVisitor;
            //Friends AST Evaluator functions!
            FRIEND_AST_EVALUATOR(ASTCallExpression);
            FRIEND_AST_EVALUATOR(ASTMemberExpression);
        public:
            void freeSymbolStores();
            void initialize();
            
            void visitNode(ASTNode * node);
            SemanticA(Tree *& _tree):tree(_tree){};
            ~SemanticA(){};
    };

    inline std::string StarbytesSemanticsError(std::string message,DocumentPosition & pos){
        return "SemanticsError:\n"+message.append("\nThis Error Has Occured at {Line:"+std::to_string(pos.line)+",Start:"+std::to_string(pos.start)+",End:"+std::to_string(pos.end)+"}");
    };

NAMESPACE_END

#endif