#include "Macros.h"

#ifndef BASE_OPTIONAL_H
#define BASE_OPTIONAL_H

STARBYTES_FOUNDATION_NAMESPACE

class VoidTy {};

template<class Obj>
class Optional {
    private:
    bool _constructed;
    Obj _value;
    public:
    bool & hasVal(){
        return _constructed;
    };
    Obj & value(){ return _value;};
    Optional():_constructed(false){};
    Optional(VoidTy v):_constructed(false),_value(v){};
    Optional(Obj v):_constructed(true),_value(v){};
};

NAMESPACE_END

#endif