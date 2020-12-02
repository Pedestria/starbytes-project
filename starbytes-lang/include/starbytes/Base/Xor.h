#include "Macros.h"
#include <cstdlib>
#include <cstring>

#ifndef BASE_XOR_H
#define BASE_XOR_H

STARBYTES_FOUNDATION_NAMESPACE
template<class A_Ty,class B_Ty>
class XOR {
    private:
        A_Ty * a = nullptr;
        B_Ty * b = nullptr;
        bool _a_xor_b;
    public:
        XOR(A_Ty _a):_a_xor_b(true){
            a = (A_Ty *)malloc(sizeof(A_Ty));
            memcpy(a,&_a,sizeof(A_Ty));
        };
        XOR(B_Ty _b):_a_xor_b(false){
            b = (B_Ty *)malloc(sizeof(B_Ty));
            memcpy(b,&_b,sizeof(B_Ty));
        };
        inline bool isFirstTy(){ return _a_xor_b;};
        inline bool isSecondTy(){ return !_a_xor_b;};
        template<class C_Ty>
        C_Ty & getValue();
        // Template Specialization
        template<>
        A_Ty & getValue<A_Ty>(){
            A_Ty * ptr;
            if(isFirstTy())
                ptr = a;
            return *ptr;
        };
        // Template Specialization
        template<>
        B_Ty & getValue<B_Ty>(){
            B_Ty * ptr;
            if(isSecondTy())
                ptr = b;
            return *ptr;
        };
};

NAMESPACE_END

#endif
