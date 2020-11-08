
#include "Token.h"
#include <vector>
#include "starbytes/Base/Document.h"
#include "starbytes/AST/AST.h"
#include "starbytes/Base/Base.h"
#include <iostream>

#ifndef PARSER_PARSER_H
#define PARSER_PARSER_H


STARBYTES_STD_NAMESPACE
    using namespace AST;

    TYPED_ENUM Scope:int {
        Global,BlockStatement,FunctionBlockStatement
    };
    class Parser{
        private:
            /*Pointer to AST*/
            AbstractSyntaxTree *Treeptr;
            unsigned currentIndex;
            std::vector<Token> & tokens;
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
            void parseComment(ASTObject *& ptr);
            void parseLineComment(ASTLineComment *&ptr);
            void parseBlockComment(ASTBlockComment *&ptr);

            inline void checkForComments(ASTObject * ptr);
            
            inline void parseIdentifier(Token * token1,ASTIdentifier *&id);
            inline void parseTypeIdentifier(ASTTypeIdentifier *&ptr);
            void parseTypecastIdentifier(ASTTypeCastIdentifier *&ptr);

            //DECLARATIONS!
            void parseAcquireDeclaration(std::vector<ASTStatement *> *& container);
            void parseVariableSpecifier(ASTVariableSpecifier *& ptr);
            void parseVariableDeclaration(std::vector<ASTStatement *> *& container);
            void parseConstantSpecifier(ASTConstantSpecifier *& ptr);
            void parseConstantDeclaration(std::vector<ASTStatement *>  *&container);
            void parseImportDeclaration(std::vector<ASTStatement *> *&container);
            void parseScopeDeclaration(std::vector<ASTStatement *> *&container);
            void parseTypeArgsDeclaration(ASTTypeArgumentsDeclaration *&ptr);
            void parseIfDeclaration(std::vector<ASTStatement *> *&container);
            void parseElseIfDeclaration(std::vector<ASTStatement *> *&container);
            void parseElseDeclaration(std::vector<ASTStatement *> *&container);
            void parseReturnDeclaration(std::vector<ASTStatement *> *& container);
            void parseFunctionDeclaration(std::vector<ASTStatement *> *&container,bool isLazy);
            void parseEnumDeclaration(std::vector<ASTStatement *> *& container);
            void parseEnumBlockStatement(ASTEnumBlockStatement *& ptr);
            void parseEnumerator(ASTEnumerator * ptr);
            void parseInterfaceDeclaration(std::vector<ASTStatement *> *& container);
            void parseInterfaceBlockStatement(ASTInterfaceBlockStatement *& ptr);
            void parseInterfaceStatement(std::vector<ASTInterfaceStatement *> * container);
            void parseInterfacePropertyDeclaration(ASTInterfacePropertyDeclaration * ptr,bool immutable,bool loose = false);
            void parseInterfaceMethodDeclaration(ASTInterfaceMethodDeclaration *& ptr);
            void parseClassDeclaration(std::vector<ASTStatement *> *& container);
            void parseClassBlockStatement(ASTClassBlockStatement *ptr);
            void parseClassStatement(std::vector<ASTClassStatement *> * container);
            void parseClassConstructorDeclaration(ASTClassConstructorDeclaration * ptr);
            void parseClassConstructorParameterDeclaration(ASTClassConstructorParameterDeclaration *& ptr,bool immutable,bool loose = false);
            void parseClassPropertyDeclaration(ASTClassPropertyDeclaration *&ptr,bool immutable,bool loose = false);
            void parseClassMethodDeclaration(ASTClassMethodDeclaration *&ptr);
            /*Parses all Declaration and places them in container. (Container is used for scope declarations)*/
            bool parseDeclaration(std::vector<ASTStatement *> *&container,Scope scope);
            void parseBlockStatement(ASTBlockStatement *&ptr,Scope scope);

            //EXPRESSIONS!
            void parseNewExpression(ASTNewExpression *&ptr);
            void parseArrayExpression(ASTArrayExpression *&ptr);
            void parseAwaitExpression(ASTAwaitExpression *&ptr);
            void parseAssignExpression(ASTAssignExpression *&ptr);
            void parseCallExpression(ASTCallExpression *&ptr,ASTExpression *&callee);
            void parseMemberExpression(ASTMemberExpression *&ptr,ASTExpression *&object);
            bool parseExpressionStatement(std::vector<ASTStatement *> *&container);
            ASTExpression * parseExpression();
            void parseStatement(std::vector<ASTStatement *> * container,Scope scope);
            void parseNumericLiteral(ASTNumericLiteral *&ptr);
            void parseStringLiteral(ASTStringLiteral *&ptr);
        public:
            Parser(std::vector<Token> & _tokens,AbstractSyntaxTree * tree) : tokens(_tokens), currentIndex(0), Treeptr(tree) {};
            void convertToAST();

    };
NAMESPACE_END

#endif