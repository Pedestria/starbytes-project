#include "AST.h"

#include <llvm/ADT/MapVector.h>
#include <llvm/ADT/ImmutableMap.h>
#include <llvm/Support/ErrorOr.h>
#include <llvm/Support/Casting.h>

#ifndef AST_ASTRAVELER_H
#define AST_ASTRAVELER_H

STARBYTES_STD_NAMESPACE

using namespace llvm;

namespace AST {

TYPED_ENUM CntxtActionTy : int{Diagnostic, Hover, Completion};

struct ContextualAction {
  CntxtActionTy type;
};
struct ASTVisitorResponse {
  bool success;
  ContextualAction *action;
};

struct ASTTravelContext {
  std::optional<ASTNode *> previous;
  ASTNode *current;
  std::optional<ASTNode *> parent;
  std::optional<ASTNode *> next;
};
template<typename _ParentTy>
using ASTFuncCallback = ASTVisitorResponse (*)(ASTTravelContext &context,_ParentTy *);

struct ASTTravelSettings {};

#define AST_TRAVEL_FUNC(name) void visit##name()

using ASTContextualMemoryBank = MapVector<unsigned, ASTNode *>;

class ASTContextualMemory {
private:
  ASTContextualMemoryBank parent_heirarchy_mem;
  ASTContextualMemoryBank next_node_heirarchy_mem;
  ASTContextualMemoryBank previous_node_heirarchy_mem;
  bool mem_bank_has_ent(ASTContextualMemoryBank &mem_bank_ref, unsigned &key) {
    return mem_bank_ref.find(key) != mem_bank_ref.end();
  };
  void push_ent_to_vec(ASTContextualMemoryBank &mem_bank_ref,
                       std::pair<unsigned, ASTNode *> &ent) {
    if (mem_bank_has_ent(mem_bank_ref, ent.first)) {
      auto found = mem_bank_ref.find(ent.first);
      if(found != mem_bank_ref.end()){
        auto & res = *found;
        res.second = ent.second;
      };
    } else {
      mem_bank_ref.insert(ent);
    }
  };

public:
  TYPED_ENUM MemType : int{Parent, NextNode, PreviousNode};
  void set_node(unsigned &level, MemType type, ASTNode *&node_ptr) {
    std::pair<unsigned, ASTNode *> ENT(level, node_ptr);
    if (type == MemType::Parent) {
      push_ent_to_vec(parent_heirarchy_mem, ENT);
    } else if (type == MemType::NextNode) {
      push_ent_to_vec(next_node_heirarchy_mem, ENT);
    } else if (type == MemType::PreviousNode) {
      push_ent_to_vec(previous_node_heirarchy_mem, ENT);
    }
  };
  ASTNode * get_node(unsigned &level, MemType type) {
    ASTNode *possibleResult;
    if (type == MemType::Parent) {
      auto node = parent_heirarchy_mem.find(level);
      if(node != parent_heirarchy_mem.end()){
        possibleResult = node->second;
      }
      else {
        possibleResult = nullptr;
      }
      
    } else if (type == MemType::NextNode) {
      auto node = next_node_heirarchy_mem.find(level);
      if(node != next_node_heirarchy_mem.end()){
        possibleResult = node->second;
      }
      else 
       possibleResult = nullptr;
    } else if (type == MemType::PreviousNode) {
      auto node = previous_node_heirarchy_mem.find(level);
      if(node != previous_node_heirarchy_mem.end()){
        possibleResult = node->second;
      }
      else 
       possibleResult = nullptr;
    }
    return possibleResult;
  };
};
using ASTContextualActionCallback = void (*)(ContextualAction *action);
template<class _ParentTy>
using ASTTravelerCallbackList = std::initializer_list<std::pair<ASTType,ASTFuncCallback<_ParentTy>>>;
template <class _ParentTy> 
class ASTTraveler {
  using ASTLookup = ImmutableMap<ASTType, ASTFuncCallback<_ParentTy>>;
  ASTLookup ast_lookup;
  ASTTravelSettings options;
  ASTTravelContext cntxt;
  ASTContextualMemory cntxtl_memory;
  unsigned current_level = 0;
  AbstractSyntaxTree *tree;
  /*
    Pointer to `ContextualAction` struct given by `ASTVisitorResponse`
  */
  ContextualAction *action_to_take;
  /*
    Callback called when Contextual Action Is NOT a nullptr
    (When an action must be taken! or when the visit to each node returns
    {success = false})
  */
  ASTContextualActionCallback action_callback;

  _ParentTy *parent_ptr;

