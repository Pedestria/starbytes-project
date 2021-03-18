#include "starbytes/Base/Base.h"
#include "BCDef.h"
#include <fstream>

STARBYTES_BYTECODE_NAMESPACE

class StreamBuffer {
    char * read_begin;
    char * read_cur;
    char * write_begin;
    char * write_cur;
    unsigned read_buf_len;
    unsigned write_buf_len;
    bool empty;

    void init_w(char * start){
        write_begin = start;
        write_cur = start;
        write_buf_len = 0;
    };
    void init_r(char * start){
        read_begin = start;
        read_cur = start;
        read_buf_len =  0;
    };
    template<class _Obj>
    void write(_Obj o){
        write_cur = ::new _Obj(o);
        write_cur += sizeof(_Obj);
    };
    
    template<class _Obj>
    void read(_Obj & o){
        o = *read_cur;
        ::delete read_cur;
        if(read_cur == read_begin)
            empty = true;
        else
            read_cur -= sizeof(_Obj);
    };
    
    const bool & isEmpty(){
        return empty;
    };
    
    StreamBuffer();
    StreamBuffer(const std::ifstream &);
    StreamBuffer(const std::ofstream &);

};

class InputStream {
    StreamBuffer & buf;
    using self = InputStream &;
public:
    self operator>>(BC);
    self operator>>(BCId);
    self operator>>(bool);
    self operator>>(int);
    self operator>>(float);
    self operator>>(double);
    InputStream() = delete;
    InputStream(const InputStream &) = delete;
    InputStream operator=(InputStream &&) = delete;
    InputStream(StreamBuffer & __in_buf):buf(__in_buf){};
};

class OutputStream {
     StreamBuffer & buf;
     using self = OutputStream &;
public:
     self operator<<(BC);
     self operator<<(BCId);
     self operator<<(bool);
     self operator<<(int);
     self operator<<(float);
     self operator<<(double);
     OutputStream() = delete;
     OutputStream(const OutputStream &) = delete;
     OutputStream operator=(OutputStream &&) = delete;
     OutputStream(StreamBuffer & __out_buf):buf(__out_buf){};
     
};

NAMESPACE_END
