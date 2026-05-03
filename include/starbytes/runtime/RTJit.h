#ifndef STARBYTES_RT_RTJIT_H
#define STARBYTES_RT_RTJIT_H

#include <cstddef>
#include <cstdint>
#include <vector>

namespace starbytes::Runtime {

enum class V2LoopIRKind : uint8_t {
    Move = 0,
    LoadI64Const,
    LoadF64Const,
    NumCast,
    Binary,
    CompareBranch,
    ArrayGet,
    ArraySet,
    ArrayUpdate,
    CallDirect,
    IntrinsicSqrt,
    Jump,
    GuardArrayNumeric,
    Deopt
};

struct V2LoopIRInstruction {
    V2LoopIRKind kind = V2LoopIRKind::Move;
    uint8_t numericKind = 0;
    uint8_t aux = 0;
    uint8_t flags = 0;
    uint32_t a = 0;
    uint32_t b = 0;
    uint32_t c = 0;
    uint32_t d = 0;
    uint32_t e = 0;
    uint32_t originalV2Pc = 0;
};

struct V2LoopIR {
    uint32_t headerPc = 0;
    uint32_t exitPc = 0;
    uint32_t backedgePc = 0;
    std::vector<V2LoopIRInstruction> instructions;
};

enum class JitTarget : uint8_t {
    None = 0,
    PortableTemplate,
    X86_64_CopyPatch,
    Arm64_CopyPatch,
};

JitTarget selectJitTarget();

enum class JitOpKind : uint8_t {
    Move = 0,
    LoadI64Const,
    LoadF64Const,
    NumCast,
    Binary,
    CompareBranch,
    ArrayGet,
    ArraySet,
    ArrayUpdate,
    CallDirect,
    IntrinsicSqrt,
    Jump,
    GuardArrayNumeric,
};

struct JitOpRecord {
    JitOpKind kind = JitOpKind::Move;
    uint8_t numericKind = 0;
    uint8_t aux = 0;
    uint8_t flags = 0;
    uint32_t a = 0;
    uint32_t b = 0;
    uint32_t c = 0;
    uint32_t d = 0;
    uint32_t branchIp = UINT32_MAX;
    uint32_t branchExitPc = 0;
    uint32_t deoptIndex = UINT32_MAX;
    uint32_t originalPc = 0;
};

struct DeoptPoint {
    uint32_t ipAtFailure = 0;
    uint32_t resumePc = 0;
    uint32_t reason = 0;
};

struct CompiledLoop {
    JitTarget target = JitTarget::None;
    uint32_t headerPc = 0;
    uint32_t exitPc = 0;
    uint32_t backedgePc = 0;
    bool deoptimized = false;
    uint32_t deoptVersion = 0;
    uint64_t executionCount = 0;
    uint64_t deoptCount = 0;
    std::vector<JitOpRecord> ops;
    std::vector<DeoptPoint> deoptPoints;
    std::size_t arenaBytes = 0;
};

enum class JitExitStatus : uint8_t {
    NormalExit = 0,
    Deopt,
    Fail,
    Return,
};

struct JitExitInfo {
    JitExitStatus status = JitExitStatus::NormalExit;
    uint32_t resumePc = 0;
};

class Tier2JitCompiler {
public:
    static bool compile(const V2LoopIR &ir, JitTarget target, CompiledLoop &out);
};

}

#endif
