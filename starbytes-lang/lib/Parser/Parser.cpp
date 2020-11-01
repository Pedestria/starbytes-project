#include "starbytes/Parser/Parser.h"
#include "starbytes/AST/AST.h"
#include "starbytes/Parser/Token.h"
#include <iostream>
#include <string>
#include <vector>

using namespace Starbytes;
using namespace AST;

TYPED_ENUM KeywordType:int {
    Scope,Import,Variable,Immutable,Class,Function,Interface,Alias,Type,Return,If,Else,New,Switch,Case,Extends,Utilizes,Loose,Enum,For,While,Lazy,Await,Acquire
};

// std::vector<ASTComment *> commentBuffer;

/*Creates Error Message For Console*/
inline std::string StarbytesParseError(std::string message,DocumentPosition & position){
    return message.append(" \n Error Occured at Position:{\nLine:"+std::to_string(position.line)+"\n Start Column:"+std::to_string(position.start)+"\n End Column:"+std::to_string(position.end)+"\n}");
}

KeywordType matchKeyword(std::string & subject) {
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
    } else if(subject == "new"){
        returncode = KeywordType::New;
    } else if(subject == "extends"){
        returncode = KeywordType::Extends;
    } else if(subject == "utilizes"){
        returncode = KeywordType::Utilizes;
    } else if(subject == "loose"){
        returncode = KeywordType::Loose;
    } else if(subject == "enum"){
        returncode = KeywordType::Enum;
    } else if(subject == "for"){
        returncode = KeywordType::For;
    } else if(subject == "while"){
        returncode = KeywordType::While;
    } else if(subject == "await"){
        returncode = KeywordType::Await;
    } else if(subject == "lazy"){
        returncode = KeywordType::Lazy;
    } else if(subject == "acquire"){
        returncode = KeywordType::Acquire;
    }
    return returncode;
}


void Parser::convertToAST(){
    while(currentIndex < tokens.size()){
        parseStatement(&Treeptr->nodes,Scope::Global);

        if(currentIndex == tokens.size()-1){
            break;
        }
        incrementToNextToken();
    }
}
void Parser::parseStatement(std::vector<ASTStatement *> * container,Scope scope){
    //First Token of Statement!
    Token *tok = currentToken();
    if(tok->getType() == TokenType::EndOfFile){
        return;
    }
    // if(parseExpressionStatement(container)){
    //     return;
    // }

    if(tok->getType() == TokenType::Keyword){
        if(parseDeclaration(container,scope)){
            return;
        }
    }
}
/*Parses all Declaration and places them in container. (Container is used for scope declarations)*/
bool Parser::parseDeclaration(std::vector<ASTStatement *> *& container,Scope scope){
    switch (matchKeyword(currentToken()->getContent())) {
        case KeywordType::Import :
            std::cout << "IMPORT DECL!" << std::endl;

            if(scope == Scope::Global){
                parseImportDeclaration(container);
                return true;
            }
            else{
                throw StarbytesParseError("Cannot use `import` on lower level scopes!",currentToken()->getPosition());
                return false;
            }
            break;
        case KeywordType::Variable :
            if(aheadToken()->getType() == TokenType::Keyword && matchKeyword(aheadToken()->getContent()) == KeywordType::Immutable){
                parseConstantDeclaration(container);
            }else {
                parseVariableDeclaration(container);
            }
            return true;
            break;
        case KeywordType::Scope :
            if(scope != Scope::FunctionBlockStatement){
                parseScopeDeclaration(container);
                return true;
            }
            else{
                throw StarbytesParseError("Cannot declare `scope` inside Function Scope",currentToken()->getPosition());
                return false;
            }
            break;
        case KeywordType::Function :
            parseFunctionDeclaration(container,false);
            return true;
            break;
        case KeywordType::Lazy :
            incrementToNextToken();
            if(currentToken()->getType() == TokenType::Keyword && matchKeyword(currentToken()->getContent()) == KeywordType::Function){
                parseFunctionDeclaration(container,true);
            }
            else{
                throw StarbytesParseError("Can only use keyword `lazy` with functions",currentToken()->getPosition());
                return false;
            }
            return true;
            break;
        case KeywordType::Class :
            if(scope != Scope::FunctionBlockStatement){
                parseClassDeclaration(container);
                return true;
            }
            else {
                throw StarbytesParseError("Cannot declare `class` inside Function Scope!",currentToken()->getPosition());
                return false;
            }
            break;
        case KeywordType::Interface :
            if(scope != Scope::FunctionBlockStatement){
                parseInterfaceDeclaration(container);
                return true;
            }
            else{
                throw StarbytesParseError("Cannot declare `interface` inside Function Scope!",currentToken()->getPosition());
                return false;
            }
            break;
        case KeywordType::Return :
            if(scope == Scope::FunctionBlockStatement){
                parseReturnDeclaration(container);
                return true;
            }
            else{
                throw StarbytesParseError("Cannot use `return` outside Function Scope!",currentToken()->getPosition());
                return false;
            }
            break;
        case KeywordType::Acquire :
            std::cout << "ACQUIRE DECL!" << std::endl;
            if(scope == Scope::Global){
                parseAcquireDeclaration(container);
                return true;
            }
            else{
                StarbytesParseError("Cannot use `acquire` outside Top level scope!",currentToken()->getPosition());
                return false;
            }
            break;
        default :
            return false;
            break;
    }
}

void Parser::parseAcquireDeclaration(std::vector<ASTStatement *> *& container){
    ASTAcquireDeclaration * node = new ASTAcquireDeclaration(ASTType::AcquireDeclaration);

    node->BeginFold = currentToken()->getPosition();
    Token * tok = nextToken();
    if(tok->getType() == TokenType::Identifier){
        ASTIdentifier *id = new ASTIdentifier();
        parseIdentifier(tok,id);
        node->id = id;
        node->EndFold = currentToken()->getPosition();
        checkForComments(node);
        container->push_back(node);
    }
    else {
        throw StarbytesParseError("Expected Identifier!",tok->getPosition());
    }
};


void Parser::parseNumericLiteral(ASTNumericLiteral *&ptr){
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
                ptr->value = numvalue;

            }
            else {
                throw StarbytesParseError("Expected Numeric!",tok2->getPosition());
            }

        }else{
            std::string numvalue;
            numvalue = tok->getContent();
            ptr->value = numvalue;
        }
        ptr->position.end = currentToken()->getPosition().end;
    } else{
        throw StarbytesParseError("Expected Numeric",tok->getPosition());
    }
}

void Parser::parseStringLiteral(ASTStringLiteral *&ptr){
    ptr->type = ASTType::StringLiteral;
    if(currentToken()->getType() == TokenType::Quote){
        Token *tok = nextToken();
        if(tok->getType() == TokenType::Quote){
            ptr->value = "";
            ptr->position = tok->getPosition();
            ptr->position.end = tok->getPosition().end;
        }else {
            ptr->value = tok->getContent();
            ptr->position = tok->getPosition();
            if(nextToken()->getType() == TokenType::Quote){
                return;
            } else {
                throw StarbytesParseError("Expected Quote",currentToken()->getPosition());
            }
        }
    }
    //Current Token: Last Quote Before OTHER_TOKEN
}

void Parser::parseImportDeclaration(std::vector<ASTStatement *> *& container){

    ASTImportDeclaration *node = new ASTImportDeclaration(ASTType::ImportDeclaration);
    // if(!commentBuffer.empty()){
    //     node->preceding_comments = commentBuffer;
    //     commentBuffer.clear();
    // }
    node->BeginFold = currentToken()->getPosition();
    node->type = ASTType::ImportDeclaration;
    Token *tok1 = nextToken();
    if(tok1->getType() == TokenType::Identifier){
        ASTIdentifier *id = new ASTIdentifier();
        parseIdentifier(tok1,id);
        node->id = id;
        node->EndFold = tok1->getPosition();
        checkForComments(node);
        container->push_back(node);
    }
    else {
        throw StarbytesParseError("Expected Identifier!",tok1->getPosition());
    }
}

