#include "starbytes/Semantics/SemanticsMain.h"
#include "starbytes/AST/AST.h"

int main (){
    using namespace Starbytes;
    using namespace Starbytes::Semantics;
    AST::AbstractSyntaxTree *ast;
    SemanticAnalyzer an = SemanticAnalyzer(ast);
    an.initialize();
}