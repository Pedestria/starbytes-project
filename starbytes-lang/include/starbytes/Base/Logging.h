#include "Macros.h"
#include <iostream>
#include <deque>

#ifndef BASE_ERROR_H
#define BASE_ERROR_H

STARBYTES_FOUNDATION_NAMESPACE

template<class _Obj_Ty>
class Buffered_Logger {
    std::deque<_Obj_Ty> _buf;
    bool buffered;
    void log(_Obj_Ty & err_ref){
        std::cout << std::move(err_ref).get() << std::endl;
    };
    void write(_Obj_Ty & err_ref){
        if(buffered)
            _buf.push(std::move(err_ref));
        else 
         log(err_ref);
    };
    using self = Buffered_Logger<_Obj_Ty>;
public:
    Buffered_Logger(bool isBuffered = true):buffered(isBuffered){};
    ~Buffered_Logger(){
        if(buffered)
            while(!_buf.empty())
                _buf.pop_front();
    };
    self operator<<(_Obj_Ty & m){
        write(m);
        return *this;
    };
    self operator<<(_Obj_Ty && m){
        write(m);
        return *this;
    };
    void clearBuffer(){
        if(buffered)
            while(!_buf.empty()){
                log(_buf.front());
                _buf.pop_front();                
            }
    };
    std::deque<_Obj_Ty> & data_buf(){
        return _buf;
    };
    
};

NAMESPACE_END

#endif

