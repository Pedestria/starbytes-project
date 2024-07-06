
#include <cstdint>
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

        bool __equals(T * str,uint32_t len);

        public:

        string_ref_t(const T *str);

        string_ref_t(const T *str,uint32_t size);

        string_ref_t(std::basic_string<T> & str);

        bool equals(std::basic_string<T> & str);

        bool equals(const T * str);

        bool operator==(const T * str);

        bool operator==(std::string & str);


        T & operator[](uint32_t index);

        string_ref_t substr_ref(uint32_t start,uint32_t end);

        uint32_t size();

        void operator>>(std::basic_istream<T> &out);

        T *getBuffer();

        std::basic_string<T> operator()();
        ~string_ref_t() = default;
    };
    template<typename T>
    inline std::basic_ostream<T> & operator<<(std::basic_ostream<T> &out,string_ref_t<T> & str){
        out.write(str.getBuffer(),str.size());
        return out;
    };

    
    typedef string_ref_t<char> string_ref;

    typedef string_ref_t<wchar_t> wstring_ref;
    

    template<typename T>
    class array_ref {
        private:

        T * buffer;
        uint32_t l;

        public:

        using iterator = T *;

        iterator begin();
        iterator end();

        array_ref(T *buf,uint32_t len);
        array_ref(std::vector<T> & vec);

        T & operator[](uint32_t index);
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


    template<typename T>
    using string_map = std::map<std::string,T>;

    template<typename T>
    using optional = std::optional<T>;
}; 

#endif