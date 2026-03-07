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
    std::cerr << "GenericDefaultsWave1Test failure: " << message << '\n';
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
def Alias<T = String> = T

func identity<T = String>(value:T) T {
    return value
}

class Box<T = String> {
    func echo<U = T>(value:U) U {
        return value
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

    auto *aliasDecl = dynamic_cast<ASTTypeAliasDecl *>(syntax.nextStatement());
    auto *funcDecl = dynamic_cast<ASTFuncDecl *>(syntax.nextStatement());
    auto *classDecl = dynamic_cast<ASTClassDecl *>(syntax.nextStatement());

    if(!aliasDecl || aliasDecl->genericParams.size() != 1 ||
       !hasDefaultType(aliasDecl->genericParams.front(),"T","String")) {
        return fail("type alias generic default was not parsed");
    }
    if(!funcDecl || funcDecl->genericParams.size() != 1 ||
       !hasDefaultType(funcDecl->genericParams.front(),"T","String")) {
        return fail("function generic default was not parsed");
    }
    if(!classDecl || classDecl->genericParams.size() != 1 ||
       !hasDefaultType(classDecl->genericParams.front(),"T","String")) {
        return fail("class generic default was not parsed");
    }
    if(classDecl->methods.size() != 1 || classDecl->methods.front()->genericParams.size() != 1 ||
       !hasDefaultType(classDecl->methods.front()->genericParams.front(),"U","T")) {
        return fail("method generic default referencing outer generic parameter was not parsed");
    }

    if(diagnostics->hasErrored()) {
        std::cerr << diagOut.str();
        return fail("lexer/parser reported unexpected diagnostics");
    }

    return 0;
}