void Parser::parseIfDeclaration(std::vector<ASTStatement *> *&container){
    ASTIfDeclaration *node = new ASTIfDeclaration(ASTType::IfDeclaration);
    // if(!commentBuffer.empty()){
    //     node->preceding_comments = commentBuffer;
    //     commentBuffer.clear();
    // }
    node->BeginFold = currentToken()->getPosition();
    node->type = ASTType::IfDeclaration;
    Token *tok = nextToken();
    if(tok->getType() == TokenType::OpenParen){
        incrementToNextToken();
        ASTExpression *exp = parseExpression();
        if(exp != nullptr) {
            node->subject = exp;
            Token *tok1 = nextToken();
            if(tok1->getType() == TokenType::CloseParen){
                Token *tok2 = aheadToken();
                if(tok2->getType() == TokenType::OpenBrace){
                    ASTBlockStatement *block = new ASTBlockStatement(ASTType::BlockStatement);
                    parseBlockStatement(block,Scope::BlockStatement);
                    node->body = block;
                    node->EndFold = currentToken()->getPosition();
                    checkForComments(node);
                    container->push_back(node);
                }
                else{
                    throw StarbytesParseError("Expected Open Brace!",tok2->getPosition());
                }
            }
            else{
                throw StarbytesParseError("Expected Close Paren!",tok1->getPosition());
            }
        }
        else{
            throw StarbytesParseError("Expected Expression!",currentToken()->getPosition());
        }
    }
    else{
        throw StarbytesParseError("Expected Open Paren!",tok->getPosition());
    }
}

void Parser::parseElseIfDeclaration(std::vector<ASTStatement *> *&container){
    ASTElseIfDeclaration *node = new ASTElseIfDeclaration(ASTType::ElseIfDeclaration);
    // if(!commentBuffer.empty()){
    //     node->preceding_comments = commentBuffer;
    //     commentBuffer.clear();
    // }
    node->BeginFold = currentToken()->getPosition();
    node->type = ASTType::ElseIfDeclaration;
    //We have checked the `else if` therefore we can skip two tokens to Open Paren!
    incrementToNextToken();
    Token *tok = nextToken();
    if(tok->getType() == TokenType::OpenParen){
        incrementToNextToken();
        ASTExpression *exp = parseExpression();
        if(exp != nullptr){
            node->subject = exp;
            Token *tok1 = nextToken();
            if(tok1->getType() == TokenType::CloseParen){
                Token *tok2 = aheadToken();
                if(tok2->getType() == TokenType::OpenBrace){
                    ASTBlockStatement *block = new ASTBlockStatement(ASTType::BlockStatement);
                    parseBlockStatement(block,Scope::BlockStatement);
                    node->body = block;
                    node->EndFold = currentToken()->getPosition();
                    checkForComments(node);
                    container->push_back(node);
                }
                else{
                    throw StarbytesParseError("Expected Open Brace!",tok2->getPosition());
                }
            }
            else{
                throw StarbytesParseError("Expected Close Paren!",tok1->getPosition());
            }
        }
        else{
            throw StarbytesParseError("Expected Expression!",currentToken()->getPosition());
        }
    }
    else{
        throw StarbytesParseError("Expected Open Paren!",tok->getPosition());
    }

}

void Parser::parseElseDeclaration(std::vector<ASTStatement *> *&container){
    ASTElseDeclaration *node = new ASTElseDeclaration(ASTType::ElseDeclaration);
    // if(!commentBuffer.empty()){
    //     node->preceding_comments = commentBuffer;
    //     commentBuffer.clear();
    // }
    node->type = ASTType::ElseDeclaration;
    node->BeginFold = currentToken()->getPosition();
    Token *tok = aheadToken();
    if(tok->getType() == TokenType::OpenBrace){
        ASTBlockStatement *block = new ASTBlockStatement(ASTType::BlockStatement);
        parseBlockStatement(block,Scope::BlockStatement);
        node->body = block;
        node->EndFold = currentToken()->getPosition();
        checkForComments(node);
        container->push_back(node);
    }
    else{
        throw StarbytesParseError("Expected Open Brace!",tok->getPosition());

    }
}



void Parser::parseEnumDeclaration(std::vector<ASTStatement *> *&container){
    ASTEnumDeclaration *node = new ASTEnumDeclaration(ASTType::EnumDeclaration);
    // if(!commentBuffer.empty()){
    //     node->preceding_comments = commentBuffer;
    //     commentBuffer.clear();
    // }
    node->BeginFold = currentToken()->getPosition();
    node->type = ASTType::EnumDeclaration;
    Token *tok0 = nextToken();
    if(tok0->getType() == TokenType::Identifier){
        ASTIdentifier *id = new ASTIdentifier();
        node->id = id;
        Token *tok1 = nextToken();
        if(tok1->getType() == TokenType::OpenBrace){
            ASTEnumBlockStatement *block = new ASTEnumBlockStatement(ASTType::EnumBlockStatement);
            parseEnumBlockStatement(block);
            node->body = block;
            node->EndFold = currentToken()->getPosition();
            checkForComments(node);
            container->push_back(node);
        }
        else{
            throw StarbytesParseError("Expected Open Brace!",tok1->getPosition());
        }
    }
    else{
        throw StarbytesParseError("Expected Identifier!",tok0->getPosition());
    }
}

void Parser::parseEnumBlockStatement(ASTEnumBlockStatement *& ptr){
    ptr->type = ASTType::EnumBlockStatement;
    ptr->BeginFold = currentToken()->getPosition();
    Token *tok0 = nextToken();
    while(true){
        if(tok0->getType() == TokenType::CloseBrace){
            ptr->EndFold = tok0->getPosition();
            break;
        }else if(tok0->getType() == TokenType::Identifier){
            ASTEnumerator *node = new ASTEnumerator(ASTType::Enumerator);
            parseEnumerator(node);
            checkForComments(node);
            ptr->nodes.push_back(node);
        }
    }
}

void Parser::parseEnumerator(ASTEnumerator * ptr){
    ptr->type = ASTType::Enumerator;
    ptr->BeginFold = currentToken()->getPosition();
    ASTIdentifier *id = new ASTIdentifier();
    parseIdentifier(currentToken(),id);
    Token *tok0 = aheadToken();
    if(tok0->getType() == TokenType::Operator && tok0->getContent() == "="){
        incrementToNextToken();
        Token *tok1 = aheadToken();
        if(tok1->getType() == TokenType::Numeric){
            ASTNumericLiteral *val = new ASTNumericLiteral(ASTType::NumericLiteral);
            parseNumericLiteral(val);
            ptr->hasValue = true;
            ptr->value = val;
        }
        else{
            throw StarbytesParseError("Expected Numeric!",tok1->getPosition());
        }
    }
    else if(tok0->getType() != TokenType::Identifier){
        throw StarbytesParseError("Expected Identifer!",tok0->getPosition());
    }
    ptr->EndFold = currentToken()->getPosition();
}

void Parser::parseTypeArgsDeclaration(ASTTypeArgumentsDeclaration *&ptr){
    Token *tok = nextToken();
    ptr->type = ASTType::TypeArgumentsDeclaration;
    ptr->BeginFold = tok->getPosition();
    Token *tok1 = nextToken();
    if(tok1->getType() == TokenType::Identifier){
        ASTIdentifier *id = new ASTIdentifier();
        parseIdentifier(tok1,id);
        ptr->args.push_back(id);
        Token *tok2 = nextToken();
        while(true){
            if(tok2->getType() == TokenType::CloseCarrot){
                ptr->EndFold = tok2->getPosition();
                break;
            }else if(tok2->getType() == TokenType::Comma){
                tok2 = nextToken();
                if(tok2->getType() == TokenType::Identifier){
                    ASTIdentifier *id = new ASTIdentifier();
                    parseIdentifier(tok2,id);
                    ptr->args.push_back(id);
                }
                else {
                    throw StarbytesParseError("Expected Identifier!",tok2->getPosition());
                }
            } else {
                throw StarbytesParseError("Expected Comma or Identifier!",tok2->getPosition());
            }
            tok2 = nextToken();
        }
    }else{
        throw StarbytesParseError("Expected Identifier!",tok1->getPosition());
    }
}

