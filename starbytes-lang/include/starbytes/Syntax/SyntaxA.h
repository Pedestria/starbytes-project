#include "Lexer.h"
#include <llvm/ADT/ArrayRef.h>

#ifndef STARBYTES_SYNTAX_SYNTAXA_H
#define STARBYTES_SYNTAX_SYNTAXA_H

namespace starbytes {

    class ASTStmt;
    class ASTDecl;
    class ASTExpr;
    class ASTIdentifier;

    struct Comment;

    namespace Syntax {
        class SyntaxA {
            llvm::ArrayRef<Tok> token_stream;
            unsigned privTokIndex;
            const Tok & nextTok();
            const Tok & aheadTok();
            void gotoNextTok();
            std::vector<Comment *> commentBuffer;
            Comment * evalComment(const Tok & first_token); 
            ASTIdentifier *buildIdentifier(const Tok & first_token,bool typeScope);
            ASTExpr * evalExpr(const Tok & first_token);
            ASTDecl * evalDecl(const Tok & first_token);
            ASTStmt * previousNode = nullptr;
        public:
            void setTokenStream(llvm::ArrayRef<Tok> toks);
            size_t getTokenStreamWidth();
            ASTStmt *nextStatement();
        };
    };
};

#endif