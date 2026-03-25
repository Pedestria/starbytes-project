#include <array>
#include <fstream>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

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

struct RuntimeFunctionProfileData {
    std::string name;
    uint64_t callCount = 0;
    uint64_t totalNs = 0;
    uint64_t selfNs = 0;
};

struct RuntimeProfileData {
    bool enabled = false;
    uint64_t totalRuntimeNs = 0;
    uint64_t dispatchCount = 0;
    uint64_t functionCallCount = 0;
    std::array<uint64_t, RuntimeProfileOpcodeCount> opcodeExecutions = {};
    std::array<uint64_t, RuntimeProfileSubsystemCount> subsystemNs = {};
    std::vector<RuntimeFunctionProfileData> functionStats;
    uint64_t quickenedSitesInstalled = 0;
    uint64_t quickenedExecutions = 0;
    uint64_t quickenedSpecializations = 0;
    uint64_t quickenedFallbacks = 0;
};


class Interp {
public:
    virtual void exec(std::istream & in) = 0;
    virtual void setProfilingEnabled(bool enabled) = 0;
    virtual bool addExtension(const std::string &path) = 0;
    virtual bool hasRuntimeError() const = 0;
    virtual std::string takeRuntimeError() = 0;
    virtual RuntimeProfileData getProfileData() const = 0;
    static std::shared_ptr<Interp> Create();
    virtual ~Interp() = default;
};

}

#endif
