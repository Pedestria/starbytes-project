#include "starbytes/compiler/Parser.h"

#include <utility>
#include "starbytes/compiler/AST.h"
#include "starbytes/compiler/ASTDumper.h"
#include <iostream>
#include <sstream>
#include <chrono>

namespace starbytes {

namespace {

void annotateStmtParentFile(ASTStmt *stmt,const std::string &parentFile);

void annotateExprParentFile(ASTExpr *expr,const std::string &parentFile){
    if(!expr){
        return;
    }
    expr->parentFile = parentFile;
    annotateExprParentFile(expr->callee,parentFile);
    annotateExprParentFile(expr->leftExpr,parentFile);
    annotateExprParentFile(expr->middleExpr,parentFile);
    annotateExprParentFile(expr->rightExpr,parentFile);
    for(auto *argExpr : expr->exprArrayData){
        annotateExprParentFile(argExpr,parentFile);
    }
    for(auto &kv : expr->dictExpr){
        annotateExprParentFile(kv.first,parentFile);
        annotateExprParentFile(kv.second,parentFile);
    }
    if(expr->id){
        expr->id->parentFile = parentFile;
    }
    if(expr->inlineFuncBlock){
        for(auto *bodyStmt : expr->inlineFuncBlock->body){
            annotateStmtParentFile(bodyStmt,parentFile);
        }
    }
}

void annotateBlockParentFile(ASTBlockStmt *block,const std::string &parentFile){
    if(!block){
        return;
    }
    for(auto *bodyStmt : block->body){
        annotateStmtParentFile(bodyStmt,parentFile);
    }
}

void annotateDeclParentFile(ASTDecl *decl,const std::string &parentFile){
    if(!decl){
        return;
    }
    decl->parentFile = parentFile;
    switch(decl->type){
        case VAR_DECL: {
            auto *varDecl = (ASTVarDecl *)decl;
            for(auto &spec : varDecl->specs){
                if(spec.id){
                    spec.id->parentFile = parentFile;
                }
                annotateExprParentFile(spec.expr,parentFile);
            }
            break;
        }
        case FUNC_DECL: {
            auto *funcDecl = (ASTFuncDecl *)decl;
            if(funcDecl->funcId){
                funcDecl->funcId->parentFile = parentFile;
            }
            for(auto &param : funcDecl->params){
                if(param.first){
                    param.first->parentFile = parentFile;
                }
            }
            annotateBlockParentFile(funcDecl->blockStmt,parentFile);
            break;
        }
        case CLASS_CTOR_DECL: {
            auto *ctorDecl = (ASTConstructorDecl *)decl;
            for(auto &param : ctorDecl->params){
                if(param.first){
                    param.first->parentFile = parentFile;
                }
            }
            annotateBlockParentFile(ctorDecl->blockStmt,parentFile);
            break;
        }
        case CLASS_DECL: {
            auto *classDecl = (ASTClassDecl *)decl;
            if(classDecl->id){
                classDecl->id->parentFile = parentFile;
            }
            for(auto *genericParam : classDecl->genericTypeParams){
                if(genericParam){
                    genericParam->parentFile = parentFile;
                }
            }
            for(auto *field : classDecl->fields){
                annotateDeclParentFile(field,parentFile);
            }
            for(auto *method : classDecl->methods){
                annotateDeclParentFile(method,parentFile);
            }
            for(auto *ctor : classDecl->constructors){
                annotateDeclParentFile(ctor,parentFile);
            }
            break;
        }
        case INTERFACE_DECL: {
            auto *interfaceDecl = (ASTInterfaceDecl *)decl;
            if(interfaceDecl->id){
                interfaceDecl->id->parentFile = parentFile;
            }
            for(auto *genericParam : interfaceDecl->genericTypeParams){
                if(genericParam){
                    genericParam->parentFile = parentFile;
                }
            }
            for(auto *field : interfaceDecl->fields){
                annotateDeclParentFile(field,parentFile);
            }
            for(auto *method : interfaceDecl->methods){
                annotateDeclParentFile(method,parentFile);
            }
            break;
        }
        case TYPE_ALIAS_DECL: {
            auto *aliasDecl = (ASTTypeAliasDecl *)decl;
            if(aliasDecl->id){
                aliasDecl->id->parentFile = parentFile;
            }
            for(auto *genericParam : aliasDecl->genericTypeParams){
                if(genericParam){
                    genericParam->parentFile = parentFile;
                }
            }
            break;
        }
        case SCOPE_DECL: {
            auto *scopeDecl = (ASTScopeDecl *)decl;
            if(scopeDecl->scopeId){
                scopeDecl->scopeId->parentFile = parentFile;
            }
            annotateBlockParentFile(scopeDecl->blockStmt,parentFile);
            break;
        }
        case RETURN_DECL: {
            auto *returnDecl = (ASTReturnDecl *)decl;
            annotateExprParentFile(returnDecl->expr,parentFile);
            break;
        }
        case COND_DECL: {
            auto *condDecl = (ASTConditionalDecl *)decl;
            for(auto &spec : condDecl->specs){
                annotateExprParentFile(spec.expr,parentFile);
                annotateBlockParentFile(spec.blockStmt,parentFile);
            }
            break;
        }
        case FOR_DECL: {
            auto *forDecl = (ASTForDecl *)decl;
            annotateExprParentFile(forDecl->expr,parentFile);
            annotateBlockParentFile(forDecl->blockStmt,parentFile);
            break;
        }
        case WHILE_DECL: {
            auto *whileDecl = (ASTWhileDecl *)decl;
            annotateExprParentFile(whileDecl->expr,parentFile);
            annotateBlockParentFile(whileDecl->blockStmt,parentFile);
            break;
        }
        case SECURE_DECL: {
            auto *secureDecl = (ASTSecureDecl *)decl;
            annotateDeclParentFile(secureDecl->guardedDecl,parentFile);
            if(secureDecl->catchErrorId){
                secureDecl->catchErrorId->parentFile = parentFile;
            }
            annotateBlockParentFile(secureDecl->catchBlock,parentFile);
            break;
        }
        case IMPORT_DECL: {
            auto *importDecl = (ASTImportDecl *)decl;
            if(importDecl->moduleName){
                importDecl->moduleName->parentFile = parentFile;
            }
            break;
        }
        default:
            break;
    }
}

void annotateStmtParentFile(ASTStmt *stmt,const std::string &parentFile){
    if(!stmt){
        return;
    }
    stmt->parentFile = parentFile;
    if(stmt->type & DECL){
        annotateDeclParentFile((ASTDecl *)stmt,parentFile);
        return;
    }
    if(stmt->type & EXPR){
        annotateExprParentFile((ASTExpr *)stmt,parentFile);
    }
}

}

