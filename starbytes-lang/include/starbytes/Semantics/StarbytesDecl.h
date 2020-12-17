#include "starbytes/Base/Base.h"
#include "starbytes/AST/ASTTraveler.h"

STARBYTES_SEMANTICS_NAMESPACE
using namespace AST;

class SemanticA;

ASTVisitorResponse atVarDecl(ASTTravelContext & context,SemanticA * sem);
ASTVisitorResponse atConstDecl(ASTTravelContext & context,SemanticA * sem);
ASTVisitorResponse atFuncDecl(ASTTravelContext & context,SemanticA * sem);
// ASTVisitorResponse atClassDecl(ASTTravelContext & context,SemanticA * sem);

NAMESPACE_END