void Parser::parseReturnDeclaration(std::vector<ASTStatement *> *& container){
    ASTReturnDeclaration *node = new ASTReturnDeclaration(ASTType::ReturnDeclaration);
    // if(!commentBuffer.empty()){
    //     node->preceding_comments = commentBuffer;
    //     commentBuffer.clear();
    // }
    node->BeginFold = currentToken()->getPosition();
    node->type = ASTType::ReturnDeclaration;
    ASTExpression *exp = parseExpression();
    if(exp != nullptr){
        node->returnee = exp;
        node->EndFold = currentToken()->getPosition();
        checkForComments(node);
        container->push_back(node);
    }
    else{
        throw StarbytesParseError("Expected Expression!",currentToken()->getPosition());
    }
}

void Parser::parseInterfaceDeclaration(std::vector<ASTStatement *> *& container){
    ASTInterfaceDeclaration *node = new ASTInterfaceDeclaration(ASTType::InterfaceDeclaration);
    node->BeginFold = currentToken()->getPosition();
    node->type = ASTType::InterfaceDeclaration;
    node->isGeneric = false;
    node->isChildInterface = false;
    Token *tok1 = nextToken();
    if(tok1->getType() == TokenType::Identifier){
        ASTIdentifier *id = new ASTIdentifier();
        parseIdentifier(tok1,id);
        node->id = id;
        Token *tok2 = aheadToken();

        if(tok2->getType() == TokenType::OpenCarrot){
            ASTTypeArgumentsDeclaration *typeargs = new ASTTypeArgumentsDeclaration(ASTType::TypeArgumentsDeclaration);
            parseTypeArgsDeclaration(typeargs);
            node->isGeneric = true;
            node->typeargs = typeargs;
            tok2 = aheadToken();
        }

        if(tok2->getType() == TokenType::Keyword){
            KeywordType match = matchKeyword(tok2->getContent());
            if(match == KeywordType::Extends){
                incrementToNextToken();
                Token *tok3 = nextToken();
                if(tok3->getType() == TokenType::Identifier){
                    ASTTypeIdentifier *tid = new ASTTypeIdentifier(ASTType::TypeIdentifier);
                    parseTypeIdentifier(tid);
                    node->superclass = tid;
                    node->isChildInterface = true;
                    tok2 = aheadToken();
                }
                else {
                    throw StarbytesParseError("Expected Identifier!",tok3->getPosition());
                }
            }
            else{
                throw StarbytesParseError("Expected Keyword `extends`!",tok1->getPosition());
            }
        }

        if(tok2->getType() == TokenType::OpenBrace){
            //CurrentToken: Identifier or Close Carrot Before Open Brace!
            ASTInterfaceBlockStatement *block = new ASTInterfaceBlockStatement(ASTType::InterfaceBlockStatement);
            parseInterfaceBlockStatement(block);
            //CurrentToken : Close Brace before OTHER_TOKEN!
            node->body = block;
            node->EndFold = currentToken()->getPosition();
            checkForComments(node);
            container->push_back(node);
        }
        else{
            throw StarbytesParseError("Expected Open Brace!",tok2->getPosition());
        }

    }
    else{
        throw StarbytesParseError("Expected Identifier!",tok1->getPosition());
    }
}
void Parser::parseInterfaceBlockStatement(ASTInterfaceBlockStatement *& ptr){
    ptr->type = ASTType::InterfaceBlockStatement;
    ptr->BeginFold = nextToken()->getPosition();
    incrementToNextToken();
    while(true){
        if(currentToken()->getType() == TokenType::CloseBrace){
            ptr->EndFold = currentToken()->getPosition();
            break;
        }else{
            parseInterfaceStatement(&ptr->nodes);
        }
        //Current Token Last token of Interface Statement before Close Brace or First Token of Interface Statement!
        incrementToNextToken();
    }

}
void Parser::parseInterfaceStatement(std::vector<ASTInterfaceStatement *> * container){
    Token *tok0 = currentToken();
    if(tok0->getType() == TokenType::Keyword){
        KeywordType match = matchKeyword(tok0->getContent());
        if(match == KeywordType::Immutable){
            ASTInterfacePropertyDeclaration *prop = new ASTInterfacePropertyDeclaration(ASTType::InterfacePropertyDeclaration);
            parseInterfacePropertyDeclaration(prop,true);
            container->push_back(prop);
        } else if(match == KeywordType::Loose){
            ASTInterfacePropertyDeclaration *prop = new ASTInterfacePropertyDeclaration(ASTType::InterfacePropertyDeclaration);
            parseInterfacePropertyDeclaration(prop,false,true);
            container->push_back(prop);
        }
        else if(match == KeywordType::Lazy){
            ASTInterfaceMethodDeclaration *method = new ASTInterfaceMethodDeclaration(ASTType::InterfaceMethodDeclaration);
            method->isLazy = true;
            Token *tok1 = nextToken();
            if(tok1->getType() == TokenType::Identifier){
                parseInterfaceMethodDeclaration(method);
                container->push_back(method);
            }
            else{
                throw StarbytesParseError("Expected Identifier!",tok1->getPosition());
            }
        }
        else{
            throw StarbytesParseError("Expected Keywords `immutable` or `loose` !",tok0->getPosition());
        }
    }
    else if(tok0->getType() == TokenType::Identifier){
        Token *tok1 = aheadToken();
        if(tok1->getType() == TokenType::Typecast){
            ASTInterfacePropertyDeclaration *prop = new ASTInterfacePropertyDeclaration(ASTType::InterfacePropertyDeclaration);
            parseInterfacePropertyDeclaration(prop,false);
            container->push_back(prop);
        }
        else{
            ASTInterfaceMethodDeclaration *method = new ASTInterfaceMethodDeclaration(ASTType::InterfaceMethodDeclaration);
            method->isLazy = false;
            parseInterfaceMethodDeclaration(method);
            container->push_back(method);
        }
    }
}
void Parser::parseInterfacePropertyDeclaration(ASTInterfacePropertyDeclaration * ptr,bool immutable,bool loose){
    ptr->BeginFold = currentToken()->getPosition();
    ptr->type = ASTType::InterfacePropertyDeclaration;
    ptr->isConstant = immutable;
    ptr->loose = loose;
    Token *tok0 = nextToken();
    if(tok0->getType() == TokenType::Keyword){
        KeywordType match = matchKeyword(tok0->getContent());
        if(immutable){
            if(match == KeywordType::Immutable){
                throw StarbytesParseError("Cannot use `immutable` twice!",tok0->getPosition());
            } else if(match == KeywordType::Loose){
                ptr->loose = true;
                tok0 = nextToken();
            }
        } else if(loose){
            if(match == KeywordType::Loose){
                throw StarbytesParseError("Cannot use `loose` twice!",tok0->getPosition());
            }
            else if(match == KeywordType::Immutable){
                ptr->isConstant = true;
                tok0 = nextToken();
            }
        }
    }

    if(tok0->getType() == TokenType::Identifier){
        ASTTypeCastIdentifier *tcid = new ASTTypeCastIdentifier(ASTType::TypecastIdentifier);
        parseTypecastIdentifier(tcid);
        ptr->tcid = tcid;
    }
    else{
        throw StarbytesParseError("Expected Identifier!",tok0->getPosition());
    }

}

