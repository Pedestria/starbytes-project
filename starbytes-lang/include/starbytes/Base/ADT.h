
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
#include "Unsafe.h"

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
        void _alloc_objs(std::initializer_list<T> & container){
             auto it = container.begin();
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
        using reference = T &;

        iterator begin(){
            return iterator(__data);
        };
        iterator end(){
            return (iterator(__data) += current_len);
        };

        reference firstEl(){
            return begin()[0];
        };
        reference lastEl(){
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

        TinyVector(){};
        TinyVector (std::initializer_list<T> ilist){
            _alloc_objs(ilist);
        };
    };

    template<class _Ty>
    class ArrRef {
        _Ty * __data;
    public:
        using size_ty = unsigned;
    private:
        size_ty len;
    public:
        using iterator = _Ty *;
        using reference = _Ty &;
        iterator begin(){
            return iterator(__data);
        };
        iterator end(){
            return iterator(__data) + len * sizeof(_Ty);
        };
        const size_ty & size(){
            return len;
        };
        reference firstEl(){
            return begin()[0];
        };
        reference lastEl(){
            return end()[-1];
        };
        reference operator[](size_ty idx){
            return begin()[idx];
        };
        ArrRef() = delete;
        ArrRef(std::vector<_Ty> &vec):__data(vec.data()),len(vec.size()){};
        template<size_t _len>
        ArrRef(std::array<_Ty,_len> &arr):__data(arr.data()),len(_len){};
        std::vector<_Ty> toVec(){
            std::vector<_Ty> vec;
            vec.reserve(len);
            std::copy(begin(),end(),vec.begin());
            return std::move(vec);
        };
    };

    template<class _Ty>
    class TinyQueue {
        _Ty * __data;
    public:
        using size_ty = unsigned;
        using pointer = _Ty *;
    private:
        size_ty len = 0;
        inline pointer get_last_ptr(){
            return __data + ((len-1) * sizeof(_Ty));
        };
    public:
        using reference = _Ty &;
        bool isEmpty(){
            return len == 0;
        };
        const size_ty & size(){
            return len;
        };
        reference first(){
            return *__data;
        };
        reference last(){
            return *get_last_ptr();
        };
    private:
        void _push_el(const _Ty & el){
            if(!isEmpty()){
                ++len;
                __data = (_Ty *)malloc(sizeof(_Ty));
            }
            else {
                ++len;
                __data = (_Ty*)realloc(__data,sizeof(_Ty) * len);
            };

            memcpy(get_last_ptr(),&el, sizeof(_Ty));
        };
    public:
        void push(const _Ty & el){
            _push_el(el);
        };
        void push(_Ty && el){
            _push_el(el);
        };
        void pop(){
            __data->~_Ty();
            //First element is deleted!
            pointer end = __data + (sizeof(_Ty) * len);
            pointer begin = __data + sizeof(_Ty);

            if(len == 1){
                --len;
                return;
            };
            //Move all elements to front!
            while(begin != end){
                memmove(begin - sizeof(_Ty),begin,sizeof(_Ty));
                begin += sizeof(_Ty);
            };
            --len;
            __data = realloc(__data,sizeof(_Ty) * len);
        };
        TinyQueue(){};
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
        Unsafe<_Val> find(_Key & key){
            _Val *ptr = nullptr;
            for(auto & ent : data){
                if(ent.first == key){
                    ptr = &ent.second;
                    break;
                }
            }
            if(ptr == nullptr){
                return Error("Cannot find val with given key!");
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
        Unsafe<_Val> find(_Key & key){
            _Val *ptr = nullptr;
            for(auto & ent : data){
                if(ent.first == key){
                    ptr = &ent.second;
                    break;
                }
            }
            if(ptr == nullptr){
                return Error("Cannpt find key!");
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
    /// String reference class!
    class StrRef{
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
        bool isSame(char * _o_data) {
            if(size() != strlen(_o_data))
                return false;
            bool res = true;
            auto c = begin();
            while(c != end()){
                if(*c != *_o_data){
                    res = false;
                    break;
                };
                ++c;
                ++_o_data;
            };
            return res;
        };
        bool operator==(const char *ptr){
            return isSame(const_cast<char *>(ptr));
        };
        bool operator==(const std::string str){
            return isSame(const_cast<char *>(str.data()));
        };
        bool operator==(std::string & str){
            return isSame(const_cast<char *>(str.data()));
        };
        void copyToBuffer(std::string & s){
            std::copy(this->begin(),this->end(),s.begin());
        };
        void copyToBuffer(StrRef & r_s){
            std::copy(this->begin(),this->end(),r_s.begin());
        };
        size_type & size(){
            return len;
        };
        char *& data(){
            return __data;
        };
        StrRef() = delete;
        StrRef(char *& r_str):__data(r_str),len(strlen(r_str)){
          
        };
        StrRef(std::string & str):__data(str.data()),len(str.size()){

        };
        std::string toStr(){
            std::string str;
            str.reserve(len);
            copyToBuffer(str);
            return std::move(str);
        };
    };

    template<class _Ty>
    class Stack {
        _Ty *__data;
    public:
        using size_ty = unsigned;
        using pointer = _Ty *;
    private:
        size_ty len = 0;
        inline pointer get_last_ptr(){
            return (__data) * (sizeof(_Ty) * (len-1));
        };  
    public:
        using reference = _Ty &;
        const size_ty & size(){
            return len;
        };
        bool isEmpty(){
            return len == 0;
        };
        reference firstEl(){
            return *__data;
        };
        reference lastEl(){
            return *get_last_ptr();
        };
    private:
        void _push_el(const _Ty & el){
            ++len;
            if(isEmpty()){
                __data = (_Ty*)malloc(sizeof(_Ty));
            }
            else {
                __data = (_Ty *)realloc(sizeof(_Ty) * len);
            };
            memmove(__data * (sizeof(_Ty) * (len-1)),&el, sizeof(_Ty));
        };
    public:
        void push(const _Ty & el){
            _push_el(el);
        };
        void push(_Ty && el){
            _push_el(el);
        };
        void clear(){
            pointer end = __data + (sizeof(_Ty) * len);
            pointer begin = __data;
            while(begin != end) {    
                delete begin;
                begin += sizeof(_Ty);
            };
            len = 0;
        };
        Stack() = default;
    };
    

NAMESPACE_END


#endif
