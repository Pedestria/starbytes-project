#include "starbytes/compiler/Parser.h"

#include <utility>
#include "starbytes/compiler/AST.h"
#include "starbytes/compiler/ASTDumper.h"
#include <iostream>

namespace starbytes {

    ModuleParseContext ModuleParseContext::Create(std::string name){
        return {std::move(name),{std::make_unique<Semantics::SymbolTable>(),{}}};
    }

    Parser::Parser(ASTStreamConsumer & astConsumer):
    diagnosticHandler(DiagnosticHandler::createDefault(std::cout)),
    lexer(std::make_unique<Syntax::Lexer>(*diagnosticHandler)),
    syntaxA(std::make_unique<Syntax::SyntaxA>()),
    semanticA(std::make_unique<SemanticA>(*syntaxA,*diagnosticHandler)),
    astConsumer(astConsumer)
    {
        ASTScopeGlobal->generateHashID();
        semanticA->start();
    }

    void Parser::parseFromStream(std::istream &in,ModuleParseContext &moduleParseContext){
        Region codeViewDoc;
        lexer->tokenizeFromIStream(in,tokenStream);
        syntaxA->setTokenStream(tokenStream);
        // diagnosticHandler->setCodeViewSrc(codeViewDoc);
       if(astConsumer.acceptsSymbolTableContext()){
           astConsumer.consumeSTableContext(&moduleParseContext.sTableContext);
       };
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
        auto rc = !diagnosticHandler->hasErrored();
        if(!diagnosticHandler->empty()){
            diagnosticHandler->logAll();
        };
        return rc;
    }
}
