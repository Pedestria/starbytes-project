
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

        string_ref_t(T *str);

        string_ref_t(std::basic_string<T> & str);

        bool equals(std::basic_string<T> & str);

        bool equals(const T * str);

        T & operator[](uint32_t index);

        string_ref_t substr_ref(uint32_t start,uint32_t end);

        uint32_t size();

        std::basic_ostream<T> & operator<<(std::basic_ostream<T> &out);
        void operator>>(std::basic_istream<T> &out);

        T *getBuffer();

        std::basic_string<T> operator()();
        ~string_ref_t() = default;
    };

    
    typedef string_ref_t<char> string_ref;

    typedef string_ref_t<wchar_t> wstring_ref;
    

    template<typename T>
    class array_ref {
        private:

        T * buffer;
        uint32_t l;

        public:

        array_ref(T *buf,uint32_t len);
        array_ref(std::vector<T> & vec);

        T & operator[](uint32_t index);
    };


    template<typename T>
    using string_map = std::map<std::string,T>;

    template<typename T>
    using optional = std::optional<T>;
}; 

#endif