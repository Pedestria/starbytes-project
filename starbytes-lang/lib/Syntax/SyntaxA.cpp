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
            
            llvm::StringRef tok_id = first_token.content;
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
                return ASTType::Create(first_token.content,parentStmt);
            };
            
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
            ASTLiteralExpr *literal_expr = nullptr;
            /// Literals
            if(tokRef.type == Tok::StringLiteral){
                literal_expr = new ASTLiteralExpr();
                literal_expr->type = STR_LITERAL;
                
                literal_expr->strValue = tokRef.content.substr(1,tokRef.content.size()-2);
                expr = literal_expr;
                tokRef = nextTok();
            }
            else if(tokRef.type == Tok::BooleanLiteral){
                literal_expr = new ASTLiteralExpr();
                
                literal_expr->type = BOOL_LITERAL;
                literal_expr->boolValue = (tokRef.content == TOK_TRUE? true : tokRef.content == TOK_FALSE? false : NULL);
                
                expr = literal_expr;
                tokRef = nextTok();
            }
            else if(tokRef.type == Tok::OpenBracket){
                ASTExpr *node = new ASTExpr();
                expr = node;
                node->type = ARRAY_EXPR;
                tokRef = nextTok();
                auto firstExpr = evalExpr(tokRef,parentScope);
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
                ASTExpr *node = new ASTExpr();
                expr = node;
                
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
            
        };

        return expr;
    };

ASTBlockStmt *SyntaxA::evalBlockStmt(const Tok & first_token,ASTScope *parentScope) {
    Tok & tok0 = const_cast<Tok &>(first_token);
    
    if(tok0.type == Tok::OpenBrace){
        ASTBlockStmt *block = new ASTBlockStmt();
        block->parentScope = parentScope;
        tok0 = nextTok();
        while(tok0.type != Tok::CloseBrace){
//            std::cout << "No!!" << std::endl;
            ASTStmt *fin;
            ASTStmt *stm = evalDecl(tok0,parentScope);
            if(!stm) {
                auto expr = evalExpr(tok0,parentScope);
                if(!expr)
                    /// Throw Error
                    return nullptr;
                else
                    fin = expr;
            }
            else
                fin = stm;
            
            block->body.push_back(fin);
            /// Upon Evalation of any node...
            /// The next token after the final node block becomes the currentToken
            tok0 = token_stream[privTokIndex];
        };
        return block;
    }
    else {
        return nullptr;
    };
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
                ASTImportDecl *imp_decl = new ASTImportDecl();
                node = imp_decl;
                node->type = IMPORT_DECL;
                ASTIdentifier *mod_id;
                if(!(mod_id = buildIdentifier(nextTok(),false))){
                    /// RETURN !
                };
                imp_decl->moduleName = mod_id;
                gotoNextTok();
            }
//            else if(first_token.content == "scope"){
//                node = new ASTDecl();
//                node->type = SCOPE_DECL;
//                ASTDecl::Property name;
//                name.type = ASTDecl::Property::Identifier;
//                if(!(name.dataPtr = buildIdentifier(nextTok(),false))){
//                    /// RETURN!
//                };
//                node->declProps.push_back(name);
//            }
            /// Var Decl Parse
            else if(first_token.content == KW_DECL){
                ASTVarDecl *varDecl = new ASTVarDecl();
                node = varDecl;
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
                    ASTVarDecl::VarSpec spec;
                    
                    spec.id = id;
                    

                    tok1 = nextTok();
                    
                    if(tok1.type == Tok::Colon){
                        tok1 = nextTok();
                        ASTType *type = buildTypeFromTokenStream(tok1,varDecl);
                        if(!type){
                        /// Throw Error;   
                        };
                        spec.type = type;

                        tok1 = nextTok();
                    }


                    if(tok1.type != Tok::Equal){
                        varDecl->specs.push_back(spec);
                        return varDecl;
                    };

                    tok1 = nextTok();
                    ASTExpr * val = evalExpr(tok1,parentScope);
                    if(!val){
                       // Throw Error 
                    };
                    /// Var Initializer

                    spec.expr = val;
                    
                    varDecl->specs.push_back(spec);
                   
                }
                else {
                    /// Throw Error.
                };
            }
            else if(first_token.content == KW_FUNC){
                ASTFuncDecl *func_node = new ASTFuncDecl();
                node = func_node;
                node->type = FUNC_DECL;
                Tok & tok0 = const_cast<Tok &>(nextTok());
                
                if(!(func_node->funcId = buildIdentifier(tok0,false))){
                    /// Throw Error
                };
                
                tok0 = nextTok();
                
                if(tok0.type == Tok::OpenParen){
                    tok0 = nextTok();
                    
                    while(tok0.type != Tok::CloseParen){
//                        std::cout << "No!!" << std::endl;
//                        std::cout << "no 1" << std::endl;
                        ASTIdentifier *param_id = buildIdentifier(tok0,false);
                        if(!param_id){
                            /// Throw Error.
                            return nullptr;
                            break;
                        };
                        
                        tok0 = nextTok();
                        
//                        std::cout << "no 2" << std::endl;
                        
                        if(tok0.type != Tok::Colon){
                            /// Throw Error.
                            return nullptr;
                            break;
                        };
                        
                        tok0 = nextTok();
                        
//                        std::cout << "no 3" << std::endl;
                        
                        ASTType *param_type = buildTypeFromTokenStream(tok0,func_node);
                        if(!param_type){
                            /// Throw Error.
                            return nullptr;
                            break;
                        };
                        func_node->params.insert(std::make_pair(param_id,param_type));
                        
                        tok0 = nextTok();
                        
//                        std::cout << "no 4" << std::endl;
//
//                        std::cout << llvm::formatv("{0}",tok0).str();
                        
                        if(tok0.type == Tok::Comma){
                            tok0 = nextTok();
                        }
                        else if(tok0.type == Tok::CloseParen){
                            break;
                        }
                        else {
                            return nullptr;
                            break;
                        };
//                        std::cout << "no 5" << std::endl;
                        
                    };
                    
//                    std::cout << "Go 1" << std::endl;
                    
                    tok0 = nextTok();
                    
                    if(tok0.type == Tok::Identifier){
                        ASTType *type;
                        if(!(type = buildTypeFromTokenStream(tok0,func_node))){
                            /// Throw Error.
                            return nullptr;
                        };
                        func_node->returnType = type;
                    };
                    
//                    std::cout << "Go 2" << std::endl;
                    
                    tok0 = (tok0.type == Tok::Identifier? nextTok() : tok0);
                    
//                    std::cout << "Go 3" << std::endl;
                    
                    if(tok0.type == Tok::OpenBrace){
                       
                        
                        ASTScope *function_scope = new ASTScope({func_node->funcId->val,ASTScope::Function,parentScope});
                        
                        auto block = evalBlockStmt(tok0,function_scope);
                        if(!block){
                            return nullptr;
                        };
                        func_node->blockStmt = block;
                        gotoNextTok();
                    }
                    else {
                        /// Throw Error
                        return nullptr;
                    };
                    
                }
                else {
                    return nullptr;
                    /// Throw Error
                };
                
            }
            else if(first_token.content == KW_CLASS){
                node->type = CLS_DECL;
                TokRef tok0 = nextTok();
//                ASTDecl::Property id;
//                id.type = ASTDecl::Property::Identifier;
//                if(!(id.dataPtr = buildIdentifier(tok0,true))){
//                    /// Throw Error
//                };
                
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
