#include "starbytes/base/Diagnostic.h"

#include <iostream>
#include <sstream>
#include <string>

namespace {

int fail(const std::string &message) {
    std::cerr << "DiagnosticBaselineTest failure: " << message << '\n';
    return 1;
}

}

int main() {
    std::ostringstream out;
    auto handler = starbytes::DiagnosticHandler::createDefault(out);
    if(!handler) {
        return fail("unable to create DiagnosticHandler");
    }

    handler->resetMetrics();
    handler->setDefaultPhase(starbytes::Diagnostic::Phase::Semantic);
    handler->setDefaultSourceName("DiagBaseline.starb");
    auto initial = handler->getMetrics();
    if(initial.pushedCount != 0 || initial.renderedCount != 0) {
        return fail("metrics should be zero after reset");
    }

    starbytes::Region region;
    region.startLine = 1;
    region.endLine = 1;
    region.startCol = 5;
    region.endCol = 8;

    handler->setCodeViewSource("DiagBaseline.starb", "decl alpha:Int = 1\n");
    handler->push(starbytes::StandardDiagnostic::createError("bad alpha", region));
    handler->push(starbytes::StandardDiagnostic::createWarning("warn alpha"));

    auto snapshot = handler->snapshot();
    if(snapshot.size() != 2) {
        return fail("snapshot should include two diagnostics");
    }
    if(snapshot[0]->code.find("SB-SEMA-E") != 0) {
        return fail("missing semantic error code family");
    }
    if(snapshot[1]->code.find("SB-SEMA-W") != 0) {
        return fail("missing semantic warning code family");
    }
    if(snapshot[0]->phaseString() != "semantic") {
        return fail("expected semantic phase");
    }
    if(snapshot[0]->id.empty()) {
        return fail("expected stable diagnostic id");
    }

    if(!handler->hasErrored()) {
        return fail("hasErrored should report true when an error exists");
    }

    auto preLog = handler->getMetrics();
    if(preLog.pushedCount != 2 || preLog.errorCount != 1 || preLog.warningCount != 1 || preLog.withLocationCount != 1) {
        return fail("pre-log metrics mismatch");
    }
    if(preLog.maxBufferedCount < 2) {
        return fail("maxBufferedCount should include buffered diagnostics");
    }

    handler->logAll();
    if(!handler->empty()) {
        return fail("buffer should be empty after logAll");
    }

    auto postLog = handler->getMetrics();
    if(postLog.renderedCount != 2) {
        return fail("renderedCount mismatch after logAll");
    }
    if(postLog.pushTimeNs < preLog.pushTimeNs) {
        return fail("push timing metric should be monotonic");
    }
    if(postLog.renderTimeNs < preLog.renderTimeNs) {
        return fail("render timing metric should be monotonic");
    }
    if(postLog.hasErroredTimeNs < preLog.hasErroredTimeNs) {
        return fail("hasErrored timing metric should be monotonic");
    }

    auto output = out.str();
    if(output.find("error:") == std::string::npos) {
        return fail("missing error output");
    }
    if(output.find("warning:") == std::string::npos) {
        return fail("missing warning output");
    }
    if(output.find("DiagBaseline.starb:1:6") == std::string::npos) {
        return fail("missing code-view location output");
    }

    std::ostringstream machineOut;
    auto machineHandler = starbytes::DiagnosticHandler::createDefault(machineOut);
    machineHandler->setDefaultPhase(starbytes::Diagnostic::Phase::Parser);
    machineHandler->setOutputMode(starbytes::DiagnosticHandler::OutputMode::Machine);
    machineHandler->setCodeViewSource("DiagMachine.starb","decl beta:Int = 1\n");
    machineHandler->push(starbytes::StandardDiagnostic::createError("bad beta", region));
    machineHandler->logAll();
    auto machineText = machineOut.str();
    if(machineText.find(" --> ") != std::string::npos) {
        return fail("machine mode should skip code-view rendering");
    }

    handler->resetMetrics();
    auto afterReset = handler->getMetrics();
    if(afterReset.pushedCount != 0 || afterReset.renderedCount != 0 || afterReset.pushTimeNs != 0 || afterReset.renderTimeNs != 0) {
        return fail("metrics reset failed");
    }

    return 0;
}
