#pragma once
#include <vector>
#include <string>
#include "Document.h"

namespace Starbytes {
    namespace AST {
        enum class ASTType:int{
            Identifier,Typecast,ImportDeclaration,ScopeDeclaration,NumericLiteral,VariableDeclaration
        };
        enum class NumericLiteralType {
            Integer,Float,Long,Double
        };
        /*Abstract Syntax Tree Node*/
        struct ASTNode {
            DocumentPosition position;
            ASTType type;
        };
        struct ASTLiteral : ASTNode {

        };
        struct ASTStringLiteral {
            std::string value;
        };
        struct ASTNumericLiteral {
            std::string numericvalue;
            NumericLiteralType type;
        };
        struct ASTStatement {
            DocumentPosition BeginFold;
            DocumentPosition EndFold;
            ASTType type;
        };
        struct ASTIdentifier : ASTNode {
            std::string value;
        };
        struct ASTExpression : ASTNode {
        
        };
        struct ASTExpressionStatement : ASTStatement {
            ASTExpression expression;
        };
        
        struct ASTAssignExpression : ASTExpression {
            ASTExpression subject;
            std::string op;
            ASTExpression objective;
        };
        struct ASTMemberExpression : ASTExpression {
            ASTExpression subject;
            ASTIdentifier prop;
        };
        struct ASTDeclaration : ASTStatement {};

        struct ASTVariableSpecifier : ASTStatement {
            //To Be Downcasted!
            ASTNode id;
            ASTExpression initializer;
        };
        struct ASTVariableDeclaration : ASTStatement {
            std::vector<ASTVariableSpecifier> decls;
        };
        struct ASTImportDeclaration : ASTDeclaration {
            ASTIdentifier id;
        };
        struct ASTTypeCastIdentifier : ASTStatement {
            ASTIdentifier id;
            ASTIdentifier type;
        };
        struct ASTScopeDeclaration : ASTDeclaration {
            ASTIdentifier id;
        };
        
        struct AbstractSyntaxTree {
            std::string filename;
            std::vector<ASTStatement> nodes;
        };
    };

}