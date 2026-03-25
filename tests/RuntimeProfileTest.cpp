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
    while(i < limit){
        total += box.read()
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
