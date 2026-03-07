#include "starbytes/base/Diagnostic.h"
#include "starbytes/compiler/AST.h"
#include "starbytes/compiler/ASTDecl.h"
#include "starbytes/compiler/Lexer.h"
#include "starbytes/compiler/SyntaxA.h"

#include <cmath>
#include <iostream>
#include <sstream>
#include <vector>

namespace {

int fail(const char *message){
    std::cerr << "NumericPhase1LexerTest failure: " << message << '\n';
    return 1;
}

bool approx(double lhs,double rhs,double epsilon = 1e-12){
    return std::fabs(lhs - rhs) <= epsilon;
}

}

int main(){
    using namespace starbytes;

    const char *source = R"starb(
decl a = 9.732e6
decl b = 9.732e-6
decl c = 1.25e-3
)starb";

    std::ostringstream diagOut;
    auto diagnostics = DiagnosticHandler::createDefault(diagOut);
    if(!diagnostics){
        return fail("unable to create diagnostics handler");
    }

    Syntax::Lexer lexer(*diagnostics);
    std::vector<Syntax::Tok> tokenStream;
    std::istringstream in(source);
    lexer.tokenizeFromIStream(in,tokenStream);

    Syntax::SyntaxA syntax;
    syntax.setTokenStream(tokenStream);

    auto *declA = dynamic_cast<ASTVarDecl *>(syntax.nextStatement());
    auto *declB = dynamic_cast<ASTVarDecl *>(syntax.nextStatement());
    auto *declC = dynamic_cast<ASTVarDecl *>(syntax.nextStatement());

    auto *literalA = declA && !declA->specs.empty() ? dynamic_cast<ASTLiteralExpr *>(declA->specs.front().expr) : nullptr;
    auto *literalB = declB && !declB->specs.empty() ? dynamic_cast<ASTLiteralExpr *>(declB->specs.front().expr) : nullptr;
    auto *literalC = declC && !declC->specs.empty() ? dynamic_cast<ASTLiteralExpr *>(declC->specs.front().expr) : nullptr;

    if(!literalA || !literalA->floatValue.has_value()){
        return fail("failed to parse positive scientific exponent literal");
    }
    if(!literalB || !literalB->floatValue.has_value()){
        return fail("failed to parse negative scientific exponent literal");
    }
    if(!literalC || !literalC->floatValue.has_value()){
        return fail("failed to parse standard scientific literal");
    }

    if(!approx(*literalA->floatValue,9732000.0)){
        return fail("unexpected value for positive scientific exponent literal");
    }
    if(!approx(*literalB->floatValue,9.732e-6)){
        return fail("unexpected value for negative scientific exponent literal");
    }
    if(!approx(*literalC->floatValue,0.00125)){
        return fail("unexpected value for standard scientific literal");
    }

    if(diagnostics->hasErrored()){
        std::cerr << diagOut.str();
        return fail("lexer/parser emitted unexpected diagnostics");
    }

    const char *invalidSource = R"starb(
decl broken = 3.52e^2
)starb";

    std::ostringstream invalidDiagOut;
    auto invalidDiagnostics = DiagnosticHandler::createDefault(invalidDiagOut);
    if(!invalidDiagnostics){
        return fail("unable to create invalid-case diagnostics handler");
    }

    Syntax::Lexer invalidLexer(*invalidDiagnostics);
    std::vector<Syntax::Tok> invalidTokenStream;
    std::istringstream invalidIn(invalidSource);
    invalidLexer.tokenizeFromIStream(invalidIn,invalidTokenStream);

    if(!invalidDiagnostics->hasErrored()){
        return fail("deprecated e^ scientific notation should be rejected");
    }
    auto invalidRecords = invalidDiagnostics->collectRecords();
    bool sawMalformedNumericLiteral = false;
    for(const auto &record : invalidRecords){
        if(record.message.find("Malformed numeric literal") != std::string::npos){
            sawMalformedNumericLiteral = true;
            break;
        }
    }
    if(!sawMalformedNumericLiteral){
        std::cerr << invalidDiagOut.str();
        return fail("unexpected diagnostic for deprecated e^ scientific notation");
    }

    return 0;
}
