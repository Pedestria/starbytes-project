#include "AST.h"
#include "starbytes/Base/ADT.h"

#ifndef AST_ASTRAVELER_H
#define AST_ASTRAVELER_H

STARBYTES_STD_NAMESPACE

namespace AST {

struct ContexualAction {};
struct ASTVistorResponse {
  bool success;
  ContexualAction * action;
};

struct ASTTravelContext {
  std::optional<ASTNode *> previous;
  ASTNode *current;
  std::optional<ASTNode *> parent;
  std::optional<ASTNode *> next;
};

using ASTFuncCallback = ASTVistorResponse (*)(ASTTravelContext &context);

struct ASTTravelSettings {};

#define AST_TRAVEL_FUNC(name) bool visit##name()

using ASTContextualMemoryBank = Foundation::DictionaryVec<unsigned,ASTNode *>;

class ASTContextualMemory {
  private:
  ASTContextualMemoryBank parent_heirarchy_mem;
  ASTContextualMemoryBank next_node_heirarchy_mem;
  ASTContextualMemoryBank previous_node_heirarchy_mem;
  public:
  enum class MemType:int {
    Parent,NextNode,PreviousNode
  };
  void set_node(unsigned & level,MemType type,ASTNode *& node_ptr){
    if(type == MemType::Parent){
      parent_heirarchy_mem.pushEntry(Foundation::DictionaryEntry<unsigned,ASTNode *>(level,node_ptr));
    }
    else if(type == MemType::NextNode){
      next_node_heirarchy_mem.pushEntry(Foundation::DictionaryEntry<unsigned,ASTNode *>(level,node_ptr));
    }
    else if(type == MemType::PreviousNode){
      previous_node_heirarchy_mem.pushEntry(Foundation::DictionaryEntry<unsigned,ASTNode *>(level,node_ptr));
    }
  };
  ASTNode * get_node(unsigned & level,MemType type){
      ASTNode *n = nullptr;
      if(type == MemType::Parent){
         n = parent_heirarchy_mem.find(level); 
      }
      else if(type == MemType::NextNode){
        n = next_node_heirarchy_mem.find(level);
      }
      else if(type == MemType::PreviousNode){
        n = previous_node_heirarchy_mem.find(level);
      }
      return n;
  };
};

class ASTTraveler {
  Foundation::ImutDictionary<ASTType, ASTFuncCallback,2> ast_lookup;
  ASTTravelSettings options;
  ASTTravelContext cntxt;
  ASTContextualMemory cntxtl_memory;
  //TODO: Contextual Memory!!!
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
      return cb(cntxt).success;
  };

  AST_TRAVEL_FUNC(Statement);
  AST_TRAVEL_FUNC(ExprStmt);

  AST_TRAVEL_FUNC(Expr);
  AST_TRAVEL_FUNC(ArrayExpr);
  AST_TRAVEL_FUNC(CallExpr);
  AST_TRAVEL_FUNC(MemberExpr);
  AST_TRAVEL_FUNC(AwaitExpr);
  AST_TRAVEL_FUNC(NewExpr);
  AST_TRAVEL_FUNC(AssignExpr);
  AST_TRAVEL_FUNC(Id);

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
                  list);
  void travel();
};
} // namespace AST

NAMESPACE_END

#endif