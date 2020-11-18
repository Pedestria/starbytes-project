#include "AST.h"
#include "starbytes/Base/ADT.h"

#ifndef AST_ASTRAVELER_H
#define AST_ASTRAVELER_H

STARBYTES_STD_NAMESPACE

namespace AST {

struct ASTTravelContext {
  std::optional<ASTNode *> previous;
  ASTNode *current;
  std::optional<ASTNode *> parent;
  std::optional<ASTNode *> next;
};

using ASTFuncCallback = bool (*)(ASTTravelContext &context);

struct ASTTravelSettings {};

#define AST_TRAVEL_FUNC(name) bool visit##name()

class ASTTraveler {
  Foundation::ImutDictionary<ASTType, ASTFuncCallback,2> ast_lookup;
  ASTTravelSettings options;
  ASTTravelContext cntxt;
  AbstractSyntaxTree *&tree;
  void setParentNode(ASTNode *node_ptr) { cntxt.parent = node_ptr; };
  void setCurrentNode(ASTNode * node_ptr) {cntxt.current = node_ptr;};
  void setNextNode(ASTNode * node_ptr) {cntxt.next = node_ptr;};
  void nextNodeAndSetAheadNode(ASTNode *node_ptr = nullptr) {
    cntxt.previous = cntxt.current;
    if (cntxt.next.has_value())
      cntxt.current = cntxt.next.value();
    else
      return;

    cntxt.next = node_ptr;
  };
  bool invokeCallback(ASTType & t){
      ASTFuncCallback & cb = ast_lookup.find(t);
      return cb(cntxt);
  };

  AST_TRAVEL_FUNC(Statement);
  AST_TRAVEL_FUNC(VariableDecl);
  AST_TRAVEL_FUNC(ConstantDecl);
  AST_TRAVEL_FUNC(FunctionDecl);
  AST_TRAVEL_FUNC(ClassDecl);
  AST_TRAVEL_FUNC(ScopeDecl);
  AST_TRAVEL_FUNC(ImportDecl);

public:
  ASTTraveler() = delete;
  ASTTraveler(AbstractSyntaxTree *&ast,
              std::initializer_list<
                  Foundation::DictionaryEntry<ASTType, ASTFuncCallback>>
                  list)
      : ast_lookup(list), tree(ast) {
  };
  void travel();
};
} // namespace AST

NAMESPACE_END

#endif