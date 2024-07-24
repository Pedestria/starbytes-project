
#include <cstdint>
#include <cstring>
#include <optional>
#include <stdint.h>
#include <string>
#include <vector>
#include <map>

#ifndef STARBYTES_BASE_ADT_H
#define STARBYTES_BASE_ADT_H


namespace starbytes {

    template<typename T>
    class string_ref_t {
        private:

        uint32_t l;

        T* buffer;

        bool __equals(T * str,uint32_t len){
            for(auto i = 0;i < len;i++){
                auto & _ptr = str[i];
                if(buffer[i] != _ptr)
                    return false;
            }
            return true;
        };

        public:

        typedef T * iterator;

        iterator begin(){
            return buffer;
        };

        iterator end(){
            return buffer + l;
        };

        string_ref_t():buffer(nullptr),l(0){
            
        }

        string_ref_t(const T *str):buffer(const_cast<T *>(str)),l(strlen(str)){

        };

        string_ref_t(const T *str,uint32_t size):buffer(const_cast<T *>(str)),l(size){

        };

        string_ref_t(std::basic_string<T> str):buffer(str.data()),l(str.size()){

        };

        bool equals(string_ref_t & str){
            return __equals(str.buffer,str.l);
        };

        bool equals(std::basic_string<T> & str){
            return __equals(str.data(),str.size());
        };

        bool equals(const T * str){
            return __equals(const_cast<char *>(str),strlen(str));
        };

        bool operator==(const T * str){
            return equals(str);
        };

        bool operator==(std::string & str){
            return equals(str);
        };


        T & operator[](uint32_t index){

            return buffer[index];
        };

        string_ref_t substr_ref(uint32_t start,uint32_t end){
            string_ref_t s;
            s.l = end - start;
            s.buffer = buffer + start;
            return s;
        };

        uint32_t size(){
            return l;
        };

        void operator>>(std::basic_istream<T> &in){
            in.read(buffer,in.gcount());
        };

        T *getBuffer(){
            return buffer;
        };

        operator std::basic_string<T> (){
            return std::basic_string<T>(buffer,l);
        };

        ~string_ref_t() = default;
    };
    template<typename T>
    inline std::basic_ostream<T> & operator<<(std::basic_ostream<T> &out,string_ref_t<T> str){
        out.write(str.getBuffer(),str.size());
        return out;
    };

    template<typename T>
    inline bool operator==(string_ref_t<T> & lhs,std::string & rhs){
        return lhs.equals(rhs);
    }

    template<typename T>
    inline bool operator==(std::string & lhs,string_ref_t<T> & rhs){
        return rhs.equals(lhs);
    }

    template<typename T>
    inline bool operator==(string_ref_t<T> & lhs,string_ref_t<T> & rhs){
        return lhs.equals(rhs);
    }


    
    typedef string_ref_t<char> string_ref;

    typedef string_ref_t<wchar_t> wstring_ref;
    

    template<typename T>
    class array_ref {
        private:

        T * buffer;
        uint32_t l;

        public:

        using iterator = T *;

        iterator begin(){
            return buffer;
        };
        iterator end(){
            return buffer + l;
        };

        array_ref():buffer(nullptr),l(0){};
        array_ref(T *buf,uint32_t len):buffer(buf),l(len){};
        array_ref(iterator begin,iterator end):buffer(begin),l(end - begin){};
        array_ref(std::vector<T> & vec):buffer(vec.data()),l(vec.size()){};

        uint32_t size(){ return l;};

        T & operator[](uint32_t index){
            return buffer[index];
        };
    };

    /**
     * @brief Similar to LLVM's `Twine` implementation, only more compact.
     * 
     */
    class twine {
        char *buf;
    public:
        using size_type = uint32_t;
    private:
        size_type len;
        void concat(char *other,size_type len);
    public:
        twine & operator+(const char * other);
        twine & operator+(string_ref & other);
        twine & operator+(std::string & other);
    };

    template<typename T1,typename T2>
    using map = std::map<T1,T2>;


    template<typename T>
    using string_map = std::map<std::string,T>;


    template<typename T>
    using optional = std::optional<T>;



    
}; 

#endif