void Parser::parseInterfaceMethodDeclaration(ASTInterfaceMethodDeclaration *& ptr){
    ptr->BeginFold = currentToken()->getPosition();
    ptr->type = ASTType::InterfaceMethodDeclaration;
    ptr->isGeneric = false;
    ASTIdentifier *id = new ASTIdentifier();
    parseIdentifier(currentToken(),id);
    ptr->id = id;
    Token *tok = aheadToken();
    if(tok->getType() == TokenType::OpenCarrot){
        ASTTypeArgumentsDeclaration *typeargs = new ASTTypeArgumentsDeclaration(ASTType::TypeArgumentsDeclaration);
        parseTypeArgsDeclaration(typeargs);
        ptr->typeargs = typeargs;
        ptr->isGeneric = true;
        tok = nextToken();
    }
    else{
        tok = nextToken();
    }
    if(tok->getType() == TokenType::OpenParen){
        Token *tok1 = nextToken();
        if(tok1->getType() == TokenType::Identifier){
            ASTTypeCastIdentifier *tcid = new ASTTypeCastIdentifier(ASTType::TypecastIdentifier);
            parseTypecastIdentifier(tcid);
            ptr->params.push_back(tcid);
            tok1 = nextToken();
        }

        while(true){
            if(tok1->getType() == TokenType::CloseParen){
                break;
            }
            else if(tok1->getType() == TokenType::Comma){
                Token *tok2 = nextToken();
                if(tok2->getType() == TokenType::Identifier){
                    ASTTypeCastIdentifier * tcid = new ASTTypeCastIdentifier(ASTType::TypecastIdentifier);
                    parseTypecastIdentifier(tcid);
                    ptr->params.push_back(tcid);
                }
                else{
                    throw StarbytesParseError("Expected Identifier!",tok2->getPosition());
                }
            }
            tok1 = nextToken();
        }

        Token *tok3 = aheadToken();
        if(tok3->getType() == TokenType::CloseCarrot){
            tok3 = nextToken();
            if(tok3->getType() == TokenType::CloseCarrot){
                Token *tok4 = nextToken();
                if(tok4->getType() == TokenType::Identifier){
                    ASTTypeIdentifier *tid = new ASTTypeIdentifier(ASTType::TypeIdentifier);
                    parseTypeIdentifier(tid);
                    ptr->returnType = tid;
                    ptr->EndFold = currentToken()->getPosition();
                }
                else{
                    throw StarbytesParseError("Expected Identifier!",tok4->getPosition());
                }
            }
            else {
                throw StarbytesParseError("Expected Close Carrot!",tok3->getPosition());
            }
        }
        else{
            ptr->EndFold = currentToken()->getPosition();
        }
    }
    else {
        throw StarbytesParseError("Expected Open Paren!",tok->getPosition());
    }
}

void Parser::parseClassDeclaration(std::vector<ASTStatement *> *&container){
    ASTClassDeclaration *node = new ASTClassDeclaration(ASTType::ClassDeclaration);
    node->BeginFold = currentToken()->getPosition();
    node->type = ASTType::ClassDeclaration;
    node->isGeneric = false;
    Token *tok1 = nextToken();
    if(tok1->getType() == TokenType::Identifier){
        ASTIdentifier * id = new ASTIdentifier();
        parseIdentifier(tok1,id);
        node->id = id;
        Token *tok2 = aheadToken();

        if(tok2->getType() == TokenType::OpenCarrot){
            ASTTypeArgumentsDeclaration *typeargs = new ASTTypeArgumentsDeclaration(ASTType::TypeArgumentsDeclaration);
            parseTypeArgsDeclaration(typeargs);
            //CurrentToken: Closing Carrot before Open Brace or `utlizes` or 'extends'
            node->typeargs = typeargs;
            node->isGeneric = true;
            tok2 = nextToken();
        }

        if(tok2->getType() == TokenType::Keyword){
            KeywordType match = matchKeyword(tok2->getContent());
            if(match == KeywordType::Extends){
                incrementToNextToken();
                Token *tok3 = aheadToken();
                if(tok3->getType() == TokenType::Identifier){
                    ASTTypeIdentifier *tid = new ASTTypeIdentifier(ASTType::TypeIdentifier);
                    parseTypeIdentifier(tid);
                    node->isChildClass = true;
                    node->superclass = tid;
                    tok2 = aheadToken();
                }
                else{
                    throw StarbytesParseError("Expected Identifier!",tok3->getPosition());
                }
            }
            KeywordType match2 = matchKeyword(tok2->getContent());
            if(match2 == KeywordType::Utilizes){
                incrementToNextToken();
                Token *tok4 = aheadToken();
                if(tok4->getType() == TokenType::Identifier){
                    ASTTypeIdentifier *tid = new ASTTypeIdentifier(ASTType::TypeIdentifier);
                    parseTypeIdentifier(tid);
                    node->implementsInterfaces = true;
                    node->interfaces.push_back(tid);
                    while(true){
                        if(aheadToken()->getType() == TokenType::Comma){
                            incrementToNextToken();
                            Token *tok5 = aheadToken();
                            if(tok5->getType() == TokenType::Identifier){
                                ASTTypeIdentifier *tid = new ASTTypeIdentifier(ASTType::TypeIdentifier);
                                parseTypeIdentifier(tid);
                                node->interfaces.push_back(tid);
                            }
                            else{
                                throw StarbytesParseError("Expected Identifier!",tok5->getPosition());
                            }
                        }
                        else{
                            break;
                        }
                    }
                }
                else{
                    throw StarbytesParseError("Expected Identifier!",tok4->getPosition());
                }
            }

        }

        if(aheadToken()->getType() == TokenType::OpenBrace){
            ASTClassBlockStatement *block = new ASTClassBlockStatement(ASTType::ClassBlockStatement);
            parseClassBlockStatement(block);
            node->body = block;
            node->EndFold = currentToken()->getPosition();
            checkForComments(node);
            container->push_back(node);
        }
        else{
            throw StarbytesParseError("Expected Open Brace!",tok2->getPosition());
        }
    }
    else{
        throw StarbytesParseError("Expected Identifier!",tok1->getPosition());
    }
}

void Parser::parseClassBlockStatement(ASTClassBlockStatement *ptr){
    ptr->type = ASTType::ClassBlockStatement;
    Token *tok = nextToken();
    ptr->BeginFold = tok->getPosition();
    if(tok->getType() == TokenType::OpenBrace){
        while(true){
            incrementToNextToken();
            if(currentToken()->getType() == TokenType::CloseBrace){
                break;
            }
            else{
                parseClassStatement(&ptr->nodes);
            }
        }
        ptr->EndFold = currentToken()->getPosition();
    }
    else{
        throw StarbytesParseError("Expected Open Brace",tok->getPosition());
    }
}

void Parser::parseClassStatement(std::vector<ASTClassStatement *> * container){
    Token *tok = currentToken();
    if(tok->getType() == TokenType::Keyword){
        KeywordType match = matchKeyword(tok->getContent());
        if(match == KeywordType::New){
            ASTClassConstructorDeclaration *construct = new ASTClassConstructorDeclaration(ASTType::ClassConstructorDeclaration);
            parseClassConstructorDeclaration(construct);
            //CurrentToken: Parenthesi before OTHER_TOKEN!
            container->push_back(construct);
        }
        else if(match == KeywordType::Immutable){
            ASTClassPropertyDeclaration *prop = new ASTClassPropertyDeclaration(ASTType::ClassPropertyDeclaration);
            parseClassPropertyDeclaration(prop,true);
            //CurrentToken: last Expression toke before OTHER_TOKEN!
            container->push_back(prop);
        }
        else if(match == KeywordType::Loose){
            ASTClassPropertyDeclaration *prop = new ASTClassPropertyDeclaration(ASTType::ClassPropertyDeclaration);
            parseClassPropertyDeclaration(prop,false,true);

            container->push_back(prop);
        }
        else if(match == KeywordType::Lazy){
            ASTClassMethodDeclaration *method = new ASTClassMethodDeclaration(ASTType::ClassMethodDeclaration);
            method->isLazy = true;
            Token *tok2 = nextToken();
            if(tok2->getType() == TokenType::Identifier){
                parseClassMethodDeclaration(method);
                container->push_back(method);
            }
            else{
                throw StarbytesParseError("Expected Identifier!",tok2->getPosition());
            }
        }
    }
    else if(tok->getType() == TokenType::Identifier){
        Token *tok = nextToken();
        if(tok->getType() == TokenType::Typecast || (tok->getType() == TokenType::Operator && tok->getContent() == "=")){
            ASTClassPropertyDeclaration *prop = new ASTClassPropertyDeclaration(ASTType::ClassPropertyDeclaration);
            parseClassPropertyDeclaration(prop,false);

            container->push_back(prop);
        } else {
            ASTClassMethodDeclaration *method = new ASTClassMethodDeclaration(ASTType::ClassMethodDeclaration);
            method->isLazy = false;
            parseClassMethodDeclaration(method);

            container->push_back(method);
        }
    }
}

