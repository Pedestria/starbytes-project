#include "Lexer.h"
#include <llvm/ADT/ArrayRef.h>

#ifndef STARBYTES_SYNTAX_SYNTAXA_H
#define STARBYTES_SYNTAX_SYNTAXA_H

namespace starbytes {

    class ASTStmt;
    class ASTDecl;
    class ASTExpr;
    class ASTIdentifier;
    class ASTType;
    struct ASTBlockStmt;
    struct ASTScope;

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
            ASTType * buildTypeFromTokenStream(const Tok & first_token,ASTStmt *parentStmt);
            ASTBlockStmt *evalBlockStmt(const Tok &first_token,ASTScope *parentScope);
            ASTExpr * evalExpr(const Tok & first_token,ASTScope *parentScope);
            ASTDecl * evalDecl(const Tok & first_token,ASTScope *parentScope);
            ASTStmt * previousNode = nullptr;
        public:
            void setTokenStream(llvm::ArrayRef<Tok> toks);
            size_t getTokenStreamWidth();
            ASTStmt *nextStatement();
        };
    };
};

#endif
