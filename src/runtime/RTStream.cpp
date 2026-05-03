#include "RTStream.h"

namespace starbytes::Runtime {

MemoryStreamBuf::MemoryStreamBuf(const char *data, size_t size){
    reset(data, size);
}

void MemoryStreamBuf::reset(const char *data, size_t size){
    auto *begin = const_cast<char *>(data);
    setg(begin, begin, begin + size);
}

MemoryStreamBuf::pos_type MemoryStreamBuf::seekoff(off_type off,
                                                   std::ios_base::seekdir dir,
                                                   std::ios_base::openmode which){
    if((which & std::ios_base::in) == 0){
        return pos_type(off_type(-1));
    }
    char *base = eback();
    char *next = nullptr;
    switch(dir){
        case std::ios_base::beg:
            next = base + off;
            break;
        case std::ios_base::cur:
            next = gptr() + off;
            break;
        case std::ios_base::end:
            next = egptr() + off;
            break;
        default:
            return pos_type(off_type(-1));
    }
    if(next < base || next > egptr()){
        return pos_type(off_type(-1));
    }
    setg(base, next, egptr());
    return pos_type(next - base);
}

MemoryStreamBuf::pos_type MemoryStreamBuf::seekpos(pos_type pos,
                                                   std::ios_base::openmode which){
    return seekoff(off_type(pos), std::ios_base::beg, which);
}

MemoryInputStream::MemoryInputStream(const char *data, size_t size)
    : std::istream(nullptr), buffer(data, size) {
    rdbuf(&buffer);
    clear();
}

}
