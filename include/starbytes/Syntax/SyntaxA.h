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
    
        struct ASTTypeContext {
            bool isPlaceholder = false;
            bool isAlias = false;
        };
        
        typedef const Tok & TokRef;
        class SyntaxA {
            llvm::ArrayRef<Tok> token_stream;
            unsigned privTokIndex;
            const Tok & nextTok();
            const Tok & aheadTok();
            void gotoNextTok();
            std::vector<Comment *> commentBuffer;
            Comment * evalComment(const Tok & first_token);
            /// @brief Build an Identifier.
            /// @param first_token The First Token to Start building.
            /// @param typeScope Whether this identifier references a type or defines a type.
            ASTIdentifier *buildIdentifier(const Tok & first_token,bool typeScope);
            ASTType * buildTypeFromTokenStream(const Tok & first_token,ASTStmt *parentStmt,ASTTypeContext & ctxt);
            ASTBlockStmt *evalBlockStmt(const Tok &first_token,ASTScope *parentScope);
            ASTExpr * evalDataExpr(const Tok & first_token,ASTScope *parentScope);
            ASTExpr * evalArgExpr(const Tok & first_token,ASTScope *parentScope);
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
