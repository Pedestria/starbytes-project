#include "AST/AST.h"
#include "Base/ADT.h"
#include <initializer_list>



// namespace Starbytes{
// namespace AST {


// template<typename _Node,ASTType type>
// inline void WalkNode(_Node *node,std::initializer_list<ASTVisitorEntry> &cbs){
//     WalkPath<_Node> * path = new WalkPath<_Node>;
//     for(auto visitor : cbs){
//         if(visitor.type == type){
//             void (*visitor_callback)(WalkPath<_Node> *) = reinterpret_cast<void (*)(WalkPath<_Node> *)>(visitor.callback);
//             visitor_callback(path);
//         }
//     }
// };





// // template<>
// // void WalkNode <ASTVariableDeclaration,ASTType::VariableDeclaration>(ASTVariableDeclaration *node,std::initializer_list<ASTCallbackEntry> &cbs){

// // }

// void WalkStatement(ASTStatement *node,std::initializer_list<ASTVisitorEntry> &cbs){
//     if(node->type == ASTType::ClassDeclaration){
//         WalkNode<ASTClassDeclaration,ASTType::ClassDeclaration>((ASTClassDeclaration *)node,cbs);
//     }
//     else if(node->type == ASTType::ExpressionStatement){
//         switch(((ASTExpressionStatement *)node)->type){
//         case ASTType::NumericLiteral :
//             WalkNode<>
//             break;
//         default:
//             break;
//         }
//     }
    
// }

// void WalkAST(AbstractSyntaxTree *tree, std::initializer_list<ASTVisitorEntry> callbacks){

//     for(auto node : tree->nodes){
//         WalkStatement(node,callbacks);
//     }

// }


// };
// }

