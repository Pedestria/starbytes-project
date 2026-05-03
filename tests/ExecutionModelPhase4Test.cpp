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
    std::cerr << "ExecutionModelPhase4Test failure: " << message << '\n';
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
func phase4Kernel(values:Double[],limit:Int,steps:Int) Double {
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
print(phase4Kernel(values,4,12))
)starb";

    const auto outputFile = std::filesystem::current_path() / "execution_model_phase4_test.stbxm";
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
        auto genContext = ModuleGenContext::Create("ExecutionModelPhase4",out,currentDir);
        genContext.bytecodeVersion = RTBYTECODE_VERSION_V2;
        gen.setContext(&genContext);

        Parser parser(gen);
        ModuleParseContext parseContext = ModuleParseContext::Create("ExecutionModelPhase4");
        std::istringstream in(source);
        parser.parseFromStream(in,parseContext);
        if(!parser.finish()) {
            cleanup();
            return fail("parser failed to compile phase 4 source");
        }
        gen.finish();
    }

    auto runScenario = [&](bool jitEnabled,RuntimeProfileData &profileOut,std::string &outputOut) -> const char * {
        auto interp = Runtime::Interp::Create();
        interp->setProfilingEnabled(true);
        interp->setJitEnabled(jitEnabled);

        std::ifstream in(outputFile,std::ios::in | std::ios::binary);
        if(!in.is_open()) {
            return "unable to open compiled module file";
        }

        std::ostringstream captured;
        auto *savedBuf = std::cout.rdbuf(captured.rdbuf());
        interp->exec(in);
        std::cout.rdbuf(savedBuf);

        if(interp->hasRuntimeError()) {
            auto message = interp->takeRuntimeError();
            std::cerr << message << '\n';
            return "interpreter reported runtime error";
        }

        profileOut = interp->getProfileData();
        outputOut = captured.str();
        return nullptr;
    };

    // Scenario 1: JIT enabled (default). Compilation triggers, OSR executes,
    // code cache holds artifacts, no deopts, output is correct.
    {
        RuntimeProfileData profile;
        std::string output;
        if(auto *err = runScenario(true,profile,output)){
            cleanup();
            return fail(err);
        }
        if(profile.executionPath != RuntimeExecutionPath::V2RegisterInterpreter){
            cleanup();
            return fail("scenario 1: expected V2 register interpreter execution");
        }
        if(profile.tier2LoopsLowered == 0){
            cleanup();
            return fail("scenario 1: expected at least one Tier 2 IR lowering");
        }
        if(profile.tier2CompiledLoops == 0){
            cleanup();
            return fail("scenario 1: expected at least one compiled loop");
        }
        if(profile.tier2CompiledExecutions == 0){
            cleanup();
            return fail("scenario 1: expected the compiled loop to execute via OSR");
        }
        if(profile.codeCacheOptimizedArtifacts == 0){
            cleanup();
            return fail("scenario 1: expected code cache to hold an optimized artifact");
        }
        if(profile.codeCacheBytesUsed == 0){
            cleanup();
            return fail("scenario 1: expected code cache to report nonzero footprint");
        }
        if(profile.tier2DeoptCount != 0){
            cleanup();
            return fail("scenario 1: expected zero deopts on a stable typed-numeric kernel");
        }
        if(!contains(output,"432")){
            cleanup();
            std::cerr << output << '\n';
            return fail("scenario 1: unexpected output (expected 432)");
        }
    }

    // Scenario 2: JIT disabled. Tier 2 lowering still happens (Phase 3),
    // but no compilation and no OSR execution. Result must match.
    {
        RuntimeProfileData profile;
        std::string output;
        if(auto *err = runScenario(false,profile,output)){
            cleanup();
            return fail(err);
        }
        if(profile.tier2LoopsLowered == 0){
            cleanup();
            return fail("scenario 2: Phase 3 lowering should still run with JIT disabled");
        }
        if(profile.tier2CompiledLoops != 0){
            cleanup();
            return fail("scenario 2: expected no compiled loops when JIT disabled");
        }
        if(profile.tier2CompiledExecutions != 0){
            cleanup();
            return fail("scenario 2: expected no OSR execution when JIT disabled");
        }
        if(profile.codeCacheOptimizedArtifacts != 0){
            cleanup();
            return fail("scenario 2: expected an empty code cache when JIT disabled");
        }
        if(!contains(output,"432")){
            cleanup();
            std::cerr << output << '\n';
            return fail("scenario 2: unexpected output (expected 432)");
        }
    }

    cleanup();
    return 0;
}
