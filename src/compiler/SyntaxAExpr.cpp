#include "starbytes/compiler/ASTNodes.def"
#include "starbytes/compiler/SyntaxA.h"
#include "starbytes/compiler/AST.h"
#include <iostream>
#include <string>
#include <cstdlib>

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

    static ASTExpr *makeUnaryExpr(const Tok &tok,ASTExpr *operand){
        auto *node = new ASTExpr();
        node->type = UNARY_EXPR;
        node->oprtr_str = tok.content;
        node->leftExpr = operand;
        node->codeRegion = regionFromToken(tok);
        return node;
    }

    static ASTExpr *makeBinaryExpr(const Tok &tok,ASTExpr *lhs,ASTExpr *rhs){
        auto *node = new ASTExpr();
        node->type = BINARY_EXPR;
        node->oprtr_str = tok.content;
        node->leftExpr = lhs;
        node->rightExpr = rhs;
        node->codeRegion = lhs? lhs->codeRegion : regionFromToken(tok);
        return node;
    }

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
                if(tokRef.type != Tok::CloseBracket){
                    auto firstExpr = evalExpr(tokRef,parentScope);
                    if(!firstExpr){
                        return nullptr;
                    }
                    node->exprArrayData.push_back(firstExpr);
                    tokRef = token_stream[privTokIndex];
                    while(tokRef.type == Tok::Comma){
                        tokRef = nextTok();
                        auto elementExpr = evalExpr(tokRef,parentScope);
                        if(!elementExpr){
                            return nullptr;
                        }
                        node->exprArrayData.push_back(elementExpr);
                        tokRef = token_stream[privTokIndex];
                    }
                    if(tokRef.type != Tok::CloseBracket){
                        return nullptr;
                    }
                }
                tokRef = nextTok();
            }
            else if(tokRef.type == Tok::OpenBrace){
                auto *node = new ASTExpr();
                expr = node;
                node->type = DICT_EXPR;
                node->codeRegion = regionFromToken(tokRef);
                tokRef = nextTok();

                if(tokRef.type != Tok::CloseBrace){
                    while(true){
                        auto *keyExpr = evalExpr(tokRef,parentScope);
                        if(!keyExpr){
                            return nullptr;
                        }
                        tokRef = token_stream[privTokIndex];
                        if(tokRef.type != Tok::Colon){
                            return nullptr;
                        }
                        tokRef = nextTok();
                        auto *valueExpr = evalExpr(tokRef,parentScope);
                        if(!valueExpr){
                            return nullptr;
                        }
                        node->dictExpr.insert(std::make_pair(keyExpr,valueExpr));
                        tokRef = token_stream[privTokIndex];
                        if(tokRef.type == Tok::Comma){
                            tokRef = nextTok();
                            continue;
                        }
                        if(tokRef.type == Tok::CloseBrace){
                            break;
                        }
                        return nullptr;
                    }
                }
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
                std::vector<ASTType *> explicitTypeArgs;
                if(tokRef.type == Tok::LessThan){
                    tokRef = nextTok();
                    ASTTypeContext typeCtxt;
                    typeCtxt.isPlaceholder = true;
                    while(true){
                        auto *typeArg = buildTypeFromTokenStream(tokRef,classExpr,typeCtxt);
                        if(!typeArg){
                            return nullptr;
                        }
                        explicitTypeArgs.push_back(typeArg);
                        tokRef = aheadTok();
                        if(tokRef.type == Tok::Comma){
                            gotoNextTok();
                            tokRef = nextTok();
                            continue;
                        }
                        if(tokRef.type == Tok::GreaterThan){
                            gotoNextTok();
                            break;
                        }
                        return nullptr;
                    }
                    tokRef = nextTok();
                }
                if(tokRef.type != Tok::OpenParen){
                    return nullptr;
                }

                ASTExpr *ctorInvoke = new ASTExpr();
                ctorInvoke->type = IVKE_EXPR;
                ctorInvoke->isConstructorCall = true;
                ctorInvoke->callee = classExpr;
                ctorInvoke->genericTypeArgs = std::move(explicitTypeArgs);
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
            /// IndexExpr
            if(tokRef.type == Tok::OpenBracket){
                auto *node = new ASTExpr();
                node->type = INDEX_EXPR;
                node->leftExpr = expr;
                node->codeRegion = expr->codeRegion;
                tokRef = nextTok();
                auto *indexExpr = evalExpr(tokRef,parentScope);
                if(!indexExpr){
                    return nullptr;
                }
                node->rightExpr = indexExpr;
                tokRef = token_stream[privTokIndex];
                if(tokRef.type != Tok::CloseBracket){
                    return nullptr;
                }
                tokRef = nextTok();
                expr = node;
                continue;
            }
            break;
        }
        return expr;
    };

    ASTExpr *SyntaxA::evalExpr(TokRef first_token,std::shared_ptr<ASTScope> parentScope){
        auto tokAt = [&](size_t index) -> Tok {
            if(index >= token_stream.size()){
                return token_stream[token_stream.size() - 1];
            }
            return token_stream[index];
        };

        auto parseUnary = [&](auto &&self,Tok tok) -> ASTExpr * {
            bool isAwaitKeyword = tok.type == Tok::Keyword && tok.content == KW_AWAIT;
            if(tok.type == Tok::Plus || tok.type == Tok::Minus || tok.type == Tok::Exclamation
               || tok.type == Tok::BitwiseNOT || isAwaitKeyword){
                Tok opTok = tok;
                tok = nextTok();
                auto *operand = self(self,tok);
                if(!operand){
                    return nullptr;
                }
                return makeUnaryExpr(opTok,operand);
            }
            return evalArgExpr(tok,parentScope);
        };

        auto parseMultiplicative = [&](auto &&self,Tok tok) -> ASTExpr * {
            auto *expr = parseUnary(parseUnary,tok);
            if(!expr){
                return nullptr;
            }
            tok = token_stream[privTokIndex];
            while(tok.type == Tok::Asterisk || tok.type == Tok::FSlash || tok.type == Tok::Percent){
                Tok opTok = tok;
                tok = nextTok();
                auto *rhs = parseUnary(parseUnary,tok);
                if(!rhs){
                    return nullptr;
                }
                expr = makeBinaryExpr(opTok,expr,rhs);
                tok = token_stream[privTokIndex];
            }
            return expr;
        };

        auto parseAdditive = [&](auto &&self,Tok tok) -> ASTExpr * {
            auto *expr = parseMultiplicative(parseMultiplicative,tok);
            if(!expr){
                return nullptr;
            }
            tok = token_stream[privTokIndex];
            while(tok.type == Tok::Plus || tok.type == Tok::Minus){
                Tok opTok = tok;
                tok = nextTok();
                auto *rhs = parseMultiplicative(parseMultiplicative,tok);
                if(!rhs){
                    return nullptr;
                }
                expr = makeBinaryExpr(opTok,expr,rhs);
                tok = token_stream[privTokIndex];
            }
            return expr;
        };

        auto parseShift = [&](auto &&self,Tok tok) -> ASTExpr * {
            auto *expr = parseAdditive(parseAdditive,tok);
            if(!expr){
                return nullptr;
            }
            tok = token_stream[privTokIndex];
            while(true){
                bool isShiftLeft = tok.type == Tok::LessThan && tokAt(privTokIndex + 1).type == Tok::LessThan;
                bool isShiftRight = tok.type == Tok::GreaterThan && tokAt(privTokIndex + 1).type == Tok::GreaterThan;
                bool isShiftAssign = (isShiftLeft || isShiftRight) && tokAt(privTokIndex + 2).type == Tok::Equal;
                if(!isShiftLeft && !isShiftRight){
                    break;
                }
                if(isShiftAssign){
                    break;
                }
                Tok opTok = tok;
                opTok.content = isShiftLeft? "<<" : ">>";
                tok = nextTok();
                tok = nextTok();
                auto *rhs = parseAdditive(parseAdditive,tok);
                if(!rhs){
                    return nullptr;
                }
                expr = makeBinaryExpr(opTok,expr,rhs);
                tok = token_stream[privTokIndex];
            }
            return expr;
        };

        auto parseRelational = [&](auto &&self,Tok tok) -> ASTExpr * {
            auto *expr = parseShift(parseShift,tok);
            if(!expr){
                return nullptr;
            }
            tok = token_stream[privTokIndex];
            while(true){
                bool lessRel = tok.type == Tok::LessThan && tokAt(privTokIndex + 1).type != Tok::LessThan;
                bool greaterRel = tok.type == Tok::GreaterThan && tokAt(privTokIndex + 1).type != Tok::GreaterThan;
                bool isRelOp = lessRel || tok.type == Tok::LessEqual
                    || greaterRel || tok.type == Tok::GreaterEqual
                    || (tok.type == Tok::Keyword && tok.content == KW_IS);
                if(!isRelOp){
                    break;
                }
                Tok opTok = tok;
                tok = nextTok();
                auto *rhs = parseShift(parseShift,tok);
                if(!rhs){
                    return nullptr;
                }
                expr = makeBinaryExpr(opTok,expr,rhs);
                tok = token_stream[privTokIndex];
            }
            return expr;
        };

        auto parseEquality = [&](auto &&self,Tok tok) -> ASTExpr * {
            auto *expr = parseRelational(parseRelational,tok);
            if(!expr){
                return nullptr;
            }
            tok = token_stream[privTokIndex];
            while(tok.type == Tok::EqualEqual || tok.type == Tok::NotEqual){
                Tok opTok = tok;
                tok = nextTok();
                auto *rhs = parseRelational(parseRelational,tok);
                if(!rhs){
                    return nullptr;
                }
                expr = makeBinaryExpr(opTok,expr,rhs);
                tok = token_stream[privTokIndex];
            }
            return expr;
        };

        auto parseBitwiseAnd = [&](auto &&self,Tok tok) -> ASTExpr * {
            auto *expr = parseEquality(parseEquality,tok);
            if(!expr){
                return nullptr;
            }
            tok = token_stream[privTokIndex];
            while(tok.type == Tok::BitwiseAND && tokAt(privTokIndex + 1).type != Tok::Equal){
                Tok opTok = tok;
                tok = nextTok();
                auto *rhs = parseEquality(parseEquality,tok);
                if(!rhs){
                    return nullptr;
                }
                expr = makeBinaryExpr(opTok,expr,rhs);
                tok = token_stream[privTokIndex];
            }
            return expr;
        };

        auto parseBitwiseXor = [&](auto &&self,Tok tok) -> ASTExpr * {
            auto *expr = parseBitwiseAnd(parseBitwiseAnd,tok);
            if(!expr){
                return nullptr;
            }
            tok = token_stream[privTokIndex];
            while(tok.type == Tok::BitwiseXOR && tokAt(privTokIndex + 1).type != Tok::Equal){
                Tok opTok = tok;
                tok = nextTok();
                auto *rhs = parseBitwiseAnd(parseBitwiseAnd,tok);
                if(!rhs){
                    return nullptr;
                }
                expr = makeBinaryExpr(opTok,expr,rhs);
                tok = token_stream[privTokIndex];
            }
            return expr;
        };

        auto parseBitwiseOr = [&](auto &&self,Tok tok) -> ASTExpr * {
            auto *expr = parseBitwiseXor(parseBitwiseXor,tok);
            if(!expr){
                return nullptr;
            }
            tok = token_stream[privTokIndex];
            while(tok.type == Tok::BitwiseOR && tokAt(privTokIndex + 1).type != Tok::Equal){
                Tok opTok = tok;
                tok = nextTok();
                auto *rhs = parseBitwiseXor(parseBitwiseXor,tok);
                if(!rhs){
                    return nullptr;
                }
                expr = makeBinaryExpr(opTok,expr,rhs);
                tok = token_stream[privTokIndex];
            }
            return expr;
        };

        auto parseLogicAnd = [&](auto &&self,Tok tok) -> ASTExpr * {
            auto *expr = parseBitwiseOr(parseBitwiseOr,tok);
            if(!expr){
                return nullptr;
            }
            tok = token_stream[privTokIndex];
            while(tok.type == Tok::LogicAND){
                Tok opTok = tok;
                tok = nextTok();
                auto *rhs = parseEquality(parseEquality,tok);
                if(!rhs){
                    return nullptr;
                }
                expr = makeBinaryExpr(opTok,expr,rhs);
                tok = token_stream[privTokIndex];
            }
            return expr;
        };

        auto parseLogicOr = [&](auto &&self,Tok tok) -> ASTExpr * {
            auto *expr = parseLogicAnd(parseLogicAnd,tok);
            if(!expr){
                return nullptr;
            }
            tok = token_stream[privTokIndex];
            while(tok.type == Tok::LogicOR){
                Tok opTok = tok;
                tok = nextTok();
                auto *rhs = parseLogicAnd(parseLogicAnd,tok);
                if(!rhs){
                    return nullptr;
                }
                expr = makeBinaryExpr(opTok,expr,rhs);
                tok = token_stream[privTokIndex];
            }
            return expr;
        };

        auto parseAssignment = [&](auto &&self,Tok tok) -> ASTExpr * {
            auto *lhs = parseLogicOr(parseLogicOr,tok);
            if(!lhs){
                return nullptr;
            }
            tok = token_stream[privTokIndex];
            std::string assignmentOp;
            size_t assignmentTokCount = 0;
            if(tok.type == Tok::Equal
               || tok.type == Tok::PlusEqual
               || tok.type == Tok::MinusEqual
               || tok.type == Tok::AsteriskEqual
               || tok.type == Tok::FSlashEqual
               || tok.type == Tok::PercentEqual){
                assignmentOp = tok.content;
                assignmentTokCount = 1;
            }
            else if((tok.type == Tok::BitwiseAND || tok.type == Tok::BitwiseOR || tok.type == Tok::BitwiseXOR)
                    && tokAt(privTokIndex + 1).type == Tok::Equal){
                assignmentOp = tok.content + "=";
                assignmentTokCount = 2;
            }
            else if(tok.type == Tok::LessThan && tokAt(privTokIndex + 1).type == Tok::LessThan
                    && tokAt(privTokIndex + 2).type == Tok::Equal){
                assignmentOp = "<<=";
                assignmentTokCount = 3;
            }
            else if(tok.type == Tok::LessThan && tokAt(privTokIndex + 1).type == Tok::LessEqual){
                assignmentOp = "<<=";
                assignmentTokCount = 2;
            }
            else if(tok.type == Tok::GreaterThan && tokAt(privTokIndex + 1).type == Tok::GreaterThan
                    && tokAt(privTokIndex + 2).type == Tok::Equal){
                assignmentOp = ">>=";
                assignmentTokCount = 3;
            }
            else if(tok.type == Tok::GreaterThan && tokAt(privTokIndex + 1).type == Tok::GreaterEqual){
                assignmentOp = ">>=";
                assignmentTokCount = 2;
            }

            if(assignmentTokCount == 0){
                return lhs;
            }

            Tok rhsTok = tok;
            for(size_t i = 0;i < assignmentTokCount;++i){
                rhsTok = nextTok();
            }
            auto *rhs = self(self,rhsTok);
            if(!rhs){
                return nullptr;
            }
            auto *assignNode = new ASTExpr();
            assignNode->type = ASSIGN_EXPR;
            assignNode->leftExpr = lhs;
            assignNode->rightExpr = rhs;
            assignNode->oprtr_str = assignmentOp;
            assignNode->codeRegion = lhs->codeRegion;
            return assignNode;
        };

        return parseAssignment(parseAssignment,first_token);
    }


}
