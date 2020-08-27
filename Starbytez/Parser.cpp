#include "Parser.h"
#include "AST.h"
#include <string>

using namespace Starbytes;
using namespace AST;

enum class KeywordType:int {
    Scope,Import,Variable,Constant,Class,Function
};
/*Creates Error Message For Console*/
std::string StarbytesParseError(std::string message,DocumentPosition position){
    return message.append(" \n Error Occured at Position:{\nLine:"+std::to_string(position.line)+"\n Start Column:"+std::to_string(position.start)+"\n End Column:"+std::to_string(position.end)+"\n}");
}

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
        case KeywordType::Variable :
            parseVariableDeclaration();
            break;
        default :
            throw StarbytesParseError("Unresolved Keyword!",currentToken()->getPosition());
            break;
    }
}

void Parser::parseImportDeclaration(){
    ASTImportDeclaration node;
    node.type = ASTType::ImportDeclaration;
    Token *tok1 = nextToken();
    if(tok1->getType() == TokenType::Identifier){
        ASTIdentifier id;
        parseIdentifier(tok1,&id);
        node.id = id;
        Treeptr->nodes.push_back(node);
    }
    else {
        throw StarbytesParseError("Expected Identifier!",tok1->getPosition());
    }
}
void Parser::parseVariableDeclaration(){
    ASTVariableDeclaration node;
    node.type = ASTType::VariableDeclaration;
    Token *tok1 = aheadToken();
    if(tok1->getType() == TokenType::Identifier){
        ASTVariableSpecifier node0;
        parseVariableSpecifier(&node0);
        
    } else {
        throw StarbytesParseError("Expected Identifer!",tok1->getPosition());
    }
}
void Parser::parseVariableSpecifier(ASTVariableSpecifier *ptr){
    Token *tok = nextToken();
    if(tok->getType() == TokenType::Identifier){
        if(aheadToken()->getType() == TokenType::Typecast){
            ASTNode id;
            ASTTypeCastIdentifier * id0 = (ASTTypeCastIdentifier *) &id;
            parseTypecastIdentifier(id0);
        }else if(aheadToken()->getContent() != "="){
            throw StarbytesParseError("Cannot imply type without Assignment",aheadToken()->getPosition());
        }
        Token *tok1 = nextToken();
        if(tok1->getType() == TokenType::Operator &&tok1->getContent() == "="){
            if(tok1)
        }
    }
}

void Parser::parseTypecastIdentifier(ASTTypeCastIdentifier * ptr){
    Token *tok = currentToken();
    Token *tok1 = nextToken();
    Token *tok2 = nextToken();
    if(tok->getType() == TokenType::Identifier){
        if (tok1->getType() == TokenType::Typecast && tok2->getType() == TokenType::Identifier){
            //Start fold!
            ptr->BeginFold = tok->getPosition();
            ASTIdentifier id1;
            parseIdentifier(tok,&id1);
            ptr->id = id1;
            ASTIdentifier id2;
            parseIdentifier(tok2,&id2);
            ptr->type = id2;
            //End Fold!
            ptr->EndFold = tok2->getPosition();
        } else {
            throw StarbytesParseError("Expected Type Identifier!",tok2->getPosition());
        }
    } else {
        throw StarbytesParseError("Expected Identifier!",tok->getPosition());
    }
    
}

void Parser::parseIdentifier(Token *token1, ASTIdentifier * id) {
    id->position = token1->getPosition();
    id->type = ASTType::Identifier;
    id->value = token1->getContent();
}