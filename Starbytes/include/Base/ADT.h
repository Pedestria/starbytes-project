#pragma once

#include <type_traits>
#include <typeinfo>

namespace Starbytes {
    namespace Foundation {
        template<typename T1,typename T2>
        bool isa (T2 & subject){
            return typeid(subject) == typeid(T1);
        }
        template<typename T1,typename T2>
        bool extends_of(T2 & subject){
            return std::is_base_of_v<T1,T2>;
        }
    };
    
};
