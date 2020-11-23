
#include <type_traits>
#include <typeinfo>
#include <memory>
#include <vector>
#include <iterator>
#include <string>
#include <cassert>
#include <iostream>
#include <cstring>
#include <any>
#include <array>
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
            // std::cout << "Pushing Item!" << std::endl; 
            
            if(isEmpty()) {
                current_len += 1;
                __data = static_cast<T *>(malloc(sizeof(T)));
            }
            else {
                current_len += 1;
                __data = static_cast<T *>(realloc(__data,(current_len) * sizeof(T)));  
            }
            // std::cout << "mem allocated!" << std::endl;
            memcpy(__data + ((current_len-1) * sizeof(T)),&item,sizeof(T));
            // std::cout << "Item Pushed!" << std::endl;
        };

        void push(T && item){
            //  std::cout << "Pushing Item!" << std::endl; 
            
            if(isEmpty()) {
                current_len += 1;
                __data = static_cast<T *>(malloc(sizeof(T)));
            }
            else {
                current_len += 1;
                __data = static_cast<T *>(realloc(__data,(current_len) * sizeof(T)));  
            }
            // std::cout << "mem allocated!" << std::endl;
            memcpy(__data + ((current_len-1) * sizeof(T)),&item,sizeof(T));
            // std::cout << "Item Pushed!" << std::endl;
        };
        T & operator [](unsigned n){
            return begin()[n];
        };

        AdvVector() = default;

        template<class Ty>
        AdvVector<Ty> (std::initializer_list<Ty> ilist){
            _alloc_objs(ilist);
        };
    };
    
    // class SmartVector {
    //     AdvVector<size_t> obj_sizes;
    //     AdvVector<std::type_info> obj_ty_ids;
    //     void * __data;
    //     unsigned current_len = 0;
    //     size_t _sum_obj_sizes(unsigned to_idx){
    //         size_t result = 0;
    //         for(auto i = 0;i < to_idx;++i){
    //             result += obj_sizes[i];
    //         }
    //         return result;
    //     };
    //     public:
    //     SmartVector() = default;
    //     template<class _obj>
    //     void push(_obj & item){
    //         size_t size = sizeof(_obj);
    //         obj_sizes.push(size);
    //         obj_ty_ids.push(typeid(_obj));

    //         if(isEmpty()) {
    //             current_len += 1;
    //             __data = malloc(size);
    //         }
    //         else {
    //             current_len += 1;
    //            __data = realloc(__data,_sum_obj_sizes(current_len));  
    //         }
    //         // std::cout << "mem allocated!" << std::endl;
    //         memcpy(static_cast<std::any *>(__data) + _sum_obj_sizes(current_len - 1),&item,size);
    //     };
    //     template<class _Objtest>
    //     _Objtest & firstEl(){
    //         if(obj_ty_ids[0].hash_code() == typeid(_Objtest).hash_code()){
    //             return *__data;
    //         }
    //     };

    // };
    template<class _key,class __val>
    using DictionaryEntry = std::pair<_key,__val>;

    template<class _Key,class _Val>
    inline DictionaryEntry<_Key,_Val> dict_vec_entry(_Key k,_Val v){
        return DictionaryEntry<_Key,_Val>(k,v);
    };
      
    template<class _Key,class _Val,unsigned len = 0>
    class ImutDictionary {
        using _Entry =  DictionaryEntry<_Key,_Val>;
        std::vector<_Entry> data;
        public:
        ImutDictionary() = delete;
        ImutDictionary(std::initializer_list<_Entry> list):data(list){};
        _Val & find(_Key & key){
            _Val *ptr = nullptr;
            for(auto & ent : data){
                if(ent.first == key){
                    ptr = &ent.second;
                    break;
                }
            }
            if(ptr == nullptr){
                
            }
            return *ptr;
        };
    };

    template<class _Key,class _Val>
    class DictionaryVec {
        using _Entry =  DictionaryEntry<_Key,_Val>;
        std::vector<_Entry> data;
        public:
        void pushEntry(_Entry & entry){
            data.push_back(entry);
        };
        bool hasEntry(_Key & key){
            bool returncode = false;
            for(auto & ent : data){
                if(ent.first == key){
                    returncode = true;
                    break;
                }
            }
            return returncode;
        };
        _Val & find(_Key & key){
            _Val *ptr = nullptr;
            for(auto & ent : data){
                if(ent.first == key){
                    ptr = &ent.second;
                    break;
                }
            }
            if(ptr == nullptr){
                
            }
            return *ptr;
        };
        bool replace(_Key & key,_Val & replacement){
            bool returncode = false;
            for(auto & ent : data){
                if(ent.first == key){
                    returncode = true;
                    //Destroy Val;
                    ent.second.~_Val();
                    ent.second = replacement;
                }
            }
            return returncode;
        };
        void popEntry(_Key & key){
            const size_t len = data.size();
            for(auto idx = 0;idx < len;++idx){
                auto elem = data[idx];
                if(elem.first == key){
                    data.erase(data.begin()+idx);
                    break;
                }
            }
        };
        DictionaryVec<_Key,_Val>() = default;
        
    };
    

NAMESPACE_END


#endif
