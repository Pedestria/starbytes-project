#include "RuntimeProfile.h"

#include "starbytes/compiler/RTCode.h"

#include <algorithm>
#include <iomanip>
#include <ostream>
#include <sstream>
#include <vector>

namespace {

double nsToMs(uint64_t ns) {
    return static_cast<double>(ns) / 1000000.0;
}

std::string jsonEscape(const std::string &text) {
    std::ostringstream out;
    for(char ch : text) {
        switch(ch) {
            case '\"': out << "\\\""; break;
            case '\\': out << "\\\\"; break;
            case '\b': out << "\\b"; break;
            case '\f': out << "\\f"; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default:
                if(static_cast<unsigned char>(ch) < 0x20) {
                    out << "\\u"
                        << std::hex << std::setw(4) << std::setfill('0')
                        << static_cast<int>(static_cast<unsigned char>(ch))
                        << std::dec << std::setfill(' ');
                }
                else {
                    out << ch;
                }
                break;
        }
    }
    return out.str();
}

const char *opcodeName(uint8_t code) {
    using namespace starbytes::Runtime;
    switch(code) {
        case CODE_MODULE_END: return "CODE_MODULE_END";
        case CODE_RTVAR: return "CODE_RTVAR";
        case CODE_RTFUNC: return "CODE_RTFUNC";
        case CODE_RTCLASS_DEF: return "CODE_RTCLASS_DEF";
        case CODE_RTOBJCREATE: return "CODE_RTOBJCREATE";
        case CODE_RTINTOBJCREATE: return "CODE_RTINTOBJCREATE";
        case CODE_RTIVKFUNC: return "CODE_RTIVKFUNC";
        case CODE_RTRETURN: return "CODE_RTRETURN";
        case CODE_RTFUNCBLOCK_BEGIN: return "CODE_RTFUNCBLOCK_BEGIN";
        case CODE_RTFUNCBLOCK_END: return "CODE_RTFUNCBLOCK_END";
        case CODE_RTVAR_REF: return "CODE_RTVAR_REF";
        case CODE_RTOBJVAR_REF: return "CODE_RTOBJVAR_REF";
        case CODE_CONDITIONAL: return "CODE_CONDITIONAL";
        case CODE_CONDITIONAL_END: return "CODE_CONDITIONAL_END";
        case CODE_RTFUNC_REF: return "CODE_RTFUNC_REF";
        case CODE_UNARY_OPERATOR: return "CODE_UNARY_OPERATOR";
        case CODE_BINARY_OPERATOR: return "CODE_BINARY_OPERATOR";
        case CODE_RTNEWOBJ: return "CODE_RTNEWOBJ";
        case CODE_RTMEMBER_GET: return "CODE_RTMEMBER_GET";
        case CODE_RTMEMBER_GET_FIELD_SLOT: return "CODE_RTMEMBER_GET_FIELD_SLOT";
        case CODE_RTMEMBER_SET: return "CODE_RTMEMBER_SET";
        case CODE_RTMEMBER_SET_FIELD_SLOT: return "CODE_RTMEMBER_SET_FIELD_SLOT";
        case CODE_RTMEMBER_IVK: return "CODE_RTMEMBER_IVK";
        case CODE_RTSECURE_DECL: return "CODE_RTSECURE_DECL";
        case CODE_RTREGEX_LITERAL: return "CODE_RTREGEX_LITERAL";
        case CODE_RTVAR_SET: return "CODE_RTVAR_SET";
        case CODE_RTINDEX_GET: return "CODE_RTINDEX_GET";
        case CODE_RTINDEX_SET: return "CODE_RTINDEX_SET";
        case CODE_RTDICT_LITERAL: return "CODE_RTDICT_LITERAL";
        case CODE_RTTYPECHECK: return "CODE_RTTYPECHECK";
        case CODE_RTTERNARY: return "CODE_RTTERNARY";
        case CODE_RTCAST: return "CODE_RTCAST";
        case CODE_RTARRAY_LITERAL: return "CODE_RTARRAY_LITERAL";
        case CODE_RTLOCAL_REF: return "CODE_RTLOCAL_REF";
        case CODE_RTLOCAL_SET: return "CODE_RTLOCAL_SET";
        case CODE_RTLOCAL_DECL: return "CODE_RTLOCAL_DECL";
        case CODE_RTSECURE_LOCAL_DECL: return "CODE_RTSECURE_LOCAL_DECL";
        case CODE_RTTYPED_LOCAL_REF: return "CODE_RTTYPED_LOCAL_REF";
        case CODE_RTTYPED_BINARY: return "CODE_RTTYPED_BINARY";
        case CODE_RTTYPED_NEGATE: return "CODE_RTTYPED_NEGATE";
        case CODE_RTTYPED_COMPARE: return "CODE_RTTYPED_COMPARE";
        case CODE_RTTYPED_INDEX_GET: return "CODE_RTTYPED_INDEX_GET";
        case CODE_RTTYPED_INDEX_SET: return "CODE_RTTYPED_INDEX_SET";
        case CODE_RTTYPED_INTRINSIC: return "CODE_RTTYPED_INTRINSIC";
        case CODE_RTCALL_DIRECT: return "CODE_RTCALL_DIRECT";
        case CODE_RTCALL_BUILTIN_MEMBER: return "CODE_RTCALL_BUILTIN_MEMBER";
        default: return "CODE_UNKNOWN";
    }
}

const char *subsystemName(starbytes::Runtime::RuntimeProfileSubsystem subsystem) {
    using starbytes::Runtime::RuntimeProfileSubsystem;
    switch(subsystem) {
        case RuntimeProfileSubsystem::FunctionCall: return "function_call";
        case RuntimeProfileSubsystem::NativeCall: return "native_call";
        case RuntimeProfileSubsystem::MemberAccess: return "member_access";
        case RuntimeProfileSubsystem::IndexAccess: return "index_access";
        case RuntimeProfileSubsystem::ObjectAllocation: return "object_allocation";
        case RuntimeProfileSubsystem::Microtasks: return "microtasks";
        case RuntimeProfileSubsystem::Count: return "count";
    }
    return "unknown";
}

const char *objectKindName(StarbytesRuntimeObjectKind kind) {
    switch(kind) {
        case StarbytesRuntimeObjectKindString: return "string";
        case StarbytesRuntimeObjectKindArray: return "array";
        case StarbytesRuntimeObjectKindDict: return "dict";
        case StarbytesRuntimeObjectKindNumber: return "number";
        case StarbytesRuntimeObjectKindBool: return "bool";
        case StarbytesRuntimeObjectKindFuncRef: return "func_ref";
        case StarbytesRuntimeObjectKindRegex: return "regex";
        case StarbytesRuntimeObjectKindTask: return "task";
        case StarbytesRuntimeObjectKindCustomClass: return "custom_class";
        case StarbytesRuntimeObjectKindCount: return "count";
    }
    return "unknown";
}

}

