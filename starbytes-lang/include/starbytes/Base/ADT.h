
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
            const _Value & get(const _Key & k){
                for(std::pair<_Key,_Value> p : map){
                    if(p.first == k){
                        return p.second;
                    }
                }
                return nullptr;
            }
            void remove(const _Key & k){
                for(int i = 0;i < map.size();++i){
                    if(map[i].first == k){
                        map.erase(map.begin()+i);
                        break;
                    }
                }
            }
            
    };
    /*Stores a reference to a string that exists, but it is a temporary!*/
    class TmpString {
        private:
            std::string & value;
        public:
            TmpString(std::string & string_value):value(string_value){};
            std::string & getValue ();
            bool exists();
            bool operator == (TmpString & str_subject);
            bool operator == (std::string & str_subject);
    };

NAMESPACE_END


#endif
