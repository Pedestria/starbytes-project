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
        Tok & tokRef = const_cast<Tok &>(first_token);
        ASTExpr *expr = nullptr;
        /// Paren Unwrapping
        if(tokRef.type == Tok::OpenParen){
            tokRef = nextTok();
            expr = evalExpr(tokRef);
            tokRef = nextTok();
            if(tokRef.type != Tok::CloseParen){
                /// ERROR.
            };
        }
        else {
            ASTExpr *node = new ASTExpr();
            if(tokRef.type == Tok::OpenBracket){
                tokRef = nextTok();
                auto firstExpr = evalExpr(nextTok());
                if(!firstExpr){
                    /// ERROR
                };

                node->arrayExpr.push_back(firstExpr);
                /// Last Tok from recent ast expr evaluation.
                while(tokRef.type == Tok::Comma){
                    tokRef = nextTok();
                    auto expr = evalExpr(tokRef);
                    if(!expr){
                        /// ERROR
                    };
                    node->arrayExpr.push_back(expr);
                };

                if(tokRef.type != Tok::CloseBracket){
                    /// ERROR
                };
                tokRef = nextTok();
            }
            else if(tokRef.type == Tok::Identifier){
                ASTIdentifier *id;
                if(!(id = buildIdentifier(tokRef,false))){
                    /// ERROR
                };
                node->id = id;
                tokRef = nextTok();
            }
        };

        return expr;
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
                    /// RETURN !
                };
                node->declProps.push_back(module_name);
                ++privTokIndex;
            }
            else if(first_token.content == "scope"){
                node->type = SCOPE_DECL;
                ASTDecl::Property name;
                name.type = ASTDecl::Property::Identifier;
                if(!(name.dataPtr = buildIdentifier(nextTok(),false))){
                    /// RETURN!
                };
                node->declProps.push_back(name);
            }
            /// Var Decl Parse
            else if(first_token.content == "decl"){
                node->type = VAR_DECL;
                TokRef tok0 = nextTok();
                if(tok0.type == Tok::Keyword){
                    if(tok0.content == "immutable")
                        node->type = CONSTVAR_DECL;
                    else; 
                        /// Throw Error;
                }
                Tok & tok1 = const_cast<Tok &>(tok0.type == Tok::Keyword? nextTok() : tok0);
                if(tok1.type == Tok::Identifier){
                    ASTIdentifier * id = buildIdentifier(tok1,false);
                    if(!id){
                     /// Throw Error;   
                    };
                    /// Var Declarator!!
                    ASTDecl *varDeclarator = new ASTDecl();
                    
                    ASTDecl::Property prop,id_prop;
                    prop.type = ASTDecl::Property::Decl;
                    /// Attach Identifier to Declarator
                    id_prop.type = ASTDecl::Property::Identifier;
                    id_prop.dataPtr = id;
                    varDeclarator->declProps.push_back(id_prop);
                    

                    tok1 = nextTok();
                    if(tok1.type == Tok::Colon){
                        ASTIdentifier *type_id = buildIdentifier(tok1,true);
                        if(!type_id){
                        /// Throw Error;   
                        };
                        ASTDecl::Property _prop;
                        prop.type = ASTDecl::Property::Identifier;
                        prop.dataPtr = type_id;
                        varDeclarator->typeProps.push_back(_prop);

                        tok1 = nextTok();
                    }

                    prop.dataPtr = varDeclarator;

                    node->declProps.push_back(prop);

                    if(tok1.type != Tok::Equal){
                        return node;
                    };

                    tok1 = nextTok();
                    ASTExpr * val = evalExpr(tok1);
                    if(!val){
                       // Throw Error 
                    };
                    /// Var Initializer

                    ASTDecl::Property prop2;
                    prop.type = ASTDecl::Property::Expr;
                    prop.dataPtr = val;
                    varDeclarator->declProps.push_back(prop2);
                }
                else {
                    /// Throw Error.
                };
            }
            else if(first_token.content == "func"){
                node->type = FUNC_DECL;
                
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
        if(!stm) {
            auto expr = evalExpr(t);
            if(!expr)
                return stm;
            else 
                return expr;
        }
        else  
            return stm;
    };
};