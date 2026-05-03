#include <array>
#include <fstream>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "starbytes/interop.h"

#ifndef STARBYTES_RT_RTENGINE_H
#define STARBYTES_RT_RTENGINE_H

namespace starbytes::Runtime {

enum class RuntimeProfileSubsystem : uint8_t {
    FunctionCall = 0,
    NativeCall,
    MemberAccess,
    IndexAccess,
    ObjectAllocation,
    Microtasks,
    Count
};

constexpr size_t RuntimeProfileOpcodeCount = 256;
constexpr size_t RuntimeProfileSubsystemCount = static_cast<size_t>(RuntimeProfileSubsystem::Count);
constexpr size_t RuntimeProfileObjectKindCount = static_cast<size_t>(StarbytesRuntimeObjectKindCount);

enum class RuntimeExecutionMode : uint8_t {
    Auto = 0,
    V1,
    V2
};

enum class RuntimeExecutionPath : uint8_t {
    Unknown = 0,
    V1StreamInterpreter,
    V2RegisterInterpreter
};

enum class RuntimeProfileSiteKind : uint8_t {
    Call = 0,
    Member,
    Index,
    Branch
};

struct RuntimeFunctionProfileData {
    std::string name;
    uint64_t callCount = 0;
    uint64_t totalNs = 0;
    uint64_t selfNs = 0;
};

struct RuntimeSiteProfileData {
    std::string functionName;
    uint64_t bytecodeOffset = 0;
    RuntimeProfileSiteKind kind = RuntimeProfileSiteKind::Call;
    uint8_t opcode = 0;
    uint64_t executionCount = 0;
    uint64_t takenCount = 0;
};

struct RuntimeProfileData {
    bool enabled = false;
    uint16_t moduleBytecodeVersion = 0;
    bool moduleHasExplicitHeader = false;
    RuntimeExecutionMode requestedExecutionMode = RuntimeExecutionMode::Auto;
    RuntimeExecutionPath executionPath = RuntimeExecutionPath::Unknown;
    uint64_t totalRuntimeNs = 0;
    uint64_t dispatchCount = 0;
    uint64_t functionCallCount = 0;
    bool subsystemTimingsOverlap = true;
    bool functionTimingsOverlap = true;
    std::array<uint64_t, RuntimeProfileOpcodeCount> opcodeExecutions = {};
    std::array<uint64_t, RuntimeProfileSubsystemCount> subsystemNs = {};
    std::array<uint64_t, RuntimeProfileObjectKindCount> objectAllocations = {};
    std::array<uint64_t, RuntimeProfileObjectKindCount> objectDeallocations = {};
    uint64_t refCountIncrements = 0;
    uint64_t refCountDecrements = 0;
    std::vector<RuntimeFunctionProfileData> functionStats;
    std::vector<RuntimeSiteProfileData> siteStats;
    uint64_t quickenedSitesInstalled = 0;
    uint64_t quickenedExecutions = 0;
    uint64_t quickenedSpecializations = 0;
    uint64_t quickenedFallbacks = 0;
    uint64_t feedbackSitesInstalled = 0;
    uint64_t feedbackCacheHits = 0;
    uint64_t feedbackCacheMisses = 0;
    uint64_t v2ExecutionImagesBuilt = 0;
    uint64_t superinstructionsInstalled = 0;
    uint64_t superinstructionExecutions = 0;
    uint64_t loopHeadersTracked = 0;
    uint64_t hotLoopTriggers = 0;
    uint64_t tier2LoopsLowered = 0;
    uint64_t tier2IrInstructionCount = 0;
    uint64_t loopGuardSamples = 0;
    uint64_t loopGuardFailures = 0;
    uint64_t tier2CompiledLoops = 0;
    uint64_t tier2CompiledExecutions = 0;
    uint64_t tier2DeoptCount = 0;
    uint64_t codeCacheBytesUsed = 0;
    uint64_t codeCacheOptimizedArtifacts = 0;
    uint64_t codeCacheWarmArtifacts = 0;
};


class Interp {
public:
    virtual void exec(std::istream & in) = 0;
    virtual void setProfilingEnabled(bool enabled) = 0;
    virtual void setExecutionMode(RuntimeExecutionMode mode) = 0;
    virtual void setJitEnabled(bool enabled) = 0;
    virtual bool addExtension(const std::string &path) = 0;
    virtual bool hasRuntimeError() const = 0;
    virtual std::string takeRuntimeError() = 0;
    virtual RuntimeProfileData getProfileData() const = 0;
    static std::shared_ptr<Interp> Create();
    virtual ~Interp() = default;
};

}

#endif
