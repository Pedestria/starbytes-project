#include "starbytes/compiler/ASTNodes.def"
#include "starbytes/compiler/SyntaxA.h"
#include "starbytes/compiler/AST.h"
#include <iostream>
#include <string>

namespace starbytes::Syntax {

    static Region regionFromToken(const Tok &tok){
        Region region;
        region.startLine = region.endLine = tok.srcPos.line;
        region.startCol = tok.srcPos.startCol;
        region.endCol = tok.srcPos.endCol;
        return region;
    }

    static bool splitRegexLiteral(const std::string &literal,std::string &pattern,std::string &flags){
        if(literal.size() < 2 || literal.front() != '/'){
            return false;
        }
        bool escaped = false;
        size_t closeIndex = std::string::npos;
        for(size_t i = 1;i < literal.size();++i){
            char c = literal[i];
            if(c == '/' && !escaped){
                closeIndex = i;
                break;
            }
            if(c == '\\' && !escaped){
                escaped = true;
            }
            else {
                escaped = false;
            }
        }
        if(closeIndex == std::string::npos){
            return false;
        }
        pattern = literal.substr(1,closeIndex - 1);
        flags = literal.substr(closeIndex + 1);
        return true;
    }

    /**
     Its important to note that there are three kinds of AST expression nodes that get evaluated.
     1. DataExpr - any identifier, or literal that represents data.
     2. ArgExpr - any invocation, member access, or index of another expr.
     3. CompoundExpr - an operation on one or more exprs (binary, unary, logical,etc.)
     */

