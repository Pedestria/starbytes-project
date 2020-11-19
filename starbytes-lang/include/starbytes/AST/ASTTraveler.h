#include "AST.h"
#include "starbytes/Base/ADT.h"

#ifndef AST_ASTRAVELER_H
#define AST_ASTRAVELER_H

STARBYTES_STD_NAMESPACE

namespace AST {

struct ContexualAction {};
struct ASTVistorResponse {
  bool success;
  ContexualAction *action;
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

using ASTContextualMemoryBank = Foundation::DictionaryVec<unsigned, ASTNode *>;

class ASTContextualMemory {
private:
  ASTContextualMemoryBank parent_heirarchy_mem;
  ASTContextualMemoryBank next_node_heirarchy_mem;
  ASTContextualMemoryBank previous_node_heirarchy_mem;
  static bool mem_bank_has_ent(ASTContextualMemoryBank &mem_bank_ref,
                               unsigned &key) {
    return mem_bank_ref.hasEntry(key);
  };
  static void
  push_ent_to_vec(ASTContextualMemoryBank &mem_bank_ref,
                  Foundation::DictionaryEntry<unsigned, ASTNode *> &ent) {
    if (mem_bank_has_ent(mem_bank_ref, ent.first)) {
      mem_bank_ref.replace(ent.first, ent.second);
    } else {
      mem_bank_ref.pushEntry(ent);
    }
  };

public:
  enum class MemType : int { Parent, NextNode, PreviousNode };
  void set_node(unsigned &level, MemType type, ASTNode *&node_ptr) {
    Foundation::DictionaryEntry<unsigned, ASTNode *> ENT(level, node_ptr);
    if (type == MemType::Parent) {
      push_ent_to_vec(parent_heirarchy_mem, ENT);
    } else if (type == MemType::NextNode) {
      push_ent_to_vec(next_node_heirarchy_mem, ENT);
    } else if (type == MemType::PreviousNode) {
      push_ent_to_vec(previous_node_heirarchy_mem, ENT);
    }
  };
  ASTNode *get_node(unsigned &level, MemType type) {
    ASTNode *n = nullptr;
    if (type == MemType::Parent) {
      n = parent_heirarchy_mem.find(level);
    } else if (type == MemType::NextNode) {
      n = next_node_heirarchy_mem.find(level);
    } else if (type == MemType::PreviousNode) {
      n = previous_node_heirarchy_mem.find(level);
    }
    return n;
  };
};

using ASTContextualActionCallback = void (*)(ContexualAction * action);

class ASTTraveler {
  Foundation::ImutDictionary<ASTType, ASTFuncCallback, 2> ast_lookup;
  ASTTravelSettings options;
  ASTTravelContext cntxt;
  ASTContextualMemory cntxtl_memory;
  unsigned current_level = 0;

  AbstractSyntaxTree *&tree;
  /*
    Callback called when Contexual Is NOT a nullptr 
    (When an action must be taken! or when the visit to each node returns {success = false})
  */
  ASTContextualActionCallback action_callback;
  void moveDown(){
    ++current_level;
  };
  void moveUp(){
    --current_level;
  };
  void setParentNode(ASTNode *node_ptr) {
    cntxtl_memory.set_node(current_level,ASTContextualMemory::MemType::Parent,cntxt.parent.value());
    cntxt.parent = node_ptr; 
  };
  void recoverPriorParentNode(){
    cntxt.parent = cntxtl_memory.get_node(current_level,ASTContextualMemory::MemType::Parent);
  };
  void setPreviousNode(ASTNode * node_ptr,bool store_in_mem = false) {
    if(store_in_mem && cntxt.previous.has_value()){
      cntxtl_memory.set_node(current_level,ASTContextualMemory::MemType::PreviousNode,cntxt.previous.value());
    }
    cntxt.previous = node_ptr;
  };
  void recoverPriorPreviousNode(){
    cntxt.previous = cntxtl_memory.get_node(current_level,ASTContextualMemory::MemType::PreviousNode);
  };
  void setCurrentNode(ASTNode *node_ptr) { cntxt.current = node_ptr; };
  void setNextNode(ASTNode *node_ptr,bool store_in_mem = false) { 
    if(store_in_mem && cntxt.next.has_value()){
      cntxtl_memory.set_node(current_level,ASTContextualMemory::MemType::NextNode,cntxt.next.value());
    }
    cntxt.next = node_ptr; 
  };
  void recoverPriorNextNode(){
    cntxt.next = cntxtl_memory.get_node(current_level,ASTContextualMemory::MemType::NextNode);
  };
  void nextNodeAndSetAheadNode(ASTNode *node_ptr = nullptr,bool remember_next_and_previous_node = false) {
    setPreviousNode(cntxt.current,remember_next_and_previous_node);
    if (cntxt.next.has_value())
      setCurrentNode(cntxt.next.value());
    else
      return;
    if(node_ptr == nullptr)
      recoverPriorNextNode();
    setNextNode(node_ptr,remember_next_and_previous_node);
  };
  bool invokeCallback(ASTType &t) {
    ASTFuncCallback &cb = ast_lookup.find(t);
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
  ASTTraveler(const ASTTraveler &) = delete;
  ASTTraveler(ASTTraveler &&) = delete;
  ASTTraveler(AbstractSyntaxTree *&ast,
              std::initializer_list<
                  Foundation::DictionaryEntry<ASTType, ASTFuncCallback>>
                  list);
  void travel();
};
} // namespace AST

NAMESPACE_END

#endif