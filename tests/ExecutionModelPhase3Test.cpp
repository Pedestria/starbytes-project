#include "starbytes/compiler/Gen.h"
#include "starbytes/compiler/Parser.h"
#include "starbytes/compiler/RTCode.h"
#include "starbytes/runtime/RTEngine.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {

int fail(const char *message) {
    std::cerr << "ExecutionModelPhase3Test failure: " << message << '\n';
    return 1;
}

bool contains(const std::string &text,const char *needle) {
    return text.find(needle) != std::string::npos;
}

}

int main() {
    using namespace starbytes;
    using namespace starbytes::Runtime;

    const char *source = R"starb(
func phase3Kernel(values:Double[],limit:Int,steps:Int) Double {
    decl step:Int = 0
    decl i:Int = 0
    decl total:Double = 0.0
    while(step < steps){
        i = 0
        while(i < limit){
            values[i] += 1.0
            total += values[i]
            i += 1
        }
        step += 1
    }
    return total
}

decl values:Double[] = [1.0,2.0,3.0,4.0]
print(phase3Kernel(values,4,12))
)starb";

    const auto outputFile = std::filesystem::current_path() / "execution_model_phase3_test.stbxm";
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
        auto genContext = ModuleGenContext::Create("ExecutionModelPhase3",out,currentDir);
        genContext.bytecodeVersion = RTBYTECODE_VERSION_V2;
        gen.setContext(&genContext);

        Parser parser(gen);
        ModuleParseContext parseContext = ModuleParseContext::Create("ExecutionModelPhase3");
        std::istringstream in(source);
        parser.parseFromStream(in,parseContext);
        if(!parser.finish()) {
            cleanup();
            return fail("parser failed to compile phase 3 source");
        }
        gen.finish();
    }

    auto interp = Runtime::Interp::Create();
    interp->setProfilingEnabled(true);
    interp->setJitEnabled(false);
    std::ifstream in(outputFile,std::ios::in | std::ios::binary);
    if(!in.is_open()) {
        cleanup();
        return fail("unable to open compiled module file");
    }

    std::ostringstream captured;
    auto *savedBuf = std::cout.rdbuf(captured.rdbuf());
    interp->exec(in);
    std::cout.rdbuf(savedBuf);

    if(interp->hasRuntimeError()) {
        auto message = interp->takeRuntimeError();
        cleanup();
        std::cerr << message << '\n';
        return fail("interpreter reported runtime error");
    }

    auto profile = interp->getProfileData();
    if(profile.executionPath != RuntimeExecutionPath::V2RegisterInterpreter) {
        cleanup();
        return fail("expected V2 register interpreter execution");
    }
    if(profile.v2ExecutionImagesBuilt == 0) {
        cleanup();
        return fail("expected a V2 execution image to be built");
    }
    if(profile.superinstructionsInstalled < 4) {
        cleanup();
        return fail("expected phase 3 superinstructions to be installed");
    }
    if(profile.superinstructionExecutions < 100) {
        cleanup();
        return fail("expected phase 3 superinstructions to execute frequently");
    }
    if(profile.loopHeadersTracked < 2) {
        cleanup();
        return fail("expected nested loop headers to be tracked");
    }
    if(profile.hotLoopTriggers < 2) {
        cleanup();
        return fail("expected hot loop detection to trigger");
    }
    if(profile.tier2LoopsLowered < 2) {
        cleanup();
        return fail("expected hot loops to lower into Tier 2 IR");
    }
    if(profile.tier2IrInstructionCount == 0) {
        cleanup();
        return fail("expected Tier 2 IR instructions to be recorded");
    }
    if(profile.loopGuardSamples == 0 || profile.loopGuardFailures != 0) {
        cleanup();
        return fail("expected loop guard stability samples without failures");
    }
    if(!contains(captured.str(),"432")) {
        cleanup();
        std::cerr << captured.str() << '\n';
        return fail("unexpected phase 3 output");
    }

    cleanup();
    return 0;
}
