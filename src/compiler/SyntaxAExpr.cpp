#include "starbytes/compiler/ASTNodes.def"
#include "starbytes/compiler/SyntaxA.h"
#include "starbytes/compiler/AST.h"
#include <iostream>
#include <string>

namespace starbytes::Syntax {


    /**
     Its important to note that there are three kinds of AST expression nodes that get evaluated.
     1. DataExpr - any identifier, or literal that represents data.
     2. ArgExpr - any invocation, member access, or index of another expr.
     3. CompoundExpr - an operation on one or more exprs (binary, unary, logical,etc.)
     */

    ASTExpr *SyntaxA::evalDataExpr(TokRef first_token,std::shared_ptr<ASTScope> parentScope){
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
                literal_expr->boolValue = (tokRef.content == TOK_TRUE? true : tokRef.content == TOK_FALSE? false : false);
                
                expr = literal_expr;
                tokRef = nextTok();
            }
            else if(tokRef.type == Tok::NumericLiteral || tokRef.type == Tok::FloatingNumericLiteral){
                literal_expr = new ASTLiteralExpr();

                literal_expr->type = NUM_LITERAL;
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
                node->type = ID_EXPR;
                expr = node;
                
                ASTIdentifier *id;
                if(!(id = buildIdentifier(tokRef,false))){
                    /// ERROR
                };
                node->id = id;
                tokRef = nextTok();
                
            }
            
        };
        return expr;
    }
    
    ASTExpr * SyntaxA::evalArgExpr(const Tok & first_token,std::shared_ptr<ASTScope> parentScope){
        auto expr = evalDataExpr(first_token,parentScope);
        Tok & tokRef = const_cast<Tok &>(token_stream[privTokIndex]);
        /// MemberExpr
        if(tokRef.type == Tok::Dot){
            ASTExpr *memberExpr = new ASTExpr();
            memberExpr->leftExpr = expr;
            tokRef = nextTok();
            auto other_expr = evalDataExpr(tokRef,parentScope);
            if(!other_expr)
                return nullptr;
            if(other_expr->type == ID_EXPR)
                memberExpr->rightExpr = other_expr;
            else;
                /// Throw Error;
            expr = memberExpr;
        }
        /// InvokeExpr
        else if(tokRef.type == Tok::OpenParen){
            auto node = new ASTExpr();
            node->type = IVKE_EXPR;
            node->callee = expr;
            
            tokRef = nextTok();
            
            while(tokRef.type != Tok::CloseParen){
                ASTExpr *expr = evalExpr(tokRef,parentScope);
                node->exprArrayData.push_back(expr);
            };
            
            tokRef = nextTok();
            expr = node;
        }
        return expr;
    };

    ASTExpr *SyntaxA::evalExpr(TokRef first_token,std::shared_ptr<ASTScope> parentScope){
        
        ASTExpr *expr = evalArgExpr(first_token,parentScope);
        Tok & tokRef = const_cast<Tok &>(token_stream[privTokIndex]);
        
        /// Eval CompoundExpr

        while(true){
            /// BinaryExpr
            if(tokRef.type == Tok::Plus){
                auto node = new ASTExpr();
                node->type = BINARY_EXPR;
                node->leftExpr = expr;
                tokRef = nextTok();
                
                ASTExpr *rexpr = evalArgExpr(tokRef,parentScope);
                node->rightExpr = rexpr;
                
                expr = node;
            }
            else if(tokRef.type == Tok::Minus){
                auto node = new ASTExpr();
                node->type = BINARY_EXPR;
                node->leftExpr = expr;
                tokRef = nextTok();
                
                ASTExpr *rexpr = evalArgExpr(tokRef,parentScope);
                node->rightExpr = rexpr;
              
                 
                expr = node;
            }
            else {
                break;
            }
        };


        return expr;
    }


}
