#include "starbytes/AST/AST.h"
#include "starbytes/Base/Base.h"



STARBYTES_STD_NAMESPACE
namespace AST {

//Intialize All Static types so RTTC can work. (Run Time Type Checking)

ASTType ASTTypeIdentifier::static_type = ASTType::TypeIdentifier;
ASTType ASTIdentifier::static_type = ASTType::Identifier;
ASTType ASTNumericLiteral::static_type = ASTType::NumericLiteral;
ASTType ASTBooleanLiteral::static_type = ASTType::BooleanLiteral;

ASTType ASTVariableDeclaration::static_type = ASTType::VariableDeclaration;
ASTType ASTVariableSpecifier::static_type = ASTType::VariableSpecifier;
ASTType ASTIfDeclaration::static_type = ASTType::IfDeclaration;
ASTType ASTElseIfDeclaration::static_type = ASTType::ElseIfDeclaration;
ASTType ASTElseDeclaration::static_type = ASTType::ElseDeclaration;
ASTType ASTClassDeclaration::static_type = ASTType::ClassDeclaration;
ASTType ASTImportDeclaration::static_type = ASTType::ImportDeclaration;
ASTType ASTTypeCastIdentifier::static_type = ASTType::TypecastIdentifier;
ASTType ASTTypeArgumentsDeclaration::static_type = ASTType::TypeArgumentsDeclaration;
ASTType ASTClassPropertyDeclaration::static_type = ASTType::ClassPropertyDeclaration;
ASTType ASTClassMethodDeclaration::static_type = ASTType::ClassMethodDeclaration;
ASTType ASTClassConstructorDeclaration::static_type = ASTType::ClassConstructorDeclaration;
ASTType ASTFunctionDeclaration::static_type = ASTType::FunctionDeclaration;
ASTType ASTConstantDeclaration::static_type = ASTType::ConstantDeclaration;
ASTType ASTConstantSpecifier::static_type = ASTType::ConstantSpecifier;

ASTType ASTAssignExpression::static_type = ASTType::AssignExpression;
ASTType ASTExpressionStatement::static_type = ASTType::ExpressionStatement;
ASTType ASTArrayExpression::static_type = ASTType::ArrayExpression;
ASTType ASTAwaitExpression::static_type = ASTType::AwaitExpression;
ASTType ASTCallExpression::static_type = ASTType::CallExpression;
ASTType ASTMemberExpression::static_type = ASTType::MemberExpression;
ASTType ASTNewExpression::static_type = ASTType::NewExpression;


}

NAMESPACE_END
