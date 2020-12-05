
#include "starbytes/Base/Base.h"
#include <initializer_list>
#include <iostream>
#include <optional>
#include <string>
#include <vector>
#include <cassert>


#ifndef AST_AST_H
#define AST_AST_H

STARBYTES_STD_NAMESPACE
namespace AST {

#define ASTDECL(name)                                                          \
  struct name : ASTDeclaration {                                               \
    name(ASTType _Type) : ASTDeclaration(_Type){};
#define ASTEXPR(name)                                                          \
  struct name : ASTExpression {                                                \
    name(ASTType _Type) : ASTExpression(_Type){};
#define ASTSTATEMENT(name)                                                     \
  struct name : ASTStatement {                                                 \
    name(ASTType _Type) : ASTStatement(_Type){};
#define ASTLITERAL(name)                                                       \
  struct name : ASTLiteral {                                                   \
    name(ASTType _Type) : ASTLiteral(_Type){};

#define CUSTOM_AST(name, type)                                                 \
  struct name : type {                                                         \
    name(ASTType _Type) : type(_Type){};

TYPED_ENUM ASTType : int{Identifier,
                         TypecastIdentifier,
                         ImportDeclaration,
                         ScopeDeclaration,
                         NumericLiteral,
                         VariableDeclaration,
                         VariableSpecifier,
                         ConstantDeclaration,
                         ConstantSpecifier,
                         StringLiteral,
                         FunctionDeclaration,
                         BlockStatement,
                         TypeIdentifier,
                         AssignExpression,
                         ArrayExpression,
                         NewExpression,
                         MemberExpression,
                         CallExpression,
                         ClassDeclaration,
                         ClassPropertyDeclaration,
                         ClassBlockStatement,
                         ClassMethodDeclaration,
                         ClassConstructorDeclaration,
                         ClassConstructorParameterDeclaration,
                         TypeArgumentsDeclaration,
                         InterfaceDeclaration,
                         InterfaceBlockStatement,
                         InterfacePropertyDeclaration,
                         InterfaceMethodDeclaration,
                         ReturnDeclaration,
                         EnumDeclaration,
                         EnumBlockStatement,
                         Enumerator,
                         IfDeclaration,
                         ElseIfDeclaration,
                         ElseDeclaration,
                         BooleanLiteral,
                         AwaitExpression,
                         ExpressionStatement,
                         AcquireDeclaration};

TYPED_ENUM CommentType : int{LineComment, BlockComment};

struct ASTComment {
  CommentType type;
};

struct ASTLineComment : ASTComment {
  std::string value;
};

struct ASTBlockComment : ASTComment {
  unsigned int line_num;
  std::vector<std::string> lines;
};

struct ASTObject {
  std::vector<ASTComment *> preceding_comments;
  ASTType type;
  ASTObject(ASTType _Type) : type(_Type){};
};
/*Abstract Syntax Tree Node*/
struct ASTNode : ASTObject {
  ASTNode(ASTType _type) : ASTObject(_type){};
  DocumentPosition position;
};
struct ASTLiteral : ASTNode {
  ASTLiteral(ASTType _type) : ASTNode(_type){};
};
ASTLITERAL(ASTStringLiteral)
static ASTType static_type;
std::string value;
};
ASTLITERAL(ASTNumericLiteral)
static ASTType static_type;
std::string value;
}
;

ASTLITERAL(ASTBooleanLiteral)
static ASTType static_type;
std::string value;
}
;

struct ASTStatement : ASTObject {
  ASTStatement(ASTType _type) : ASTObject(_type){};
  DocumentPosition BeginFold;
  DocumentPosition EndFold;
};
struct ASTIdentifier : ASTNode {
  ASTIdentifier() : ASTNode(ASTType::Identifier){};
  static ASTType static_type;
  std::string value;
};
ASTSTATEMENT(ASTTypeIdentifier)
static ASTType static_type;
std::string type_name;
bool isGeneric;
std::vector<ASTTypeIdentifier *> typeargs;
bool isArrayType;
unsigned int array_count = 0;
}
;
ASTSTATEMENT(ASTTypeCastIdentifier)
static ASTType static_type;
ASTIdentifier *id;
ASTTypeIdentifier *tid;
}
;
ASTSTATEMENT(ASTExpression)
}
;
ASTSTATEMENT(ASTExpressionStatement)
static ASTType static_type;
ASTExpression *expression;
}
;
ASTEXPR(ASTCallExpression)
static ASTType static_type;
ASTExpression *callee;
std::vector<ASTExpression *> params;
}
;
ASTEXPR(ASTNewExpression)
static ASTType static_type;
ASTTypeIdentifier *decltid;
std::vector<ASTExpression *> params;
}
;
ASTEXPR(ASTArrayExpression)
static ASTType static_type;
std::vector<ASTExpression *> items;
}
;
ASTEXPR(ASTAssignExpression)
static ASTType static_type;
ASTExpression *subject;
std::string op;
ASTExpression *object;
}
;
ASTEXPR(ASTMemberExpression)
static ASTType static_type;
ASTExpression *object;
ASTExpression *prop;
}
;

