#include "starbytes/Parser/Parser.h"
#include "starbytes/AST/AST.h"
#include "starbytes/AST/ASTDumper.h"

namespace starbytes {

    ModuleParseContext ModuleParseContext::Create(std::string name){
        return {name,std::make_unique<Semantics::SymbolTable>()};
    };

    Parser::Parser(ASTStreamConsumer & astConsumer):
    astConsumer(astConsumer),
    diagnosticsEngine(std::make_unique<DiagnosticBufferedLogger>()),
    lexer(std::make_unique<Syntax::Lexer>(*diagnosticsEngine)),
    syntaxA(std::make_unique<Syntax::SyntaxA>()),
    semanticA(std::make_unique<SemanticA>(*syntaxA,*diagnosticsEngine))
    {
        semanticA->start();
    };

    void Parser::parseFromStream(std::istream &in,ModuleParseContext &moduleParseContext){
        lexer->tokenizeFromIStream(in,tokenStream);
        syntaxA->setTokenStream(tokenStream);
        if(astConsumer.acceptsSymbolTableContext()){
            astConsumer.consumeSTableContext(&moduleParseContext.sTableContext);
        };
        ASTStmt *stmt;
        while((stmt = syntaxA->nextStatement()) != nullptr){
            if(stmt->type & DECL){
                astConsumer.consumeDecl((ASTDecl *)stmt);
                // semanticA->addSTableEntryForDecl((ASTDecl *)stmt,moduleParseContext.table.get());
            }
            else if(stmt->type & EXPR) {
                astConsumer.consumeStmt(stmt);
            };
            // semanticA->checkSymbolsForStmt(stmt,moduleParseContext.sTableContext);
        };
        tokenStream.clear();
       
    };
};
