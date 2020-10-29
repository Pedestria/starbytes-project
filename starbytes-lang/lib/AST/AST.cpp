#include "starbytes/AST/AST.h"
#include "starbytes/Base/Base.h"



STARBYTES_STD_NAMESPACE
namespace AST {

#define SET_INIT_TYPE(_type,_enum) ASTType _type::static_type = ASTType::_enum

//Intialize All Static types so RTTC can work. (Run Time Type Checking)

SET_INIT_TYPE(ASTTypeIdentifier,TypeIdentifier);
SET_INIT_TYPE(ASTIdentifier,Identifier);
SET_INIT_TYPE(ASTNumericLiteral,NumericLiteral);
SET_INIT_TYPE(ASTBooleanLiteral,BooleanLiteral);
SET_INIT_TYPE(ASTStringLiteral,StringLiteral);

SET_INIT_TYPE(ASTVariableDeclaration,VariableDeclaration);
SET_INIT_TYPE(ASTVariableSpecifier,VariableSpecifier);
SET_INIT_TYPE(ASTIfDeclaration,IfDeclaration);
SET_INIT_TYPE(ASTElseIfDeclaration,ElseIfDeclaration);
SET_INIT_TYPE(ASTElseDeclaration,ElseDeclaration);
SET_INIT_TYPE(ASTClassDeclaration,ClassDeclaration);
SET_INIT_TYPE(ASTImportDeclaration,ImportDeclaration);
SET_INIT_TYPE(ASTTypeCastIdentifier,TypecastIdentifier);
SET_INIT_TYPE(ASTTypeArgumentsDeclaration,TypeArgumentsDeclaration);
SET_INIT_TYPE(ASTClassPropertyDeclaration,ClassPropertyDeclaration);
SET_INIT_TYPE(ASTClassMethodDeclaration,ClassMethodDeclaration);
SET_INIT_TYPE(ASTClassConstructorDeclaration,ClassConstructorDeclaration);
SET_INIT_TYPE(ASTInterfaceDeclaration,InterfaceDeclaration);
SET_INIT_TYPE(ASTInterfacePropertyDeclaration,InterfacePropertyDeclaration);
SET_INIT_TYPE(ASTInterfaceMethodDeclaration,InterfaceMethodDeclaration);
SET_INIT_TYPE(ASTFunctionDeclaration,FunctionDeclaration);
SET_INIT_TYPE(ASTConstantDeclaration,ConstantDeclaration);
SET_INIT_TYPE(ASTConstantSpecifier,ConstantSpecifier);
SET_INIT_TYPE(ASTScopeDeclaration,ScopeDeclaration);
SET_INIT_TYPE(ASTReturnDeclaration,ReturnDeclaration);
SET_INIT_TYPE(ASTAcquireDeclaration,AcquireDeclaration);

SET_INIT_TYPE(ASTAssignExpression,AssignExpression);
SET_INIT_TYPE(ASTExpressionStatement,ExpressionStatement);
SET_INIT_TYPE(ASTArrayExpression,ArrayExpression);
SET_INIT_TYPE(ASTAwaitExpression,AwaitExpression);
SET_INIT_TYPE(ASTCallExpression,CallExpression);
SET_INIT_TYPE(ASTMemberExpression,MemberExpression);
SET_INIT_TYPE(ASTNewExpression,NewExpression);

#undef SET_INIT_TYPE

}

NAMESPACE_END
