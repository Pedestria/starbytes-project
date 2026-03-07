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
    std::cerr << "GenericFreeFunctionPhase2Test failure: " << message << '\n';
    return 1;
}

starbytes::ASTExpr *requireInvokeExpr(starbytes::ASTVarDecl *decl) {
    if(!decl || decl->specs.empty()) {
        return nullptr;
    }
    auto *expr = decl->specs.front().expr;
    if(!expr || expr->type != IVKE_EXPR) {
        return nullptr;
    }
    return expr;
}

}

int main() {
    using namespace starbytes;

    const char *source = R"starb(
func identity<T>(value:T) T {
    return value
}

decl a = identity<Int>(4)

scope Math {
    func keep<T>(value:T) T {
        return value
    }
}

decl b = Math.keep<String>("ok")

func lessThanThree(value:Int) Bool {
    return value < 3
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

    auto *funcDecl = dynamic_cast<ASTFuncDecl *>(syntax.nextStatement());
    if(!funcDecl || !funcDecl->funcId || funcDecl->funcId->val != "identity") {
        return fail("failed to parse generic free function declaration");
    }

    auto *directCallDecl = dynamic_cast<ASTVarDecl *>(syntax.nextStatement());
    auto *directCallExpr = requireInvokeExpr(directCallDecl);
    if(!directCallExpr) {
        return fail("failed to parse direct generic invocation");
    }
    if(directCallExpr->genericTypeArgs.size() != 1 || !directCallExpr->genericTypeArgs.front() ||
       directCallExpr->genericTypeArgs.front()->getName().str() != "Int") {
        return fail("direct generic invocation did not retain explicit type arguments");
    }
    if(!directCallExpr->callee || directCallExpr->callee->type != ID_EXPR || !directCallExpr->callee->id ||
       directCallExpr->callee->id->val != "identity") {
        return fail("direct generic invocation callee was not parsed as identifier");
    }

    auto *scopeDecl = dynamic_cast<ASTScopeDecl *>(syntax.nextStatement());
    if(!scopeDecl || !scopeDecl->scopeId || scopeDecl->scopeId->val != "Math") {
        return fail("failed to parse scope declaration for scoped generic invocation");
    }

    auto *scopedCallDecl = dynamic_cast<ASTVarDecl *>(syntax.nextStatement());
    auto *scopedCallExpr = requireInvokeExpr(scopedCallDecl);
    if(!scopedCallExpr) {
        return fail("failed to parse scoped generic invocation");
    }
    if(scopedCallExpr->genericTypeArgs.size() != 1 || !scopedCallExpr->genericTypeArgs.front() ||
       scopedCallExpr->genericTypeArgs.front()->getName().str() != "String") {
        return fail("scoped generic invocation did not retain explicit type arguments");
    }
    if(!scopedCallExpr->callee || scopedCallExpr->callee->type != MEMBER_EXPR ||
       !scopedCallExpr->callee->rightExpr || !scopedCallExpr->callee->rightExpr->id ||
       scopedCallExpr->callee->rightExpr->id->val != "keep") {
        return fail("scoped generic invocation callee was not parsed as member expression");
    }

    auto *lessThanDecl = dynamic_cast<ASTFuncDecl *>(syntax.nextStatement());
    if(!lessThanDecl || !lessThanDecl->blockStmt || lessThanDecl->blockStmt->body.empty()) {
        return fail("failed to parse comparison disambiguation function");
    }
    auto *returnDecl = dynamic_cast<ASTReturnDecl *>(lessThanDecl->blockStmt->body.front());
    if(!returnDecl || !returnDecl->expr || returnDecl->expr->type != BINARY_EXPR ||
       !returnDecl->expr->oprtr_str.has_value() || returnDecl->expr->oprtr_str.value() != "<") {
        return fail("identifier comparison was misparsed as generic invocation syntax");
    }

    if(diagnostics->hasErrored()) {
        std::cerr << diagOut.str();
        return fail("lexer/parser reported unexpected diagnostics");
    }

    return 0;
}
