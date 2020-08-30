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
    } else if(subject == "func"){
        returncode = KeywordType::Function;
    }
    return returncode;
}

void Parser::convertToAST(){
    Token *tok = currentToken();
    while(currentIndex < tokens.size()){
        if(tok->getType() == TokenType::Keyword){
            parseDeclaration(&Treeptr->nodes);
        } else if(tok->getType() == TokenType::Identifier){
            parseExpression();
        }
        
        if(currentIndex == tokens.size()-1){
            break;
        }
        tok = nextToken();
    }
}
/*Parses all Declaration and places them in container. (Container is used for scope declarations)*/
void Parser::parseDeclaration(std::vector<ASTStatement *> * container){
    switch (matchKeyword(currentToken()->getContent())) {
        case KeywordType::Import :
            parseImportDeclaration(container);
            break;
        case KeywordType::Variable :
            if(aheadToken()->getType() == TokenType::Keyword && matchKeyword(aheadToken()->getContent()) == KeywordType::Immutable){
                parseConstantDeclaration(container);
            }else {
                 parseVariableDeclaration(container);
            }
            break;
        case KeywordType::Scope :
            parseScopeDeclaration(container);
            break;
        case KeywordType::Function :
            parseFunctionDeclaration(container);
            break;
        default :
            throw StarbytesParseError("Unresolved Keyword!",currentToken()->getPosition());
            break;
    }
}



void Parser::parseNumericLiteral(ASTNumericLiteral *ptr){
    Token * tok = nextToken();
    //CurrentToken: All of the Numeric or 1/3 of the numeric tokens!
    ptr->type = ASTType::NumericLiteral;
    ptr->position = tok->getPosition();
    if(tok->getType() == TokenType::Numeric){
        if(aheadToken()->getType() == TokenType::Dot){
            Token *tok1 = nextToken();
            Token *tok2 = nextToken();
            //SKIPPED TWO TOKENS!
            if(tok2->getType() == TokenType::Numeric){
                std::string numvalue;
                numvalue = tok->getContent().append(tok1->getContent()+tok2->getContent());
                ptr->numericvalue = numvalue;

            }
            else {
                throw StarbytesParseError("Expected Numeric!",tok2->getPosition());
            }

        }else{
            std::string numvalue;
            numvalue = tok->getContent();
            ptr->numericvalue = numvalue;
        }
        ptr->position.end = currentToken()->getPosition().end;
    } else{
        throw StarbytesParseError("Expected Numeric",tok->getPosition());
    }
}

void Parser::parseStringLiteral(ASTStringLiteral *ptr){
    ptr->type = ASTType::StringLiteral;
    Token* tok = nextToken();
    if(tok->getType() == TokenType::Quote){
        if(aheadToken()->getType() == TokenType::Quote){
            ptr->value = "";
            ptr->position = nextToken()->getPosition();
            ptr->position.end = nextToken()->getPosition().end;
        } else {
            ptr->value = nextToken()->getContent();
            ptr->position = currentToken()->getPosition();
            if(nextToken()->getType() == TokenType::Quote){
                return;
            } else {
                throw StarbytesParseError("Expected Quote",currentToken()->getPosition());
            }
        }
    }
    //Current Token: Last Quote Before OTHER_TOKEN
}

void Parser::parseImportDeclaration(std::vector<ASTStatement *> * container){
    ASTImportDeclaration *node = new ASTImportDeclaration();
    node->BeginFold = currentToken()->getPosition();
    node->type = ASTType::ImportDeclaration;
    Token *tok1 = nextToken();
    if(tok1->getType() == TokenType::Identifier){
        ASTIdentifier *id = new ASTIdentifier();
        parseIdentifier(tok1,id);
        node->id = id;
        node->EndFold = tok1->getPosition();
        container->push_back(node);
    }
    else {
        throw StarbytesParseError("Expected Identifier!",tok1->getPosition());
    }
}
void Parser::parseBlockStatement(ASTBlockStatement *ptr){
    ptr->type = ASTType::BlockStatement;
    Token *tok = nextToken();
    ptr->BeginFold = tok->getPosition();
    while(true){
        if(tok->getType() == TokenType::Keyword){
            parseDeclaration(&ptr->nodes);
        } else if(tok->getType() == TokenType::Identifier){
            parseExpression();
        }
        else if(tok->getType() == TokenType::CloseBrace){
            break;
        }
        tok = nextToken();
    }
    ptr->EndFold = currentToken()->getPosition();
}

