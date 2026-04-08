#include "starbytes/compiler/Gen.h"
#include "starbytes/compiler/Parser.h"
#include "starbytes/compiler/RTCode.h"
#include "starbytes/runtime/RTEngine.h"
#include "starbytes/runtime/RTDisasm.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

namespace {

int fail(const char *message) {
    std::cerr << "BytecodeV2Phase1Test failure: " << message << '\n';
    return 1;
}

bool contains(const std::string &text,const char *needle) {
    return text.find(needle) != std::string::npos;
}

bool runModule(const std::filesystem::path &modulePath,
               starbytes::Runtime::RuntimeExecutionMode mode,
               std::string &outputOut,
               std::string &runtimeErrorOut,
               starbytes::Runtime::RuntimeProfileData &profileOut) {
    auto interp = starbytes::Runtime::Interp::Create();
    interp->setExecutionMode(mode);
    interp->setProfilingEnabled(true);

    std::ifstream in(modulePath,std::ios::in | std::ios::binary);
    if(!in.is_open()) {
        runtimeErrorOut = "unable to open module";
        return false;
    }

    std::ostringstream captured;
    auto *savedBuf = std::cout.rdbuf(captured.rdbuf());
    interp->exec(in);
    std::cout.rdbuf(savedBuf);

    outputOut = captured.str();
    if(interp->hasRuntimeError()) {
        runtimeErrorOut = interp->takeRuntimeError();
        return false;
    }

    profileOut = interp->getProfileData();
    return true;
}

}

int main() {
    using namespace starbytes;
    using namespace starbytes::Runtime;

    const char *source = R"starb(
func A(i:Int,j:Int) Double {
    decl ij:Int = i + j
    decl denom:Int = ((ij * (ij + 1) / 2) + i + 1)
    return 1.0 / Double(denom)
}

decl n:Int = 4
decl u:Double[] = [1.0, 2.0, 3.0, 4.0]
decl tmp:Double[] = [0.0, 0.0, 0.0, 0.0]
decl i:Int = 0
decl total:Double = 0.0

while(i < n){
    tmp[i] = A(i, i) * u[i]
    total += tmp[i]
    i += 1
}

print(total)
)starb";

    const auto moduleFile = std::filesystem::current_path() / "bytecode_v2_phase1_test.stbxm";
    auto cleanup = [&]() {
        std::error_code ignored;
        std::filesystem::remove(moduleFile,ignored);
    };
    cleanup();

    {
        std::ofstream out(moduleFile,std::ios::out | std::ios::binary);
        if(!out.is_open()) {
            return fail("unable to create V2 module output file");
        }

        auto currentDir = std::filesystem::current_path();
        Gen gen;
        auto genContext = ModuleGenContext::Create("BytecodeV2Phase1",out,currentDir);
        genContext.bytecodeVersion = RTBYTECODE_VERSION_V2;
        gen.setContext(&genContext);

        Parser parser(gen);
        ModuleParseContext parseContext = ModuleParseContext::Create("BytecodeV2Phase1");
        std::istringstream in(source);
        parser.parseFromStream(in,parseContext);
        if(!parser.finish()) {
            cleanup();
            return fail("parser failed to compile V2 phase 1 source");
        }
        gen.finish();
    }

    {
        std::ifstream moduleIn(moduleFile,std::ios::in | std::ios::binary);
        if(!moduleIn.is_open()) {
            cleanup();
            return fail("unable to reopen V2 module");
        }
        auto header = prepareRTModuleStream(moduleIn);
        if(!header.hasExplicitHeader || header.bytecodeVersion != RTBYTECODE_VERSION_V2) {
            cleanup();
            return fail("expected explicit Bytecode V2 header");
        }
    }

    {
        std::ifstream disasmIn(moduleFile,std::ios::in | std::ios::binary);
        if(!disasmIn.is_open()) {
            cleanup();
            return fail("unable to reopen V2 module for disasm");
        }
        std::ostringstream disasmOut;
        runDisasm(disasmIn,disasmOut);
        auto text = disasmOut.str();
        if(!contains(text,"CODE_RTFUNCBLOCK_BEGIN_V2") || !contains(text,"RTV2_OP")) {
            cleanup();
            return fail("V2 disassembly did not expose the V2 function body");
        }
    }

    {
        std::string output;
        std::string runtimeError;
        RuntimeProfileData profile;
        if(!runModule(moduleFile,RuntimeExecutionMode::Auto,output,runtimeError,profile)) {
            cleanup();
            std::cerr << runtimeError << '\n';
            return fail("V2 module execution failed in auto mode");
        }
        if(profile.moduleBytecodeVersion != RTBYTECODE_VERSION_V2) {
            cleanup();
            return fail("runtime profile did not report Bytecode V2");
        }
        if(profile.executionPath != RuntimeExecutionPath::V2RegisterInterpreter) {
            cleanup();
            return fail("runtime profile did not report V2 register interpreter execution");
        }
        if(!contains(output,"1.79077")) {
            cleanup();
            std::cerr << output << '\n';
            return fail("V2 module output did not match the expected Phase 1 result");
        }
    }

    {
        std::string output;
        std::string runtimeError;
        RuntimeProfileData profile;
        if(runModule(moduleFile,RuntimeExecutionMode::V1,output,runtimeError,profile)) {
            cleanup();
            return fail("V1 runtime mode unexpectedly accepted a V2 module");
        }
        if(!contains(runtimeError,"runtime mode `v1` cannot execute Bytecode V2 modules")) {
            cleanup();
            std::cerr << runtimeError << '\n';
            return fail("unexpected V1-on-V2 runtime error");
        }
    }

    cleanup();
    return 0;
}