    ASTExpr *SyntaxA::evalDataExpr(TokRef first_token,std::shared_ptr<ASTScope> parentScope){
        Tok tokRef = first_token;
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
                literal_expr->codeRegion = regionFromToken(tokRef);
                
                literal_expr->strValue = tokRef.content.substr(1,tokRef.content.size()-2);
                expr = literal_expr;
                tokRef = nextTok();
            }
            else if(tokRef.type == Tok::RegexLiteral){
                std::string pattern;
                std::string flags;
                if(!splitRegexLiteral(tokRef.content,pattern,flags)){
                    return nullptr;
                }
                literal_expr = new ASTLiteralExpr();
                literal_expr->type = REGEX_LITERAL;
                literal_expr->codeRegion = regionFromToken(tokRef);
                literal_expr->regexPattern = std::move(pattern);
                literal_expr->regexFlags = std::move(flags);
                expr = literal_expr;
                tokRef = nextTok();
            }
            else if(tokRef.type == Tok::BooleanLiteral){
                literal_expr = new ASTLiteralExpr();
                
                literal_expr->type = BOOL_LITERAL;
                literal_expr->codeRegion = regionFromToken(tokRef);
                literal_expr->boolValue = (tokRef.content == TOK_TRUE? true : tokRef.content == TOK_FALSE? false : false);
                
                expr = literal_expr;
                tokRef = nextTok();
            }
            else if(tokRef.type == Tok::NumericLiteral || tokRef.type == Tok::FloatingNumericLiteral){
                literal_expr = new ASTLiteralExpr();

                literal_expr->type = NUM_LITERAL;
                literal_expr->codeRegion = regionFromToken(tokRef);
                if(tokRef.type == Tok::FloatingNumericLiteral){
                   starbytes_float_t val = ::atof(tokRef.content.c_str());
                   literal_expr->floatValue = val;
                }
                else {
                    starbytes_int_t val = std::stoi(tokRef.content);
                    literal_expr->intValue = val;
                };
                expr = literal_expr;
                tokRef = nextTok();
            }
            else if(tokRef.type == Tok::OpenBracket){
                ASTExpr *node = new ASTExpr();
                expr = node;
                node->type = ARRAY_EXPR;
                node->codeRegion = regionFromToken(tokRef);
                tokRef = nextTok();
                auto firstExpr = evalExpr(tokRef,parentScope);
                if(!firstExpr){
                    /// ERROR
                };

                node->exprArrayData.push_back(firstExpr);
                tokRef = token_stream[privTokIndex];
                /// Last Tok from recent ast expr evaluation.
                while(tokRef.type == Tok::Comma){
                    tokRef = nextTok();
                    auto expr = evalExpr(tokRef,parentScope);
                    if(!expr){
                        /// ERROR
                    };
                    node->exprArrayData.push_back(expr);
                    tokRef = token_stream[privTokIndex];
                };

                if(tokRef.type != Tok::CloseBracket){
                    /// ERROR
                };
                tokRef = nextTok();
            }
            else if(tokRef.type == Tok::Identifier){
                ASTExpr *node = new ASTExpr();
                node->type = ID_EXPR;
                expr = node;
                
                ASTIdentifier *id;
                if(!(id = buildIdentifier(tokRef,false))){
                    /// ERROR
                };
                node->id = id;
                node->codeRegion = id->codeRegion;
                tokRef = nextTok();
                
            }
            else if(tokRef.type == Tok::Keyword && tokRef.content == KW_NEW){
                tokRef = nextTok();
                if(tokRef.type != Tok::Identifier){
                    return nullptr;
                }
                ASTExpr *classExpr = new ASTExpr();
                classExpr->type = ID_EXPR;
                classExpr->id = buildIdentifier(tokRef,false);
                if(!classExpr->id){
                    return nullptr;
                }
                classExpr->codeRegion = classExpr->id->codeRegion;
                tokRef = nextTok();
                while(tokRef.type == Tok::Dot){
                    tokRef = nextTok();
                    if(tokRef.type != Tok::Identifier){
                        return nullptr;
                    }
                    ASTExpr *memberExpr = new ASTExpr();
                    memberExpr->type = MEMBER_EXPR;
                    memberExpr->leftExpr = classExpr;
                    auto *rightExpr = new ASTExpr();
                    rightExpr->type = ID_EXPR;
                    rightExpr->id = buildIdentifier(tokRef,false);
                    if(!rightExpr->id){
                        return nullptr;
                    }
                    memberExpr->rightExpr = rightExpr;
                    memberExpr->codeRegion = rightExpr->id->codeRegion;
                    classExpr = memberExpr;
                    tokRef = nextTok();
                }
                if(tokRef.type != Tok::OpenParen){
                    return nullptr;
                }

                ASTExpr *ctorInvoke = new ASTExpr();
                ctorInvoke->type = IVKE_EXPR;
                ctorInvoke->isConstructorCall = true;
                ctorInvoke->callee = classExpr;
                ctorInvoke->codeRegion = classExpr->codeRegion;

                tokRef = nextTok();
                while(tokRef.type != Tok::CloseParen){
                    ASTExpr *argExpr = evalExpr(tokRef,parentScope);
                    if(!argExpr){
                        return nullptr;
                    }
                    ctorInvoke->exprArrayData.push_back(argExpr);
                    tokRef = token_stream[privTokIndex];
                    if(tokRef.type == Tok::Comma){
                        tokRef = nextTok();
                    }
                }
                tokRef = nextTok();
                expr = ctorInvoke;
            }
            
        };
        return expr;
    }
    
    ASTExpr * SyntaxA::evalArgExpr(const Tok & first_token,std::shared_ptr<ASTScope> parentScope){
        auto expr = evalDataExpr(first_token,parentScope);
        if(!expr){
            return nullptr;
        }
        Tok tokRef = token_stream[privTokIndex];
        while(true){
            /// MemberExpr
            if(tokRef.type == Tok::Dot){
                ASTExpr *memberExpr = new ASTExpr();
                memberExpr->type = MEMBER_EXPR;
                memberExpr->leftExpr = expr;
                tokRef = nextTok();
                if(tokRef.type != Tok::Identifier){
                    return nullptr;
                }
                ASTExpr *right = new ASTExpr();
                right->type = ID_EXPR;
                right->id = buildIdentifier(tokRef,false);
                if(!right->id){
                    return nullptr;
                }
                memberExpr->rightExpr = right;
                memberExpr->codeRegion = right->id->codeRegion;
                expr = memberExpr;
                tokRef = nextTok();
                continue;
            }
            /// InvokeExpr
            if(tokRef.type == Tok::OpenParen){
                auto node = new ASTExpr();
                node->type = IVKE_EXPR;
                node->callee = expr;
                node->codeRegion = expr->codeRegion;
                
                tokRef = nextTok();
                
                while(tokRef.type != Tok::CloseParen){
                    ASTExpr *argExpr = evalExpr(tokRef,parentScope);
                    if(!argExpr){
                        return nullptr;
                    }
                    node->exprArrayData.push_back(argExpr);
                    tokRef = token_stream[privTokIndex];
                    if(tokRef.type == Tok::Comma){
                        tokRef = nextTok();
                    }
                };
                
                tokRef = nextTok();
                expr = node;
                continue;
            }
            break;
        }
        return expr;
    };

    ASTExpr *SyntaxA::evalExpr(TokRef first_token,std::shared_ptr<ASTScope> parentScope){
        
        ASTExpr *expr = evalArgExpr(first_token,parentScope);
        if(!expr){
            return nullptr;
        }
        Tok tokRef = token_stream[privTokIndex];

        if(tokRef.type == Tok::Equal){
            auto node = new ASTExpr();
            node->type = ASSIGN_EXPR;
            node->leftExpr = expr;
            node->codeRegion = expr->codeRegion;
            tokRef = nextTok();
            auto *rhsExpr = evalExpr(tokRef,parentScope);
            if(!rhsExpr){
                return nullptr;
            }
            node->rightExpr = rhsExpr;
            expr = node;
            return expr;
        }
        
        /// Eval CompoundExpr

        while(true){
            /// BinaryExpr
            if(tokRef.type == Tok::Plus){
                auto node = new ASTExpr();
                node->type = BINARY_EXPR;
                node->leftExpr = expr;
                node->codeRegion = expr->codeRegion;
                tokRef = nextTok();
                
                ASTExpr *rexpr = evalArgExpr(tokRef,parentScope);
                node->rightExpr = rexpr;
                
                expr = node;
                tokRef = token_stream[privTokIndex];
            }
            else if(tokRef.type == Tok::Minus){
                auto node = new ASTExpr();
                node->type = BINARY_EXPR;
                node->leftExpr = expr;
                node->codeRegion = expr->codeRegion;
                tokRef = nextTok();
                
                ASTExpr *rexpr = evalArgExpr(tokRef,parentScope);
                node->rightExpr = rexpr;
              
                 
                expr = node;
                tokRef = token_stream[privTokIndex];
            }
            else {
                break;
            }
        };


        return expr;
    }


}