    ModuleParseContext ModuleParseContext::Create(string_ref name){
        ModuleParseContext context;
        context.name = name.str();
        context.sTableContext.main = std::make_unique<Semantics::SymbolTable>();
        return context;
    }

    Parser::Parser(ASTStreamConsumer & astConsumer,std::unique_ptr<DiagnosticHandler> diagnostics):
    diagnosticHandler(diagnostics ? std::move(diagnostics) : DiagnosticHandler::createDefault(std::cout)),
    lexer(std::make_unique<Syntax::Lexer>(*diagnosticHandler)),
    syntaxA(std::make_unique<Syntax::SyntaxA>()),
    semanticA(std::make_unique<SemanticA>(*syntaxA,*diagnosticHandler)),
    astConsumer(astConsumer)
    {
        ASTScopeGlobal->generateHashID();
        diagnosticHandler->setDefaultPhase(Diagnostic::Phase::Parser);
        diagnosticHandler->setDefaultSourceName("starbytes-parser");
        semanticA->setPrefer64BitNumberInference(infer64BitNumbers);
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
                auto diag = StandardDiagnostic::createError(fmtString("Unexpected token `@{0}`.",tok.content),region);
                if(diag){
                    diag->phase = Diagnostic::Phase::Parser;
                    diag->code = "SB-PARSE-E0001";
                }
                diagnosticHandler->push(diag);
                syntaxA->consumeCurrentTok();
                continue;
            }
            if(profilingEnabled){
                profileData.statementCount += 1;
            }
            annotateStmtParentFile(stmt,moduleParseContext.name);
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

    void Parser::setInfer64BitNumbers(bool enabled){
        infer64BitNumbers = enabled;
        if(semanticA){
            semanticA->setPrefer64BitNumberInference(enabled);
        }
    }

    bool Parser::getInfer64BitNumbers() const{
        return infer64BitNumbers;
    }

    const Parser::ProfileData & Parser::getProfileData() const{
        return profileData;
    }

    void Parser::resetProfileData(){
        profileData = {};
    }

    DiagnosticHandler *Parser::getDiagnosticHandler(){
        return diagnosticHandler.get();
    }
}
