
#include <type_traits>
#include <typeinfo>
#include <memory>
#include <vector>
#include <iterator>
#include <string>
#include <cassert>
#include <iostream>
#include <cstring>
#include "Macros.h"

#ifndef BASE_ADT_H
#define BASE_ADT_H


STARBYTES_FOUNDATION_NAMESPACE
    template<class _Obj>
    class AdvVector_Iterator {
        private:
        size_t obj_size;
        _Obj * ptr;
        public:
        AdvVector_Iterator(_Obj *& _data_ptr):ptr(_data_ptr){
            obj_size = sizeof(_Obj);
        };
        AdvVector_Iterator & operator ++(){
            ptr += (1 * obj_size);
            return *this;
        };
        AdvVector_Iterator & operator --(){
            ptr -= (1 * obj_size);
            return *this;
        };
        AdvVector_Iterator & operator +=(int n){
            ptr += (n * obj_size);
            return *this;
        };
        bool operator==(const AdvVector_Iterator<_Obj> & it_ref){
            return ptr == it_ref.ptr;
        };
        bool operator!=(const AdvVector_Iterator<_Obj> & it_ref){
            std::cout << "Self_ptr:" << ptr << std::endl;
            std::cout << "Other_ptr:" << it_ref.ptr << std::endl;
            return ptr != it_ref.ptr;
        };
        _Obj & operator * (){
            return *ptr;
        };
        _Obj & operator[](int n){
            operator+=(n);
            return *ptr;
        };
        
    };
    template<class T>
    class AdvVector {
        private:
        T * __data;
        unsigned current_len = 0;
        template<class _Cont>
        inline void _alloc_objs(_Cont & container){
            typename _Cont::iterator it = container.begin();
            __data = malloc(sizeof(T) * container.size());
            int index = 0;
            while(it != container.end()){
                memcpy(__data + (index * sizeof(T)),it,sizeof(T));
                ++it;
                ++index;
            }
            current_len = container.size();
        };
        public:
        using iterator = AdvVector_Iterator<T>;
        using l_value_ref = T &;

        iterator begin(){
            return iterator(__data);
        };
        iterator end(){
            return (iterator(__data) += current_len);
        };

        l_value_ref firstEl(){
            return begin()[0];
        };
        l_value_ref lastEl(){
            return end()[-1];
        };
        __NO_DISCARD bool isEmpty(){
            return current_len == 0;
        };
        const T * data(){
            return __data;
        };
        __NO_DISCARD unsigned size(){
            return current_len;
        };

        

        void push(const T & item){
            std::cout << "Pushing Item!" << std::endl; 
            
            if(isEmpty()) {
                current_len += 1;
                __data = static_cast<T *>(malloc(sizeof(T)));
            }
            else {
                current_len += 1;
                __data = static_cast<T *>(realloc(__data,(current_len) * sizeof(T)));  
            }
            std::cout << "mem allocated!" << std::endl;
            memcpy(__data + ((current_len-1) * sizeof(T)),&item,sizeof(T));
            std::cout << "Item Pushed!" << std::endl;
        };

        // void push(T && item){

        // };

        AdvVector() = default;

        template<class Ty>
        AdvVector<Ty> (std::initializer_list<Ty> ilist){
            _alloc_objs(ilist);
        };
    };

    template<typename _Key,typename _Value>
    class OrderedMap {
        private:
            std::vector<std::pair<_Key,_Value>> map;
        public:
            OrderedMap(std::initializer_list<std::pair<_Key,_Value>> ilist): map(ilist){}
            ~OrderedMap(){};
            void set(std::pair<_Key,_Value> & val){
                map.push_back(val);
            }
            const _Value & get(const _Key & k){
                for(std::pair<_Key,_Value> p : map){
                    if(p.first == k){
                        return p.second;
                    }
                }
                return nullptr;
            }
             void remove(const _Key & k){
                for(int i = 0;i < map.size();++i){
                    if(map[i].first == k){
                        map.erase(map.begin()+i);
                        break;
                    }
                }
            }
            
    };

NAMESPACE_END


#endif
