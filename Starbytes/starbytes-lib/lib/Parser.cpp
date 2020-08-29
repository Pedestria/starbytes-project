#include "Parser.h"
#include "AST.h"
#include <string>

using namespace Starbytes;
using namespace AST;

enum class KeywordType:int {
    Scope,Import,Variable,Immutable,Class,Function,Interface,Alias,Type,Return,If,Else
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
    } else if(subject == "decl"){
        returncode = KeywordType::Variable;
    } else if(subject == "immutable"){
        returncode = KeywordType::Immutable;
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
            if(aheadToken()->getType() == TokenType::Keyword && matchKeyword(aheadToken()->getContent()) == KeywordType::Immutable){
                parseConstantDeclaration();
            }else {
                 parseVariableDeclaration();
            }
            break;
        default :
            throw StarbytesParseError("Unresolved Keyword!",currentToken()->getPosition());
            break;
    }
}

void Parser::parseImportDeclaration(){
    ASTImportDeclaration *node = new ASTImportDeclaration();
    node->BeginFold = currentToken()->getPosition();
    node->type = ASTType::ImportDeclaration;
    Token *tok1 = nextToken();
    if(tok1->getType() == TokenType::Identifier){
        ASTIdentifier *id = new ASTIdentifier();
        parseIdentifier(tok1,id);
        node->id = id;
        node->EndFold = tok1->getPosition();
        Treeptr->nodes.push_back(node);
    }
    else {
        throw StarbytesParseError("Expected Identifier!",tok1->getPosition());
    }
}
//NEEDS TO BE REVIEWED!!
void Parser::parseConstantDeclaration(){
    ASTConstantDeclaration *node = new ASTConstantDeclaration();
    node->type = ASTType::ConstantDeclaration;
    //Begins at '[HERE]decl immutable value = 4'
    node->BeginFold = currentToken()->getPosition();
    Token *tok = nextToken();
    if(tok->getType() == TokenType::Identifier){
         ASTConstantSpecifier *node0 = new ASTConstantSpecifier();
        //CurrentToken: Keyword before Id!
        parseConstantSpecifier(node0);
        node->specifiers.push_back(node0);
        //CurrentToken: EXPRESSION_RELATED_KEYWORD before Comma or OTHER_TOKEN
        if(aheadToken()->getType() == TokenType::Comma){
            while(true){
                if(aheadToken()->getType() == TokenType::Comma){
                    ASTConstantSpecifier * node1 = new ASTConstantSpecifier();
                    parseConstantSpecifier(node1);
                    node->specifiers.push_back(node1);
                } else {
                    break;
                }
            }
        }
        //CurrentToken: before OTHER_Statement
        node->EndFold = currentToken()->getPosition();

    }else {
        throw StarbytesParseError("Expected Identifer!",tok->getPosition());
    }

}

void Parser::parseConstantSpecifier(ASTConstantSpecifier *ptr){
    ptr->type = ASTType::ConstantSpecifier;
    Token * tok = nextToken();
    ptr->BeginFold = tok->getPosition();
    //CurrentToken: ID after keyword: `immutable`.
    if(tok->getType() == TokenType::Identifier){
        //CurrentToken: ID Before Colon or Equals
        if(aheadToken()->getType() == TokenType::Typecast){
            ASTTypeCastIdentifier *tid = new ASTTypeCastIdentifier();
            ASTNode *id = (ASTNode *) tid;
            //CurrentToken: ID before Colon
            parseTypecastIdentifier(tid);
            //CurrentToken: ID After Colon Just Before Equals
        }
        else {
            throw StarbytesParseError("Expected Colon!",aheadToken()->getPosition());
        }

        Token *tok3 = nextToken();
        //CurrentToken: ID Before Equals
        if(tok3->getType() == TokenType::Operator && tok3->getContent() == "="){
            //PARSE EXPRESSION! OR LITERAL!
        }
        else {
            throw StarbytesParseError("Expected `=` Operator! (Constants require assignment)",tok3->getPosition());
        }
    }else {
        throw StarbytesParseError("Expected Identifer!",tok->getPosition());
    }
}

void Parser::parseVariableDeclaration(){
    ASTVariableDeclaration *node = new ASTVariableDeclaration();
    node->type = ASTType::VariableDeclaration;
    node->BeginFold = currentToken()->getPosition();
    Token *tok1 = aheadToken();
    if(tok1->getType() == TokenType::Identifier){
  
        ASTVariableSpecifier *node0 = new ASTVariableSpecifier();
        //CurrentToken: Keyword before ID!
        parseVariableSpecifier(node0);
        node->specifiers.push_back(node0);
        //CurrentToken: Before Comma or OTHER_TOKEN
        if(aheadToken()->getType() == TokenType::Comma){
            while(true){
                if(aheadToken()->getType() == TokenType::Comma){
                    ASTVariableSpecifier *node1 = new ASTVariableSpecifier();
                    parseVariableSpecifier(node1);
                    node->specifiers.push_back(node1);
                    //CurrentToken: Before Comma
                }
                else {
                    break;
                }
            }
        }
        //CurrentToken: Before OTHER_TOKEN
        node->EndFold = currentToken()->getPosition();

    } else {
        throw StarbytesParseError("Expected Identifer!",tok1->getPosition());
    }
}
void Parser::parseVariableSpecifier(ASTVariableSpecifier *ptr){
    Token *tok = nextToken();
    ptr->type = ASTType::VariableSpecifier;
    ptr->BeginFold = tok->getPosition();
    //CurrentToken: ID after Comma or Keyword 'decl'!
    if(tok->getType() == TokenType::Identifier){
        if(aheadToken()->getType() == TokenType::Typecast){
            ASTTypeCastIdentifier *id0 = new ASTTypeCastIdentifier();
            ASTNode *id = (ASTNode*) id0;
            parseTypecastIdentifier(id0);
            ptr->id = id;
        }else if(aheadToken()->getContent() != "="){
            throw StarbytesParseError("Cannot imply type without Assignment",aheadToken()->getPosition());
        }
        else if (aheadToken()->getContent() == "=") {
            ASTIdentifier *id0 = new ASTIdentifier();
            ASTNode *id = (ASTNode *) id0;
            parseIdentifier(tok, id0);
            ptr->id = id;
        }
        Token *tok1 = nextToken();
        if(tok1->getType() == TokenType::Operator &&tok1->getContent() == "="){
            //PARSE EXPRESSION! OR LITERAL!
        }
        
    }
    //Current token: Before Comma or OTHER_TOKEN
}

void Parser::parseTypecastIdentifier(ASTTypeCastIdentifier * ptr){
    Token *tok = currentToken();
    Token *tok1 = nextToken();
    Token *tok2 = nextToken();
    if(tok->getType() == TokenType::Identifier){
        if (tok1->getType() == TokenType::Typecast && tok2->getType() == TokenType::Identifier){
            ptr->type = ASTType::TypecastIdentifier;
            //Start fold!
            ptr->BeginFold = tok->getPosition();
            ASTIdentifier *id1 = new ASTIdentifier();
            parseIdentifier(tok,id1);
            ptr->id = id1;
            ASTIdentifier *id2 = new ASTIdentifier();
            parseIdentifier(tok2,id2);
            ptr->tid = id2;
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