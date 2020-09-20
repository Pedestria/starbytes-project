#pragma once
#include <initializer_list>
#include <vector>
#include <string>
#include "Document.h"

namespace Starbytes {
    namespace AST {
        enum class ASTType:int{
            Identifier,TypecastIdentifier,ImportDeclaration,ScopeDeclaration,NumericLiteral,VariableDeclaration,VariableSpecifier,ConstantDeclaration,ConstantSpecifier,StringLiteral,FunctionDeclaration,BlockStatement,
            TypeIdentifier,AssignExpression,ArrayExpression,NewExpression,MemberExpression,CallExpression,ClassDeclaration,ClassPropertyDeclaration,ClassBlockStatement,ClassMethodDeclaration,ClassConstructorDeclaration,
            ClassConstructorParameterDeclaration,TypeArgumentsDeclaration,InterfaceDeclaration,InterfaceBlockStatement,InterfacePropertyDeclaration,InterfaceMethodDeclaration,ReturnDeclaration,EnumDeclaration,EnumBlockStatement,
            Enumerator,IfDeclaration,ElseIfDeclaration,ElseDeclaration,BooleanLiteral,AwaitExpression
        };
        /*Abstract Syntax Tree Node*/
        struct ASTNode {
            DocumentPosition position;
            ASTType type;
        };
        struct ASTLiteral : ASTNode {};
        struct ASTStringLiteral : ASTLiteral {
            std::string value;
        };
        struct ASTNumericLiteral :ASTLiteral {
            std::string value;
        };

        struct ASTBooleanLiteral : ASTLiteral {
            std::string value;
        };

        struct ASTStatement {
            DocumentPosition BeginFold;
            DocumentPosition EndFold;
            ASTType type;
        };
        struct ASTIdentifier : ASTNode {
            std::string value;
        };
        struct ASTTypeIdentifier : ASTStatement {
            std::string type_name;
            bool isGeneric;
            std::vector<ASTTypeIdentifier *> typeargs;
            bool isArrayType;
            unsigned int array_count = 0;  
        };
        struct ASTTypeCastIdentifier : ASTStatement {
            ASTIdentifier *id;
            ASTTypeIdentifier *tid;
        };
        struct ASTExpression : ASTStatement {
        
        };
        struct ASTExpressionStatement : ASTStatement {
            ASTExpression* expression;
        };
        struct ASTCallExpression : ASTExpression {
            ASTExpression * callee;
            std::vector<ASTExpression *> params;
        };
        struct ASTNewExpression : ASTExpression {
            ASTTypeIdentifier* decltid;
            std::vector<ASTExpression *> params;
        };
        struct ASTArrayExpression : ASTExpression {
            std::vector<ASTExpression *> items;
        };
        struct ASTAssignExpression : ASTExpression {
            ASTExpression* subject;
            std::string op;
            ASTExpression* object;
        };
        struct ASTMemberExpression : ASTExpression {
            ASTExpression *object;
            ASTExpression *prop;
        };

        struct ASTUnaryExpression : ASTExpression {
            std::string oprtr;
            ASTExpression *left;
            ASTExpression *right;
        };
        struct ASTAwaitExpression : ASTExpression {
            ASTCallExpression *callee;
        };
        struct ASTBlockStatement : ASTStatement {
            std::vector<ASTStatement*> nodes;
        };
        struct ASTDeclaration : ASTStatement {};
        struct ASTTypeArgumentsDeclaration : ASTDeclaration {
            std::vector<ASTIdentifier *> args;
        };
        struct ASTClassStatement : ASTStatement {};
        struct ASTClassConstructorParameterDeclaration : ASTStatement {
            bool loose;
            bool isConstant;
            ASTTypeCastIdentifier *tcid;
        };
        struct ASTClassConstructorDeclaration : ASTClassStatement {
            std::vector<ASTClassConstructorParameterDeclaration *> params;
        };
        struct ASTClassPropertyDeclaration : ASTClassStatement {
            bool loose;
            bool isConstant;
            //EITHER a ASTIdentifier or ASTTypeCastIdentifier
            ASTNode *id;
            ASTExpression *initializer;
        };
        struct ASTClassMethodDeclaration : ASTClassStatement {
            ASTIdentifier *id;
            bool isGeneric;
            bool isLazy;
            ASTTypeArgumentsDeclaration *typeargs;
            std::vector<ASTTypeCastIdentifier*> params;
            ASTTypeIdentifier* returnType;
            ASTBlockStatement* body;
        };
        struct ASTClassBlockStatement : ASTStatement {
            std::vector<ASTClassStatement *> nodes;
        };
        struct ASTClassDeclaration : ASTDeclaration {
            ASTIdentifier * id;
            bool isGeneric;
            ASTTypeArgumentsDeclaration *typeargs;
            bool isChildClass;
            ASTTypeIdentifier * superclass;
            bool implementsInterfaces;
            std::vector<ASTTypeIdentifier *> interfaces;
            ASTClassBlockStatement *body;
        };
        struct ASTInterfaceStatement : ASTStatement {};

