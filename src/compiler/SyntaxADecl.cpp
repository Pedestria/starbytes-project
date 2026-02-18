#include "starbytes/compiler/ASTNodes.def"
#include "starbytes/compiler/SyntaxA.h"
#include "starbytes/compiler/AST.h"
#include <cstdlib>
#include <string>

namespace starbytes::Syntax {


ASTBlockStmt *SyntaxA::evalBlockStmt(const Tok & first_token,std::shared_ptr<ASTScope> parentScope) {
    Tok tok0 = first_token;
    
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

    ASTDecl *SyntaxA::evalDecl(TokRef first_token,std::shared_ptr<ASTScope> parentScope){
        Tok currentTok = first_token;
        std::vector<ASTAttribute> parsedAttributes;

        auto parseAttributeValueExpr = [&](TokRef tok) -> ASTExpr * {
            if(tok.type == Tok::StringLiteral){
                auto *literal = new ASTLiteralExpr();
                literal->type = STR_LITERAL;
                literal->strValue = tok.content.substr(1,tok.content.size()-2);
                return literal;
            }
            if(tok.type == Tok::BooleanLiteral){
                auto *literal = new ASTLiteralExpr();
                literal->type = BOOL_LITERAL;
                literal->boolValue = (tok.content == TOK_TRUE);
                return literal;
            }
            if(tok.type == Tok::NumericLiteral || tok.type == Tok::FloatingNumericLiteral){
                auto *literal = new ASTLiteralExpr();
                literal->type = NUM_LITERAL;
                if(tok.type == Tok::FloatingNumericLiteral){
                    literal->floatValue = (starbytes_float_t)::atof(tok.content.c_str());
                }
                else {
                    literal->intValue = (starbytes_int_t)std::stoll(tok.content);
                }
                return literal;
            }
            if(tok.type == Tok::Identifier){
                auto *expr = new ASTExpr();
                expr->type = ID_EXPR;
                expr->id = buildIdentifier(tok,false);
                return expr;
            }
            return nullptr;
        };

        auto parseAttributes = [&]() -> bool {
            while(currentTok.type == Tok::AtSign){
                ASTAttribute attr;
                Tok attrNameTok = nextTok();
                if(attrNameTok.type != Tok::Identifier){
                    return false;
                }
                attr.name = attrNameTok.content;

                Tok lookahead = aheadTok();
                if(lookahead.type == Tok::OpenParen){
                    gotoNextTok();
                    Tok argTok = nextTok();
                    while(argTok.type != Tok::CloseParen){
                        ASTAttributeArg arg;
                        if(argTok.type == Tok::Identifier){
                            Tok maybeEq = aheadTok();
                            if(maybeEq.type == Tok::Equal){
                                arg.key = argTok.content;
                                gotoNextTok();
                                argTok = nextTok();
                            }
                        }

                        auto *valueExpr = parseAttributeValueExpr(argTok);
                        if(!valueExpr){
                            return false;
                        }
                        arg.value = valueExpr;
                        attr.args.push_back(arg);

                        argTok = nextTok();
                        if(argTok.type == Tok::Comma){
                            argTok = nextTok();
                            continue;
                        }
                        if(argTok.type != Tok::CloseParen){
                            return false;
                        }
                    }
                    gotoNextTok();
                }
                else {
                    gotoNextTok();
                }

                parsedAttributes.push_back(std::move(attr));
                currentTok = token_stream[privTokIndex];
            }
            return true;
        };

        if(!parseAttributes()){
            return nullptr;
        }

        if(currentTok.type == Tok::Keyword){
            ASTDecl *node;
            // if(!commentBuffer.empty()){   
            //     previousNode->afterComments = commentBuffer;             
            //     node->beforeComments = commentBuffer;
            //     commentBuffer.clear();
            // };

            /// Import Decl Parse
            if(currentTok.content == KW_IMPORT){
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
            else if(currentTok.content == KW_SCOPE){
                auto *scopeDecl = new ASTScopeDecl();
                node = scopeDecl;
                node->type = SCOPE_DECL;
                node->scope = parentScope;

                Tok tok0 = nextTok();
                scopeDecl->scopeId = buildIdentifier(tok0,false);
                if(!scopeDecl->scopeId){
                    return nullptr;
                }
                tok0 = nextTok();
                if(tok0.type != Tok::OpenBrace){
                    return nullptr;
                }

                auto scopeNode = std::shared_ptr<ASTScope>(new ASTScope{scopeDecl->scopeId->val,ASTScope::Namespace,parentScope});
                scopeNode->generateHashID();
                auto *blockStmt = evalBlockStmt(tok0,scopeNode);
                if(!blockStmt){
                    return nullptr;
                }
                scopeDecl->blockStmt = blockStmt;
                gotoNextTok();
            }
            else if(currentTok.content == KW_IF){
                ASTConditionalDecl *condDecl = new ASTConditionalDecl();
                node = condDecl;
                node->type = COND_DECL;
                
                Tok tok0 = nextTok();
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
                std::shared_ptr<ASTScope> scopeConditionalIF (new ASTScope({"IF_COND_DECL",ASTScope::Neutral,parentScope}));
                
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
                            std::shared_ptr<ASTScope> scopeConditionalIF (new ASTScope({"ELIF_COND_DECL",ASTScope::Neutral,parentScope}));
                            
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
                        std::shared_ptr<ASTScope> scopeConditionalElse (new ASTScope{"ELSE_COND_DECL",ASTScope::Neutral,parentScope});
                        
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
            else if(currentTok.content == KW_FOR || currentTok.content == KW_WHILE){
                bool isWhileLoop = (currentTok.content == KW_WHILE);
                ASTExpr *conditionExpr = nullptr;
                ASTBlockStmt *loopBlock = nullptr;

                if(isWhileLoop){
                    auto *whileDecl = new ASTWhileDecl();
                    node = whileDecl;
                    node->type = WHILE_DECL;
                    node->scope = parentScope;
                }
                else {
                    auto *forDecl = new ASTForDecl();
                    node = forDecl;
                    node->type = FOR_DECL;
                    node->scope = parentScope;
                }

                Tok tok0 = nextTok();
                if(tok0.type != Tok::OpenParen){
                    return nullptr;
                }

                tok0 = nextTok();
                conditionExpr = evalExpr(tok0,parentScope);
                if(!conditionExpr){
                    return nullptr;
                }

                tok0 = token_stream[privTokIndex];
                if(tok0.type != Tok::CloseParen){
                    return nullptr;
                }

                tok0 = nextTok();
                if(tok0.type != Tok::OpenBrace){
                    return nullptr;
                }

                std::shared_ptr<ASTScope> loopScope(new ASTScope({isWhileLoop ? "WHILE_LOOP_DECL" : "FOR_LOOP_DECL",ASTScope::Neutral,parentScope}));
                loopScope->generateHashID();
                loopBlock = evalBlockStmt(tok0,loopScope);
                if(!loopBlock){
                    return nullptr;
                }

                if(isWhileLoop){
                    auto *whileDecl = (ASTWhileDecl *)node;
                    whileDecl->expr = conditionExpr;
                    whileDecl->blockStmt = loopBlock;
                }
                else {
                    auto *forDecl = (ASTForDecl *)node;
                    forDecl->expr = conditionExpr;
                    forDecl->blockStmt = loopBlock;
                }
                gotoNextTok();
            }
            else if(currentTok.content == KW_SECURE){
                auto *secureDecl = new ASTSecureDecl();
                node = secureDecl;
                node->type = SECURE_DECL;
                node->scope = parentScope;

                Tok tok0 = nextTok();
                if(tok0.type != Tok::OpenParen){
                    return nullptr;
                }

                tok0 = nextTok();
                auto *innerDecl = evalDecl(tok0,parentScope);
                if(!innerDecl || innerDecl->type != VAR_DECL){
                    return nullptr;
                }
                auto *guardedDecl = (ASTVarDecl *)innerDecl;
                guardedDecl->isSecureWrapped = true;
                if(guardedDecl->specs.size() != 1 || !guardedDecl->specs.front().expr){
                    return nullptr;
                }
                secureDecl->guardedDecl = guardedDecl;

                tok0 = token_stream[privTokIndex];
                if(tok0.type != Tok::CloseParen){
                    return nullptr;
                }

                tok0 = nextTok();
                if(tok0.type != Tok::Keyword || tok0.content != KW_CATCH){
                    return nullptr;
                }

                tok0 = nextTok();
                if(tok0.type == Tok::OpenParen){
                    Tok errTok = nextTok();
                    if(errTok.type != Tok::Identifier){
                        return nullptr;
                    }
                    secureDecl->catchErrorId = buildIdentifier(errTok,false);
                    if(!secureDecl->catchErrorId){
                        return nullptr;
                    }

                    Tok colonTok = nextTok();
                    if(colonTok.type != Tok::Colon){
                        return nullptr;
                    }

                    Tok typeTok = nextTok();
                    ASTTypeContext typeCtxt;
                    typeCtxt.isPlaceholder = true;
                    secureDecl->catchErrorType = buildTypeFromTokenStream(typeTok,secureDecl,typeCtxt);
                    if(!secureDecl->catchErrorType){
                        return nullptr;
                    }

                    Tok closeCatchSigTok = nextTok();
                    if(closeCatchSigTok.type != Tok::CloseParen){
                        return nullptr;
                    }

                    tok0 = nextTok();
                }

                if(tok0.type != Tok::OpenBrace){
                    return nullptr;
                }

                std::shared_ptr<ASTScope> catchScope(new ASTScope({"SECURE_CATCH_DECL",ASTScope::Neutral,parentScope}));
                catchScope->generateHashID();
                secureDecl->catchBlock = evalBlockStmt(tok0,catchScope);
                if(!secureDecl->catchBlock){
                    return nullptr;
                }
                gotoNextTok();
            }
            else if(currentTok.content == KW_RETURN){
                ASTReturnDecl *return_decl = new ASTReturnDecl();
                node = return_decl;
                node->type = RETURN_DECL;
                node->scope = parentScope;
                Tok tok0 = nextTok();
                ASTExpr * val = evalExpr(tok0,parentScope);
                if(!val){
                    return_decl->expr = nullptr;
                }
                else {
                    return_decl->expr = val;
                }

                
                
            }
            /// Var Decl Parse
            else if(currentTok.content == KW_DECL){
                ASTVarDecl *varDecl = new ASTVarDecl();
                node = varDecl;
                node->type = VAR_DECL;
                node->scope = parentScope;
                Tok tok0 = nextTok();
                if(tok0.type == Tok::Keyword){
                    if(tok0.content == KW_IMUT)
                        varDecl->isConst = true;
                    else; 
                        /// Throw Error;
                }
                Tok tok1 = (tok0.type == Tok::Keyword? nextTok() : tok0);
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
                        ASTTypeContext type_ctxt;
                        type_ctxt.isPlaceholder = true;
                        ASTType *type = buildTypeFromTokenStream(tok1,varDecl,type_ctxt);
                        if(!type){
                        /// Throw Error;   
                        };
                        spec.type = type;

                        tok1 = nextTok();
                    }


                    if(tok1.type != Tok::Equal){
                        varDecl->specs.push_back(spec);
                    }
                    else {
                        tok1 = nextTok();
                        ASTExpr * val = evalExpr(tok1,parentScope);
                        if(!val){
                           // Throw Error 
                        };
                        /// Var Initializer
                        spec.expr = val;
                        varDecl->specs.push_back(spec);
                    }
                   
                }
                else {
                    /// Throw Error.
                };
            }
            else if(currentTok.content == KW_FUNC){
                ASTFuncDecl *func_node = new ASTFuncDecl();
                node = func_node;
                node->type = FUNC_DECL;
                node->scope = parentScope;
                Tok tok0 = nextTok();
                
                if(!(func_node->funcId = buildIdentifier(tok0,false))){
                    /// Throw Error
                };
                ASTTypeContext type_ctxt;
                type_ctxt.isPlaceholder = false;
                func_node->funcType = buildTypeFromTokenStream(tok0,func_node,type_ctxt);
                
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
                        
                        ASTTypeContext type_ctxt;
                        type_ctxt.isPlaceholder = true;
                        
                        ASTType *param_type = buildTypeFromTokenStream(tok0,func_node,type_ctxt);
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
                        ASTTypeContext type_ctxt;
                        type_ctxt.isPlaceholder = true;
                        if(!(type = buildTypeFromTokenStream(tok0,func_node,type_ctxt))){
                            /// Throw Error.
                            return nullptr;
                        };
                        func_node->returnType = type;
                    };
                    
//                    std::cout << "Go 2" << std::endl;
                    
                    tok0 = (tok0.type == Tok::Identifier? nextTok() : tok0);
                    
//                    std::cout << "Go 3" << std::endl;
                    
                    if(tok0.type == Tok::OpenBrace){
                       
                        
                        std::shared_ptr<ASTScope> function_scope (new ASTScope({func_node->funcId->val,ASTScope::Function,parentScope}));
                        
                        function_scope->generateHashID();
                        
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
            else if(currentTok.content == KW_NEW && parentScope && parentScope->type == ASTScope::Class){
                auto *ctorNode = new ASTConstructorDecl();
                node = ctorNode;
                node->type = CLASS_CTOR_DECL;
                node->scope = parentScope;

                Tok tok0 = nextTok();
                if(tok0.type != Tok::OpenParen){
                    return nullptr;
                }
                tok0 = nextTok();
                while(tok0.type != Tok::CloseParen){
                    ASTIdentifier *param_id = buildIdentifier(tok0,false);
                    if(!param_id){
                        return nullptr;
                    }
                    tok0 = nextTok();
                    if(tok0.type != Tok::Colon){
                        return nullptr;
                    }
                    tok0 = nextTok();
                    ASTTypeContext type_ctxt;
                    type_ctxt.isPlaceholder = true;
                    ASTType *param_type = buildTypeFromTokenStream(tok0,ctorNode,type_ctxt);
                    if(!param_type){
                        return nullptr;
                    }
                    ctorNode->params.insert(std::make_pair(param_id,param_type));
                    tok0 = nextTok();
                    if(tok0.type == Tok::Comma){
                        tok0 = nextTok();
                        continue;
                    }
                    if(tok0.type != Tok::CloseParen){
                        return nullptr;
                    }
                }

                tok0 = nextTok();
                if(tok0.type != Tok::OpenBrace){
                    return nullptr;
                }
                std::shared_ptr<ASTScope> ctor_scope(new ASTScope({"new",ASTScope::Function,parentScope}));
                ctor_scope->generateHashID();
                auto block = evalBlockStmt(tok0,ctor_scope);
                if(!block){
                    return nullptr;
                }
                ctorNode->blockStmt = block;
                gotoNextTok();
            }
            // else {
            //     node = nullptr;
            // }
            else if(currentTok.content == KW_CLASS){
                auto n = new ASTClassDecl();
                node = n;
                node->type = CLASS_DECL;
                node->scope = parentScope;
                Tok tok0 = nextTok();
                
                if(!(n->id = buildIdentifier(tok0,false))){
                    return nullptr;
                }
//
                ASTTypeContext ctxt;
                ctxt.isPlaceholder = false;
                n->classType = buildTypeFromTokenStream(tok0,n,ctxt);
                
                
                tok0 = nextTok();
                if(tok0.type != Tok::OpenBrace){
                    return nullptr;
                }

                std::shared_ptr<ASTScope> s (new ASTScope {n->id->val,ASTScope::Class,parentScope});
                
                s->generateHashID();

                tok0 = nextTok();
                while(tok0.type != Tok::CloseBrace){
                    auto decl = evalDecl(tok0,s);
                    if(!decl){
                        return nullptr;
                    }
                    if(decl->type == VAR_DECL){
                        n->fields.push_back((ASTVarDecl *)decl);
                    }
                    else if(decl->type == FUNC_DECL){
                        n->methods.push_back((ASTFuncDecl *)decl);
                    }
                    else if(decl->type == CLASS_CTOR_DECL){
                        n->constructors.push_back((ASTConstructorDecl *)decl);
                    }
                    else {
                        std::cout << std::hex << decl->type << " type not allowed in class scope. Only "
                                  << VAR_DECL << ", " << FUNC_DECL << ", and " << CLASS_CTOR_DECL << " allowed." << std::endl;
                        return nullptr;
                    }
                    tok0 = token_stream[privTokIndex];
                }
                gotoNextTok();
            }
            else if(currentTok.content == KW_IMUT){
                /// Throw Unknown Error;
                return nullptr;
            }
            else if(currentTok.content == KW_ELSE){
                return nullptr;
            }
            else if(currentTok.content == KW_CATCH){
                return nullptr;
            };

            node->attributes = std::move(parsedAttributes);
            if(node && node->codeRegion.startLine == 0){
                node->codeRegion.startLine = node->codeRegion.endLine = first_token.srcPos.line;
                node->codeRegion.startCol = first_token.srcPos.startCol;
                node->codeRegion.endCol = first_token.srcPos.endCol;
            }
            return node;
        }
        else {
            
            return nullptr;
        };
    }


}
