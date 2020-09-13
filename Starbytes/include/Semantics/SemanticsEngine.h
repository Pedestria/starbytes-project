# pragma once
#include "Parser/AST.h"
#include "Parser/Document.h"
#include "Parser/Parser.h"
#include <string>
#include <vector>

namespace Starbytes {

    enum class SymbolType:int {
        Class,Interface,Function,Variable,Constant,Alias,Type,Property,Unresolved
    };

    struct SemanticToken {
        std::string type;
        DocumentPosition start;
        DocumentPosition end;
    };
    struct SymbolOptions {};

    struct VariableOptions : SymbolOptions {
        std::string type_reference;
    };
    struct SymbolStoreEntry {
        std::string symbol;
        SymbolType type;
        SymbolOptions *options;
    };


    class SymbolStore {
        public:
            SymbolStore(std::string _name,Scope _scope_type): name(_name),scope_type(_scope_type){};
            Scope scope_type;
            std::string name;
            //Adds a Symbol to the store.
            void addSymbol(SymbolStoreEntry *);
            //Verifies if a symbol exists in this store. (Type Senstive!)
            bool verifySymbolTS(std::string,SymbolType);
            //Verifies if a symbol exists in this store. (Type IN-Senstive!)
            bool verifySymbolTIS(std::string);
            SymbolType getSymbolTypeFromSymbol(std::string);
            std::vector<SymbolStoreEntry *> * getSymbols();
        private:
            std::vector<SymbolStoreEntry *> symbols;

    };

    class SemanticAnalyzer {
        public:
            SemanticAnalyzer(AST::AbstractSyntaxTree *_syntaxTree): syntaxTree(_syntaxTree) {};
            void freeSymbolStores();
            void check(bool semanticTokens = false,std::vector<SemanticToken *> *semTokArray = nullptr);
        private:
            AST::AbstractSyntaxTree *syntaxTree;
            std::vector<SymbolStore *> stores;
            std::vector<std::string> currentStoreScopes;
            std::vector<SemanticToken *> *semanticTokArray;
            bool outputSemanticTok;
            std::string exactCurrentStoreScope;
            void createSymbolStore(std::string name,Scope scope_type);
            void addSymbolToStore(std::string store_name,SymbolStoreEntry* symbol);
            SymbolType getSymbolTypeFromSymbolInCurrentStores(std::string symbol);
            bool verifySymbolInCurrentStores(std::string symbol,SymbolType symbol_type,DocumentPosition *position);
            void addToCurrentScopes(std::string scope_name);
            void removeFromCurrentScopes(std::string scope_name);
            //
            //Analyzers!
            //
            void checkStatement(ASTStatement *node);
            void checkScopeDeclaration(ASTScopeDeclaration *node);
            void checkFunctionDeclaration(ASTFunctionDeclaration *node);
            void checkClassDeclaration(ASTClassDeclaration *node);
            void checkVariableDeclaration(ASTVariableDeclaration *node);
            void checkConstantDeclaration(ASTConstantDeclaration *node);
            void checkTypeArgumentsDeclaration(ASTTypeArgumentsDeclaration *node);
            void checkFuncArguments(std::vector<ASTTypeCastIdentifier *> * args);
            //
            //Type Getters!
            //
            std::string * getTypeOnExpression(ASTExpression *node);

    };
}