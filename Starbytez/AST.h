#pragma once
#include <vector>
#include <string>
#include "Document.h"

namespace Starbytes {
    namespace AST {
        enum class ASTType:int{
            Identifier,Typecast,ImportDeclaration,ScopeDeclaration
        };
        enum class NumericLiteralType {
            Integer,Float,Long,Double
        };
        /*Abstract Syntax Tree Node*/
        struct ASTNode {
            DocumentPosition position;
            ASTType type;
        };
        struct ASTLiteral {

        };
        struct ASTStringLiteral {
            std::string value;
        };
        struct ASTNumericLiteral {
            std::string numericvalue;
            NumericLiteralType type;
        };
        struct ASTStatement : ASTNode {};
        struct ASTIdentifier : ASTNode {
            std::string value;
        };
        struct ASTDeclaration : ASTStatement {};
        struct ASTImportDeclaration : ASTDeclaration {
            std::vector<ASTIdentifier> specifiers;
        };
        struct ASTTypeCast : ASTNode {
            ASTIdentifier id;
            ASTIdentifier type;
        };
        struct ASTScopeDeclaration : ASTDeclaration {
            ASTIdentifier id;
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
        struct AbstractSyntaxTree {
            std::string filename;
            std::vector<ASTStatement> nodes;
        };
    };

}