ASTEXPR(ASTUnaryExpression)
static ASTType static_type;
std::string oprtr;
ASTExpression *left;
ASTExpression *right;
}
;
ASTEXPR(ASTAwaitExpression)
static ASTType static_type;
ASTCallExpression *callee;
}
;
ASTSTATEMENT(ASTBlockStatement)
static ASTType static_type;
std::vector<ASTStatement *> nodes;
}
;
ASTSTATEMENT(ASTDeclaration)
}
;
ASTDECL(ASTTypeArgumentsDeclaration)
static ASTType static_type;
std::vector<ASTIdentifier *> args;
}
;
ASTSTATEMENT(ASTClassStatement)
}
;
ASTSTATEMENT(ASTClassConstructorParameterDeclaration)
static ASTType static_type;
bool loose;
bool isConstant;
ASTTypeCastIdentifier *tcid;
}
;
CUSTOM_AST(ASTClassConstructorDeclaration, ASTClassStatement)
static ASTType static_type;
std::vector<ASTClassConstructorParameterDeclaration *> params;
}
;
CUSTOM_AST(ASTClassPropertyDeclaration, ASTClassStatement)
static ASTType static_type;
bool loose;
bool isConstant;
// EITHER a ASTIdentifier or ASTTypeCastIdentifier
ASTNode *id;
ASTExpression *initializer;
}
;
CUSTOM_AST(ASTClassMethodDeclaration, ASTClassStatement)
static ASTType static_type;
ASTIdentifier *id;
bool isGeneric;
bool isLazy;
ASTTypeArgumentsDeclaration *typeargs;
std::vector<ASTTypeCastIdentifier *> params;
ASTTypeIdentifier *returnType;
ASTBlockStatement *body;
}
;
ASTSTATEMENT(ASTClassBlockStatement)
std::vector<ASTClassStatement *> nodes;
}
;
ASTDECL(ASTClassDeclaration)
static ASTType static_type;
ASTIdentifier *id;
bool isGeneric;
ASTTypeArgumentsDeclaration *typeargs;
bool isChildClass;
ASTTypeIdentifier *superclass;
bool implementsInterfaces;
std::vector<ASTTypeIdentifier *> interfaces;
ASTClassBlockStatement *body;
}
;
ASTSTATEMENT(ASTInterfaceStatement)
}
;

ASTSTATEMENT(ASTReturnDeclaration)
static ASTType static_type;
ASTExpression *returnee;
}
;

ASTSTATEMENT(ASTEnumStatement)
}
;
CUSTOM_AST(ASTEnumerator, ASTEnumStatement)
static ASTType static_type;
ASTIdentifier *id;
bool hasValue;
ASTNumericLiteral *value;
}
;

