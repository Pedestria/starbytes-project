#include "starbytes/compiler/AST.h"
#include "starbytes/compiler/Parser.h"

#include <fstream>
#include <iostream>

namespace {

class NullConsumer final : public starbytes::ASTStreamConsumer {
public:
    bool acceptsSymbolTableContext() override {
        return true;
    }

    void consumeSTableContext(starbytes::Semantics::STableContext *) override {}

    void consumeStmt(starbytes::ASTStmt *) override {}

    void consumeDecl(starbytes::ASTDecl *) override {}
};

int fail(const char *message) {
    std::cerr << "CompileProfileTest failure: " << message << '\n';
    return 1;
}

}

int main() {
    std::ifstream in("./test.starb");
    if(!in.is_open()) {
        in.open("./tests/test.starb");
    }
    if(!in.is_open()) {
        return fail("cannot open test source");
    }

    NullConsumer consumer;
    starbytes::Parser parser(consumer);
    parser.resetProfileData();
    parser.setProfilingEnabled(true);

    auto parseContext = starbytes::ModuleParseContext::Create("CompileProfileTest");
    parser.parseFromStream(in, parseContext);
    if(!parser.finish()) {
        return fail("parser finished with diagnostics");
    }

    auto profile = parser.getProfileData();
    if(profile.fileCount == 0 || profile.sourceBytes == 0 || profile.tokenCount == 0 || profile.statementCount == 0) {
        return fail("profile counters were not populated");
    }
    if(profile.totalNs == 0 || profile.lexNs == 0 || profile.syntaxNs == 0 || profile.semanticNs == 0) {
        return fail("profile timings were not populated");
    }

    return 0;
}
