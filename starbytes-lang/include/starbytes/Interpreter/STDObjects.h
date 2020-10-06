#include "starbytes/Base/Base.h"

#ifndef INTERPRETER_STDOBJECTS_H
#define INTERPRETER_STDOBJECTS_H

STARBYTES_INTERPRETER_NAMESPACE
        struct RuntimeObject {};
        enum class SBObjectType:int {
                String,Array,Set,ClassInstance,Number,Dictionary,Boolean
        };

        class StarbytesObject : public RuntimeObject {
                protected:
                    SBObjectType type;
                public:
                    explicit StarbytesObject(SBObjectType _type):type(_type){};
                    bool isType(SBObjectType _subject){
                        return type == _subject;
                    }
                    virtual bool __is_equal(StarbytesObject *_obj){
                        return false;
                    };
                    
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
                bool __is_equal(StarbytesObject *_obj) override{
                    bool return_code;
                    if(_obj->isType(SBObjectType::Number)){
                        StarbytesNumber<_numType> *n = (StarbytesNumber<_numType> *)_obj;
                        if(n->INTERNAL_VALUE == INTERNAL_VALUE){
                            return_code = true;
                        }
                        else{
                            return_code = false;
                        }
                    }
                    else{
                        return_code = false;
                    }
                    return return_code;
                };
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
                bool __is_equal(StarbytesObject *_obj) override;
        };
        template <class _Type>
        class StarbytesArray : public StarbytesObject {
            private:
                std::vector<_Type> INTERNAL_ARRAY;
            public:
                StarbytesArray<_Type>(std::vector<_Type> v):INTERNAL_ARRAY(v),StarbytesObject(SBObjectType::Array){};
                ~StarbytesArray();
                void push(_Type & item){
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
                bool push(){

                };
                void filter(bool condition);
                void find(bool condition);
                _Type * getAt(int i);
        };

        template<typename _Key,typename _Val>
        class StarbytesDictionary : public StarbytesObject {
            private:
                std::vector<std::pair<_Key,_Val>> INTERNAL_MAP;
            public:
                template<std::enable_if_t<std::is_same_v<std::remove_pointer_t<_Key>,StarbytesString>,int> = 0>
                StarbytesDictionary(std::vector<std::pair<_Key,_Val>> _vector):INTERNAL_MAP(_vector),StarbytesObject(SBObjectType::Dictionary){};
                ~StarbytesDictionary();
                bool set(_Key __key,_Val & __val){
                    for(std::pair<_Key,_Val> & p : INTERNAL_MAP){
                        if(p.first->__get_interal() == __key->__get_interal()){
                            return false;
                        }
                    }

                    INTERNAL_MAP.push_back(std::pair(__key,__val));
                    return true;
                };
                _Val & get(_Key & __key){
                    for(std::pair<_Key,_Val> & p : INTERNAL_MAP){
                        if(p.first == __key){
                            return p.second;
                        }
                    }
                };
                //Internal FUNCTION ONLY!
                std::vector<std::pair<_Key,_Val>> & getIterator(){
                    return INTERNAL_MAP;
                };
        };
    class StarbytesBoolean : public StarbytesObject {
        private:
            bool INTERNAL_BOOLEAN;
        public:
            StarbytesBoolean(bool __val):INTERNAL_BOOLEAN(__val),StarbytesObject(SBObjectType::Boolean){};
            ~StarbytesBoolean();
            bool & __get_interal(){
                return INTERNAL_BOOLEAN;
            }
            bool __is_equal(StarbytesObject *_obj) override;

    };

NAMESPACE_END

#endif