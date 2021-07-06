#include "starbytes/Parser/Parser.h"

#include <utility>
#include "starbytes/AST/AST.h"
#include "starbytes/AST/ASTDumper.h"

namespace starbytes {

    ModuleParseContext ModuleParseContext::Create(std::string name){
        return {std::move(name),{std::make_unique<Semantics::SymbolTable>(),{}}};
    }

    Parser::Parser(ASTStreamConsumer & astConsumer):
    diagnosticsEngine(std::make_unique<DiagnosticBufferedLogger>()),
    lexer(std::make_unique<Syntax::Lexer>(*diagnosticsEngine)),
    syntaxA(std::make_unique<Syntax::SyntaxA>()),
    semanticA(std::make_unique<SemanticA>(*syntaxA,*diagnosticsEngine)),
    astConsumer(astConsumer)
    {
        semanticA->start();
    }

    void Parser::parseFromStream(std::istream &in,ModuleParseContext &moduleParseContext){
        CodeViewSrc codeViewDoc;
        lexer->tokenizeFromIStream(in,tokenStream,codeViewDoc);
        syntaxA->setTokenStream(tokenStream);
//        if(astConsumer.acceptsSymbolTableContext()){
//            astConsumer.consumeSTableContext(&moduleParseContext.sTableContext);
//        };
        ASTStmt *stmt;
        while((stmt = syntaxA->nextStatement()) != nullptr){
            auto check  = semanticA->checkSymbolsForStmt(stmt,moduleParseContext.sTableContext);
            if(check){
                if(stmt->type & DECL){
                    semanticA->addSTableEntryForDecl((ASTDecl *)stmt,moduleParseContext.sTableContext.main.get());
                    astConsumer.consumeDecl((ASTDecl *)stmt);
                     
                }
                else {
                    astConsumer.consumeStmt(stmt);
                };
            }
            else {
//                break;
            }
        
        };
        tokenStream.clear();
       
    }

    bool Parser::finish(){
        auto rc = !diagnosticsEngine->hasErrored();
        if(!diagnosticsEngine->empty()){
            diagnosticsEngine->logAll();
        };
        return rc;
    }
}