        struct ASTReturnDeclaration : ASTStatement {
            ASTExpression *returnee;
        };

        struct ASTEnumStatement : ASTStatement {};
        struct ASTEnumerator : ASTEnumStatement {
            ASTIdentifier *id;
            bool hasValue;
            ASTNumericLiteral *value;
        };

        struct ASTEnumBlockStatement : ASTStatement {
            std::vector<ASTEnumerator *> nodes;
        };
        struct ASTEnumDeclaration : ASTStatement {
            ASTIdentifier *id;
            ASTEnumBlockStatement *body;
        };
        struct ASTInterfacePropertyDeclaration : ASTInterfaceStatement {
            bool loose;
            bool isConstant;
            ASTTypeCastIdentifier *tcid;
        };
        struct ASTInterfaceMethodDeclaration : ASTInterfaceStatement {
            ASTIdentifier *id;
            bool isGeneric;
            bool isLazy;
            ASTTypeArgumentsDeclaration *typeargs;
            std::vector<ASTTypeCastIdentifier*> params;
            ASTTypeIdentifier *returnType;
        };
        struct ASTInterfaceBlockStatement : ASTStatement {
            std::vector<ASTInterfaceStatement *> nodes;
        };
        struct ASTInterfaceDeclaration : ASTDeclaration {
            ASTIdentifier * id;
            bool isGeneric;
            ASTTypeArgumentsDeclaration *typeargs;
            bool isChildInterface;
            ASTTypeIdentifier *superclass;
            ASTInterfaceBlockStatement *body;

        };

        struct ASTIfDeclaration : ASTDeclaration {
            ASTExpression *subject;
            ASTBlockStatement *body;
        };
        struct ASTElseIfDeclaration : ASTDeclaration {
            ASTExpression *subject;
            ASTBlockStatement *body;
        };

        struct ASTElseDeclaration : ASTDeclaration {
            ASTBlockStatement *body;
        };
        struct ASTVariableSpecifier : ASTStatement {
            //EITHER a ASTIdentifier or ASTTypeCastIdentifier
            ASTNode *id;
            ASTExpression *initializer;
        };
        struct ASTVariableDeclaration : ASTDeclaration {
            std::vector<ASTVariableSpecifier*> specifiers;
        };
        struct ASTConstantSpecifier : ASTStatement {
            //EITHER a ASTIdentifier or ASTTypeCastIdentifier
            ASTNode *id;
            ASTExpression *initializer;
        };
        struct ASTConstantDeclaration : ASTDeclaration {
            std::vector<ASTConstantSpecifier *> specifiers;
        };
        struct ASTImportDeclaration : ASTDeclaration {
            ASTIdentifier *id;
        };

        struct ASTScopeDeclaration : ASTDeclaration {
            ASTIdentifier *id;
            ASTBlockStatement* body;
        };
        struct ASTFunctionDeclaration : ASTDeclaration {
            ASTIdentifier *id;
            bool isAsync;
            bool isGeneric;
            ASTTypeArgumentsDeclaration *typeargs;
            std::vector<ASTTypeCastIdentifier*> params;
            ASTTypeIdentifier *returnType;
            ASTBlockStatement* body;
        };
        
        struct AbstractSyntaxTree {
            std::string filename;
            std::vector<ASTStatement*> nodes;
        };
        template <typename NODE = ASTNode>
        struct WalkPath {
          NODE * currentNode; 
          
        };

        struct ASTCallbackEntry {
            ASTType type;
            void (*callback)(WalkPath<> *);
        };

        void WalkAST(AbstractSyntaxTree *tree,std::initializer_list<ASTCallbackEntry> callbacks);
    };

}