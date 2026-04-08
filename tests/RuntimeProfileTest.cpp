#include "starbytes/compiler/Gen.h"
#include "starbytes/compiler/Parser.h"
#include "starbytes/compiler/RTCode.h"
#include "starbytes/runtime/RTEngine.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace {

int fail(const char *message) {
    std::cerr << "RuntimeProfileTest failure: " << message << '\n';
    return 1;
}

bool hasSiteKind(const starbytes::Runtime::RuntimeProfileData &profile,
                 starbytes::Runtime::RuntimeProfileSiteKind kind) {
    for(const auto &entry : profile.siteStats) {
        if(entry.kind == kind && entry.executionCount > 0) {
            return true;
        }
    }
    return false;
}

}

int main() {
    using namespace starbytes;

    const char *source = R"starb(
class Box {
    decl value:Int = 2

    func read() Int {
        return self.value
    }
}

func kernel(limit:Int) Int {
    decl i:Int = 0
    decl total:Int = 0
    decl box = new Box()
    decl values:Int[] = [3,4]
    while(i < limit){
        total += box.read()
        total += values[0]
        i += 1
    }
    return total
}

decl sink:Int = kernel(4)
)starb";

    const auto outputFile = std::filesystem::current_path() / "runtime_profile_test.stbxm";
    auto cleanup = [&]() {
        std::error_code ignored;
        std::filesystem::remove(outputFile,ignored);
    };
    cleanup();

    {
        std::ofstream out(outputFile,std::ios::out | std::ios::binary);
        if(!out.is_open()) {
            return fail("unable to open output module file");
        }

        auto currentDir = std::filesystem::current_path();
        Gen gen;
        auto genContext = ModuleGenContext::Create("RuntimeProfileTest",out,currentDir);
        gen.setContext(&genContext);

        Parser parser(gen);
        ModuleParseContext parseContext = ModuleParseContext::Create("RuntimeProfileTest");
        std::istringstream in(source);
        parser.parseFromStream(in,parseContext);
        if(!parser.finish()) {
            cleanup();
            return fail("parser failed to compile runtime profile source");
        }
        gen.finish();
    }

    auto interp = Runtime::Interp::Create();
    interp->setProfilingEnabled(true);
    std::ifstream in(outputFile,std::ios::in | std::ios::binary);
    if(!in.is_open()) {
        cleanup();
        return fail("unable to open compiled module file");
    }
    interp->exec(in);
    if(interp->hasRuntimeError()) {
        auto message = interp->takeRuntimeError();
        cleanup();
        std::cerr << message << '\n';
        return fail("interpreter reported runtime error");
    }

    auto profile = interp->getProfileData();
    if(!profile.enabled) {
        cleanup();
        return fail("runtime profile should be enabled");
    }
    if(profile.moduleBytecodeVersion != Runtime::RTBYTECODE_VERSION_V1 || !profile.moduleHasExplicitHeader) {
        cleanup();
        return fail("expected explicit Bytecode V1 module metadata");
    }
    if(profile.executionPath != Runtime::RuntimeExecutionPath::V1StreamInterpreter) {
        cleanup();
        return fail("expected V1 stream interpreter execution path");
    }
    if(profile.totalRuntimeNs == 0 || profile.dispatchCount == 0) {
        cleanup();
        return fail("expected total runtime and dispatch count");
    }
    if(profile.opcodeExecutions[CODE_RTNEWOBJ] == 0 || profile.opcodeExecutions[CODE_RTMEMBER_IVK] == 0) {
        cleanup();
        return fail("expected object allocation and member invoke opcode counts");
    }
    if(profile.subsystemNs[static_cast<size_t>(Runtime::RuntimeProfileSubsystem::FunctionCall)] == 0) {
        cleanup();
        return fail("expected function-call subsystem timing");
    }
    if(profile.subsystemNs[static_cast<size_t>(Runtime::RuntimeProfileSubsystem::MemberAccess)] == 0) {
        cleanup();
        return fail("expected member-access subsystem timing");
    }
    if(profile.subsystemNs[static_cast<size_t>(Runtime::RuntimeProfileSubsystem::ObjectAllocation)] == 0) {
        cleanup();
        return fail("expected object-allocation subsystem timing");
    }
    if(profile.refCountIncrements == 0 || profile.refCountDecrements == 0) {
        cleanup();
        return fail("expected refcount counters");
    }
    if(profile.objectAllocations[static_cast<size_t>(StarbytesRuntimeObjectKindCustomClass)] == 0) {
        cleanup();
        return fail("expected custom-class allocation counts");
    }
    if(profile.objectAllocations[static_cast<size_t>(StarbytesRuntimeObjectKindNumber)] == 0) {
        cleanup();
        return fail("expected number allocation counts");
    }
    if(profile.objectDeallocations[static_cast<size_t>(StarbytesRuntimeObjectKindCustomClass)] == 0) {
        cleanup();
        return fail("expected custom-class deallocation counts");
    }
    if(!hasSiteKind(profile,Runtime::RuntimeProfileSiteKind::Call)) {
        cleanup();
        return fail("expected per-site call counters");
    }
    if(!hasSiteKind(profile,Runtime::RuntimeProfileSiteKind::Member)) {
        cleanup();
        return fail("expected per-site member counters");
    }
    if(!hasSiteKind(profile,Runtime::RuntimeProfileSiteKind::Index)) {
        cleanup();
        return fail("expected per-site index counters");
    }
    if(!hasSiteKind(profile,Runtime::RuntimeProfileSiteKind::Branch)) {
        cleanup();
        return fail("expected per-site branch counters");
    }

    bool sawKernel = false;
    for(const auto &entry : profile.functionStats) {
        if(entry.name == "kernel" && entry.callCount > 0 && entry.totalNs > 0) {
            sawKernel = true;
            break;
        }
    }
    if(!sawKernel) {
        cleanup();
        return fail("expected kernel function profile entry");
    }

    cleanup();
    return 0;
}
