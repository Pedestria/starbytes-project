#pragma once

#include "Token.h"
#include <vector>
#include "Document.h"
#include "AST.h"

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
            /*Move to next Token*/
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
            void parseTypecastIdentifier(ASTTypeCastIdentifier *ptr);
            void parseVariableSpecifier(ASTVariableSpecifier * ptr);
            void parseVariableDeclaration();
            void parseImportDeclaration();
            void parseDeclaration();
            void parseExpression();
            void parseStatement();
        public:
            Parser(std::vector<Token>& _tokens,AbstractSyntaxTree * tree) : tokens(_tokens), currentIndex(0), Treeptr(tree) {};
            void convertToAST();
    };
}