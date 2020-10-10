
#include <type_traits>
#include <typeinfo>
#include <memory>
#include <vector>
#include <iterator>
#include <string>
#include "Macros.h"

#ifndef BASE_ADT_H
#define BASE_ADT_H


STARBYTES_FOUNDATION_NAMESPACE
    void execute_cmd(std::string & cmd);
    //Type checks variable with another!
    template<typename T1,typename T2>
    bool isa (T2 & subject){
        return true;
    }
    template<typename T1,typename T2>
    bool is_child_of(T2 & subject){
        return std::is_base_of_v<T1,T2>;
    }
    template<typename _Key,typename _Value>
    class OrderedMap {
        private:
            std::vector<std::pair<_Key,_Value>> map;
        public:
            OrderedMap(std::initializer_list<std::pair<_Key,_Value>> ilist): map(ilist){}
            ~OrderedMap(){};
            void set(std::pair<_Key,_Value> & val){
                map.push_back(val);
            }
            _Value & get(_Key k){
                for(std::pair<_Key,_Value> p : map){
                    if(p.first == k){
                        return p.second;
                    }
                }
                return nullptr;
            }
            void remove(_Key k){
                for(int i = 0;i < map.size();++i){
                    if(map[i].first == k){
                        map.erase(map.begin()+i);
                        break;
                    }
                }
            }
            
    };
NAMESPACE_END


#endif
