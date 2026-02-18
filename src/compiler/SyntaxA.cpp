#include "starbytes/compiler/SyntaxA.h"
#include "starbytes/compiler/AST.h"

namespace starbytes::Syntax {

void SyntaxA::setTokenStream(array_ref<Tok> toks){
    token_stream = toks;
    privTokIndex = 0;
}

void SyntaxA::gotoNextTok(){
    ++privTokIndex;
}

TokRef SyntaxA::nextTok(){
    return token_stream[++privTokIndex];
}

TokRef SyntaxA::aheadTok(){
    return token_stream[privTokIndex + 1];
}

size_t SyntaxA::getTokenStreamWidth(){
    return token_stream.size();
}

bool SyntaxA::isAtEnd(){
    if(token_stream.size() == 0){
        return true;
    }
    return token_stream[privTokIndex].type == Tok::EndOfFile;
}

const Tok &SyntaxA::currentTok(){
    return token_stream[privTokIndex];
}

void SyntaxA::consumeCurrentTok(){
    if(token_stream.size() == 0){
        return;
    }
    if(token_stream[privTokIndex].type == Tok::EndOfFile){
        return;
    }
    ++privTokIndex;
}

 ASTStmt * SyntaxA::nextStatement(){
        TokRef t = token_stream[privTokIndex];
        if(t.type == Tok::EndOfFile){
            return nullptr;
        };
        ASTStmt *stm = evalDecl(t,ASTScopeGlobal);
        if(!stm) {
            auto expr = evalExpr(t,ASTScopeGlobal);
            if(!expr)
                return nullptr;
            else 
                return expr;
        }
        else  
            return stm;
    }

     Comment *SyntaxA::evalComment(TokRef first_token){
        Comment *rc;
        switch (first_token.type) {
            case Tok::BlockCommentBegin : {
                rc = new Comment();
                rc->type = Comment::Block;
                TokRef next = nextTok();
                while(next.type != Tok::BlockCommentEnd){
                    rc->val += next.content;
                }
                return rc;
                break;
            }
            case Tok::LineCommentBegin : {
                rc = new Comment();
                rc->type = Comment::Line;
                return rc;
                break;
            }
            default: {
                return nullptr;
                break;
            }
        }
    }

    ASTIdentifier *SyntaxA::buildIdentifier(const Tok & first_token,bool typeScope){
        if(typeScope){
            
        }
        else {
            if(first_token.type == Tok::Identifier){
                ASTIdentifier *id = new ASTIdentifier();
                id->val = first_token.content;
                id->codeRegion.startLine = id->codeRegion.endLine = first_token.srcPos.line;
                id->codeRegion.startCol = first_token.srcPos.startCol;
                id->codeRegion.endCol = first_token.srcPos.endCol;
                return id;
            }
            else {
                return nullptr;
            };
        }
    }

    ASTType *SyntaxA::buildTypeFromTokenStream(TokRef first_token,ASTStmt *parentStmt,ASTTypeContext & ctxt){
        if(first_token.type != Tok::Identifier){
            return nullptr;
        }

        ASTType *baseType = nullptr;
        bool isBuiltinType = false;
        string_ref tok_id (first_token.content);
        if(tok_id == "Void"){
            baseType = VOID_TYPE;
            isBuiltinType = true;
        }
        else if(tok_id == "String"){
            baseType = STRING_TYPE;
            isBuiltinType = true;
        }
        else if(tok_id == "Bool"){
            baseType = BOOL_TYPE;
            isBuiltinType = true;
        }
        else if(tok_id == "Array"){
            baseType = ARRAY_TYPE;
            isBuiltinType = true;
        }
        else if(tok_id == "Dict"){
            baseType = DICTIONARY_TYPE;
            isBuiltinType = true;
        }
        else if(tok_id == "Int"){
            baseType = INT_TYPE;
            isBuiltinType = true;
        }
        else if(tok_id == "Float"){
            baseType = FLOAT_TYPE;
            isBuiltinType = true;
        }
        else if(tok_id == "Regex"){
            baseType = REGEX_TYPE;
            isBuiltinType = true;
        }
        else if(tok_id == "Any"){
            baseType = ANY_TYPE;
            isBuiltinType = true;
        }
        else if(tok_id == "Task"){
            baseType = TASK_TYPE;
            isBuiltinType = true;
        }
        else {
            baseType = ASTType::Create(first_token.content.c_str(),parentStmt,ctxt.isPlaceholder,ctxt.isAlias);
            if(ctxt.genericTypeParams && ctxt.genericTypeParams->find(first_token.content) != ctxt.genericTypeParams->end()){
                baseType->isGenericParam = true;
            }
        }

        Tok tok = aheadTok();
        if(isBuiltinType && tok.type == Tok::LessThan){
            baseType = ASTType::Create(baseType->getName(),parentStmt,false,false);
            isBuiltinType = false;
        }
        if(tok.type == Tok::LessThan){
            gotoNextTok();
            tok = nextTok();
            while(true){
                ASTType *paramType = buildTypeFromTokenStream(tok,parentStmt,ctxt);
                if(!paramType){
                    return nullptr;
                }
                baseType->addTypeParam(paramType);

                tok = aheadTok();
                if(tok.type == Tok::Comma){
                    gotoNextTok();
                    tok = nextTok();
                    continue;
                }
                if(tok.type == Tok::GreaterThan){
                    gotoNextTok();
                    break;
                }
                return nullptr;
            }
        }

        bool sawOptional = false;
        bool sawThrowable = false;
        while(true){
            auto qualifierTok = aheadTok();
            if(qualifierTok.type == Tok::QuestionMark){
                sawOptional = true;
                gotoNextTok();
                continue;
            }
            if(qualifierTok.type == Tok::Exclamation){
                sawThrowable = true;
                gotoNextTok();
                continue;
            }
            break;
        }

        if(!sawOptional && !sawThrowable){
            return baseType;
        }

        if(!isBuiltinType){
            baseType->isOptional = baseType->isOptional || sawOptional;
            baseType->isThrowable = baseType->isThrowable || sawThrowable;
            return baseType;
        }

        auto *qualifiedType = ASTType::Create(baseType->getName(),parentStmt,false,false);
        qualifiedType->isOptional = baseType->isOptional || sawOptional;
        qualifiedType->isThrowable = baseType->isThrowable || sawThrowable;
        return qualifiedType;
    }

}
