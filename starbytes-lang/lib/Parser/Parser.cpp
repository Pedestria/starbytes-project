#include "starbytes/Parser/Parser.h"
#include "starbytes/AST/AST.h"
#include "starbytes/AST/ASTDumper.h"

namespace starbytes {

    ModuleParseContext ModuleParseContext::Create(std::string name){
        return {name,std::make_unique<Semantics::SymbolTable>()};
    };

    Parser::Parser():
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
        ASTStmt *stmt;
        auto consumer = ASTDumper::CreateStdoutASTDumper();
        while((stmt = syntaxA->nextStatement()) != nullptr){
            if(stmt->type & DECL){
                consumer->printDecl((ASTDecl *)stmt,0);
                // semanticA->addSTableEntryForDecl((ASTDecl *)stmt,moduleParseContext.table.get());
            }
            else {
                consumer->printStmt(stmt,0);
            };
            // semanticA->checkSymbolsForStmt(stmt,moduleParseContext.sTableContext);
        };
        tokenStream.clear();
       
    };
};
