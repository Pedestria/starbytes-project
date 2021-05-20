#include <istream>
#include "starbytes/Syntax/Lexer.h"
#include "starbytes/Syntax/SyntaxA.h"
#include "SemanticA.h"
#include "starbytes/AST/AST.h"

#ifndef STARBYTES_PARSER_PARSER_H
#define STARBYTES_PARSER_PARSER_H
namespace starbytes {
    struct ModuleParseContext {
        std::string name;
        Semantics::STableContext sTableContext;
        static ModuleParseContext Create(std::string name);
    };

    class Parser {
        std::unique_ptr<DiagnosticBufferedLogger> diagnosticsEngine;
        std::unique_ptr<Syntax::Lexer> lexer;
        std::unique_ptr<Syntax::SyntaxA> syntaxA;
        std::vector<Syntax::Tok> tokenStream;
        std::unique_ptr<SemanticA> semanticA;
        ASTStreamConsumer & astConsumer;
    public:
        void parseFromStream(std::istream & in,ModuleParseContext &moduleParseContext);
        bool finish();
        Parser(ASTStreamConsumer & astConsumer);
    };
};

#endif
