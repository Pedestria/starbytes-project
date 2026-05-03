#ifndef STARBYTES_RT_RTARGBUFFER_H
#define STARBYTES_RT_RTARGBUFFER_H

#include "starbytes/interop.h"

#include <array>
#include <cstddef>
#include <vector>

namespace starbytes::Runtime {

class DirectArgBuffer {
    static constexpr size_t InlineCapacity = 8;
    std::array<StarbytesObject, InlineCapacity> inlineStorage = {};
    std::vector<StarbytesObject> heapStorage;
    StarbytesObject *buffer = inlineStorage.data();
    unsigned count = 0;

public:
    explicit DirectArgBuffer(unsigned argCount);

    StarbytesObject *data();
    const StarbytesObject *data() const;
    unsigned size() const;
    void releaseAll();
};

}

#endif
