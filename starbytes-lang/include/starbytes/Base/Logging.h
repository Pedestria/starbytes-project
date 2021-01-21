#include "Macros.h"
#include <iostream>
#include <deque>
#include <cstdarg>

#ifndef BASE_LOGGING_H
#define BASE_LOGGING_H

STARBYTES_FOUNDATION_NAMESPACE

struct ObjectMessage {
    virtual std::string get();
};

template<class _Obj_Ty>
class Buffered_Logger {
    std::deque<_Obj_Ty> _buf;
    bool buffered;
    void log(ObjectMessage & err_ref){
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

    template<std::enable_if_t<std::is_base_of_v<ObjectMessage,_Obj_Ty>,bool> = 0>
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

std::string fstring(const char *format,...);

NAMESPACE_END

#endif

