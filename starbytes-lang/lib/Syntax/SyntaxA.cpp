#include "starbytes/Syntax/SyntaxA.h"
#include "starbytes/AST/AST.h"

namespace starbytes::Syntax {

    typedef const Tok & TokRef;

    void SyntaxA::setTokenStream(llvm::ArrayRef<Tok> toks){
        token_stream = toks;
        privTokIndex = 0;
    };

    void SyntaxA::gotoNextTok(){
        ++privTokIndex;
    };

    TokRef SyntaxA::nextTok(){
        return token_stream[++privTokIndex];
    };

    TokRef SyntaxA::aheadTok(){
        return token_stream[privTokIndex + 1];
    };

    size_t SyntaxA::getTokenStreamWidth(){
        return token_stream.size();
    };


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
    };

    ASTIdentifier *SyntaxA::buildIdentifier(const Tok & first_token,bool typeScope){
        if(first_token.type == Tok::Identifier){
            ASTIdentifier *id = new ASTIdentifier();
            id->val = first_token.content;
            id->loc.startLine = id->loc.endLine = first_token.srcPos.line;
            id->loc.startCol = first_token.srcPos.startCol;
            id->loc.endCol = first_token.srcPos.endCol;
            return id;
        }
        else {
            return nullptr;
        };
    };


    ASTExpr *SyntaxA::evalExpr(TokRef first_token){

    };

    ASTDecl *SyntaxA::evalDecl(TokRef first_token){
        if(first_token.type == Tok::Keyword){
            ASTDecl *node = new ASTDecl();
            if(!commentBuffer.empty()){   
                previousNode->afterComments = commentBuffer;             
                node->beforeComments = commentBuffer;
                commentBuffer.clear();
            };

            /// Import Decl Parse
            if(first_token.content == "import"){
                node->type = IMPORT_DECL;
                ASTDecl::Property module_name;
                module_name.type = ASTDecl::Property::Identifier;
                if(!(module_name.dataPtr = buildIdentifier(nextTok(),false))){
                    /// BAIL!!!
                };
            }
            // else if(first_token.content == "scope"){
                
            // }
            else if(first_token.content == "decl"){
                node->type = VAR_DECL;
                TokRef tok0 = nextTok();
                if(tok0.type == Tok::Keyword){
                    if(tok0.content == "immutable")
                        node->type = CONSTVAR_DECL;
                    else; 
                        /// Throw Error;
                }
                TokRef tok1 = tok0.type == Tok::Keyword? nextTok() : tok0;
                if(tok0.type == Tok::Identifier){

                }
                else {
                    /// Throw Error.
                };
            }
            else if(first_token.content == "class"){
                node->type = CLS_DECL;
                TokRef tok0 = nextTok();
                ASTDecl *cls = new ASTDecl();
                ASTDecl::Property id;
                id.type = ASTDecl::Property::Identifier;
                if(!(id.dataPtr = buildIdentifier(tok0,true))){
                    /// Throw Error
                };
                
            }
            else if(first_token.content == "immutable"){
                /// Throw Unknown Error;
                return nullptr;
            };

            return node;
        }
        else {
            return nullptr;
        };
    };

    ASTStmt * SyntaxA::nextStatement(){
        TokRef t = token_stream[privTokIndex];
        ASTStmt *stm = evalDecl(t);
        if(!stm)
            return evalExpr(t);
        else  
            return stm;
    };
};