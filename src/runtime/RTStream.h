#ifndef STARBYTES_RT_RTSTREAM_H
#define STARBYTES_RT_RTSTREAM_H

#include <cstddef>
#include <istream>
#include <ios>
#include <streambuf>

namespace starbytes::Runtime {

class MemoryStreamBuf final : public std::streambuf {
public:
    MemoryStreamBuf(const char *data, size_t size);
    void reset(const char *data, size_t size);

protected:
    pos_type seekoff(off_type off, std::ios_base::seekdir dir, std::ios_base::openmode which) override;
    pos_type seekpos(pos_type pos, std::ios_base::openmode which) override;
};

class MemoryInputStream final : public std::istream {
    MemoryStreamBuf buffer;
public:
    MemoryInputStream(const char *data, size_t size);
};

}

#endif