void Parser::parseScopeDeclaration(std::vector<ASTStatement *> * container){
    //CurrentToken: Keyword before ID
    ASTScopeDeclaration *node = new ASTScopeDeclaration();
    node->BeginFold = currentToken()->getPosition();
    node->type = ASTType::ScopeDeclaration;
    Token *tok1 = nextToken();
    if(tok1->getType() == TokenType::Identifier){
        ASTIdentifier *id0 = new ASTIdentifier();
        parseIdentifier(tok1,id0);
        node->id = id0;
        if(aheadToken()->getType() == TokenType::OpenBrace){
            ASTBlockStatement *block = new ASTBlockStatement();
            //PARSE BODY AND STOP FOLD!
            parseBlockStatement(block);
            node->body = block;
            node->EndFold = currentToken()->getPosition();
            container->push_back(node);

            
        }
        else {
            throw StarbytesParseError("Expected Open Brace",currentToken()->getPosition());
        }
    }else{
        throw StarbytesParseError("Expected Identifier!",tok1->getPosition());
    }

}

//NEEDS TO BE REVIEWED!!
void Parser::parseConstantDeclaration(std::vector<ASTStatement *> * container){
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
        container->push_back(node);
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

void Parser::parseVariableDeclaration(std::vector<ASTStatement *> * container){
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
        container->push_back(node);
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
            if(aheadToken()->getType() == TokenType::Quote){
                ASTStringLiteral *node = new ASTStringLiteral();
                ASTExpression *cast = (ASTExpression*) node;
                parseStringLiteral(node);
                ptr->initializer = cast;
            }
        }
        //Token Last Token of expression before OTHER_TOKEN
        ptr->EndFold = currentToken()->getPosition();
        
    }
    //Current token: Before Comma or OTHER_TOKEN
}
//TODO: More Advanced Type Funcitonality Required! (Generics,Arrays,Dictionaries,etc...)
void Parser::parseFunctionDeclaration(std::vector<ASTStatement *> * container){
    ASTFunctionDeclaration *node = new ASTFunctionDeclaration();
    node->BeginFold = currentToken()->getPosition();
    node->type = ASTType::FunctionDeclaration;
    Token *tok = nextToken();
    if(tok->getType() == TokenType::Identifier){
        ASTIdentifier *id = new ASTIdentifier();
        parseIdentifier(tok,id);
        node->id = id;
        Token *tok0 = nextToken();
        if(tok0->getType() == TokenType::OpenParen){
            Token *tok1 = nextToken();
            if(tok1->getType() == TokenType::Identifier){
                ASTTypeCastIdentifier *ptr = new ASTTypeCastIdentifier();
                parseTypecastIdentifier(ptr);
                node->params.push_back(ptr);
                Token *tok2 = nextToken();
                while(true){
                    if(tok2->getType() == TokenType::Comma){
                        if(nextToken()->getType() == TokenType::Identifier){
                            ASTTypeCastIdentifier *ptr = new ASTTypeCastIdentifier();
                            parseTypecastIdentifier(ptr);
                            node->params.push_back(ptr);
                        }
                        else{
                            throw StarbytesParseError("Expected Identifier!",tok1->getPosition());
                        }
                    } else if(tok2->getType() == TokenType::CloseParen){
                        break;
                    }
                    tok2 = nextToken();
                }
                //CurrentToken: Close Parnethesi before Open Brace!
                Token *tok3 = aheadToken();
                if(tok3->getType() == TokenType::CloseCarrot){
                    incrementToNextToken();
                    Token *tok4 = nextToken();
                    if(tok4->getType() == TokenType::CloseCarrot){
                        //PARSE TYPED IDENTIFIER!
                        ASTTypeIdentifer *tid = new ASTTypeIdentifer();
                        parseTypeIdentifier(tid);
                        node->returnType = tid;
                    }else {
                        throw StarbytesParseError("Expected Close Carrot!",tok3->getPosition());
                    }
                }
                //CurrentToken: Before Block Statement Open Brace!
                if(aheadToken()->getType() == TokenType::OpenBrace){
                    ASTBlockStatement *block = new ASTBlockStatement();
                    parseBlockStatement(block);
                    node->body = block;
                    node->EndFold = currentToken()->getPosition();
                    container->push_back(node);
                    //CurrentToken: Close Brace before OTHER_TOKEN
                }
                else {
                    throw StarbytesParseError("Expected Open Brace!",tok3->getPosition());
                }
            }
            else if(tok0->getType() == TokenType::CloseParen){
                Token *tok2 = aheadToken();
                if(tok2->getType() == TokenType::OpenBrace){
                    ASTBlockStatement *block = new ASTBlockStatement();
                    parseBlockStatement(block);
                    node->body = block;
                    node->EndFold = currentToken()->getPosition();
                    container->push_back(node);
                }
                else {
                    throw StarbytesParseError("Expected Open Brace!",tok2->getPosition());
                }
            } else {
                throw StarbytesParseError("Expected Identifier or Close Paren!",tok0->getPosition());
            }
        }else{
            throw StarbytesParseError("Expected Open Paren!",tok0->getPosition());
        }
    }else {
        throw StarbytesParseError("Expected Identifier!",tok->getPosition());
    }
}