void Parser::parseClassConstructorDeclaration(ASTClassConstructorDeclaration *ptr){
    Token *tok = currentToken();
    ptr->BeginFold = tok->getPosition();
    ptr->type = ASTType::ClassConstructorDeclaration;
    Token *tok1 = nextToken();
    if(tok1->getType() == TokenType::OpenParen){
        Token *tok2 = nextToken();
        while(true){
            if(tok2->getType() == TokenType::CloseParen){
                ptr->EndFold = tok2->getPosition();
                break;
            }else if(tok2->getType() == TokenType::Comma) {
                Token *tok3 = nextToken();
                if(tok3->getType() == TokenType::Keyword){
                    KeywordType match = matchKeyword(tok3->getContent());
                    if(match == KeywordType::Immutable){
                        ASTClassConstructorParameterDeclaration *param = new ASTClassConstructorParameterDeclaration(ASTType::ClassConstructorParameterDeclaration);
                        parseClassConstructorParameterDeclaration(param,true);
                        incrementToNextToken();
                        ptr->params.push_back(param);
                    } else if(match == KeywordType::Loose){
                        ASTClassConstructorParameterDeclaration *param = new ASTClassConstructorParameterDeclaration(ASTType::ClassConstructorParameterDeclaration);
                        parseClassConstructorParameterDeclaration(param,false,true);
                        incrementToNextToken();
                        ptr->params.push_back(param);
                    }
                } else if(tok3->getType() == TokenType::Identifier){
                    ASTClassConstructorParameterDeclaration *param = new ASTClassConstructorParameterDeclaration(ASTType::ClassConstructorParameterDeclaration);
                    parseClassConstructorParameterDeclaration(param,false);
                    incrementToNextToken();
                    ptr->params.push_back(param);
                }
                else {
                    throw StarbytesParseError("Expected Identifier or Keywords: `immutable` or `loose`",tok3->getPosition());
                }
            }
            else {
                throw StarbytesParseError("Expected Comma or Close Paren!",tok2->getPosition());
            }
            tok2 = nextToken();
        }
    }
    else {
        throw StarbytesParseError("Expected Open Paren!",tok1->getPosition());
    }
}

void Parser::parseClassConstructorParameterDeclaration(ASTClassConstructorParameterDeclaration *& ptr,bool immutable,bool loose){
    ptr->BeginFold = currentToken()->getPosition();
    ptr->type = ASTType::ClassConstructorParameterDeclaration;
    Token *tok = nextToken();
    if(tok->getType() == TokenType::Keyword){
        KeywordType match = matchKeyword(tok->getContent());
        if(immutable){
            ptr->isConstant = true;
            if(match == KeywordType::Immutable){
                throw StarbytesParseError("Cannot define `immutable` twice!",tok->getPosition());
            } else if(match == KeywordType::Loose){
                ptr->loose = true;
                incrementToNextToken();
            }
        } else if(loose){
            ptr->loose = true;
            KeywordType match = matchKeyword(tok->getContent());
            if(match == KeywordType::Loose){
                throw StarbytesParseError("Cannot define `loose` twice!",tok->getPosition());
            }
            else if(match == KeywordType::Immutable){
                ptr->isConstant = true;
                incrementToNextToken();
            }
        }
    }
    if(currentToken()->getType() == TokenType::Identifier){
        ASTTypeCastIdentifier *tcid = new ASTTypeCastIdentifier(ASTType::TypecastIdentifier);
        parseTypecastIdentifier(tcid);
        ptr->tcid = tcid;
        ptr->EndFold = currentToken()->getPosition();
    }
}

void Parser::parseClassPropertyDeclaration(ASTClassPropertyDeclaration *&ptr,bool immutable,bool loose){
    ptr->type = ASTType::ClassPropertyDeclaration;
    ptr->BeginFold = currentToken()->getPosition();
    Token *tok = nextToken();
    if(tok->getType() == TokenType::Keyword){
        KeywordType match = matchKeyword(tok->getContent());
        if(immutable){
            if(match == KeywordType::Immutable){
                throw StarbytesParseError("Cannot define `immutable` twice!",tok->getPosition());
            }else if(match == KeywordType::Loose){
                ptr->loose = true;
                incrementToNextToken();
            }
        } else if(loose){
            if(match == KeywordType::Loose){
                throw StarbytesParseError("Cannot define `loose` twice!",tok->getPosition());
            } else if(match == KeywordType::Immutable){
                ptr->isConstant = true;
                incrementToNextToken();
            }
        }
    }

    if(currentToken()->getType() == TokenType::Identifier) {
        if(aheadToken()->getType() == TokenType::Typecast){
            ASTTypeCastIdentifier *tcid = new ASTTypeCastIdentifier(ASTType::TypecastIdentifier);
            parseTypecastIdentifier(tcid);
            ptr->id = (ASTNode *)tcid;
        }
        else {
            ASTIdentifier *id = new ASTIdentifier();
            parseIdentifier(currentToken(),id);
            ptr->id = (ASTNode *)id;
        }
        Token *tok = nextToken();
        if(tok->getType() == TokenType::Operator && tok->getContent() == "="){
            incrementToNextToken();
            ASTExpression *exp = parseExpression();
            if(exp != nullptr){
                ptr->initializer = exp;
                ptr->EndFold = currentToken()->getPosition();
            }
            else {
                throw StarbytesParseError("Expected Expression!",tok->getPosition());
            }
        }

    }
    else {
        throw StarbytesParseError("Expected Identifier!",currentToken()->getPosition());
    }
}
//TODO: Generics to be added eventually! (CHECK LATER!! NEEDS POTENTIAL FIXING!)
void Parser::parseClassMethodDeclaration(ASTClassMethodDeclaration *&ptr){
    ptr->BeginFold = currentToken()->getPosition();
    ptr->type = ASTType::ClassMethodDeclaration;
    ASTIdentifier *id = new ASTIdentifier();
    ptr->id = id;
    ptr->isGeneric = false;
    Token *tok = aheadToken();
    if(tok->getType() == TokenType::OpenCarrot){
        ASTTypeArgumentsDeclaration *typeargs = new ASTTypeArgumentsDeclaration(ASTType::TypeArgumentsDeclaration);
        parseTypeArgsDeclaration(typeargs);
        ptr->typeargs = typeargs;
        ptr->isGeneric = true;
        tok = nextToken();
    }else{
        tok = nextToken();
    }
    if(tok->getType() == TokenType::OpenParen){
        Token *tok1 = nextToken();
        if(tok1->getType() == TokenType::Identifier){
            ASTTypeCastIdentifier *tcid = new ASTTypeCastIdentifier(ASTType::TypecastIdentifier);
            parseTypecastIdentifier(tcid);
            Token *tok1 = nextToken();
            if(tok1->getType() == TokenType::Comma){
                while(true){
                    if(tok1->getType() == TokenType::Comma){
                        Token *tok2 = nextToken();
                        if(tok2->getType() == TokenType::Identifier){
                            ASTTypeCastIdentifier *tcid = new ASTTypeCastIdentifier(ASTType::TypecastIdentifier);
                            parseTypecastIdentifier(tcid);
                        }
                    } else {
                        break;
                    }
                    tok1 = nextToken();
                }
            }

            if(tok1->getType() == TokenType::CloseParen){
                Token *tok2 = nextToken();
                if(tok2->getType() == TokenType::CloseCarrot){
                    tok2 = nextToken();
                    if(tok2->getType() == TokenType::CloseCarrot){
                        tok2 = nextToken();
                        if(tok2->getType() == TokenType::Identifier){
                            ASTTypeIdentifier *tid =  new ASTTypeIdentifier(ASTType::TypeIdentifier);
                            parseTypeIdentifier(tid);
                            ptr->returnType = tid;
                        }
                        else{
                            throw StarbytesParseError("Expected Identifier!",tok2->getPosition());
                        }
                    }
                    else {
                        throw StarbytesParseError("Expected Close Carrot!",tok2->getPosition());
                    }
                }
                Token *tok3 = aheadToken();
                if(tok3->getType() == TokenType::OpenBrace){
                    ASTBlockStatement *block = new ASTBlockStatement(ASTType::BlockStatement);
                    parseBlockStatement(block,Scope::FunctionBlockStatement);
                    ptr->EndFold = currentToken()->getPosition();
                    ptr->body = block;
                }
                else {
                    throw StarbytesParseError("Expected Open Brace!",tok3->getPosition());
                }

            }
        }
        else if(tok1->getType() == TokenType::CloseParen) {
             Token *tok2 = nextToken();
                if(tok2->getType() == TokenType::CloseCarrot){
                    tok2 = nextToken();
                    if(tok2->getType() == TokenType::CloseCarrot){
                        tok2 = nextToken();
                        if(tok2->getType() == TokenType::Identifier){
                            ASTTypeIdentifier *tid =  new ASTTypeIdentifier(ASTType::TypeIdentifier);
                            parseTypeIdentifier(tid);
                            ptr->returnType = tid;
                        }
                        else{
                            throw StarbytesParseError("Expected Identifier!",tok2->getPosition());
                        }
                    }
                    else {
                        throw StarbytesParseError("Expected Close Carrot!",tok2->getPosition());
                    }
                }
                Token *tok3 = aheadToken();
                if(tok3->getType() == TokenType::OpenBrace){
                    ASTBlockStatement *block = new ASTBlockStatement(ASTType::BlockStatement);
                    parseBlockStatement(block,Scope::FunctionBlockStatement);
                    ptr->EndFold = currentToken()->getPosition();
                    ptr->body = block;
                }
                else {
                    throw StarbytesParseError("Expected Open Brace!",tok3->getPosition());
                }

        }
    }
    else {
        throw StarbytesParseError("Expected Open Paren!",tok->getPosition());
    }
}

