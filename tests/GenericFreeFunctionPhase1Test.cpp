#include "starbytes/compiler/AST.h"
#include "starbytes/compiler/ASTDecl.h"
#include "starbytes/compiler/Parser.h"

#include <iostream>
#include <sstream>
#include <vector>

namespace {

struct CaptureConsumer final : public starbytes::ASTStreamConsumer {
    std::vector<starbytes::ASTDecl *> decls;

    bool acceptsSymbolTableContext() override {
        return false;
    }

    void consumeStmt(starbytes::ASTStmt *) override {}

    void consumeDecl(starbytes::ASTDecl *stmt) override {
        decls.push_back(stmt);
    }
};

int fail(const char *message) {
    std::cerr << "GenericFreeFunctionPhase1Test failure: " << message << '\n';
    return 1;
}

}

int main() {
    using namespace starbytes;

    const char *source = R"starb(
func identity<T>(value:T) T {
    decl copy:T = value
    return copy
}

print("ok")
)starb";

    std::istringstream in(source);
    CaptureConsumer consumer;
    Parser parser(consumer);
    auto parseContext = ModuleParseContext::Create("GenericFreeFunctionPhase1Test");
    parser.parseFromStream(in, parseContext);

    if(consumer.decls.empty()) {
        return fail("expected at least one declaration");
    }

    auto *funcDecl = dynamic_cast<ASTFuncDecl *>(consumer.decls.front());
    if(!funcDecl) {
        return fail("first declaration was not a function");
    }
    if(!funcDecl->funcId || funcDecl->funcId->val != "identity") {
        return fail("unexpected function identifier");
    }
    if(funcDecl->genericParams.size() != 1) {
        return fail("expected exactly one generic parameter");
    }
    auto *genericParam = funcDecl->genericParams.front();
    if(!genericParam || !genericParam->id || genericParam->id->val != "T") {
        return fail("expected generic parameter T");
    }
    if(genericParam->variance != ASTGenericVariance::Invariant || genericParam->defaultType != nullptr ||
       !genericParam->bounds.empty()) {
        return fail("expected wave 0 generic metadata defaults for T");
    }
    if(funcDecl->params.size() != 1) {
        return fail("expected exactly one function parameter");
    }
    auto param = *funcDecl->params.begin();
    if(!param.second || param.second->getName().str() != "T") {
        return fail("parameter type did not retain generic type parameter");
    }
    if(!funcDecl->returnType || funcDecl->returnType->getName().str() != "T") {
        return fail("return type did not retain generic type parameter");
    }
    if(!funcDecl->blockStmt || funcDecl->blockStmt->body.empty()) {
        return fail("expected function body to be present");
    }
    auto *localDecl = dynamic_cast<ASTVarDecl *>(funcDecl->blockStmt->body.front());
    if(!localDecl || localDecl->specs.empty() || !localDecl->specs.front().type) {
        return fail("expected typed local declaration inside generic function body");
    }
    if(localDecl->specs.front().type->getName().str() != "T") {
        return fail("local typed declaration did not resolve generic type parameter");
    }

    if(!parser.finish()) {
        return fail("parser.finish() rejected generic free-function declaration");
    }

    return 0;
}
