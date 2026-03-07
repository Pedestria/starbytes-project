#include "starbytes/base/Diagnostic.h"
#include "starbytes/compiler/AST.h"
#include "starbytes/compiler/ASTDecl.h"
#include "starbytes/compiler/Lexer.h"
#include "starbytes/compiler/SyntaxA.h"

#include <iostream>
#include <sstream>
#include <vector>

namespace {

int fail(const char *message) {
    std::cerr << "GenericMethodPhase5Test failure: " << message << '\n';
    return 1;
}

}

int main() {
    using namespace starbytes;

    const char *source = R"starb(
class Box<T> {
    func echo<U>(left:T, right:U) U {
        return right
    }
}
)starb";

    std::ostringstream diagOut;
    auto diagnostics = DiagnosticHandler::createDefault(diagOut);
    if(!diagnostics) {
        return fail("unable to create diagnostics handler");
    }

    Syntax::Lexer lexer(*diagnostics);
    std::vector<Syntax::Tok> tokenStream;
    std::istringstream in(source);
    lexer.tokenizeFromIStream(in,tokenStream);

    Syntax::SyntaxA syntax;
    syntax.setTokenStream(tokenStream);

    auto *classDecl = dynamic_cast<ASTClassDecl *>(syntax.nextStatement());
    if(!classDecl || !classDecl->id || classDecl->id->val != "Box") {
        return fail("failed to parse generic class declaration");
    }
    if(classDecl->genericTypeParams.size() != 1 || !classDecl->genericTypeParams.front() ||
       classDecl->genericTypeParams.front()->val != "T") {
        return fail("class generic parameter list was not parsed as `<T>`");
    }
    if(classDecl->methods.size() != 1) {
        return fail("expected exactly one class method");
    }

    auto *methodDecl = classDecl->methods.front();
    if(!methodDecl || !methodDecl->funcId || methodDecl->funcId->val != "echo") {
        return fail("failed to parse generic method declaration");
    }
    if(methodDecl->genericTypeParams.size() != 1 || !methodDecl->genericTypeParams.front() ||
       methodDecl->genericTypeParams.front()->val != "U") {
        return fail("method generic parameter list was not parsed as `<U>`");
    }
    if(methodDecl->params.size() != 2) {
        return fail("expected exactly two method parameters");
    }

    auto paramIt = methodDecl->params.begin();
    auto *leftType = paramIt->second;
    ++paramIt;
    auto *rightType = paramIt->second;
    if(!leftType || leftType->getName().str() != "T") {
        return fail("class generic parameter was not visible inside generic method parameter types");
    }
    if(!rightType || rightType->getName().str() != "U") {
        return fail("method generic parameter was not visible inside generic method parameter types");
    }
    if(!methodDecl->returnType || methodDecl->returnType->getName().str() != "U") {
        return fail("method generic parameter was not visible in return type");
    }

    if(diagnostics->hasErrored()) {
        std::cerr << diagOut.str();
        return fail("lexer/parser reported unexpected diagnostics");
    }

    return 0;
}
