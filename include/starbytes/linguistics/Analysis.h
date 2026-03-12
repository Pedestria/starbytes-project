#ifndef STARBYTES_LINGUISTICS_ANALYSIS_H
#define STARBYTES_LINGUISTICS_ANALYSIS_H

#include <memory>
#include <vector>

#include "Session.h"
#include "starbytes/base/Diagnostic.h"
#include "starbytes/compiler/AST.h"
#include "starbytes/compiler/Parser.h"

namespace starbytes::linguistics {

struct CompilerLintAnalysis {
    const LinguisticsSession *session = nullptr;
    const std::vector<ASTStmt *> *statements = nullptr;
    const Semantics::STableContext *symbolTableContext = nullptr;
    const std::vector<DiagnosticRecord> *syntaxDiagnostics = nullptr;
    const std::vector<DiagnosticRecord> *semanticDiagnostics = nullptr;
    bool syntaxHadError = false;
    bool semanticHadError = false;

    bool readyForSemanticLint() const {
        return session != nullptr && statements != nullptr && symbolTableContext != nullptr;
    }
};

struct OwnedCompilerLintAnalysis {
    LinguisticsSession session;
    std::unique_ptr<ModuleParseContext> moduleContext;
    std::vector<ASTStmt *> statements;
    std::vector<DiagnosticRecord> syntaxDiagnostics;
    std::vector<DiagnosticRecord> semanticDiagnostics;
    bool syntaxHadError = false;
    bool semanticHadError = false;

    CompilerLintAnalysis view() const;
};

struct CompilerLintAnalysisRequest {
    bool infer64BitNumbers = false;
};

OwnedCompilerLintAnalysis buildCompilerLintAnalysis(
    const LinguisticsSession &session,
    const CompilerLintAnalysisRequest &request = CompilerLintAnalysisRequest());

} // namespace starbytes::linguistics

#endif // STARBYTES_LINGUISTICS_ANALYSIS_H
