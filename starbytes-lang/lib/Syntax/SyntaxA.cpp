#include "starbytes/Syntax/SyntaxA.h"
#include "starbytes/AST/AST.h"
#include <iostream>

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

    ASTType *SyntaxA::buildTypeFromTokenStream(TokRef first_token,ASTStmt *parentStmt){
        if(first_token.type == Tok::Identifier){
            return ASTType::Create(first_token.content,parentStmt);
        }
        else {
            return nullptr;
        };
    };


    ASTExpr *SyntaxA::evalExpr(TokRef first_token,ASTScope *parentScope){
        Tok & tokRef = const_cast<Tok &>(first_token);
        ASTExpr *expr = nullptr;
        /// Paren Unwrapping
        if(tokRef.type == Tok::OpenParen){
            tokRef = nextTok();
            expr = evalExpr(tokRef,parentScope);
            tokRef = nextTok();
            if(tokRef.type != Tok::CloseParen){
                /// ERROR.
            };
        }
        else {
            ASTExpr *node = new ASTExpr();
            expr = node;
            if(tokRef.type == Tok::OpenBracket){
                node->type = ARRAY_EXPR;
                tokRef = nextTok();
                auto firstExpr = evalExpr(nextTok(),parentScope);
                if(!firstExpr){
                    /// ERROR
                };

                node->exprArrayData.push_back(firstExpr);
                /// Last Tok from recent ast expr evaluation.
                while(tokRef.type == Tok::Comma){
                    tokRef = nextTok();
                    auto expr = evalExpr(tokRef,parentScope);
                    if(!expr){
                        /// ERROR
                    };
                    node->exprArrayData.push_back(expr);
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
                
                if(tokRef.type != Tok::OpenParen){
                    node->type = ID_EXPR;
                    return expr;
                };
                
                node->type = IVKE_EXPR;
                
                tokRef = nextTok();
                
                while(tokRef.type != Tok::CloseParen){
                    ASTExpr *expr = evalExpr(tokRef,parentScope);
                    node->exprArrayData.push_back(expr);
                };
                
                tokRef = nextTok();
            }
            else if(tokRef.type == Tok::StringLiteral){
                node->type = STR_LITERAL;
                
                node->literalValue = tokRef.content.substr(1,tokRef.content.size()-2);
                tokRef = nextTok();
            };
        };

        return expr;
    };

    ASTDecl *SyntaxA::evalDecl(TokRef first_token,ASTScope *parentScope){
        if(first_token.type == Tok::Keyword){
            ASTDecl *node;
            if(!commentBuffer.empty()){   
                previousNode->afterComments = commentBuffer;             
                node->beforeComments = commentBuffer;
                commentBuffer.clear();
            };

            /// Import Decl Parse
            if(first_token.content == KW_IMPORT){
                node = new ASTDecl();
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
                node = new ASTDecl();
                node->type = SCOPE_DECL;
                ASTDecl::Property name;
                name.type = ASTDecl::Property::Identifier;
                if(!(name.dataPtr = buildIdentifier(nextTok(),false))){
                    /// RETURN!
                };
                node->declProps.push_back(name);
            }
            /// Var Decl Parse
            else if(first_token.content == KW_DECL){
                node = new ASTDecl();
                node->type = VAR_DECL;
                TokRef tok0 = nextTok();
                if(tok0.type == Tok::Keyword){
                    if(tok0.content == KW_IMUT)
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
                        tok1 = nextTok();
                        ASTType *type = buildTypeFromTokenStream(tok1,varDeclarator);
                        if(!type){
                        /// Throw Error;   
                        };
                        varDeclarator->declType = type;

                        tok1 = nextTok();
                    }

                    prop.dataPtr = varDeclarator;

                    node->declProps.push_back(prop);

                    if(tok1.type != Tok::Equal){
                        return node;
                    };

                    tok1 = nextTok();
                    ASTExpr * val = evalExpr(tok1,parentScope);
                    if(!val){
                       // Throw Error 
                    };
                    /// Var Initializer

                    ASTDecl::Property prop2;
                    prop2.type = ASTDecl::Property::Expr;
                    prop2.dataPtr = val;
                    varDeclarator->declProps.push_back(prop2);
                }
                else {
                    /// Throw Error.
                };
            }
            else if(first_token.content == KW_FUNC){
                node = new ASTFuncDecl();
                ASTFuncDecl *func_node = (ASTFuncDecl *)node;
                node->type = FUNC_DECL;
                Tok & tok0 = const_cast<Tok &>(nextTok());
                ASTDecl::Property id;
                id.type = ASTDecl::Property::Identifier;
                if(!(id.dataPtr = buildIdentifier(tok0,false))){
                    /// Throw Error
                };
                
                func_node->declProps.push_back(id);
                
                tok0 = nextTok();
                
                if(tok0.type == Tok::OpenParen){
                    tok0 = nextTok();
                    
                    while(tok0.type != Tok::CloseParen){
                        ASTIdentifier *param_id = buildIdentifier(tok0,false);
                        if(!param_id){
                            /// Throw Error.
                            return nullptr;
                            break;
                        };
                        
                        tok0 = nextTok();
                        
                        ASTType *param_type = buildTypeFromTokenStream(tok0,func_node);
                        if(!param_type){
                            /// Throw Error.
                            return nullptr;
                            break;
                        };
                        func_node->params.insert(std::make_pair(param_id,param_type));
                        
                        tok0 = nextTok();
                        
                        if(tok0.type != Tok::Comma || tok0.type != Tok::CloseParen){
                            /// Throw Error.
                            return nullptr;
                            break;
                        };
                    };
                    
                    tok0 = nextTok();
                    
                    if(tok0.type == Tok::Identifier){
                        ASTType *type;
                        if(!(type = buildTypeFromTokenStream(tok0,func_node))){
                            /// Throw Error.
                            return nullptr;
                        };
                        func_node->declType = type;
                    };
                    
                    tok0 = tok0.type == Tok::Identifier? nextTok() : tok0;
                    
                    if(tok0.type == Tok::OpenBrace){
                        ASTBlockStmt block;
                        
                        ASTScope *function_scope = new ASTScope({((ASTIdentifier *)func_node->declProps[0].dataPtr)->val,ASTScope::Function,parentScope});
                        
                        tok0 = aheadTok();
                        while(tok0.type != Tok::CloseBrace){
                            ASTStmt *fin;
                            ASTStmt *stm = evalDecl(tok0,function_scope);
                            if(!stm) {
                                auto expr = evalExpr(tok0,function_scope);
                                if(!expr)
                                    /// Throw Error
                                    return nullptr;
                                else
                                    fin = expr;
                            }
                            else
                                fin = stm;
                            
                            block.body.push_back(fin);
                            /// Upon Evalation of any node...
                            /// The next token after the final node scope is becomes the currentToken
                            tok0 = token_stream[privTokIndex];
                        };
                    }
                    else {
                        /// Throw Error
                        return nullptr;
                    };
                    
                }
                else {
                    /// Throw Error
                };
                
                ASTBlockStmt block;
                
            }
            else if(first_token.content == KW_CLASS){
                node->type = CLS_DECL;
                TokRef tok0 = nextTok();
                ASTDecl::Property id;
                id.type = ASTDecl::Property::Identifier;
                if(!(id.dataPtr = buildIdentifier(tok0,true))){
                    /// Throw Error
                };
                
            }
            else if(first_token.content == KW_IMUT){
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
    };
};
