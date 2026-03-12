#include "starbytes/linguistics/Analysis.h"

#include "starbytes/base/Diagnostic.h"

#include <sstream>
#include <utility>

namespace starbytes::linguistics {

namespace {

class CollectingASTConsumer final : public ASTStreamConsumer {
public:
    bool acceptsSymbolTableContext() override {
        return true;
    }

    bool acceptsRawStatements() override {
        return true;
    }

    void consumeSTableContext(Semantics::STableContext *table) override {
        (void)table;
    }

    void consumeRawStmt(ASTStmt *stmt,bool semanticAccepted) override {
        (void)semanticAccepted;
        statements.push_back(stmt);
    }

    void consumeStmt(ASTStmt *stmt) override {
        (void)stmt;
    }

    void consumeDecl(ASTDecl *stmt) override {
        (void)stmt;
    }

    const std::vector<ASTStmt *> &getStatements() const {
        return statements;
    }

private:
    std::vector<ASTStmt *> statements;
};

} // namespace

CompilerLintAnalysis OwnedCompilerLintAnalysis::view() const {
    CompilerLintAnalysis analysis;
    analysis.session = &session;
    analysis.statements = &statements;
    analysis.symbolTableContext = moduleContext ? &moduleContext->sTableContext : nullptr;
    analysis.syntaxDiagnostics = &syntaxDiagnostics;
    analysis.semanticDiagnostics = &semanticDiagnostics;
    analysis.syntaxHadError = syntaxHadError;
    analysis.semanticHadError = semanticHadError;
    return analysis;
}

OwnedCompilerLintAnalysis buildCompilerLintAnalysis(const LinguisticsSession &session,
                                                   const CompilerLintAnalysisRequest &request) {
    OwnedCompilerLintAnalysis analysis;
    analysis.session = session;
    analysis.moduleContext = std::make_unique<ModuleParseContext>(
        ModuleParseContext::Create(session.getSourceName()));

    CollectingASTConsumer consumer;
    std::ostringstream diagnosticsSink;
    auto diagnostics = DiagnosticHandler::createDefault(diagnosticsSink);
    diagnostics->setOutputMode(DiagnosticHandler::OutputMode::MachineJson);

    Parser parser(consumer, std::move(diagnostics));
    parser.setInfer64BitNumbers(request.infer64BitNumbers);

    std::istringstream in(session.getSourceText());
    parser.parseFromStream(in, *analysis.moduleContext);
    analysis.statements = consumer.getStatements();

    auto records = parser.getDiagnosticHandler()->collectRecords(false);
    for(const auto &record : records) {
        if(record.phase == Diagnostic::Phase::Parser) {
            if(record.isError()) {
                analysis.syntaxHadError = true;
            }
            analysis.syntaxDiagnostics.push_back(record);
        }
        else if(record.phase == Diagnostic::Phase::Semantic) {
            if(record.isError()) {
                analysis.semanticHadError = true;
            }
            analysis.semanticDiagnostics.push_back(record);
        }
    }

    return analysis;
}

} // namespace starbytes::linguistics
