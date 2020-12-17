#include "Xor.h"
#include <string>

#ifndef BASE_UNSAFE_H
#define BASE_UNSAFE_H


STARBYTES_FOUNDATION_NAMESPACE

struct Error {
    std::string message;
    Error(std::string && _message):message(_message){};
};


template<class _Ty>
class Unsafe {
    XOR<_Ty,Error> data;
    // bool __not_set = false;
public:
    Unsafe() = delete;
    Unsafe(const Unsafe<_Ty> &) = delete;
    Unsafe(Error err):data(err){};
    Unsafe(_Ty res):data(res){};
    // Unsafe(const Error & err):data(err){};
    // Unsafe(const _Ty & res):data(res){};
    // inline bool unInitialized(){
    //     return __not_set;
    // };
    bool hasError(){
        return data.isSecondTy();
    };
    _Ty & getResult(){
        return data.template getValue<_Ty>();
    };
    Error & getError(){
        return data.template getValue<Error>();
    };
};

NAMESPACE_END
#endif
