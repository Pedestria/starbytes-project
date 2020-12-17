#include "starbytes/Base/Base.h"
#include "BCDef.h"

STARBYTES_BYTECODE_NAMESPACE

class BC_StreamBuffer {
    void * read_begin;
    void * read_cur;
    void * write_begin;
    void * write_cur;

    void init_w(void * start){
        write_begin = start;
        write_cur = start;
        write_buf_len = 0;
    };
    void init_r(void * start){
        read_begin = start;
        read_cur = start;
        read_buf_len =  0;
    };

    unsigned read_buf_len;
    unsigned write_buf_len;
};

// // class BC_IStream {

// // };

// class BC_OStream {
//     BC_StreamBuffer buf;
//     public:
//     void widen();
//     void put();
//     void write();
//     BC_OStream & operator<<(BC);
//     BC_OStream & operator<<(BCId);
//     BC_OStream & operator<<(bool);
//     BC_OStream & operator<<(int);
//     BC_OStream & operator<<(float);
//     BC_OStream & operator<<(double);
// };

NAMESPACE_END