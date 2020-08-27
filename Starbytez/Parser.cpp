#include "Parser.h"
#include "AST.h"
#include <string>

using namespace Starbytes;
using namespace AST;

enum class KeywordType:int {
    Scope,Import,Variable,Constant,Class,Function
};

KeywordType matchKeyword(std::string subject) {
    KeywordType returncode;
    if(subject == "import"){
        returncode = KeywordType::Import;
    } else if(subject == "scope"){
        returncode = KeywordType::Scope;
    } else if(subject == "var"){
        returncode = KeywordType::Variable;
    } else if(subject == "const"){
        returncode = KeywordType::Constant;
    }
    return returncode;
}

void Parser::convertToAST(){
    Token *tok = &tokens[0];
    while(currentIndex < tokens.size()){
        if(tok->getType() == TokenType::Keyword){
            parseDeclaration();
        } else if(tok->getType() == TokenType::Identifier){
            parseExpression();
        }
        tok = nextToken();
    }
}

void Parser::parseDeclaration(){
    switch (matchKeyword(currentToken()->getContent())) {
        case KeywordType::Import :
            parseImportDeclaration();
            break;
        case KeywordType::Class :

        default :
            throw "Unresolved Keyword!";
            break;
    }
}

void Parser::parseImportDeclaration(){
    ASTImportDeclaration node;
}

void Parser::parseIdentifier(Token *token1, ASTIdentifier * id) {
    id->position = token1->getPosition();
    id->type = ASTType::Identifier;
    id->value = token1->getContent();
}