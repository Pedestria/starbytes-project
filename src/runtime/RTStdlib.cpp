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
    
        void _print_rt_internal_obj(StarbytesObject object,map<StarbytesClassType,std::string> &reg);
    
        void _print_rt_obj(StarbytesObject object,map<StarbytesClassType,std::string> &reg){
            if(StarbytesObjectIs(object)){
                _print_rt_internal_obj(object,reg);
            }
            else {
                StarbytesClassType t = StarbytesClassObjectGetClass(object);
                
                std::cout << reg[t] << "(" << std::flush;
                unsigned prop_n = StarbytesObjectGetPropertyCount(object);
                for(unsigned i = 0;i < prop_n;i++){
                    if(i > 0){
                        std::cout << ", " << std::flush;
                    }
                    auto prop = StarbytesObjectIndexProperty(object,i);
                    std::cout << prop->name << "=" << std::flush;
                    _print_rt_obj(prop->data,reg);
                }
                std::cout << ")" << std::flush;
            };
        }
    
        void _print_rt_internal_obj(StarbytesObject object,map<StarbytesClassType,std::string> &reg){
            if(StarbytesObjectTypecheck(object,StarbytesNumType())){
                    starbytes_float_t v;
            
                    auto floatObj = StarbytesNumConvertTo(object,NumTypeFloat);
                    v = StarbytesNumGetFloatValue(floatObj);
                    StarbytesObjectRelease(floatObj);
            
                    std::cout << "\x1b[33m" << std::flush;
                    double int_temp;
                    if(std::modf(v,&int_temp) == 0.f){
                        std::cout << starbytes_int_t(v);
                    }
                    else {
                        std::cout << std::dec << v;
                    };
                    std::cout << "\x1b[0m" << std::flush;
                }
            else if(StarbytesObjectTypecheck(object,StarbytesBoolType())){
                    auto val = (bool)StarbytesBoolValue(object);
                    std::cout << "\x1b[35m" << std::boolalpha << val << std::noboolalpha << "\x1b[0m" << std::flush;
                }
            else if(StarbytesObjectTypecheck(object,StarbytesStrType())){
                    auto data = StarbytesStrGetBuffer(object);
                    std::cout << "\x1b[32m" << "\"" << data << "\"" << "\x1b[0m" << std::flush;
                }
            else if(StarbytesObjectTypecheck(object,StarbytesArrayType())){
                    
                    std::cout << "[" << std::flush;
                    auto length = StarbytesArrayGetLength(object);
                    for(unsigned i = 0;i < length;i++){
                        
                        if(i > 0){
                            std::cout << "," << std::flush;
                        };
                        
                        _print_rt_obj(StarbytesArrayIndex(object,i),reg);
                    };
                    std::cout << "]" << std::flush;
                }
            /// Print StarbytesDict
            else {
                    std::cout << "{" << std::flush;
                    StarbytesArray keys = StarbytesObjectGetProperty(object,"keys"),vals = StarbytesObjectGetProperty(object,"values");
                    
                    auto len = StarbytesArrayGetLength(keys);
                    for(unsigned i = 0;i < len;i++){
                        _print_rt_obj(StarbytesArrayIndex(keys,i),reg);
                        std::cout << ":" << std::flush;
                        _print_rt_obj(StarbytesArrayIndex(vals,i),reg);
                    }
                    std::cout << "}" << std::flush;
                }
            }
    
        /// Print Function
        void print(StarbytesObject object,map<StarbytesClassType,std::string> &reg){
            // map<StarbytesClassType,std::string> &reg
//            _print_timestamp();
            _print_rt_obj(object,reg);
            std::cout << std::endl;
        }
    
    }

    
}
