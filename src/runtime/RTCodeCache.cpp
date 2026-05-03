#include "starbytes/runtime/RTCodeCache.h"

#include <algorithm>

namespace starbytes::Runtime {

CodeCache::CodeCache() = default;
CodeCache::~CodeCache() = default;

std::size_t CodeCache::loopFootprintBytes(const CompiledLoop &loop){
    return sizeof(CompiledLoop)
         + loop.ops.capacity() * sizeof(JitOpRecord)
         + loop.deoptPoints.capacity() * sizeof(DeoptPoint);
}

CompiledLoop *CodeCache::allocateOptimized(){
    Entry entry;
    entry.loop = std::make_unique<CompiledLoop>();
    entry.arena = CodeCacheArena::Optimized;
    auto *raw = entry.loop.get();
    entries.push_back(std::move(entry));
    optimizedCount += 1;
    return raw;
}

void CodeCache::demoteToWarm(CompiledLoop *loop){
    if(!loop){
        return;
    }
    auto it = std::find_if(entries.begin(), entries.end(), [&](const Entry &e){
        return e.loop.get() == loop;
    });
    if(it == entries.end() || it->arena != CodeCacheArena::Optimized){
        return;
    }
    it->arena = CodeCacheArena::Warm;
    if(optimizedCount > 0){
        optimizedCount -= 1;
    }
    warmCount += 1;
}

CodeCacheStats CodeCache::stats() const{
    CodeCacheStats out;
    out.bytesUsed = 0;
    out.optimizedArtifacts = optimizedCount;
    out.warmArtifacts = warmCount;
    out.vmArtifacts = vmCount;
    for(const auto &e : entries){
        out.bytesUsed += loopFootprintBytes(*e.loop);
    }
    return out;
}

void CodeCache::clear(){
    entries.clear();
    totalBytes = 0;
    optimizedCount = 0;
    warmCount = 0;
    vmCount = 0;
}

}
