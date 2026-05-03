#include "RTArgBuffer.h"

namespace starbytes::Runtime {

DirectArgBuffer::DirectArgBuffer(unsigned argCount) : count(argCount) {
    if(argCount > InlineCapacity){
        heapStorage.resize(argCount, nullptr);
        buffer = heapStorage.data();
    }
}

StarbytesObject *DirectArgBuffer::data(){
    return buffer;
}

const StarbytesObject *DirectArgBuffer::data() const{
    return buffer;
}

unsigned DirectArgBuffer::size() const{
    return count;
}

void DirectArgBuffer::releaseAll(){
    for(unsigned i = 0; i < count; ++i){
        if(buffer[i]){
            StarbytesObjectRelease(buffer[i]);
            buffer[i] = nullptr;
        }
    }
}

}
