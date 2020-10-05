
#include <initializer_list>
#include <vector>
#include <string>
#include "starbytes/Base/Base.h"

#ifndef AST_AST_H
#define AST_AST_H 


STARBYTES_STD_NAMESPACE
    namespace AST {

        #define ASTDECL(name) struct name : ASTDeclaration

        
        TYPED_ENUM ASTType:int{
            Identifier,TypecastIdentifier,ImportDeclaration,ScopeDeclaration,NumericLiteral,VariableDeclaration,VariableSpecifier,ConstantDeclaration,ConstantSpecifier,StringLiteral,FunctionDeclaration,BlockStatement,
            TypeIdentifier,AssignExpression,ArrayExpression,NewExpression,MemberExpression,CallExpression,ClassDeclaration,ClassPropertyDeclaration,ClassBlockStatement,ClassMethodDeclaration,ClassConstructorDeclaration,
            ClassConstructorParameterDeclaration,TypeArgumentsDeclaration,InterfaceDeclaration,InterfaceBlockStatement,InterfacePropertyDeclaration,InterfaceMethodDeclaration,ReturnDeclaration,EnumDeclaration,EnumBlockStatement,
            Enumerator,IfDeclaration,ElseIfDeclaration,ElseDeclaration,BooleanLiteral,AwaitExpression,ExpressionStatement
        };

        TYPED_ENUM CommentType:int {
            LineComment,BlockComment
        };

        struct ASTComment {
            CommentType type;
        };

        struct ASTLineComment : ASTComment{
            std::string value;
        };

        struct ASTBlockComment : ASTComment {
            unsigned int line_num;
            std::vector<std::string> lines;
        };

        struct ASTObject {
            std::vector<ASTComment *> preceding_comments;
        };
        /*Abstract Syntax Tree Node*/
        struct ASTNode : ASTObject {
            DocumentPosition position;
            ASTType type;
        };
        struct ASTLiteral : ASTNode {};
        struct ASTStringLiteral : ASTLiteral {
            static ASTType static_type;
            std::string value;
        };
        struct ASTNumericLiteral :ASTLiteral {
            static ASTType static_type;
            std::string value;
        };

        struct ASTBooleanLiteral : ASTLiteral {
            static ASTType static_type;
            std::string value;
        };

        struct ASTStatement : ASTObject {
            DocumentPosition BeginFold;
            DocumentPosition EndFold;
            ASTType type;
        };
        struct ASTIdentifier : ASTNode {
            static ASTType static_type;
            std::string value;
        };
        struct ASTTypeIdentifier : ASTStatement {
            static ASTType static_type;
            std::string type_name;
            bool isGeneric;
            std::vector<ASTTypeIdentifier *> typeargs;
            bool isArrayType;
            unsigned int array_count = 0;  
        };
        struct ASTTypeCastIdentifier : ASTStatement {
            static ASTType static_type;
            ASTIdentifier *id;
            ASTTypeIdentifier *tid;
        };
        struct ASTExpression : ASTStatement {
        
        };
        struct ASTExpressionStatement : ASTStatement {
            static ASTType static_type;
            ASTExpression* expression;
        };
        struct ASTCallExpression : ASTExpression {
            static ASTType static_type;
            ASTExpression * callee;
            std::vector<ASTExpression *> params;
        };
        struct ASTNewExpression : ASTExpression {
            static ASTType static_type;
            ASTTypeIdentifier* decltid;
            std::vector<ASTExpression *> params;
        };
        struct ASTArrayExpression : ASTExpression {
            static ASTType static_type;
            std::vector<ASTExpression *> items;
        };
        struct ASTAssignExpression : ASTExpression {
            static ASTType static_type;
            ASTExpression* subject;
            std::string op;
            ASTExpression* object;
        };
        struct ASTMemberExpression : ASTExpression {
            static ASTType static_type;
            ASTExpression *object;
            ASTExpression *prop;
        };

        struct ASTUnaryExpression : ASTExpression {
            static ASTType static_type;
            std::string oprtr;
            ASTExpression *left;
            ASTExpression *right;
        };
        struct ASTAwaitExpression : ASTExpression {
            static ASTType static_type;
            ASTCallExpression *callee;
        };
        struct ASTBlockStatement : ASTStatement {
            static ASTType static_type;
            std::vector<ASTStatement*> nodes;
        };
        struct ASTDeclaration : ASTStatement {};
        struct ASTTypeArgumentsDeclaration : ASTDeclaration {
            static ASTType static_type;
            std::vector<ASTIdentifier *> args;
        };
        struct ASTClassStatement : ASTStatement {};
        struct ASTClassConstructorParameterDeclaration : ASTStatement {
            static ASTType static_type;
            bool loose;
            bool isConstant;
            ASTTypeCastIdentifier *tcid;
        };
        struct ASTClassConstructorDeclaration : ASTClassStatement {
            static ASTType static_type;
            std::vector<ASTClassConstructorParameterDeclaration *> params;
        };
        struct ASTClassPropertyDeclaration : ASTClassStatement {
            static ASTType static_type;
            bool loose;
            bool isConstant;
            //EITHER a ASTIdentifier or ASTTypeCastIdentifier
            ASTNode *id;
            ASTExpression *initializer;
        };
        struct ASTClassMethodDeclaration : ASTClassStatement {
            static ASTType static_type;
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
        ASTDECL(ASTClassDeclaration) {
            static ASTType static_type;
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
            static ASTType static_type;
            ASTExpression *returnee;
        };

        struct ASTEnumStatement : ASTStatement {};
        struct ASTEnumerator : ASTEnumStatement {
            static ASTType static_type;
            ASTIdentifier *id;
            bool hasValue;
            ASTNumericLiteral *value;
        };

        struct ASTEnumBlockStatement : ASTStatement {
            static ASTType static_type;
            std::vector<ASTEnumerator *> nodes;
        };
        struct ASTEnumDeclaration : ASTStatement {
            static ASTType static_type;
            ASTIdentifier *id;
            ASTEnumBlockStatement *body;
        };
        struct ASTInterfacePropertyDeclaration : ASTInterfaceStatement {
            static ASTType static_type;
            bool loose;
            bool isConstant;
            ASTTypeCastIdentifier *tcid;
        };
        struct ASTInterfaceMethodDeclaration : ASTInterfaceStatement {
            static ASTType static_type;
            ASTIdentifier *id;
            bool isGeneric;
            bool isLazy;
            ASTTypeArgumentsDeclaration *typeargs;
            std::vector<ASTTypeCastIdentifier*> params;
            ASTTypeIdentifier *returnType;
        };
        struct ASTInterfaceBlockStatement : ASTStatement {
            static ASTType static_type;
            std::vector<ASTInterfaceStatement *> nodes;
        };
        ASTDECL(ASTInterfaceDeclaration) {
            static ASTType static_type;
            ASTIdentifier * id;
            bool isGeneric;
            ASTTypeArgumentsDeclaration *typeargs;
            bool isChildInterface;
            ASTTypeIdentifier *superclass;
            ASTInterfaceBlockStatement *body;

        };

        ASTDECL(ASTIfDeclaration) {
            static ASTType static_type;
            ASTExpression *subject;
            ASTBlockStatement *body;
        };
        ASTDECL(ASTElseIfDeclaration) {
            static ASTType static_type;
            ASTExpression *subject;
            ASTBlockStatement *body;
        };

        ASTDECL(ASTElseDeclaration) {
            static ASTType static_type;
            ASTBlockStatement *body;
        };
        struct ASTVariableSpecifier : ASTStatement {
            static ASTType static_type;
            //EITHER a ASTIdentifier or ASTTypeCastIdentifier
            ASTNode *id;
            ASTExpression *initializer;
        };
        ASTDECL(ASTVariableDeclaration) {
            static ASTType static_type;
            std::vector<ASTVariableSpecifier*> specifiers;
        };
        struct ASTConstantSpecifier : ASTStatement {
            static ASTType static_type;
            //EITHER a ASTIdentifier or ASTTypeCastIdentifier
            ASTNode *id;
            ASTExpression *initializer;
        };
        ASTDECL(ASTConstantDeclaration) {
            static ASTType static_type;
            std::vector<ASTConstantSpecifier *> specifiers;
        };
        ASTDECL(ASTImportDeclaration) {
            static ASTType static_type;
            ASTIdentifier *id;
        };

        ASTDECL(ASTScopeDeclaration) {
            static ASTType static_type;
            ASTIdentifier *id;
            ASTBlockStatement* body;
        };
        ASTDECL(ASTFunctionDeclaration) {
            static ASTType static_type;
            ASTIdentifier *id;
            bool isAsync;
            bool isGeneric;
            ASTTypeArgumentsDeclaration *typeargs;
            std::vector<ASTTypeCastIdentifier*> params;
            ASTTypeIdentifier *returnType;
            ASTBlockStatement* body;
        };
        
        struct AbstractSyntaxTree {
            static ASTType static_type;
            std::string filename;
            std::vector<ASTStatement*> nodes;
        };
        template<class _NodeTy>
        inline bool astnode_is(ASTNode *node){
            if(node->type == _NodeTy::static_type){
                return true;
            }
            else{
                return false;
            }
        }

        #define AST_NODE_CAST(ptr) ((ASTNode *)ptr)
        #define AST_NODE_IS(ptr,type) (astnode_is<type>(AST_NODE_CAST(ptr)))
        #define ASSERT_AST_NODE(ptr,type) ((type *)ptr)
    };

NAMESPACE_END

#endif