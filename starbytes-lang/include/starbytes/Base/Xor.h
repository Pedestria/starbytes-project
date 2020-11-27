#include "Macros.h"
#ifndef BASE_XOR_H
#define BASE_XOR_H

STARBYTES_FOUNDATION_NAMESPACE
template<class A_Ty,class B_Ty>
class XOR {
    private:
        A_Ty a;
        B_Ty b;
        bool _a_xor_b;
    public:
        XOR(A_Ty _a):a(_a),_a_xor_b(true){};
        XOR(B_Ty _b):b(_b),_a_xor_b(false){};
        inline bool isFirstTy(){ return _a_xor_b;};
        inline bool isSecondTy(){ return !_a_xor_b;};
        template<class C_Ty>
        C_Ty & getValue();
        // Template Specialization
        template<>
        A_Ty & getValue<A_Ty>(){
            A_Ty * ptr;
            if(isFirstTy())
                ptr = &a;
            return *ptr;
        };
        // Template Specialization
        template<>
        B_Ty & getValue<B_Ty>(){
            B_Ty * ptr;
            if(isSecondTy())
                ptr = &b;
            return *ptr;
        };
};

NAMESPACE_END

#endif