void Parser::parseTypecastIdentifier(ASTTypeCastIdentifier * ptr){
    Token *tok = currentToken();
    Token *tok1 = nextToken();
    Token *tok2 = aheadToken();
    if(tok->getType() == TokenType::Identifier){
        if (tok1->getType() == TokenType::Typecast && tok2->getType() == TokenType::Identifier){
            ptr->type = ASTType::TypecastIdentifier;
            //Start fold!
            ptr->BeginFold = tok->getPosition();
            ASTIdentifier *id1 = new ASTIdentifier();
            parseIdentifier(tok,id1);
            ptr->id = id1;
            ASTTypeIdentifer *tid = new ASTTypeIdentifer();
            parseTypeIdentifier(tid);
            ptr->tid = tid;
            //CurrentToken: Last Token of TYPE_ID Before OTHER_TOKEN
            //End Fold!
            ptr->EndFold = tok2->getPosition();
        } else {
            throw StarbytesParseError("Expected Type Identifier!",tok2->getPosition());
        }
    } else {
        throw StarbytesParseError("Expected Identifier!",tok->getPosition());
    }
    
}
//TODO: More Advanced Type Funcitonality Required! (Generics,Arrays,Dictionaries,etc...)
void Parser::parseTypeIdentifier(ASTTypeIdentifer * ptr){
    //CurrenToken: Before ID
    ptr->type = ASTType::TypeIdentifier;
    Token *tok = nextToken();
    ptr->BeginFold = tok->getPosition();
    if(tok->getType() == TokenType::Identifier){
        ptr->value = tok->getContent();
        ptr->EndFold = tok->getPosition();
    }
    else {
        throw StarbytesParseError("Expected Identifier!",tok->getPosition());
    }
}

void Parser::parseIdentifier(Token *token1, ASTIdentifier * id) {
    id->position = token1->getPosition();
    id->type = ASTType::Identifier;
    id->value = token1->getContent();
}

void Parser::parseExpression(){

}