#pragma once
#include <any>
#include <string>
#include <vector>
#include <optional>

namespace Starbytes {
    namespace Runtime {
        namespace Engine {
        struct RuntimeObject {};
        enum class SBObjectType:int {
                String,Array,Set,ClassInstance,Number
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
        enum class PtrType:int {
            CreateFunc,InvokeFunc,CreateVariable,SetVariable,CreateFunctionArgs,ReturnFunc
        };
        template<typename ARGTYPE = std::any>
        struct InternalFuncPtr : RuntimeObject {
            PtrType type;
            ARGTYPE args;
        };

        template <typename _ValType = StarbytesObject *>
        struct StoredVariable {
            std::string name;
            _ValType val;
            bool is_reference;
        };
        struct StoredFunction {
            std::string name;
            bool lazy;
            std::vector<StoredVariable<> *> args;
            std::initializer_list<InternalFuncPtr<> *> body;
        };

            struct StarbytesClassProperty {
                std::string name;
                bool not_constructible;
                bool loose;
                bool immutable;
                StarbytesObject *value;
            };

            struct StarbytesClassMethod {
                bool lazy;
                Engine::StoredFunction *func;
            };
            
            struct StarbytesClass {
                std::string name;
                std::vector<StarbytesClassProperty *> props;
                std::vector<StarbytesClassMethod *> methods;
            };
            struct StarbytesClassInstance : public StarbytesObject {
                StarbytesClassInstance(std::string _name,std::string _type):StarbytesObject(SBObjectType::ClassInstance),name(_name),type(_type){};
                std::string name;
                std::string type;
                std::vector<StarbytesClassProperty *> properties;
                std::vector<StarbytesClassMethod *> methods;
            };
            template<typename _numType>
            class StarbytesNumber : public StarbytesObject {
                private:
                    _numType INTERNAL_VALUE;
                public: 
                    void add(StarbytesNumber<_numType> * _otherNum){
                        INTERNAL_VALUE += _otherNum->INTERNAL_VALUE;
                    };
                    void subtract(StarbytesNumber<_numType> *_otherNum){
                        INTERNAL_VALUE -= _otherNum->INTERNAL_VALUE;
                    }
                    _numType & __get_interal(){
                        return INTERNAL_VALUE;
                    }
                    StarbytesNumber(_numType num):INTERNAL_VALUE(num),StarbytesObject(SBObjectType::Number){};
                    ~StarbytesNumber(){};
            };
            
            /*Starbytes String*/
            class StarbytesString : public StarbytesObject{
                private:
                    std::string INTERAL_STRING;
                public:
                    StarbytesString(std::string v):INTERAL_STRING(v),StarbytesObject(SBObjectType::String){};
                    void append(StarbytesString *obj);
                    StarbytesString * atIndex(int i);
                    void sliceAt(int i);
                    void findSubstr(StarbytesString *substr);
                    std::string & __get_interal(){
                        return INTERAL_STRING;
                    };
            };
            template <class _Type>
            class StarbytesArray : public StarbytesObject {
                private:
                    std::vector<_Type> INTERNAL_ARRAY;
                public:
                    StarbytesArray<_Type>(std::vector<_Type> v):INTERNAL_ARRAY(v),StarbytesObject(SBObjectType::Array){};
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
                    std::vector<_Type> & getIterator(){
                        return INTERNAL_ARRAY;
                    }
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

        void Program();
        };
    }
};

