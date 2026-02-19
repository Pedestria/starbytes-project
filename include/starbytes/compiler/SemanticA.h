#include "SyntaxA.h"
#include "SymTable.h"
#include <set>

#ifndef STARBYTES_PARSER_SEMANTICA_H
#define STARBYTES_PARSER_SEMANTICA_H

namespace starbytes {

    struct ASTScope;

    struct SemanticADiagnostic final : public Diagnostic {
        typedef enum : int {
          Error,
          Warning,
          Suggestion
        } Ty;
        Ty type;
        ASTStmt *stmt;
        static DiagnosticPtr create(string_ref message,ASTStmt *stmt,Type ty);
        ~SemanticADiagnostic() override = default;
    };

    struct ASTScopeSemanticsContext {
        std::shared_ptr<ASTScope> scope;
        std::map<ASTIdentifier *,ASTType *> *args = nullptr;
        const string_set *genericTypeParams = nullptr;
    };
    /**
     * @brief The Semantics Analyzer
     * */
    class SemanticA {
        Syntax::SyntaxA & syntaxARef;
        DiagnosticHandler & errStream;
        bool typeExists(ASTType *type,
                        Semantics::STableContext & symbolTableContext,
                        std::shared_ptr<ASTScope> scope,
                        const string_set *genericTypeParams = nullptr,
                        ASTStmt *diagNode = nullptr);
        ASTType *evalExprForTypeId(ASTExpr *expr_to_eval,
                                   Semantics::STableContext & symbolTableContext,
                                   ASTScopeSemanticsContext & scopeContext);
        bool typeMatches(ASTType *type,
                         ASTExpr *expr_to_eval,
                         Semantics::STableContext & symbolTableContext,
                         ASTScopeSemanticsContext & scopeContext);
        
        ASTType * evalGenericDecl(ASTDecl *stmt,
                                  Semantics::STableContext & symbolTableContext,
                                  ASTScopeSemanticsContext & scopeContext,
                                  bool* hasErrored);
        /**
            @param args Used with BlockStmts embedded in Function Decls.
        */
        ASTType * evalBlockStmtForASTType(ASTBlockStmt *block,
                                          Semantics::STableContext & symbolTableContext,
                                          bool * hasErrored,
                                          ASTScopeSemanticsContext & scopeContext,
                                          bool inFuncContext = false);

        bool checkSymbolsForStmtInScope(ASTStmt *stmt,
                                        Semantics::STableContext & symbolTableContext,
                                        std::shared_ptr<ASTScope> scope,
                                        optional<Semantics::SymbolTable> tempSTable = {});
    public:
        static void start();
        void finish();
        void addSTableEntryForDecl(ASTDecl *decl,
                                   Semantics::SymbolTable *tablePtr);
        bool checkSymbolsForStmt(ASTStmt *stmt,
                                 Semantics::STableContext & symbolTableContext);

        SemanticA(Syntax::SyntaxA & syntaxARef,
                  DiagnosticHandler & errStream);
    };
}

#endif
