#pragma once
#include <string>
#include <vector>

namespace Starbytes {
    namespace Runtime {
        namespace Lib {
            /*Starbytes String*/
            class SBString{
                private:
                    std::string INTERAL_STRING;
                public:
                    SBString(std::string v):INTERAL_STRING(v){};
                    void append();
                    SBString * atIndex(int i);
                    void sliceAt(int i);
                    void findSubstr(SBString *substr);
            };
            template <class _Type>
            class SBArray {
                private:
                    std::vector<_Type> INTERNAL_ARRAY;
                public:
                    SBArray<_Type>(std::vector<_Type> v):INTERNAL_ARRAY(v){};
                    void push(_Type item){
                        INTERNAL_ARRAY.push_back(item);
                    };
                    void filter(bool (*cond)(_Type item)){
                        for(int i = 0;i < INTERNAL_ARRAY.size();++i){
                            if(cond(INTERNAL_ARRAY[i])){
                                INTERNAL_ARRAY.erase(INTERNAL_ARRAY.begin()+i);
                            }
                        }
                    };
                    bool find(bool (*cond)(_Type item)){
                        for(_Type it : INTERNAL_ARRAY){
                            if(cond(it)){
                                return true;
                            }
                        }
                        return false;
                    };
                    _Type * getAt(int i);
            };

            template <class _Type>
            class SBSet {
                private:
                    std::vector<_Type> INTERNAL_SET;
                public:
                    SBSet<_Type>(std::vector<_Type> v):INTERNAL_SET(v){};
                    bool push();
                    void filter(bool condition);
                    void find(bool condition);
                    _Type * getAt(int i);
            };

        };
    };
};