ASTSTATEMENT(ASTEnumBlockStatement)
static ASTType static_type;
std::vector<ASTEnumerator *> nodes;
}
;
ASTSTATEMENT(ASTEnumDeclaration)
static ASTType static_type;
ASTIdentifier *id;
ASTEnumBlockStatement *body;
}
;
CUSTOM_AST(ASTInterfacePropertyDeclaration, ASTInterfaceStatement)
static ASTType static_type;
bool loose;
bool isConstant;
ASTTypeCastIdentifier *tcid;
}
;
CUSTOM_AST(ASTInterfaceMethodDeclaration, ASTInterfaceStatement)
static ASTType static_type;
ASTIdentifier *id;
bool isGeneric;
bool isLazy;
ASTTypeArgumentsDeclaration *typeargs;
std::vector<ASTTypeCastIdentifier *> params;
ASTTypeIdentifier *returnType;
}
;
ASTSTATEMENT(ASTInterfaceBlockStatement)
static ASTType static_type;
std::vector<ASTInterfaceStatement *> nodes;
}
;
ASTDECL(ASTInterfaceDeclaration)
static ASTType static_type;
ASTIdentifier *id;
bool isGeneric;
ASTTypeArgumentsDeclaration *typeargs;
bool isChildInterface;
ASTTypeIdentifier *superclass;
ASTInterfaceBlockStatement *body;
}
;

ASTDECL(ASTIfDeclaration)
static ASTType static_type;
ASTExpression *subject;
ASTBlockStatement *body;
}
;
ASTDECL(ASTElseIfDeclaration)
static ASTType static_type;
ASTExpression *subject;
ASTBlockStatement *body;
}
;

ASTDECL(ASTElseDeclaration)
static ASTType static_type;
ASTBlockStatement *body;
}
;
ASTSTATEMENT(ASTVariableSpecifier)
static ASTType static_type;
// EITHER a ASTIdentifier or ASTTypeCastIdentifier
ASTObject *id;
std::optional<ASTExpression *> initializer;
}
;
ASTDECL(ASTVariableDeclaration)
static ASTType static_type;
std::vector<ASTVariableSpecifier *> specifiers;
}
;
ASTSTATEMENT(ASTConstantSpecifier)
static ASTType static_type;
// EITHER a ASTIdentifier or ASTTypeCastIdentifier
ASTTypeCastIdentifier *id;
std::optional<ASTExpression *> initializer;
}
;
ASTDECL(ASTConstantDeclaration)
static ASTType static_type;
std::vector<ASTConstantSpecifier *> specifiers;
}
;
ASTDECL(ASTImportDeclaration)
static ASTType static_type;
ASTIdentifier *id;
}
;

ASTDECL(ASTScopeDeclaration)
static ASTType static_type;
ASTIdentifier *id;
ASTBlockStatement *body;
}
;
ASTDECL(ASTFunctionDeclaration)
static ASTType static_type;
ASTIdentifier *id;
bool isAsync;
bool isGeneric;
ASTTypeArgumentsDeclaration *typeargs;
std::vector<ASTTypeCastIdentifier *> params;
ASTTypeIdentifier *returnType;
ASTBlockStatement *body;
}
;

ASTDECL(ASTAcquireDeclaration)
static ASTType static_type;
ASTIdentifier *id;
}
;

class AbstractSyntaxTree {
public:
  std::string filename;
  std::vector<ASTStatement *> nodes;
  AbstractSyntaxTree(){};
  // AbstractSyntaxTree(std::string & str_ref):filename(str_ref){};
};

template <class _NodeTy, class PtrTy> inline bool astnode_is(PtrTy *node) {
  if (node->type == _NodeTy::static_type) {
    return true;
  } else {
    return false;
  }
}

// template<class _NodeTy,class PtrT> inline _NodeTy * astnode_assert(PtrT *node){
//   assert((node->type == _NodeTy::static_type) && "ASTNode cannot be downcasted!");
//   return (_NodeTy *)node;
// };

#define AST_NODE_CAST(ptr) ((ASTNode *)ptr)
#define AST_NODE_IS(ptr, type) (astnode_is<type>(ptr))
#define ASSERT_AST_NODE(ptr, type) ((type *)ptr)
}
;

NAMESPACE_END

#endif