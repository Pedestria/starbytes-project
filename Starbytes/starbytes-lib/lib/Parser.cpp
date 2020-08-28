#include "Parser.h"
#include "AST.h"
#include <string>

using namespace Starbytes;
using namespace AST;

enum class KeywordType:int {
    Scope,Import,Variable,Constant,Class,Function,Interface,Alias,Type,Return,If,Else
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
    } else if(subject == "class"){
        returncode = KeywordType::Class;
    } else if(subject == "interface"){
        returncode = KeywordType::Interface;
    } else if(subject == "return"){
        returncode = KeywordType::Return;
    } else if(subject == "deftype"){
        returncode = KeywordType::Type;
    } else if(subject == "if"){
        returncode = KeywordType::If;
    } else if(subject == "else"){
        returncode = KeywordType::Else;
    }
    return returncode;
}

void Parser::convertToAST(){
    Token *tok = currentToken();
    while(currentIndex < tokens.size()){
        if(tok->getType() == TokenType::Keyword){
            parseDeclaration();
        } else if(tok->getType() == TokenType::Identifier){
            parseExpression();
        }
        
        if(currentIndex == tokens.size()-1){
            break;
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
    node.BeginFold = currentToken()->getPosition();
    node.type = ASTType::ImportDeclaration;
    Token *tok1 = nextToken();
    if(tok1->getType() == TokenType::Identifier){
        ASTIdentifier id;
        parseIdentifier(tok1,&id);
        node.id = id;
        node.EndFold = tok1->getPosition();
        Treeptr->nodes.push_back(node);
    }
    else {
        throw StarbytesParseError("Expected Identifier!",tok1->getPosition());
    }
}
void Parser::parseVariableDeclaration(){
    ASTVariableDeclaration node;
    node.type = ASTType::VariableDeclaration;
    node.BeginFold = currentToken()->getPosition();
    Token *tok1 = aheadToken();
    if(tok1->getType() == TokenType::Identifier){
  
        ASTVariableSpecifier node0;
        //CurrentToken: Keyword before Id!
        parseVariableSpecifier(&node0);
        node.decls.push_back(node0);
        //Semicolon, Comma, or Else;
        if (currentToken()->getType() == TokenType::Comma) {
            Token* t = aheadToken();
            while (true) {
                if (t->getType() == TokenType::Identifier) {
                    ASTVariableSpecifier node1;
                    parseVariableSpecifier(&node1);
                    node.decls.push_back(node1);
                }
                if (t->getType() == TokenType::Semicolon) {
                    break;
                }
                t = nextToken();
            }
        }
        else if (currentToken()->getType() == TokenType::Semicolon) {
            node.EndFold = currentToken()->getPosition();
        }
        else {
            node.EndFold = behindToken()->getPosition();
        }
        nextToken();
        //Current Token: (NOT A SEMICOLON OR COMMA!)
    } else {
        throw StarbytesParseError("Expected Identifer!",tok1->getPosition());
    }
}
void Parser::parseVariableSpecifier(ASTVariableSpecifier *ptr){
    Token *tok = nextToken();
    ptr->BeginFold = tok->getPosition();
    if(tok->getType() == TokenType::Identifier){
        if(aheadToken()->getType() == TokenType::Typecast){
            ASTNode id;
            //Try dyanmic cast!
            ASTTypeCastIdentifier * id0 = (ASTTypeCastIdentifier *) &id;
            parseTypecastIdentifier(id0);
            ptr->id = id;
        }else if(aheadToken()->getContent() != "="){
            throw StarbytesParseError("Cannot imply type without Assignment",aheadToken()->getPosition());
        }
        else if (aheadToken()->getContent() == "=") {
            ASTNode id;
            //Try dyanmic cast!
            ASTIdentifier* id0 = (ASTIdentifier*)&id;
            parseIdentifier(tok, id0);
            ptr->id = id;
        }
        Token *tok1 = nextToken();
        if(tok1->getType() == TokenType::Operator &&tok1->getContent() == "="){
            Token* tok2 = nextToken();
            if (tok2->getType() == TokenType::Identifier || tok2->getType() == TokenType::Numeric || tok2->getType() == TokenType::Quote) {
                //Parse Ids, Numbers, Quotes Here!
            }
        }
        
    }
    //Current token: SemiColon or Other thing.
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

void Parser::parseExpression(){

}

void Parser::parseIdentifier(Token *token1, ASTIdentifier * id) {
    id->position = token1->getPosition();
    id->type = ASTType::Identifier;
    id->value = token1->getContent();
}