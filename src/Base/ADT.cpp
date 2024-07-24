#include "starbytes/base/ADT.h"
#include <assert.h>
#include <cstdint>

namespace starbytes {
    // template<typename T>
    // string_ref_t<T>::string_ref_t(const T *buffer):
    // l(std::char_traits<T>::length(buffer)),
    // buffer(buffer){
    //     assert(l > 0 && "CANNOT CREATE STRING_REF FROM NULL SIZE");
    // }

    //  template<typename T>
    // string_ref_t<T>::string_ref_t(std::basic_string<T> buffer):
    // l(buffer.size()),
    // buffer(buffer.c_str()){
    //     assert(l > 0 && "CANNOT CREATE STRING_REF FROM NULL SIZE");
    // }


    // template<typename T>
    // bool string_ref_t<T>::__equals(T *buf,uint32_t len){
    //     for(auto i = 0;i < len;i++){
    //         auto & _ptr = buf[i];
    //         if(buffer[i] != _ptr)
    //             return false;
    //     }
    //     return true;
    // }
    // template<typename T>
    // bool string_ref_t<T>::equals(std::basic_string<T> & str){
    //     return __equals(str.c_str(),str.size());
    // }
    // template<typename T>
    // bool string_ref_t<T>::equals(const T *str){
    //     return __equals(str,std::char_traits<T>::length(str));
    // }

    // template<typename T>
    // bool string_ref_t<T>::operator==(const T *str){
    //     return __equals(str,std::char_traits<T>::length(str));
    // }

    //  template<typename T>
    // bool string_ref_t<T>::operator==(std::string & str){
    //     return equals(str);
    // }


    // template<typename T>
    // T & string_ref_t<T>::operator[](uint32_t index){
    //     return buffer[index];
    // };

    // template<typename T>
    // typename string_ref_t<T>::iterator string_ref_t<T>::begin(){
    //     return buffer;
    // }

    // template<typename T>
    // typename string_ref_t<T>::iterator string_ref_t<T>::end(){
    //     return buffer + l;
    // }

    // template<typename T>
    // T *string_ref_t<T>::getBuffer(){
    //     return buffer;
    // }
    // // template<typename T>
    // // std::basic_ostream<T> & string_ref_t<T>::operator<<(std::basic_ostream<T> & out){
    // //     out.write(buffer,l);
    // //     return out;
    // // };
    // template<typename T>
    // void string_ref_t<T>::operator>>(std::basic_istream<T> & in){
    //     in.read(buffer,in.gcount());
    // };
    // template<typename T>
    // string_ref_t<T> string_ref_t<T>::substr_ref(uint32_t start,uint32_t end){
    //     string_ref_t s;
    //     s.l = end - start;
    //     s.buffer = buffer + start;
    //     return s;
    // }

    //   template<typename T>
    //   uint32_t string_ref_t<T>::size(){
    //       return l;
    //   }

    //   template<typename T>
    //   array_ref<T>::array_ref():l(0),buffer(nullptr){}

    //   template<typename T>
    //   array_ref<T>::array_ref(typename array_ref<T>::iterator begin,typename array_ref<T>::iterator end):l(end - begin),buffer(begin){

    //   }

    //   template<typename T>
    //   array_ref<T>::array_ref(std::vector<T> & v):l(v.size()),buffer(v.data()){}

    //   template<typename T>
    //   typename array_ref<T>::iterator array_ref<T>::begin(){
    //     return buffer;
    //   }

    //   template<typename T>
    //   typename array_ref<T>::iterator array_ref<T>::end(){
    //     return buffer + l;
    //   }

}