namespace starbytes::driver::profile {

void printRuntimeProfileJson(std::ostream &out,const RuntimeProfileReport &report) {
    if(!report.enabled || !report.runtime.enabled) {
        return;
    }

    out << std::fixed << std::setprecision(3);
    out << "{\n";
    out << "  \"type\": \"runtime_profile\",\n";
    out << "  \"version\": 1,\n";
    out << "  \"command\": \"" << jsonEscape(report.command) << "\",\n";
    out << "  \"success\": " << (report.success ? "true" : "false") << ",\n";
    out << "  \"input\": \"" << jsonEscape(report.input) << "\",\n";
    out << "  \"module\": \"" << jsonEscape(report.moduleName) << "\",\n";
    out << "  \"runtime_error\": \"" << jsonEscape(report.runtimeError) << "\",\n";
    out << "  \"overlap_rules\": {\n";
    out << "    \"subsystems_are_inclusive\": " << (report.runtime.subsystemTimingsOverlap ? "true" : "false") << ",\n";
    out << "    \"function_totals_are_inclusive\": " << (report.runtime.functionTimingsOverlap ? "true" : "false") << "\n";
    out << "  },\n";
    out << "  \"counts\": {\n";
    out << "    \"dispatch\": " << report.runtime.dispatchCount << ",\n";
    out << "    \"function_calls\": " << report.runtime.functionCallCount << ",\n";
    out << "    \"refcount_increments\": " << report.runtime.refCountIncrements << ",\n";
    out << "    \"refcount_decrements\": " << report.runtime.refCountDecrements << ",\n";
    out << "    \"runtime_quickened_sites\": " << report.runtime.quickenedSitesInstalled << ",\n";
    out << "    \"runtime_quickened_executions\": " << report.runtime.quickenedExecutions << ",\n";
    out << "    \"runtime_quickened_specializations\": " << report.runtime.quickenedSpecializations << ",\n";
    out << "    \"runtime_quickened_fallbacks\": " << report.runtime.quickenedFallbacks << "\n";
    out << "  },\n";
    out << "  \"objects\": {\n";
    out << "    \"allocations\": [\n";
    bool firstAllocation = true;
    for(size_t i = 0; i < report.runtime.objectAllocations.size(); ++i) {
        auto count = report.runtime.objectAllocations[i];
        if(count == 0) {
            continue;
        }
        if(!firstAllocation) {
            out << ",\n";
        }
        firstAllocation = false;
        out << "      {\"kind\":\"" << objectKindName(static_cast<StarbytesRuntimeObjectKind>(i))
            << "\",\"count\":" << count << "}";
    }
    out << "\n    ],\n";
    out << "    \"deallocations\": [\n";
    bool firstDeallocation = true;
    for(size_t i = 0; i < report.runtime.objectDeallocations.size(); ++i) {
        auto count = report.runtime.objectDeallocations[i];
        if(count == 0) {
            continue;
        }
        if(!firstDeallocation) {
            out << ",\n";
        }
        firstDeallocation = false;
        out << "      {\"kind\":\"" << objectKindName(static_cast<StarbytesRuntimeObjectKind>(i))
            << "\",\"count\":" << count << "}";
    }
    out << "\n    ]\n";
    out << "  },\n";
    out << "  \"timings_ms\": {\n";
    out << "    \"total_runtime\": " << nsToMs(report.runtime.totalRuntimeNs) << "\n";
    out << "  },\n";
    out << "  \"subsystems\": [\n";
    bool firstSubsystem = true;
    for(size_t i = 0; i < report.runtime.subsystemNs.size(); ++i) {
        auto totalNs = report.runtime.subsystemNs[i];
        if(totalNs == 0) {
            continue;
        }
        if(!firstSubsystem) {
            out << ",\n";
        }
        firstSubsystem = false;
        out << "    {\"name\":\"" << subsystemName(static_cast<Runtime::RuntimeProfileSubsystem>(i))
            << "\",\"total_ms\":" << nsToMs(totalNs) << "}";
    }
    out << "\n  ],\n";
    out << "  \"opcodes\": [\n";
    bool firstOpcode = true;
    for(size_t code = 0; code < report.runtime.opcodeExecutions.size(); ++code) {
        auto count = report.runtime.opcodeExecutions[code];
        if(count == 0) {
            continue;
        }
        if(!firstOpcode) {
            out << ",\n";
        }
        firstOpcode = false;
        out << "    {\"code\":" << code
            << ",\"name\":\"" << opcodeName(static_cast<uint8_t>(code))
            << "\",\"count\":" << count << "}";
    }
    out << "\n  ],\n";
    out << "  \"functions\": [\n";
    auto functions = report.runtime.functionStats;
    std::sort(functions.begin(),functions.end(),[](const auto &lhs,const auto &rhs){
        if(lhs.totalNs != rhs.totalNs) {
            return lhs.totalNs > rhs.totalNs;
        }
        if(lhs.selfNs != rhs.selfNs) {
            return lhs.selfNs > rhs.selfNs;
        }
        if(lhs.callCount != rhs.callCount) {
            return lhs.callCount > rhs.callCount;
        }
        return lhs.name < rhs.name;
    });
    bool firstFunction = true;
    for(const auto &entry : functions) {
        if(!firstFunction) {
            out << ",\n";
        }
        firstFunction = false;
        out << "    {\"name\":\"" << jsonEscape(entry.name)
            << "\",\"calls\":" << entry.callCount
            << ",\"total_ms\":" << nsToMs(entry.totalNs)
            << ",\"self_ms\":" << nsToMs(entry.selfNs) << "}";
    }
    out << "\n  ]\n";
    out << "}\n";
    out.unsetf(std::ios::floatfield);
}

void printRuntimeProfileSummary(std::ostream &out,
                                const RuntimeProfileReport &report,
                                size_t maxSubsystems,
                                size_t maxFunctions) {
    if(!report.enabled || !report.runtime.enabled) {
        return;
    }

    out << std::fixed << std::setprecision(3);
    out << "Runtime Profile Summary\n";
    out << "total runtime: " << nsToMs(report.runtime.totalRuntimeNs) << " ms\n";
    out << "dispatch count: " << report.runtime.dispatchCount << "\n";
    out << "function calls: " << report.runtime.functionCallCount << "\n";
    out << "refcount increments: " << report.runtime.refCountIncrements << "\n";
    out << "refcount decrements: " << report.runtime.refCountDecrements << "\n";
    if(report.runtime.subsystemTimingsOverlap) {
        out << "Note: subsystem timings are overlapping/inclusive and should not be summed.\n";
    }
    if(report.runtime.functionTimingsOverlap) {
        out << "Note: function totals are inclusive; parent and child time overlaps by design.\n";
    }

    struct SubsystemRow {
        Runtime::RuntimeProfileSubsystem subsystem = Runtime::RuntimeProfileSubsystem::FunctionCall;
        uint64_t totalNs = 0;
    };
    std::vector<SubsystemRow> subsystems;
    for(size_t i = 0; i < report.runtime.subsystemNs.size(); ++i) {
        auto totalNs = report.runtime.subsystemNs[i];
        if(totalNs == 0) {
            continue;
        }
        subsystems.push_back({static_cast<Runtime::RuntimeProfileSubsystem>(i), totalNs});
    }
    std::sort(subsystems.begin(),subsystems.end(),[](const auto &lhs,const auto &rhs){
        return lhs.totalNs > rhs.totalNs;
    });

    out << "Top subsystems:\n";
    if(subsystems.empty()) {
        out << "  (none)\n";
    }
    else {
        size_t limit = std::min(maxSubsystems,subsystems.size());
        for(size_t i = 0; i < limit; ++i) {
            out << "  " << subsystemName(subsystems[i].subsystem)
                << ": " << nsToMs(subsystems[i].totalNs) << " ms\n";
        }
    }

    auto functions = report.runtime.functionStats;
    std::sort(functions.begin(),functions.end(),[](const auto &lhs,const auto &rhs){
        if(lhs.totalNs != rhs.totalNs) {
            return lhs.totalNs > rhs.totalNs;
        }
        if(lhs.selfNs != rhs.selfNs) {
            return lhs.selfNs > rhs.selfNs;
        }
        if(lhs.callCount != rhs.callCount) {
            return lhs.callCount > rhs.callCount;
        }
        return lhs.name < rhs.name;
    });

    out << "Top functions:\n";
    if(functions.empty()) {
        out << "  (none)\n";
    }
    else {
        size_t limit = std::min(maxFunctions,functions.size());
        for(size_t i = 0; i < limit; ++i) {
            const auto &entry = functions[i];
            out << "  " << entry.name
                << ": calls=" << entry.callCount
                << ", total=" << nsToMs(entry.totalNs) << " ms"
                << ", self=" << nsToMs(entry.selfNs) << " ms\n";
        }
    }

    struct ObjectCountRow {
        StarbytesRuntimeObjectKind kind = StarbytesRuntimeObjectKindString;
        uint64_t allocations = 0;
        uint64_t deallocations = 0;
    };
    std::vector<ObjectCountRow> objectCounts;
    for(size_t i = 0; i < report.runtime.objectAllocations.size(); ++i) {
        auto allocations = report.runtime.objectAllocations[i];
        auto deallocations = report.runtime.objectDeallocations[i];
        if(allocations == 0 && deallocations == 0) {
            continue;
        }
        objectCounts.push_back({static_cast<StarbytesRuntimeObjectKind>(i), allocations, deallocations});
    }
    std::sort(objectCounts.begin(),objectCounts.end(),[](const auto &lhs,const auto &rhs){
        auto lhsTotal = lhs.allocations + lhs.deallocations;
        auto rhsTotal = rhs.allocations + rhs.deallocations;
        if(lhsTotal != rhsTotal) {
            return lhsTotal > rhsTotal;
        }
        if(lhs.allocations != rhs.allocations) {
            return lhs.allocations > rhs.allocations;
        }
        return lhs.deallocations > rhs.deallocations;
    });

    out << "Object counts:\n";
    if(objectCounts.empty()) {
        out << "  (none)\n";
    }
    else {
        for(const auto &entry : objectCounts) {
            out << "  " << objectKindName(entry.kind)
                << ": allocations=" << entry.allocations
                << ", deallocations=" << entry.deallocations << "\n";
        }
    }
    out.unsetf(std::ios::floatfield);
}

}
