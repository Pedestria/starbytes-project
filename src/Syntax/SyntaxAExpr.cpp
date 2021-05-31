#include "starbytes/Syntax/SyntaxA.h"
#include "starbytes/AST/AST.h"
#include <iostream>

namespace starbytes::Syntax {



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
                    return node;
                };
                
                node->type = IVKE_EXPR;
                // std::cout << "InvkExpr" << std::endl;
                
                tokRef = nextTok();
                
                while(tokRef.type != Tok::CloseParen){
                    ASTExpr *expr = evalExpr(tokRef,parentScope);
                    node->exprArrayData.push_back(expr);
                };
                
                tokRef = nextTok();
                return node;
            }
            
        };

        return expr;
    }


}