  void moveDown() { ++current_level; };
  void moveUp() { --current_level; };
  void setParentNode(ASTNode *node_ptr) {
    cntxtl_memory.set_node(current_level, ASTContextualMemory::MemType::Parent,
                           cntxt.parent.value());
    cntxt.parent = node_ptr;
  };
  void recoverPriorParentNode() {
    auto res = cntxtl_memory.get_node(current_level,
                                          ASTContextualMemory::MemType::Parent);
    if(res == nullptr){
      //TODO: Log Error!
    }
    else {
      cntxt.parent = res;
    };
  };
  void setPreviousNode(ASTNode *node_ptr, bool store_in_mem = false) {
    if (store_in_mem && cntxt.previous.has_value()) {
      cntxtl_memory.set_node(current_level,
                             ASTContextualMemory::MemType::PreviousNode,
                             cntxt.previous.value());
    }
    cntxt.previous = node_ptr;
  };
  void recoverPriorPreviousNode() {
    auto res = cntxtl_memory.get_node(
        current_level, ASTContextualMemory::MemType::PreviousNode);
    if(res == nullptr){
      //TODO: Log Error!
    }
    else {
      cntxt.previous = res;
    }
  };
  void setCurrentNode(ASTNode *node_ptr) { cntxt.current = node_ptr; };
  void setNextNode(ASTNode *node_ptr, bool store_in_mem = false) {
    if (store_in_mem && cntxt.next.has_value()) {
      cntxtl_memory.set_node(current_level,
                             ASTContextualMemory::MemType::NextNode,
                             cntxt.next.value());
    }
    cntxt.next = node_ptr;
  };
  void recoverPriorNextNode() {
    auto res = cntxtl_memory.get_node(current_level,
                                        ASTContextualMemory::MemType::NextNode);
    if(res == nullptr){
      //TODO: Log Error!
    }
    else {
      cntxt.next = res;
    };
  };
  void nextNodeAndSetAheadNode(ASTNode *node_ptr = nullptr,
                               bool remember_next_and_previous_node = false) {
    setPreviousNode(cntxt.current, remember_next_and_previous_node);
    if (cntxt.next.has_value())
      setCurrentNode(cntxt.next.value());
    else
      return;
    if (node_ptr == nullptr)
      recoverPriorNextNode();
    setNextNode(node_ptr, remember_next_and_previous_node);
  };
  bool invokeCallback(ASTType &t) {
    //If callback entry for ASTType `t` has not been put in, then skip!
    if(ast_lookup.contains(t)){
      std::cout << "Invoking Callback" << std::endl;
      auto cb_res = ast_lookup.lookup(t);
      if(cb_res.hasError()){
        return true;
        //TODO: Skip! No Error To Return Other than "Callback Not FOund!"
      }
      else {
        ASTFuncCallback<_ParentTy> & cb = cb_res.getResult();
        ASTVisitorResponse response = cb(cntxt,parent_ptr);
        if (!response.success)
          action_to_take = response.action;
        else
          action_to_take = nullptr;
        return response.success;
      };
    }
    else {
      return true;
    };
    
  };

