#include "starbytes/AST/AST.h"
#include "starbytes/Base/Base.h"
#include "Scope.h"

#ifndef SEMANTICS_SEMANTICS_MAIN_H
#define SEMANTICS_SEMANTICS_MAIN_H

STARBYTES_SEMANTICS_NAMESPACE

using namespace AST;
        
    class STBType;
    struct STBObjectMethod;
    struct STBObjectProperty;
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
            template<typename _Node>
            friend inline void construct_methods_and_props(std::vector<STBObjectMethod> *methods,std::vector<STBObjectProperty> *props,_Node *node,SemanticA *& sem);
            //Friends AST Evaluator functions!
            FRIEND_AST_EVALUATOR(ASTCallExpression);
            FRIEND_AST_EVALUATOR(ASTMemberExpression);
            FRIEND_AST_EVALUATOR(ASTStringLiteral);
            FRIEND_AST_EVALUATOR(ASTBooleanLiteral);
            FRIEND_AST_EVALUATOR(ASTNumericLiteral);
            FRIEND_AST_EVALUATOR(ASTArrayExpression);
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