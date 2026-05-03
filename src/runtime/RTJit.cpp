#include "starbytes/runtime/RTJit.h"

#include <climits>
#include <cstdlib>
#include <unordered_map>

namespace starbytes::Runtime {

JitTarget selectJitTarget(){
    if(const char *env = std::getenv("STARBYTES_JIT_TARGET")){
        if(env[0] == '0' || env[0] == 'n' || env[0] == 'N' || env[0] == 'o'){
            return JitTarget::None;
        }
    }
    return JitTarget::PortableTemplate;
}

namespace {

JitOpKind translateKind(V2LoopIRKind kind){
    switch(kind){
        case V2LoopIRKind::Move:              return JitOpKind::Move;
        case V2LoopIRKind::LoadI64Const:      return JitOpKind::LoadI64Const;
        case V2LoopIRKind::LoadF64Const:      return JitOpKind::LoadF64Const;
        case V2LoopIRKind::NumCast:           return JitOpKind::NumCast;
        case V2LoopIRKind::Binary:            return JitOpKind::Binary;
        case V2LoopIRKind::CompareBranch:     return JitOpKind::CompareBranch;
        case V2LoopIRKind::ArrayGet:          return JitOpKind::ArrayGet;
        case V2LoopIRKind::ArraySet:          return JitOpKind::ArraySet;
        case V2LoopIRKind::ArrayUpdate:       return JitOpKind::ArrayUpdate;
        case V2LoopIRKind::CallDirect:        return JitOpKind::CallDirect;
        case V2LoopIRKind::IntrinsicSqrt:     return JitOpKind::IntrinsicSqrt;
        case V2LoopIRKind::Jump:              return JitOpKind::Jump;
        case V2LoopIRKind::GuardArrayNumeric: return JitOpKind::GuardArrayNumeric;
        case V2LoopIRKind::Deopt:             return JitOpKind::Move;
    }
    return JitOpKind::Move;
}

}

bool Tier2JitCompiler::compile(const V2LoopIR &ir, JitTarget target, CompiledLoop &out){
    out = CompiledLoop();
    out.target = target;
    out.headerPc = ir.headerPc;
    out.exitPc = ir.exitPc;
    out.backedgePc = ir.backedgePc;

    if(target == JitTarget::None){
        return false;
    }

    if(ir.instructions.empty()){
        return false;
    }

    std::unordered_map<uint32_t,uint32_t> pcToIp;
    out.ops.reserve(ir.instructions.size());

    for(const auto &irInstr : ir.instructions){
        if(irInstr.kind == V2LoopIRKind::GuardArrayNumeric){
            uint32_t deoptIndex = (uint32_t)out.deoptPoints.size();
            DeoptPoint dp;
            dp.ipAtFailure = (uint32_t)out.ops.size();
            dp.resumePc = irInstr.d != 0 ? irInstr.d : irInstr.c;
            dp.reason = (uint32_t)V2LoopIRKind::GuardArrayNumeric;
            out.deoptPoints.push_back(dp);

            JitOpRecord rec;
            rec.kind = JitOpKind::GuardArrayNumeric;
            rec.numericKind = irInstr.numericKind;
            rec.aux = irInstr.aux;
            rec.a = irInstr.a;
            rec.b = irInstr.b;
            rec.c = irInstr.c;
            rec.d = irInstr.d;
            rec.deoptIndex = deoptIndex;
            rec.originalPc = irInstr.originalV2Pc;
            out.ops.push_back(rec);
            continue;
        }
        if(irInstr.kind == V2LoopIRKind::Deopt){
            // The Deopt IR pseudo-op is paired with the preceding guard.
            // We've already recorded the DeoptPoint from the guard; skip here.
            continue;
        }

        JitOpRecord rec;
        rec.kind = translateKind(irInstr.kind);
        rec.numericKind = irInstr.numericKind;
        rec.aux = irInstr.aux;
        rec.flags = irInstr.flags;
        rec.a = irInstr.a;
        rec.b = irInstr.b;
        rec.c = irInstr.c;
        rec.d = irInstr.d;
        rec.branchIp = UINT32_MAX;
        rec.branchExitPc = 0;
        rec.deoptIndex = UINT32_MAX;
        rec.originalPc = irInstr.originalV2Pc;
        out.ops.push_back(rec);
    }

    // Build PC -> ip map. The IR was emitted in PC order from headerPc..backedgePc,
    // but a single V2 PC can produce multiple IR ops (guard + deopt + body).
    // The branch-target PC for a CompareBranch always refers to a V2 PC; map it
    // to the first JIT op produced for that V2 PC. For ArrayGet/Set/Update,
    // the first op produced is the GuardArrayNumeric.
    {
        uint32_t ip = 0;
        for(const auto &irInstr : ir.instructions){
            if(irInstr.kind == V2LoopIRKind::Deopt){
                continue;
            }
            // The original V2 PC for non-array, non-guard ops is not in the IR
            // operand; we can only map known PCs (headerPc, backedgePc, and
            // PCs encoded in guard ops). For our purposes the only branch
            // target we need to translate is the loop header (backedge target).
            ip += 1;
        }
        // The backedge Jump targets headerPc and resumes at ip = 0.
        pcToIp[ir.headerPc] = 0;
    }

    // Resolve branch targets.
    for(auto &rec : out.ops){
        if(rec.kind == JitOpKind::Jump){
            uint32_t target = rec.a;
            auto it = pcToIp.find(target);
            if(it != pcToIp.end()){
                rec.branchIp = it->second;
            } else {
                rec.branchIp = UINT32_MAX;
                rec.branchExitPc = target;
            }
        }
        else if(rec.kind == JitOpKind::CompareBranch){
            uint32_t target = rec.c;
            auto it = pcToIp.find(target);
            if(it != pcToIp.end()){
                rec.branchIp = it->second;
            } else {
                rec.branchIp = UINT32_MAX;
                rec.branchExitPc = target;
            }
        }
    }

    return true;
}

}
