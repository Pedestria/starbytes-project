
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
    class Tiny_Vector_Iterator {
        private:
        size_t obj_size;
        _Obj * ptr;
        public:
        Tiny_Vector_Iterator(_Obj *& _data_ptr):ptr(_data_ptr){
            obj_size = sizeof(_Obj);
        };
        Tiny_Vector_Iterator & operator ++(){
            ptr += (1 * obj_size);
            return *this;
        };
        Tiny_Vector_Iterator & operator --(){
            ptr -= (1 * obj_size);
            return *this;
        };
        Tiny_Vector_Iterator & operator +=(int n){
            ptr += (n * obj_size);
            return *this;
        };
        bool operator==(const Tiny_Vector_Iterator<_Obj> & it_ref){
            return ptr == it_ref.ptr;
        };
        bool operator!=(const Tiny_Vector_Iterator<_Obj> & it_ref){
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
    class TinyVector {
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
        using iterator = Tiny_Vector_Iterator<T>;
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

        TinyVector() = default;

        template<class Ty>
        TinyVector<Ty> (std::initializer_list<Ty> ilist){
            _alloc_objs(ilist);
        };
    };
    template<class T>
    class UniqVector {
        TinyVector<T> data;
        bool hasVal(T & val){
            bool rc = false;
            for(auto i : data){
                if(i == val){
                    rc = true;
                    break;
                }
            }
            return rc;
        };
        public:
        UniqVector() = default;
        void add(const T &item){
            if(!hasVal(item))
                data.push(item);
        };
        void add(T && item){
            if(!hasVal(item))
                data.push(item);
        };
    };
    
    
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
    //Tiny string reference class!
    class RString {
        private:
            char * __data;
            unsigned len;
        public:
        using size_type = unsigned;
        using iterator = char *;
        using const_iterator = const char *;
        using reverse_iterator = std::reverse_iterator<iterator>;
        iterator begin(){
            return __data;
        };
        iterator end(){
            return __data + (len * sizeof(char));
        };
        char & operator[](unsigned idx){
            return *(begin() + (idx * sizeof(char)));
        };
        void copyToBuffer(std::string & s){
            std::copy(this->begin(),this->end(),s.begin());
        };
        void copyToBuffer(RString & r_s){
            std::copy(this->begin(),this->end(),r_s.begin());
        };
        size_type & size(){
            return len;
        };
        char *& data(){
            return __data;
        };
        RString() = delete;
        RString(char *& r_str):__data(r_str),len(strlen(r_str)){
          
        };
        RString(std::string & str):__data(str.data()),len(str.size()){

        };
    };
    

NAMESPACE_END


#endif
