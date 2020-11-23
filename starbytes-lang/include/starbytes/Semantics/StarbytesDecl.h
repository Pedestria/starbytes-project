#include "starbytes/Base/Base.h"
#include "starbytes/AST/ASTTraveler.h"

STARBYTES_SEMANTICS_NAMESPACE
using namespace AST;

class SemanticA;

ASTVisitorResponse atVarSpec(ASTTravelContext & context,SemanticA * sem);
ASTVisitorResponse atConstSpec(ASTTravelContext & context,SemanticA * sem);
ASTVisitorResponse atFuncDecl(ASTTravelContext & context,SemanticA * sem);
// ASTVisitorResponse atClassDecl(ASTTravelContext & context,SemanticA * sem);

NAMESPACE_END