#pragma once

#include "Token.h"
#include <vector>
#include "Document.h"
#include "AST.h"
#include <iostream>

namespace Starbytes {
    using namespace AST;
    typedef unsigned int UInt;
    class Parser{
        private:
            /*Pointer to AST*/
            AbstractSyntaxTree *Treeptr;
            UInt currentIndex;
            std::vector<Token> tokens;
            Token * currentToken(){
                return &tokens[currentIndex];
            }
            /*Move to next Token but does NOT return Token*/
            void incrementToNextToken() {
                ++currentIndex;
            }
            /*Move to next Token. Returns Token*/
            Token* nextToken() {
                return &tokens[++currentIndex];
            };
            /*Get ahead token (Does not affect currentindex.)*/
            Token* aheadToken() {
                return &tokens[currentIndex + 1];
            };
            /*Get token n times ahead of current token (Does not affect currentindex.)*/
            Token * aheadNToken(int n){
                return &tokens[currentIndex+n];
            }
            /*Get behind token (Does not affect currentindex.)*/
            Token* behindToken() {
                return &tokens[currentIndex - 1];
            }
            void parseIdentifier(Token* token1,ASTIdentifier *id);
            void parseTypeIdentifier(ASTTypeIdentifer *ptr);
            void parseTypecastIdentifier(ASTTypeCastIdentifier *ptr);
            void parseVariableSpecifier(ASTVariableSpecifier * ptr);
            void parseVariableDeclaration(std::vector<ASTStatement *> * container);
            void parseConstantSpecifier(ASTConstantSpecifier * ptr);
            void parseConstantDeclaration(std::vector<ASTStatement *>  *container);
            void parseImportDeclaration(std::vector<ASTStatement *> *container);
            void parseScopeDeclaration(std::vector<ASTStatement *> *container);
            void parseFunctionDeclaration(std::vector<ASTStatement *> *container);
            /*Parses all Declaration and places them in container. (Container is used for scope declarations)*/
            void parseDeclaration(std::vector<ASTStatement *> *container);
            void parseBlockStatement(ASTBlockStatement *ptr);
            void parseExpression();
            void parseStatement();
            void parseNumericLiteral(ASTNumericLiteral *ptr);
            void parseStringLiteral(ASTStringLiteral *ptr);
        public:
            Parser(std::vector<Token>& _tokens,AbstractSyntaxTree * tree) : tokens(_tokens), currentIndex(0), Treeptr(tree) {};
            void convertToAST();

    };
}