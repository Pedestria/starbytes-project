#include <istream>
#include "starbytes/Syntax/Lexer.h"
#include "starbytes/Syntax/SyntaxA.h"
#include "SemanticA.h"

#ifndef STARBYTES_PARSER_PARSER_H
#define STARBYTES_PARSER_PARSER_H
namespace starbytes {
    struct ModuleParseContext {
        std::unique_ptr<Semantics::SymbolTable> table;
        Semantics::STableContext sTableContext;
    };

    class Parser {
        std::unique_ptr<DiagnosticBufferedLogger> diagnosticsEngine;
        std::unique_ptr<Syntax::Lexer> lexer;
        std::unique_ptr<Syntax::SyntaxA> syntaxA;
        std::vector<Syntax::Tok> tokenStream;
        std::unique_ptr<SemanticA> semanticA;
    public:
        void parseFromStream(std::istream & in,ModuleParseContext &moduleParseContext);
        Parser();
    };
};

#endif
