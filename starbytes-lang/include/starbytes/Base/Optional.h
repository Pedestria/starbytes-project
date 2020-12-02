#include "Macros.h"
#include <cstring>
#include <cstdlib>

#ifndef BASE_OPTIONAL_H
#define BASE_OPTIONAL_H

STARBYTES_FOUNDATION_NAMESPACE

class VoidTy {};

template<class Obj>
class Optional {
private:
    bool _constructed;
    Obj * _value = nullptr;
public:
    bool & hasVal(){
        return _constructed;
    };
    Obj & value(){ return *_value;};
    Optional():_constructed(false){};
    Optional(VoidTy v):_constructed(false){
        _value = (Obj*)new VoidTy(v);
    };
    Optional(Obj v):_constructed(true){
        _value = (Obj *)malloc(sizeof(Obj));
        memcpy(_value,&v,sizeof(Obj));
    };
};

NAMESPACE_END

#endif