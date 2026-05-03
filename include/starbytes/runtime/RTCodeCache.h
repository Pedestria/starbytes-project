#ifndef STARBYTES_RT_RTCODECACHE_H
#define STARBYTES_RT_RTCODECACHE_H

#include "starbytes/runtime/RTJit.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace starbytes::Runtime {

enum class CodeCacheArena : uint8_t {
    Optimized = 0,
    Warm,
    Vm,
};

struct CodeCacheStats {
    uint64_t bytesUsed = 0;
    uint64_t optimizedArtifacts = 0;
    uint64_t warmArtifacts = 0;
    uint64_t vmArtifacts = 0;
};

class CodeCache {
public:
    CodeCache();
    ~CodeCache();

    CompiledLoop *allocateOptimized();
    void demoteToWarm(CompiledLoop *loop);
    CodeCacheStats stats() const;
    void clear();

    static std::size_t loopFootprintBytes(const CompiledLoop &loop);

private:
    struct Entry {
        std::unique_ptr<CompiledLoop> loop;
        CodeCacheArena arena = CodeCacheArena::Optimized;
        std::size_t bytes = 0;
    };

    std::vector<Entry> entries;
    uint64_t totalBytes = 0;
    uint64_t optimizedCount = 0;
    uint64_t warmCount = 0;
    uint64_t vmCount = 0;
};

}

#endif
