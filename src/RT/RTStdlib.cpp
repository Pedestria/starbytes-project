#include "RTStdlib.h"

#include <ctime>
#include <iomanip>
#include <iostream>

namespace starbytes {
    namespace Runtime::stdlib {
        inline void _print_timestamp(){
//            auto t = time(NULL);
//            std::cout << "[" << std::asctime(gmtime(&t)) << "]" << std::flush;
        }
    
        void _print_rt_internal_obj(RTInternalObject *object);
    
        void _print_rt_obj(RTObject *object){
            if(object->isInternal){
                _print_rt_internal_obj((RTInternalObject *)object);
            }
            else {
                
            };
        }
    
        void _print_rt_internal_obj(RTInternalObject *object){
            switch (object->type) {
//                case RTInternalObject::Integer : {
//                    break;
//                }
//                case RTInternalObject::Float : {
//                    break;
//                }
                case RTINTOBJ_BOOL : {
                    auto *params = (RTInternalObject::BoolParams *)object->data;
                    std::cout << "\x1b[35m" << std::boolalpha << params->value << std::noboolalpha << "\x1b[0m" << std::flush;
                    break;
                }
                case RTINTOBJ_STR : {
                    auto *params = (RTInternalObject::StringParams *)object->data;
                    std::cout << "\x1b[32m" << "\"" << params->str << "\"" << "\x1b[0m" << std::flush;
                    break;
                }
                case RTINTOBJ_ARRAY :{
                    auto *params = (RTInternalObject::ArrayParams *)object->data;
                    std::cout << "[" << std::flush;
                    auto obj_it = params->data.begin();
                    while(obj_it != params->data.end()){
                        
                        if(obj_it != params->data.begin()){
                            std::cout << "," << std::flush;
                        };
                        
                        auto &obj = *obj_it;
                        _print_rt_obj(obj);
                        ++obj_it;
                    };
                    std::cout << "]" << std::flush;
                    break;
                }
                case RTINTOBJ_DICTIONARY: {
                    break;
                }
            }
            
        }
    
        /// Print Function
        void print(RTObject *object){
//            _print_timestamp();
            _print_rt_obj(object);
            std::cout << std::endl;
        }
    
    
        /// Array Object

        void array_push(RTInternalObject::ArrayParams &array,RTObject *any){
            array.data.push_back(any);
        }
        RTObject *array_index(RTInternalObject::ArrayParams &array,unsigned idx){
            assert(idx < array.data.size());
            return array.data[idx];
        }
        unsigned array_length(RTInternalObject::ArrayParams &array){
            return array.data.size();
        }


        /// Integer Object

        int int_add(RTInternalObject::IntegerParams & lhs,RTInternalObject::IntegerParams & rhs){
            return lhs.value + rhs.value;
        }

        int int_sub(RTInternalObject::IntegerParams & lhs,RTInternalObject::IntegerParams & rhs){
            return lhs.value - rhs.value;
        }
        /// String Object

        void string_concat(RTInternalObject::StringParams & in1,RTInternalObject::StringParams & in2,std::string & out){
            out = in1.str + in2.str;
        }

        void string_substring(RTInternalObject::StringParams & in,unsigned idx,unsigned len,std::string & out){
            out = in.str.substr(idx,len);
        }
    
        unsigned string_length(RTInternalObject::StringParams & in){
            return in.str.size();
        }
    
    
    }
}
