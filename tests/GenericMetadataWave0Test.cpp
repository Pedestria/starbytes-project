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
    std::cerr << "GenericMetadataWave0Test failure: " << message << '\n';
    return 1;
}

bool assertGenericParam(starbytes::ASTGenericParamDecl *param,const char *name) {
    if(!param || !param->id) {
        return false;
    }
    if(param->id->val != name) {
        return false;
    }
    if(param->variance != starbytes::ASTGenericVariance::Invariant) {
        return false;
    }
    if(param->defaultType != nullptr) {
        return false;
    }
    if(!param->bounds.empty()) {
        return false;
    }
    return true;
}

}

int main() {
    using namespace starbytes;

    const char *source = R"starb(
def Alias<T> = T

interface Mapper<T> {
    func map<U>(value:U) T
}

class Box<T> {
    new(seed:T) {}

    func echo<U>(value:U) U {
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
    auto *interfaceDecl = dynamic_cast<ASTInterfaceDecl *>(syntax.nextStatement());
    auto *classDecl = dynamic_cast<ASTClassDecl *>(syntax.nextStatement());

    if(!aliasDecl || !assertGenericParam(aliasDecl->genericParams.empty() ? nullptr : aliasDecl->genericParams.front(),"T")) {
        return fail("type alias generic metadata was not parsed into wave 0 node shape");
    }
    if(!interfaceDecl || !assertGenericParam(interfaceDecl->genericParams.empty() ? nullptr : interfaceDecl->genericParams.front(),"T")) {
        return fail("interface generic metadata was not parsed into wave 0 node shape");
    }
    if(interfaceDecl->methods.size() != 1 ||
       !assertGenericParam(interfaceDecl->methods.front()->genericParams.empty() ? nullptr
                                                                                 : interfaceDecl->methods.front()->genericParams.front(),
                           "U")) {
        return fail("interface method generic metadata was not parsed into wave 0 node shape");
    }
    if(!classDecl || !assertGenericParam(classDecl->genericParams.empty() ? nullptr : classDecl->genericParams.front(),"T")) {
        return fail("class generic metadata was not parsed into wave 0 node shape");
    }
    if(classDecl->constructors.size() != 1) {
        return fail("expected exactly one constructor");
    }
    if(!classDecl->constructors.front()->genericParams.empty()) {
        return fail("constructor generic metadata slot should exist and be empty before wave 2 parsing");
    }
    if(classDecl->methods.size() != 1 ||
       !assertGenericParam(classDecl->methods.front()->genericParams.empty() ? nullptr
                                                                             : classDecl->methods.front()->genericParams.front(),
                           "U")) {
        return fail("class method generic metadata was not parsed into wave 0 node shape");
    }

    if(diagnostics->hasErrored()) {
        std::cerr << diagOut.str();
        return fail("lexer/parser reported unexpected diagnostics");
    }

    return 0;
}