void Parser::parseBlockStatement(ASTBlockStatement *&ptr,Scope scope){
    ptr->type = ASTType::BlockStatement;
    Token *tok = nextToken();
    ptr->BeginFold = tok->getPosition();
    if(tok->getType() == TokenType::OpenBrace){
        while(true){
            if(currentToken()->getType() == TokenType::CloseBrace){
                break;
            } else{
                parseStatement(&ptr->nodes,scope);
            }
            incrementToNextToken();
        }
        ptr->EndFold = currentToken()->getPosition();
    }
    else {
        throw StarbytesParseError("Expected Open Brace!",tok->getPosition());
    }

}

void Parser::parseScopeDeclaration(std::vector<ASTStatement *> *& container){
    //CurrentToken: Keyword before ID
    ASTScopeDeclaration *node = new ASTScopeDeclaration(ASTType::ScopeDeclaration);
    // if(!commentBuffer.empty()){
    //     node->preceding_comments = commentBuffer;
    //     commentBuffer.clear();
    // }
    node->BeginFold = currentToken()->getPosition();
    node->type = ASTType::ScopeDeclaration;
    Token *tok1 = nextToken();
    if(tok1->getType() == TokenType::Identifier){
        ASTIdentifier *id0 = new ASTIdentifier();
        parseIdentifier(tok1,id0);
        node->id = id0;
        if(aheadToken()->getType() == TokenType::OpenBrace){
            ASTBlockStatement *block = new ASTBlockStatement(ASTType::BlockStatement);
            //PARSE BODY AND STOP FOLD!
            parseBlockStatement(block,Scope::BlockStatement);
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
void Parser::parseConstantDeclaration(std::vector<ASTStatement *> *& container){
    ASTConstantDeclaration *node = new ASTConstantDeclaration(ASTType::ConstantDeclaration);
    // if(!commentBuffer.empty()){
    //     node->preceding_comments = commentBuffer;
    //     commentBuffer.clear();
    // }
    node->type = ASTType::ConstantDeclaration;
    //Begins at '[HERE]decl immutable value = 4'
    node->BeginFold = currentToken()->getPosition();
    //Skip to `immutable` keyword!
    incrementToNextToken();
    ASTConstantSpecifier *node0 = new ASTConstantSpecifier(ASTType::ConstantSpecifier);
    //CurrentToken: Keyword before Id!
    parseConstantSpecifier(node0);
    node->specifiers.push_back(node0);
    //CurrentToken: EXPRESSION_RELATED_KEYWORD before Comma or OTHER_TOKEN
    if(aheadToken()->getType() == TokenType::Comma){
        while(true){
            if(aheadToken()->getType() == TokenType::Comma){
                ASTConstantSpecifier * node1 = new ASTConstantSpecifier(ASTType::ConstantSpecifier);
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

}

void Parser::parseConstantSpecifier(ASTConstantSpecifier *&ptr){
    ptr->type = ASTType::ConstantSpecifier;
    Token * tok = nextToken();
    ptr->BeginFold = tok->getPosition();
    //CurrentToken: ID after keyword: `immutable`.
    if(tok->getType() == TokenType::Identifier){
        //CurrentToken: ID Before Colon or Equals
        if(aheadToken()->getType() == TokenType::Typecast){
            ASTTypeCastIdentifier *tcid = new ASTTypeCastIdentifier(ASTType::TypecastIdentifier);
            //CurrentToken: ID before Colon
            parseTypecastIdentifier(tcid);
            ptr->id = tcid;
            //CurrentToken: ID After Colon Just Before Equals
        }
        else {
            throw StarbytesParseError("Expected Colon!",aheadToken()->getPosition());
        }

        Token *tok3 = nextToken();
        //CurrentToken: ID Before Equals
        if(tok3->getType() == TokenType::Operator && tok3->getContent() == "="){
            //PARSE EXPRESSION! OR LITERAL!
            incrementToNextToken();
            ASTExpression *node = parseExpression();
            if(node == nullptr){
                throw StarbytesParseError("Expected Expression!",currentToken()->getPosition());
            }
            ptr->initializer = node;
            ptr->EndFold = currentToken()->getPosition();
        }
        else {
            throw StarbytesParseError("Expected `=` Operator! (Constants require assignment)",tok3->getPosition());
        }
    }else {
        throw StarbytesParseError("Expected Identifer!",tok->getPosition());
    }
}

void Parser::parseVariableDeclaration(std::vector<ASTStatement *> *& container){
    ASTVariableDeclaration *node = new ASTVariableDeclaration(ASTType::VariableDeclaration);
    // if(!commentBuffer.empty()){
    //     node->preceding_comments = commentBuffer;
    //     commentBuffer.clear();
    // }
    node->type = ASTType::VariableDeclaration;
    node->BeginFold = currentToken()->getPosition();
    Token *tok1 = aheadToken();
    if(tok1->getType() == TokenType::Identifier){

        ASTVariableSpecifier *node0 = new ASTVariableSpecifier(ASTType::VariableSpecifier);
        //CurrentToken: Keyword before ID!
        parseVariableSpecifier(node0);
        node->specifiers.push_back(node0);
        //CurrentToken: Before Comma or OTHER_TOKEN
        if(aheadToken()->getType() == TokenType::Comma){
            while(true){
                if(aheadToken()->getType() == TokenType::Comma){
                    incrementToNextToken();
                    ASTVariableSpecifier *node1 = new ASTVariableSpecifier(ASTType::VariableSpecifier);
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
        checkForComments(node);
        container->push_back(node);
    } else {
        throw StarbytesParseError("Expected Identifer!",tok1->getPosition());
    }
}

void Parser::parseVariableSpecifier(ASTVariableSpecifier *&ptr){
    Token *tok = nextToken();
    ptr->type = ASTType::VariableSpecifier;
    ptr->BeginFold = tok->getPosition();
    //CurrentToken: ID after Comma or Keyword 'decl'!
    if(tok->getType() == TokenType::Identifier){
        if(aheadToken()->getType() == TokenType::Typecast){
            ASTTypeCastIdentifier *id0 = new ASTTypeCastIdentifier(ASTType::TypecastIdentifier);
            parseTypecastIdentifier(id0);
            ptr->id = id0;
        }else if(aheadToken()->getContent() != "="){
            throw StarbytesParseError("Cannot imply type without Assignment",aheadToken()->getPosition());
        }else if (aheadToken()->getContent() == "=") {
            ASTIdentifier *id0 = new ASTIdentifier();
            parseIdentifier(tok, id0);
            ptr->id = id0;
        }
        Token *tok1 = aheadToken();
        if(tok1->getType() == TokenType::Operator && tok1->getContent() == "="){
            incrementToNextToken();
            //PARSE EXPRESSION! OR LITERAL!
            incrementToNextToken();
            ASTExpression *node = parseExpression();
            ptr->initializer = node;
        }
        //Token Last Token of expression before OTHER_TOKEN
        ptr->EndFold = currentToken()->getPosition();

    }
    //Current token: Before Comma or OTHER_TOKEN
}
//TODO: More Advanced Type Funcitonality Required! (Generics,Arrays,Dictionaries,etc...)
void Parser::parseFunctionDeclaration(std::vector<ASTStatement *> *& container,bool isLazy){
    ASTFunctionDeclaration *node = new ASTFunctionDeclaration(ASTType::FunctionDeclaration);
    // if(!commentBuffer.empty()){
    //     node->preceding_comments = commentBuffer;
    //     commentBuffer.clear();
    // }
    node->BeginFold = currentToken()->getPosition();
    node->type = ASTType::FunctionDeclaration;
    node->isGeneric = false;
    node->isAsync = isLazy;
    Token *tok = nextToken();
    if(tok->getType() == TokenType::Identifier){
        ASTIdentifier *id = new ASTIdentifier();
        parseIdentifier(tok,id);
        node->id = id;
        Token *tok0 = aheadToken();
        if(tok0->getType() == TokenType::OpenCarrot){
            ASTTypeArgumentsDeclaration *typeargs = new ASTTypeArgumentsDeclaration(ASTType::TypeArgumentsDeclaration);
            parseTypeArgsDeclaration(typeargs);
            node->isGeneric = true;
            node->typeargs = typeargs;
            tok0 = nextToken();
        }else{
            tok0 = nextToken();
        }
        //CurrentToken: Open Paren after Closing Carrot or Identifier!
        if(tok0->getType() == TokenType::OpenParen){
            Token *tok1 = nextToken();
            if(tok1->getType() == TokenType::Identifier){
                ASTTypeCastIdentifier *ptr = new ASTTypeCastIdentifier(ASTType::TypecastIdentifier);
                parseTypecastIdentifier(ptr);
                node->params.push_back(ptr);
                Token *tok2 = nextToken();
                while(true){
                    if(tok2->getType() == TokenType::Comma){
                        if(nextToken()->getType() == TokenType::Identifier){
                            ASTTypeCastIdentifier *ptr = new ASTTypeCastIdentifier(ASTType::TypecastIdentifier);
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
                        ASTTypeIdentifier *tid = new ASTTypeIdentifier(ASTType::TypeIdentifier);
                        parseTypeIdentifier(tid);
                        node->returnType = tid;
                    }else {
                        throw StarbytesParseError("Expected Close Carrot!",tok3->getPosition());
                    }
                }
                //CurrentToken: Before Block Statement Open Brace!
                if(aheadToken()->getType() == TokenType::OpenBrace){
                    ASTBlockStatement *block = new ASTBlockStatement(ASTType::BlockStatement);
                    parseBlockStatement(block,Scope::FunctionBlockStatement);
                    node->body = block;
                    node->EndFold = currentToken()->getPosition();
                    checkForComments(node);
                    container->push_back(node);
                    //CurrentToken: Close Brace before OTHER_TOKEN
                }
                else {
                    throw StarbytesParseError("Expected Open Brace!",aheadToken()->getPosition());
                }
            }
            else if(tok1->getType() == TokenType::CloseParen){
                Token *tok2 = aheadToken();
                if(tok2->getType() == TokenType::CloseCarrot){
                    incrementToNextToken();
                    Token *tok4 = nextToken();
                    if(tok4->getType() == TokenType::CloseCarrot){
                        //PARSE TYPED IDENTIFIER!
                        ASTTypeIdentifier *tid = new ASTTypeIdentifier(ASTType::TypeIdentifier);
                        parseTypeIdentifier(tid);
                        node->returnType = tid;
                        tok2 = nextToken();
                    }else {
                        throw StarbytesParseError("Expected Close Carrot!",tok2->getPosition());
                    }
                }
                if(tok2->getType() == TokenType::OpenBrace){
                    ASTBlockStatement *block = new ASTBlockStatement(ASTType::BlockStatement);
                    parseBlockStatement(block,Scope::FunctionBlockStatement);
                    node->body = block;
                    node->EndFold = currentToken()->getPosition();
                    checkForComments(node);
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

void Parser::parseTypecastIdentifier(ASTTypeCastIdentifier *& ptr){
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
            ASTTypeIdentifier *tid = new ASTTypeIdentifier(ASTType::TypeIdentifier);
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
void Parser::parseTypeIdentifier(ASTTypeIdentifier *& ptr){
    //CurrenToken: Before ID
    ptr->type = ASTType::TypeIdentifier;
    Token *tok = nextToken();
    ptr->BeginFold = tok->getPosition();
    ptr->isArrayType = false;
    ptr->isGeneric = false;
    if(tok->getType() == TokenType::Identifier){
        ptr->type_name = tok->getContent();
        Token *tok1 = aheadToken();
        if(tok1->getType() == TokenType::OpenCarrot){
            incrementToNextToken();
            ptr->isGeneric = true;
            while(true){
                tok1 = aheadToken();
                if(tok1->getType() == TokenType::CloseCarrot){
                    incrementToNextToken();
                    break;
                }
                else{
                    ASTTypeIdentifier *tid = new ASTTypeIdentifier(ASTType::TypeIdentifier);
                    parseTypeIdentifier(tid);
                    ptr->typeargs.push_back(tid);
                }
            }
        }
        while(true){
            if(currentToken()->getType() == TokenType::OpenBracket){
                ptr->isArrayType = true;
                Token *tok2 = nextToken();
                if(tok2->getType() != TokenType::CloseBracket){
                    throw StarbytesParseError("Expected Close Bracket!",tok2->getPosition());
                }
                else{
                    ++(ptr->array_count);
                }
            }
            if(aheadToken()->getType() != TokenType::OpenBracket){
                break;
            }
            else{
                incrementToNextToken();
                continue;
            }
        }
        ptr->EndFold = currentToken()->getPosition();
    }
    else {
        throw StarbytesParseError("Expected Identifier!",tok->getPosition());
    }
}

void Parser::parseIdentifier(Token *token1, ASTIdentifier *& id) {
    id->position = token1->getPosition();
    id->type = ASTType::Identifier;
    id->value = token1->getContent();
}

ASTExpression * Parser::parseExpression(){
    ASTExpression *ptr = nullptr;
    bool isNotLiteral = false;
    if(currentToken()->getType() == TokenType::Keyword){
        KeywordType match = matchKeyword(currentToken()->getContent());
        if(match == KeywordType::New){
            ASTNewExpression *node3 = new ASTNewExpression(ASTType::NewExpression);
            parseNewExpression(node3);
            ptr = node3;
            isNotLiteral = true;
        }
        else if(match == KeywordType::Await){
            ASTAwaitExpression *node4 = new ASTAwaitExpression(ASTType::AwaitExpression);
            parseAwaitExpression(node4);
            ptr = node4;
            isNotLiteral = true;
        }
    } else{
        if(currentToken()->getType() == TokenType::Numeric){
            ASTNumericLiteral *node0 = new ASTNumericLiteral(ASTType::NumericLiteral);
            parseNumericLiteral(node0);
            ptr = (ASTExpression *)node0;
        } else if(currentToken()->getType() == TokenType::Quote){
            ASTStringLiteral *node1 = new ASTStringLiteral(ASTType::StringLiteral);
            parseStringLiteral(node1);
            ptr = (ASTExpression *)node1;
        }else if(currentToken()->getType() == TokenType::OpenBracket) {
            ASTArrayExpression *node2 = new ASTArrayExpression(ASTType::ArrayExpression);
            parseArrayExpression(node2);
            ptr = node2;
            isNotLiteral = true;
        }
        else if(currentToken()->getType() == TokenType::Identifier){
            ASTIdentifier *id = new ASTIdentifier();
            parseIdentifier(currentToken(),id);
            ptr = (ASTExpression *)id;
            isNotLiteral = true;
        }

    }

    Token *tok0 = aheadToken();
    if(tok0->getType() == TokenType::Dot){
        if(isNotLiteral){
            ASTMemberExpression *membexp = new ASTMemberExpression(ASTType::MemberExpression);
            parseMemberExpression(membexp,ptr);
            ptr = membexp;
            Token *tok1 = aheadToken();
            if(tok1->getType() == TokenType::OpenParen){
                ASTCallExpression *callexp = new ASTCallExpression(ASTType::CallExpression);
                parseCallExpression(callexp,ptr);
                ptr = callexp;
                tok1 = aheadToken();
            }
            while(true){
                if(tok1->getType() == TokenType::Dot){
                    ASTMemberExpression *membexp = new ASTMemberExpression(ASTType::MemberExpression);
                    parseMemberExpression(membexp,ptr);
                    ptr = membexp;
                    Token *tok3 = aheadToken();
                    if(tok3->getType() == TokenType::OpenParen){
                        ASTCallExpression *callexp = new ASTCallExpression(ASTType::CallExpression);
                        parseCallExpression(callexp,ptr);
                        ptr = callexp;
                    }
                }
                else{
                    break;
                }
                tok1 = aheadToken();
            }
        }
        else{
            throw StarbytesParseError("Literals can be embedded in member expressions!",tok0->getPosition());
        }
    }

    if(aheadToken()->getType() == TokenType::OpenParen){
         ASTCallExpression *callexp = new ASTCallExpression(ASTType::CallExpression);
         parseCallExpression(callexp,ptr);
         ptr = callexp;
    }

    return ptr;
}

bool Parser::parseExpressionStatement(std::vector<ASTStatement *> *& container){
    ASTExpressionStatement *node = new ASTExpressionStatement(ASTType::ExpressionStatement);
    // if(!commentBuffer.empty()){
    //     node->preceding_comments = commentBuffer;
    //     commentBuffer.clear();
    // }
    node->type = AST::ASTType::ExpressionStatement;
    ASTExpression *node1 = parseExpression();
    if(node1 != nullptr){
        node->expression = node1;
        node->BeginFold = node1->BeginFold;
        node->EndFold = node1->EndFold;
        checkForComments(node);
        container->push_back(node);
        return true;
    } else {
        delete node;
        return false;
    }
}
void Parser::parseArrayExpression(ASTArrayExpression *&ptr){
    ptr->type = ASTType::ArrayExpression;
    ptr->BeginFold = currentToken()->getPosition();
    Token *tok = nextToken();
    while(true){
        if(tok->getType() == TokenType::CloseBracket){
            ptr->EndFold = tok->getPosition();
            break;
        } else {
            ASTExpression *node = parseExpression();
            if(node != nullptr){
                ptr->items.push_back(node);
            }
            Token *tok1 = nextToken();
            if(tok1->getType() == TokenType::CloseBracket){
                ptr->EndFold = tok->getPosition();
                break;
            }
            else if(tok1->getType() != TokenType::Comma){
                throw StarbytesParseError("Expected Comma!",tok1->getPosition());
            }
        }
        tok = nextToken();
    }
}

void Parser::parseNewExpression(ASTNewExpression *&ptr){
    ptr->type = ASTType::NewExpression;
    ptr->BeginFold = currentToken()->getPosition();
    if(aheadToken() ->getType() == TokenType::Identifier){
        ASTTypeIdentifier *node0 = new ASTTypeIdentifier(ASTType::TypeIdentifier);
        parseTypeIdentifier(node0);
        ptr->decltid = node0;
        Token *tok0 = nextToken();
        if(tok0->getType() == TokenType::OpenParen){
            while(true){
                if(tok0->getType() == TokenType::CloseParen){
                    ptr->EndFold = tok0->getPosition();
                    break;
                }else {
                    ASTExpression *node1 = parseExpression();
                    if(node1 != nullptr){
                        ptr->params.push_back(node1);
                    }
                    Token *tok1 = nextToken();
                    if(tok1->getType() != TokenType::Comma){
                        throw StarbytesParseError("Expected Comma!",tok1->getPosition());
                    }
                }
                tok0 = nextToken();
            }
        }
        else{
            throw StarbytesParseError("Expected Open Paren!",tok0->getPosition());
        }
    }
    else {
        throw StarbytesParseError("Expected Identifier!",aheadToken()->getPosition());
    }
}

void Parser::parseAwaitExpression(ASTAwaitExpression *&ptr){
    //CurrentToken: `await` Keyword!
    ptr->type = AST::ASTType::AwaitExpression;
    ptr->BeginFold = currentToken()->getPosition();
    Token *tok0 = nextToken();
    if(tok0->getType() == TokenType::OpenParen){
        Token *tok1 = nextToken();
        if(tok1->getType() == TokenType::Identifier){
            ASTExpression *exp = parseExpression();
            if(exp->type == AST::ASTType::CallExpression){
                ptr->callee = (ASTCallExpression *)exp;
                Token *tok2 = nextToken();
                if(tok2->getType() == TokenType::CloseParen){
                    ptr->EndFold = tok2->getPosition();
                }
                else{
                    throw StarbytesParseError("Expected Close Paren!",tok2->getPosition());
                }
            }
            else{
                throw StarbytesParseError("Expected Call Expression!",tok1->getPosition());
            }
        }
        else{
            throw StarbytesParseError("Expected Identifier!",tok1->getPosition());
        }
    }
    else{
        ASTExpression *exp = parseExpression();
        if(exp->type == AST::ASTType::CallExpression){
            ptr->callee = (ASTCallExpression *)exp;
            ptr->EndFold = currentToken()->getPosition();
        }
        else{
            throw StarbytesParseError("Expected Call Expression!",tok0->getPosition());
        }
    }
}


void Parser::parseMemberExpression(ASTMemberExpression *&ptr,ASTExpression *&object){
    ptr->type = AST::ASTType::MemberExpression;
    ptr->BeginFold = object->BeginFold;
    ptr->object = object;
    Token *tok = nextToken();
    if(tok->getType() == TokenType::Dot){
        Token *tok1 = nextToken();
        if(tok1->getType() == TokenType::Identifier){
            ASTIdentifier *id = new ASTIdentifier();
            parseIdentifier(tok1,id);
            ptr->prop = (ASTExpression *) id;
            ptr->EndFold = currentToken()->getPosition();
        }
        else{
            throw StarbytesParseError("Expected Identifier!",tok1->getPosition());
        }
    }
}

void Parser::parseCallExpression(ASTCallExpression *&ptr,ASTExpression *&callee){
    ptr->type = AST::ASTType::CallExpression;
    ptr->BeginFold = callee->BeginFold;
    ptr->callee = callee;
    Token *tok0 = nextToken();
    if(tok0->getType() == TokenType::OpenParen){
        Token *tok1 = nextToken();
        if(tok1->getType() != TokenType::CloseParen){
            ASTExpression *exp = parseExpression();
            ptr->params.push_back(exp);
            tok1 = nextToken();
            while(true){
                if(tok1->getType() == TokenType::CloseParen){
                    break;
                }
                else if(tok1->getType() == TokenType::Comma){
                    incrementToNextToken();
                    ASTExpression *exp = parseExpression();
                    ptr->params.push_back(exp);
                    tok1 = nextToken();
                }
                else{
                    throw StarbytesParseError("Expected Comma or Close Paren!",tok1->getPosition());
                }
            }
        }

    }
}

void Parser::parseComment(ASTObject *& ptr){
    Token *tok = currentToken();
    if(tok->getType() == TokenType::LineCommentDBSlash){
        ASTLineComment *com = new ASTLineComment();
        parseLineComment(com);
        ptr->preceding_comments.push_back(com);
    }
    else if(tok->getType() == TokenType::BlockCommentStart){
        ASTBlockComment *com = new ASTBlockComment();
        parseBlockComment(com);
        ptr->preceding_comments.push_back(com);
    }
}

void Parser::parseLineComment(ASTLineComment *&ptr){
    ptr->type = AST::CommentType::LineComment;
    Token *tok = nextToken();
    if(tok->getType() == TokenType::Comment){
        ptr->value = tok->getContent();
    }
    else{
        throw StarbytesParseError("Expected A Comment!",tok->getPosition());
    }

}

void Parser::parseBlockComment(ASTBlockComment *&ptr){
    ptr->type = AST::CommentType::BlockComment;
    Token *tok = nextToken();
    ptr->line_num = 0;
    while(true){
        if(tok->getType() == TokenType::BlockCommentEnd){
            break;
        }
        else if(tok->getType() == TokenType::Comment){
            ptr->line_num += 1;
            ptr->lines.push_back(tok->getContent());
        }
        tok = nextToken();
    }
}

void Parser::checkForComments(ASTObject * ptr){
    Token *tok = aheadToken();
    while(true){
        if(tok->getType() == TokenType::LineCommentDBSlash || tok->getType() == TokenType::BlockCommentStart){
            parseComment(ptr);
        }
        else {
            break;
        }
    }

}
