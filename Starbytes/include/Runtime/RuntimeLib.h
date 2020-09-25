#pragma once
#include <string>
#include <vector>

namespace Starbytes {
    namespace Runtime {
        struct RuntimeObject {};
        namespace Lib {
            enum class SBObjectType:int {
                String,Array,Set
            };
            class StarbytesObject : public RuntimeObject {
                protected:
                    SBObjectType type;
                public:
                    explicit StarbytesObject(SBObjectType _type):type(_type){};
                    bool isType(SBObjectType _subject){
                        return type == _subject;
                    }
                    
            };

            struct StarbytesClassProperty {
                std::string name;
                bool loose;
                bool immutable;
                StarbytesObject *value;
            };

            struct StarbytesClassMethod {

            };
            struct StarbytesClassInstance : public StarbytesObject {
                std::string name;
                std::vector<StarbytesClassProperty *> properties;
                
            };
            
            /*Starbytes String*/
            class StarbytesString : public StarbytesObject{
                private:
                    std::string INTERAL_STRING;
                public:
                    StarbytesString(std::string v):INTERAL_STRING(v),StarbytesObject(SBObjectType::String){};
                    void append();
                    StarbytesString * atIndex(int i);
                    void sliceAt(int i);
                    void findSubstr(StarbytesString *substr);
                    std::string & __get_interal();
            };
            template <class _Type>
            class StarbytesArray : public StarbytesObject {
                private:
                    std::vector<_Type> INTERNAL_ARRAY;
                public:
                    StarbytesArray<_Type>(std::vector<_Type> v):INTERNAL_ARRAY(v){};
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
            class StarbytesSet : public StarbytesObject {
                private:
                    std::vector<_Type> INTERNAL_SET;
                public:
                    StarbytesSet<_Type>(std::vector<_Type> v):INTERNAL_SET(v){};
                    bool push();
                    void filter(bool condition);
                    void find(bool condition);
                    _Type * getAt(int i);
            };

        };
    };
};