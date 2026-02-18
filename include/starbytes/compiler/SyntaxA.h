#include "Lexer.h"
#include <set>

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
            const std::set<std::string> *genericTypeParams = nullptr;
        };
        
        typedef const Tok & TokRef;
        class SyntaxA {
            array_ref<Tok> token_stream;
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
            ASTBlockStmt *evalBlockStmt(const Tok &first_token,std::shared_ptr<ASTScope> parentScope);
            ASTExpr * evalDataExpr(const Tok & first_token,std::shared_ptr<ASTScope> parentScope);
            ASTExpr * evalArgExpr(const Tok & first_token,std::shared_ptr<ASTScope> parentScope);
            ASTExpr * evalExpr(const Tok & first_token,std::shared_ptr<ASTScope> parentScope);
            ASTDecl * evalDecl(const Tok & first_token,std::shared_ptr<ASTScope> parentScope);
            ASTStmt * previousNode = nullptr;
            std::vector<std::set<std::string>> genericTypeParamStack;
        public:
            void setTokenStream(array_ref<Tok> toks);
            size_t getTokenStreamWidth();
            bool isAtEnd();
            const Tok &currentTok();
            void consumeCurrentTok();
            ASTStmt *nextStatement();
        };
    };
};

#endif
