#include "starbytes/AST/ASTNodes.def"
#include "starbytes/Syntax/SyntaxA.h"
#include "starbytes/AST/AST.h"

namespace starbytes::Syntax {


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
}

    ASTDecl *SyntaxA::evalDecl(TokRef first_token,ASTScope *parentScope){
        if(first_token.type == Tok::Keyword){
            ASTDecl *node;
            // if(!commentBuffer.empty()){   
            //     previousNode->afterComments = commentBuffer;             
            //     node->beforeComments = commentBuffer;
            //     commentBuffer.clear();
            // };

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
            else if(first_token.content == KW_IF){
                ASTConditionalDecl *condDecl = new ASTConditionalDecl();
                node = condDecl;
                node->type = COND_DECL;
                
                Tok & tok0 = const_cast<Tok &>(nextTok());
                if(tok0.type != Tok::OpenParen){
                    /// Throw Error
                };
                tok0 = nextTok();
                
                ASTExpr *boolExpr = evalExpr(tok0,parentScope);
                if(!boolExpr)
                    return nullptr;
                
                tok0 = token_stream[privTokIndex];
                if(tok0.type != Tok::CloseParen){
                    /// Throw Error.
                };
                ASTScope *scopeConditionalIF = new ASTScope({"IF_COND_DECL",ASTScope::Neutral,parentScope});
                
                tok0 = nextTok();
                ASTBlockStmt *blockStmt = evalBlockStmt(tok0,scopeConditionalIF);
                if(!blockStmt)
                {
                    return nullptr;
                }
                ASTConditionalDecl::CondDecl cond;
                cond.expr = boolExpr;
                cond.blockStmt = blockStmt;
                condDecl->specs.push_back(cond);
                
                tok0 = nextTok();
                if(tok0.type == Tok::Keyword){
                    while(true){
                        if(tok0.content == KW_ELIF){
                            tok0 = nextTok();
                            if(tok0.type != Tok::OpenParen){
                                /// Throw Error
                            };
                            tok0 = nextTok();
                            
                            ASTExpr *boolExpr = evalExpr(tok0,parentScope);
                            if(!boolExpr)
                                /// Throw Error.
                                return nullptr;
                            
                            tok0 = token_stream[privTokIndex];
                            if(tok0.type != Tok::CloseParen){
                                /// Throw Error.
                                return nullptr;
                            };
                            ASTScope *scopeConditionalIF = new ASTScope({"ELIF_COND_DECL",ASTScope::Neutral,parentScope});
                            
                            tok0 = nextTok();
                            ASTBlockStmt *blockStmt = evalBlockStmt(tok0,scopeConditionalIF);
                            if(!blockStmt)
                            {
                                 /// Throw Error.
                                return nullptr;
                            }
                            ASTConditionalDecl::CondDecl cond;
                            cond.expr = boolExpr;
                            cond.blockStmt = blockStmt;
                            condDecl->specs.push_back(cond);
                            
                            tok0 = nextTok();
                            
                            if(tok0.content != KW_ELIF){
                                break;
                            };
                        }
                        else {
                            break;
                        }
                    }
                    if(tok0.content == KW_ELSE){
                        ASTScope *scopeConditionalElse = new ASTScope({"ELSE_COND_DECL",ASTScope::Neutral,parentScope});
                        
                        tok0 = nextTok();
                        ASTBlockStmt *blockStmt = evalBlockStmt(tok0,scopeConditionalElse);
                        if(!blockStmt)
                        {
                            return nullptr;
                        }
                        
                        ASTConditionalDecl::CondDecl cond;
                        cond.expr = nullptr;
                        cond.blockStmt = blockStmt;
                        condDecl->specs.push_back(cond);
                        gotoNextTok();
                    }
                };
                
            }
            else if(first_token.content == KW_RETURN){
                ASTReturnDecl *return_decl = new ASTReturnDecl();
                node = return_decl;
                node->type = RETURN_DECL;
                node->scope = parentScope;
                TokRef tok0 = nextTok();
                ASTExpr * val = evalExpr(tok0,parentScope);
                if(!val){
                    return_decl->expr = nullptr;
                }
                else {
                    return_decl->expr = val;
                }

                
                
            }
            /// Var Decl Parse
            else if(first_token.content == KW_DECL){
                ASTVarDecl *varDecl = new ASTVarDecl();
                node = varDecl;
                node->type = VAR_DECL;
                node->scope = parentScope;
                TokRef tok0 = nextTok();
                if(tok0.type == Tok::Keyword){
                    if(tok0.content == KW_IMUT)
                        varDecl->isConst = true;
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
                node->scope = parentScope;
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
            // else {
            //     node = nullptr;
            // }
            else if(first_token.content == KW_CLASS){
                auto n = new ASTClassDecl();
                node = n;
                node->type = CLASS_DECL;
                TokRef tok0 = nextTok();
//               
                
            }
            else if(first_token.content == KW_IMUT){
                /// Throw Unknown Error;
                return nullptr;
            }
            else if(first_token.content == KW_ELSE){
                return nullptr;
            };

            return node;
        }
        else {
            
            return nullptr;
        };
    }


}