#include "starbytes/compiler/Parser.h"

#include <utility>
#include "starbytes/compiler/AST.h"
#include "starbytes/compiler/ASTDumper.h"
#include <iostream>
#include <sstream>
#include <chrono>

namespace starbytes {

    ModuleParseContext ModuleParseContext::Create(string_ref name){
        ModuleParseContext context;
        context.name = name.str();
        context.sTableContext.main = std::make_unique<Semantics::SymbolTable>();
        return context;
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
        auto parseStart = std::chrono::steady_clock::now();
        std::ostringstream sourceBuffer;
        sourceBuffer << in.rdbuf();
        auto sourceText = sourceBuffer.str();
        if(profilingEnabled){
            profileData.fileCount += 1;
            profileData.sourceBytes += sourceText.size();
        }
        diagnosticHandler->setCodeViewSource(moduleParseContext.name,sourceText);
        std::istringstream lexerInput(sourceText);
        auto lexStart = std::chrono::steady_clock::now();
        lexer->tokenizeFromIStream(lexerInput,tokenStream);
        if(profilingEnabled){
            auto lexEnd = std::chrono::steady_clock::now();
            profileData.lexNs += std::chrono::duration_cast<std::chrono::nanoseconds>(lexEnd - lexStart).count();
            profileData.tokenCount += tokenStream.size();
        }
        syntaxA->setTokenStream(tokenStream);
       if(astConsumer.acceptsSymbolTableContext()){
           astConsumer.consumeSTableContext(&moduleParseContext.sTableContext);
       };
        while(true){
            auto syntaxStart = std::chrono::steady_clock::now();
            ASTStmt *stmt = syntaxA->nextStatement();
            if(profilingEnabled){
                auto syntaxEnd = std::chrono::steady_clock::now();
                profileData.syntaxNs += std::chrono::duration_cast<std::chrono::nanoseconds>(syntaxEnd - syntaxStart).count();
            }
            if(!stmt){
                if(syntaxA->isAtEnd()){
                    break;
                }
                auto tok = syntaxA->currentTok();
                Region region;
                region.startLine = region.endLine = tok.srcPos.line;
                region.startCol = tok.srcPos.startCol;
                region.endCol = tok.srcPos.endCol;
                diagnosticHandler->push(StandardDiagnostic::createError(fmtString("Unexpected token `@{0}`.",tok.content),region));
                syntaxA->consumeCurrentTok();
                continue;
            }
            if(profilingEnabled){
                profileData.statementCount += 1;
            }
            auto semanticStart = std::chrono::steady_clock::now();
            auto check  = semanticA->checkSymbolsForStmt(stmt,moduleParseContext.sTableContext);
            if(profilingEnabled){
                auto semanticEnd = std::chrono::steady_clock::now();
                profileData.semanticNs += std::chrono::duration_cast<std::chrono::nanoseconds>(semanticEnd - semanticStart).count();
            }
            if(check){
                if(stmt->type & DECL){
                    auto semanticAddStart = std::chrono::steady_clock::now();
                    semanticA->addSTableEntryForDecl((ASTDecl *)stmt,moduleParseContext.sTableContext.main.get());
                    if(profilingEnabled){
                        auto semanticAddEnd = std::chrono::steady_clock::now();
                        profileData.semanticNs += std::chrono::duration_cast<std::chrono::nanoseconds>(semanticAddEnd - semanticAddStart).count();
                    }
                    auto consumerStart = std::chrono::steady_clock::now();
                    astConsumer.consumeDecl((ASTDecl *)stmt);
                    if(profilingEnabled){
                        auto consumerEnd = std::chrono::steady_clock::now();
                        profileData.consumerNs += std::chrono::duration_cast<std::chrono::nanoseconds>(consumerEnd - consumerStart).count();
                    }
                     
                }
                else {
                    auto consumerStart = std::chrono::steady_clock::now();
                    astConsumer.consumeStmt(stmt);
                    if(profilingEnabled){
                        auto consumerEnd = std::chrono::steady_clock::now();
                        profileData.consumerNs += std::chrono::duration_cast<std::chrono::nanoseconds>(consumerEnd - consumerStart).count();
                    }
                };
            }
            else {
//                break;
            }
        }
        if(profilingEnabled){
            auto parseEnd = std::chrono::steady_clock::now();
            profileData.totalNs += std::chrono::duration_cast<std::chrono::nanoseconds>(parseEnd - parseStart).count();
        }
        tokenStream.clear();
       
    }

    bool Parser::finish(){
        auto rc = !diagnosticHandler->hasErrored();
        if(!diagnosticHandler->empty()){
            diagnosticHandler->logAll();
        };
        return rc;
    }

    void Parser::setProfilingEnabled(bool enabled){
        profilingEnabled = enabled;
    }

    const Parser::ProfileData & Parser::getProfileData() const{
        return profileData;
    }

    void Parser::resetProfileData(){
        profileData = {};
    }
}