  void takeAction() { action_callback(action_to_take); };

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
  // ASTTraveler(const ASTTraveler &) = delete;
  // ASTTraveler(ASTTraveler &&) = delete;
  // void operator=(ASTTraveler &) = delete;
  ASTTraveler(_ParentTy *ptr_to_parent,
              ASTTravelerCallbackList<_ParentTy>
                   & list)
      : parent_ptr(ptr_to_parent){
        typename ASTLookup::Factory factory;

        auto it = list.begin();
        ast_lookup = factory.getEmptyMap();
        while(it != list.end()){
          factory.add(ast_lookup,*(it).first,*(it).second);
          ++it;
        }
        
      };
  void travel(AbstractSyntaxTree *__tree);
};


template <class _ParentTy> void ASTTraveler<_ParentTy>::visitImportDecl() {
  // bool retr_code = true;
  ASTImportDeclaration *_current =
      ASSERT_AST_NODE(cntxt.current, ASTImportDeclaration);
  if (!invokeCallback(_current->type))
    takeAction();
  // retr_code = false;
  // return retr_code;
};
template <class _ParentTy> void ASTTraveler<_ParentTy>::visitScopeDecl() {
  // bool retr_code = true;
  ASTScopeDeclaration *_current =
      ASSERT_AST_NODE(cntxt.current, ASTScopeDeclaration);
  if (invokeCallback(_current->type)) {
    setParentNode(ASSERT_AST_NODE(_current, ASTNode));
    moveDown();
    auto it = _current->body->nodes.begin();
    setNextNode(ASSERT_AST_NODE(_current->body->nodes[0], ASTNode));
    while (it != _current->body->nodes.end()) {
      // TODO: Ensure next node is to next node and NOT nullptr!
      nextNodeAndSetAheadNode(ASSERT_AST_NODE(*(it + 1), ASTNode), true);

      visitStatement();
      ++it;
    };
    moveUp();
    recoverPriorParentNode();
  } else {
    // retr_code = false;
    takeAction();
  }
  // return retr_code;
};

template <class _ParentTy> void ASTTraveler<_ParentTy>::visitVariableDecl() {
  // bool retr_code = true;
  ASTVariableDeclaration *_current =
      ASSERT_AST_NODE(cntxt.current, ASTVariableDeclaration);
      std::cout << "Visit Variable Decl!" << std::endl;
  if(!invokeCallback(_current->type))
    takeAction();
  //  retr_code = false;
  // setParentNode(nullptr);
  // return retr_code;
};
template <class _ParentTy> void ASTTraveler<_ParentTy>::visitConstantDecl() {
  // bool retr_code = true;
  ASTConstantDeclaration *_current =
      ASSERT_AST_NODE(cntxt.current, ASTConstantDeclaration);
  // if(invokeCallback(_current->type))
  if (true) {
    setParentNode(ASSERT_AST_NODE(_current, ASTNode));
    moveDown();
    auto it = _current->specifiers.begin();
    setNextNode(ASSERT_AST_NODE(_current->specifiers[0], ASTNode));
    while (it != _current->specifiers.end()) {
      // TODO: Ensure next node is to next node and NOT nullptr!
      nextNodeAndSetAheadNode(ASSERT_AST_NODE(*(it + 1), ASTNode));
      if (invokeCallback((*it)->type))
        ++it;
      else {
        takeAction();
      }
    }
    moveUp();
    recoverPriorParentNode();
  } else
    takeAction();
  // setParentNode(nullptr);
  // return retr_code;
};
// TODO : Finish Implementation of this function!
template <class _ParentTy> void ASTTraveler<_ParentTy>::visitFunctionDecl() {
  // bool retr_code = true;
  ASTFunctionDeclaration *_current =
      ASSERT_AST_NODE(cntxt.current, ASTFunctionDeclaration);
  if (invokeCallback(_current->type)) {
    // TODO: Possibly visit each parameter!
    setParentNode(ASSERT_AST_NODE(_current, ASTNode));
    moveDown();
    auto it = _current->body->nodes.begin();
    setNextNode(ASSERT_AST_NODE(_current->body->nodes[0], ASTNode));
    while (it != _current->body->nodes.end()) {
      // TODO: Ensure next node is to next node and NOT nullptr!
      nextNodeAndSetAheadNode(ASSERT_AST_NODE(*(it + 1), ASTNode));
      if (invokeCallback((*it)->type)) {
        ++it;
      } else {
        takeAction();
      }
    };
    moveUp();
    recoverPriorParentNode();
  } else
    takeAction();
  //     retr_code = false;
  // return retr_code;
};
// TODO : Finish Implementation of this function!
template <class _ParentTy> void ASTTraveler<_ParentTy>::visitClassDecl() {
  // bool retr_code = true;
  ASTClassDeclaration *_current =
      ASSERT_AST_NODE(cntxt.current, ASTClassDeclaration);
  if (invokeCallback(_current->type)) {
    setParentNode(ASSERT_AST_NODE(_current, ASTNode));
    auto it = _current->body->nodes.begin();
    setNextNode(ASSERT_AST_NODE(_current->body->nodes[0], ASTNode));
    while (it != _current->body->nodes.end()) {
      // TODO: Ensure next node is to next node and NOT nullptr!
      nextNodeAndSetAheadNode(ASSERT_AST_NODE(*(it + 1), ASTNode));
      if (invokeCallback((*it)->type)) {
        ++it;
      } else {
        takeAction();
      }
    };
  } else
    takeAction();
  //     retr_code = false;
  // return retr_code;
};
template <class _ParentTy> void ASTTraveler<_ParentTy>::visitId() {
  ASTIdentifier *_current = ASSERT_AST_NODE(cntxt.current, ASTIdentifier);
  if (!invokeCallback(_current->type))
    takeAction();
};
template <class _ParentTy> void ASTTraveler<_ParentTy>::visitArrayExpr() {
  // bool retr_code = true;
  ASTArrayExpression *_current =
      ASSERT_AST_NODE(cntxt.current, ASTArrayExpression);
  if (invokeCallback(_current->type)) {
    setParentNode(ASSERT_AST_NODE(_current, ASTNode));
    auto it = _current->items.begin();
    setNextNode(ASSERT_AST_NODE(_current->items[0], ASTNode));
    while (it != _current->items.end()) {
      // TODO: Ensure next node is to next node and NOT nullptr!
      nextNodeAndSetAheadNode(ASSERT_AST_NODE(*(it + 1), ASTNode));
      if (invokeCallback((*it)->type)) {
        ++it;
      } else {
        takeAction();
      }
    };
  } else
    takeAction();
};
template <class _ParentTy> void ASTTraveler<_ParentTy>::visitNewExpr() {
  ASTNewExpression *_current = ASSERT_AST_NODE(cntxt.current, ASTNewExpression);
  if (!invokeCallback(_current->type))
    takeAction();
};
template <class _ParentTy> void ASTTraveler<_ParentTy>::visitCallExpr() {
  bool retr_code = true;
  ASTCallExpression *_current =
      ASSERT_AST_NODE(cntxt.current, ASTCallExpression);
  setParentNode(ASSERT_AST_NODE(_current, ASTNode));
  setCurrentNode(ASSERT_AST_NODE(_current->callee, ASTNode));

  visitExpr();

  auto it = _current->params.begin();
  setCurrentNode(ASSERT_AST_NODE(_current->params[0], ASTNode));
  while (it != _current->params.end()) {
    nextNodeAndSetAheadNode(ASSERT_AST_NODE(*(it + 1), ASTNode));
    if (invokeCallback((*it)->type)) {
      ++it;
    } else {
      takeAction();
    }
  };
};
template <class _ParentTy> void ASTTraveler<_ParentTy>::visitMemberExpr(){

};
template <class _ParentTy> void ASTTraveler<_ParentTy>::visitAwaitExpr(){
  
};
template <class _ParentTy> void ASTTraveler<_ParentTy>::visitExpr() {
  ASTNode *&_current = cntxt.current;
  // bool retr_code = false;
  if (AST_NODE_IS(_current, ASTCallExpression)) {
    visitCallExpr();
  } else if (AST_NODE_IS(_current, ASTIdentifier)) {
    visitId();
  } else if (AST_NODE_IS(_current, ASTMemberExpression)) {
    visitMemberExpr();
  } else if (AST_NODE_IS(_current, ASTArrayExpression)) {
    visitArrayExpr();
  } else if (AST_NODE_IS(_current, ASTNewExpression)) {
    visitNewExpr();
  } else if (AST_NODE_IS(_current, ASTAwaitExpression)) {
    visitAwaitExpr();
  }

  // return retr_code;
};
template <class _ParentTy> void ASTTraveler<_ParentTy>::visitExprStmt() {
  ASTExpressionStatement *_current =
      ASSERT_AST_NODE(cntxt.current, ASTExpressionStatement);
  setCurrentNode(ASSERT_AST_NODE(_current->expression, ASTNode));
  visitExpr();
};
template <class _ParentTy> void ASTTraveler<_ParentTy>::visitStatement() {
  ASTNode *&_current = cntxt.current;
  // bool retr_code = false;
  if (llvm::isa<ASTFunctionDeclaration>(_current)) {
    visitFunctionDecl();
  } else if (llvm::isa<ASTVariableDeclaration>(_current)) {
    visitVariableDecl();
  } else if (llvm::isa<ASTConstantDeclaration>(_current)) {
    visitConstantDecl();
  } else if (AST_NODE_IS(_current, ASTClassDeclaration)) {
    visitClassDecl();
  } else if (AST_NODE_IS(_current, ASTScopeDeclaration)) {
    visitScopeDecl();
  } else if (AST_NODE_IS(_current, ASTImportDeclaration)) {
    visitImportDecl();
  } else if (AST_NODE_IS(_current, ASTExpressionStatement)) {
    visitExprStmt();
  }
  // return retr_code;
};
template <class _ParentTy>
void ASTTraveler<_ParentTy>::travel(AbstractSyntaxTree *__tree) {
  tree = __tree;
  unsigned ast_top_level_len = tree->nodes.size();
  unsigned idx = 0;
  while (ast_top_level_len != idx) {
    if (idx == 0) {
      ASTStatement *&node = tree->nodes[idx];
      setCurrentNode(ASSERT_AST_NODE(node, ASTNode));
      if(ast_top_level_len != 1)
        setNextNode(ASSERT_AST_NODE(tree->nodes[idx + 1], ASTNode));
      else
        setNextNode(nullptr);
    } 
    else if (ast_top_level_len == (idx + 1)) {
        nextNodeAndSetAheadNode();
        //Escape!
    }
    else 
        nextNodeAndSetAheadNode(ASSERT_AST_NODE(tree->nodes[idx + 1], ASTNode));

    visitStatement();
    ++idx;
  }
};
}; // namespace AST

NAMESPACE_END

#endif