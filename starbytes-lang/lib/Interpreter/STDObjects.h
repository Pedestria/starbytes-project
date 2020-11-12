#include "starbytes/Base/Base.h"
#include <optional>

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
        class StarbytesNumber : public StarbytesObject {
            private:
                class DynNum {
                    typedef void * AnyNum;
                    private:
                    bool retainDyNumType;
                    TYPED_ENUM DynNumTy:int {
                        INT,FLOAT,LONG,DOUBLE
                    };
                    DynNumTy type;

                    int * _int_val;
                    double * _double_val;
                    float * _float_val;
                    long * _long_val;
                    inline void __cast_dyn_num_to_this_type(DynNum & other){
                        if(type == DynNumTy::FLOAT)
                            other.toFloat();
                        
                        else if(type == DynNumTy::DOUBLE)
                            other.toDouble();
                        
                        else if(type == DynNumTy::INT)
                            other.toInt();
                        
                        else if(type == DynNumTy::LONG)
                            other.toLong();
                    };

                    inline void __cast_this_dyn_num_to_other_type(DynNum & other){
                        if(other.type == DynNumTy::FLOAT)
                            toFloat();
                        
                        else if(other.type == DynNumTy::DOUBLE)
                            toDouble();
                        
                        else if(other.type == DynNumTy::INT)
                            toInt();
                        
                        else if(other.type == DynNumTy::LONG)
                            toLong();
                    };

                    public:
                    template<typename NUMTY>
                    DynNum(int _type,NUMTY & val,bool _retainDyNumType = true):type(DynNumTy(_type)){
                        if(type == DynNumTy::INT)
                            _int_val = new int(val);
                        else if(type == DynNumTy::DOUBLE)
                            _double_val = new double(val);
                        
                        else if(type == DynNumTy::FLOAT)
                            _float_val = new float(val);
                        
                        else if(type == DynNumTy::LONG)
                            _long_val = new long(val);

                        retainDyNumType = _retainDyNumType;
                    };
                    ~DynNum(){
                        if(type == DynNumTy::INT){
                            delete _int_val;
                        }
                        else if(type == DynNumTy::FLOAT){
                            delete _float_val;
                        }
                        else if(type == DynNumTy::LONG){
                            delete _long_val;
                        }
                        else if(type == DynNumTy::DOUBLE){
                            delete _double_val;
                        }
                    };
                    int _get_type(){
                        return int(type);
                    };
                    void toInt(){
                        if(type == DynNumTy::FLOAT){
                            float tmp = *_float_val;
                            _int_val = new int(tmp);
                            delete _float_val;
                        }
                        else if(type == DynNumTy::LONG){
                            long tmp = *_long_val;
                            _int_val = new int(tmp);
                            delete _long_val;
                        }
                        else if(type == DynNumTy::DOUBLE){
                            double tmp = *_double_val;
                            _int_val = new int(tmp);
                            delete _double_val;
                        }
                        type = DynNumTy::INT;
                    };
                    void toFloat(){
                        if(type == DynNumTy::INT){
                            int tmp = *_int_val;
                            _float_val = new float(tmp);
                            delete _int_val;
                        }
                        else if(type == DynNumTy::LONG){
                            long tmp = *_long_val;
                            _float_val = new float(tmp);
                            delete _long_val;
                        }
                        else if(type == DynNumTy::DOUBLE){
                            double tmp = *_double_val;
                            _float_val = new float(tmp);
                            delete _double_val;
                        }
                        type = DynNumTy::FLOAT;
                    };
                    void toLong(){
                        if(type == DynNumTy::INT){
                            int tmp = *_int_val;
                            _long_val = new long(tmp);
                            delete _int_val;
                        }
                        if(type == DynNumTy::FLOAT){
                            float tmp = *_float_val;
                            _long_val = new long(tmp);
                            delete _float_val;
                        }
                        else if(type == DynNumTy::DOUBLE){
                            double tmp = *_double_val;
                            _long_val = new long(tmp);
                            delete _double_val;
                        }
                        type = DynNumTy::LONG;
                    };
                    void toDouble(){
                        if(type == DynNumTy::FLOAT){
                            float tmp = *_float_val;
                            _double_val = new double(tmp);
                            delete _float_val;
                        }
                        else if(type == DynNumTy::LONG){
                            long tmp = *_long_val;
                            _double_val = new double(tmp);
                            delete _long_val;
                        }
                        else if(type == DynNumTy::INT){
                            int tmp = *_int_val;
                            _double_val = new double(tmp);
                            delete _int_val;
                        }
                        type = DynNumTy::DOUBLE;
                    };
                    void operator +=(DynNum & num){
                        //If type is same as `this` number!
                        if(type == num.type){
                            if(num.type == DynNumTy::DOUBLE){
                                double * tmp_loc = num._double_val;
                                (*_double_val) += *tmp_loc;
                            }
                            else if(num.type == DynNumTy::FLOAT){
                                float * tmp_loc = num._float_val;
                                (*_float_val) += *tmp_loc;
                            }
                            else if(num.type == DynNumTy::INT){
                                int * tmp_loc = num._int_val;
                                (*_int_val) += *tmp_loc;
                            }
                            else if(num.type == DynNumTy::LONG){
                                long * tmp_loc = num._long_val;
                                (*_long_val) += *tmp_loc;
                            }
                        }
                        else{
                            if(retainDyNumType)
                                __cast_dyn_num_to_this_type(num);
                            else 
                                __cast_this_dyn_num_to_other_type(num);

                            //Peform Addition!

                            if(type == DynNumTy::DOUBLE){
                                double * tmp_loc = num._double_val;
                                (*_double_val) += *tmp_loc;
                            }
                            else if(type == DynNumTy::FLOAT){
                                float * tmp_loc = num._float_val;
                                (*_float_val) += *tmp_loc;
                            }
                            else if(type == DynNumTy::INT){
                                int * tmp_loc = num._int_val;
                                (*_int_val) += *tmp_loc;
                            }
                            else if(type == DynNumTy::LONG){
                                long * tmp_loc = num._long_val;
                                (*_long_val) += *tmp_loc;
                            }
                        }
                    };
                    void operator -=(DynNum & num){

                    };
                    bool operator ==(DynNum & num){

                    };
                    void * __get_val(){
                        if(type == DynNumTy::DOUBLE)
                            return _double_val;
                        else if(type == DynNumTy::INT)
                            return _int_val;
                        else if(type == DynNumTy::FLOAT)
                            return _float_val;
                        else if(type == DynNumTy::LONG)
                            return _long_val;
                    };
                };
                DynNum INTERNAL_VALUE;
            public: 
                void add(StarbytesNumber * _otherNum){
                    INTERNAL_VALUE += _otherNum->INTERNAL_VALUE;
                };
                void subtract(StarbytesNumber *_otherNum){
                    INTERNAL_VALUE -= _otherNum->INTERNAL_VALUE;
                }
                void * __get_interal(){
                    return INTERNAL_VALUE.__get_val();
                }
                template<typename NumTy>
                StarbytesNumber(NumTy num,int t):StarbytesObject(SBObjectType::Number),INTERNAL_VALUE(DynNum(t,num)){
                    
                };
                ~StarbytesNumber(){};
                bool __is_equal(StarbytesObject *_obj) override{
                    bool return_code;
                    if(_obj->isType(SBObjectType::Number)){
                        StarbytesNumber *n = (StarbytesNumber *)_obj;
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
                int get_dyn_num_type(){
                    return INTERNAL_VALUE._get_type();
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