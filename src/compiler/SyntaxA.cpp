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
                rc->type = Comment::Line;
                return rc;
                break;
            }
            case Tok::LineCommentBegin : {
                rc = new Comment();
                rc->type = Comment::Block;
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
        if(first_token.type == Tok::Identifier){
            
            string_ref tok_id (first_token.content);
            if(tok_id == "Void"){
                return VOID_TYPE;
            }
            else if(tok_id == "String"){
                return STRING_TYPE;
            }
            else if(tok_id == "Bool"){
                return BOOL_TYPE;
            }
            else if(tok_id == "Array"){
                return ARRAY_TYPE;
            }
            else {
                return ASTType::Create(first_token.content.c_str(),parentStmt,ctxt.isPlaceholder,ctxt.isAlias);
            };
            
        }
        else {
            return nullptr;
        };
    }

}
