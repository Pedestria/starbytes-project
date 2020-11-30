#include "ADT.h"

#ifndef BASE_GRAPH_H
#define BASE_GRAPH_H

STARBYTES_FOUNDATION_NAMESPACE

// template<class _Ty>
// class GraphNode {
//     _Ty data;
//     bool visited = false;
//     public:
//     GraphNode() = delete;
//     GraphNode(const GraphNode<_Ty> &) = delete;
//     GraphNode(_Ty val):data(val){};
// };

// template<class _NodeTy,typename _IdTy>
// class Graph_Iterator {
//     DictionaryVec<_IdTy,GraphNode<_NodeTy>> & nodes;
//     DictionaryVec<_IdTy,TinyVector<_IdTy>> & links;
//     _IdTy & start_id;
//     _IdTy current_id_node;
//     typename TinyVector<_IdTy>::iterator current_link_list_it;
//     public:
//     _NodeTy & operator *(){
//         return nodes.find(start_id);
//     };
//     Graph_Iterator & operator ++(){
//         operator+=(1);
//     };
//     Graph_Iterator & operator --(){
//         operator-=(1);
//     };
//     Graph_Iterator & operator +=(unsigned n){
//         TinyVector<_IdTy> _node_links_list = links.find(current_id_node);
//         for(auto c = 0;c < n;c++){
//             if(current_link_list_it == _node_links_list.end()){
                
//             }
//             else {
                
//             };
//         }
//     };
//     Graph_Iterator & operator -=(unsigned n){
        
//     };
//     Graph_Iterator() = delete;
//     Graph_Iterator(const Graph_Iterator<_NodeTy,_IdTy> &) = delete;
//     Graph_Iterator(DictionaryVec<_IdTy,GraphNode<_NodeTy>> & _nodes,_IdTy & id,DictionaryVec<_IdTy,TinyVector<_IdTy>> & _links):nodes(_nodes),links(_links),start_id(id){};
// };

// template<class _NodeTy,typename _IdTy = unsigned>
// class Graph {
//     _IdTy start_id;
//     DictionaryVec<_IdTy,GraphNode<_NodeTy>> nodes;
//     DictionaryVec<_IdTy,TinyVector<_IdTy>> links;
//     public:
//     using iterator = Graph_Iterator<_NodeTy,_IdTy>;
//     using reverse_iterator = std::reverse_iterator<iterator>;
//     iterator begin(){
//         return iterator(nodes,start_id);
//     };
//     iterator end(){

//     };
// };

NAMESPACE_END

#endif