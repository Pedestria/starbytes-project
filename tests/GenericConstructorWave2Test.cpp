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
    std::cerr << "GenericConstructorWave2Test failure: " << message << '\n';
    return 1;
}

bool hasDefaultType(starbytes::ASTGenericParamDecl *param,const char *paramName,const char *defaultTypeName) {
    return param &&
           param->id &&
           param->id->val == paramName &&
           param->defaultType &&
           param->defaultType->getName().str() == defaultTypeName;
}

}

int main() {
    using namespace starbytes;

    const char *source = R"starb(
class Box<T> {
    new<U = T>(seed:U, fallback:T) {
        decl mirror:U = seed
        decl value:T = fallback
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
    if(!classDecl) {
        return fail("expected class declaration");
    }
    if(classDecl->constructors.size() != 1) {
        return fail("expected exactly one constructor");
    }
    auto *ctorDecl = classDecl->constructors.front();
    if(!ctorDecl || ctorDecl->genericParams.size() != 1 ||
       !hasDefaultType(ctorDecl->genericParams.front(),"U","T")) {
        return fail("constructor generic parameters were not parsed");
    }
    if(ctorDecl->params.size() != 2) {
        return fail("constructor parameters were not parsed");
    }
    if(!ctorDecl->blockStmt || ctorDecl->blockStmt->body.size() != 2) {
        return fail("constructor body was not parsed");
    }

    if(diagnostics->hasErrored()) {
        std::cerr << diagOut.str();
        return fail("lexer/parser reported unexpected diagnostics");
    }

    return 0;
}
