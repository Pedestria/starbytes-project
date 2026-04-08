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
    std::cerr << "BytecodeVersionCompatibilityTest failure: " << message << '\n';
    return 1;
}

bool contains(const std::string &text,const char *needle) {
    return text.find(needle) != std::string::npos;
}

bool runModule(const std::filesystem::path &modulePath,
               bool expectExplicitHeader,
               starbytes::Runtime::RuntimeExecutionMode mode,
               std::string &runtimeErrorOut) {
    auto interp = starbytes::Runtime::Interp::Create();
    interp->setExecutionMode(mode);
    interp->setProfilingEnabled(true);

    std::ifstream in(modulePath,std::ios::in | std::ios::binary);
    if(!in.is_open()) {
        runtimeErrorOut = "unable to open module";
        return false;
    }

    interp->exec(in);
    if(interp->hasRuntimeError()) {
        runtimeErrorOut = interp->takeRuntimeError();
        return false;
    }

    auto profile = interp->getProfileData();
    if(profile.moduleHasExplicitHeader != expectExplicitHeader) {
        runtimeErrorOut = "unexpected module header mode";
        return false;
    }
    if(profile.moduleBytecodeVersion != starbytes::Runtime::RTBYTECODE_VERSION_V1) {
        runtimeErrorOut = "unexpected bytecode version";
        return false;
    }
    if(profile.executionPath != starbytes::Runtime::RuntimeExecutionPath::V1StreamInterpreter) {
        runtimeErrorOut = "unexpected runtime execution path";
        return false;
    }
    return true;
}

}

int main() {
    using namespace starbytes;
    using namespace starbytes::Runtime;

    const char *source = R"starb(
func add(left:Int,right:Int) Int {
    return left + right
}

decl sink:Int = add(2,3)
)starb";

    const auto versionedFile = std::filesystem::current_path() / "bytecode_version_compatibility_test.stbxm";
    const auto legacyFile = std::filesystem::current_path() / "bytecode_version_compatibility_test_legacy.stbxm";
    auto cleanup = [&]() {
        std::error_code ignored;
        std::filesystem::remove(versionedFile,ignored);
        std::filesystem::remove(legacyFile,ignored);
    };
    cleanup();

    {
        std::ofstream out(versionedFile,std::ios::out | std::ios::binary);
        if(!out.is_open()) {
            return fail("unable to open versioned output file");
        }

        auto currentDir = std::filesystem::current_path();
        Gen gen;
        auto genContext = ModuleGenContext::Create("BytecodeVersionCompatibility",out,currentDir);
        gen.setContext(&genContext);

        Parser parser(gen);
        ModuleParseContext parseContext = ModuleParseContext::Create("BytecodeVersionCompatibility");
        std::istringstream in(source);
        parser.parseFromStream(in,parseContext);
        if(!parser.finish()) {
            cleanup();
            return fail("parser failed to compile compatibility source");
        }
        gen.finish();
    }

    {
        std::ifstream versionedIn(versionedFile,std::ios::in | std::ios::binary);
        if(!versionedIn.is_open()) {
            cleanup();
            return fail("unable to reopen versioned module");
        }
        auto header = prepareRTModuleStream(versionedIn);
        if(!header.hasExplicitHeader || header.bytecodeVersion != RTBYTECODE_VERSION_V1) {
            cleanup();
            return fail("expected explicit Bytecode V1 header");
        }
    }

    {
        std::ifstream versionedIn(versionedFile,std::ios::in | std::ios::binary);
        if(!versionedIn.is_open()) {
            cleanup();
            return fail("unable to read versioned module bytes");
        }
        std::vector<char> bytes((std::istreambuf_iterator<char>(versionedIn)),
                                std::istreambuf_iterator<char>());
        if(bytes.size() <= RTModuleHeaderByteCount) {
            cleanup();
            return fail("versioned module too small to contain header");
        }

        std::ofstream legacyOut(legacyFile,std::ios::out | std::ios::binary);
        if(!legacyOut.is_open()) {
            cleanup();
            return fail("unable to create legacy module");
        }
        legacyOut.write(bytes.data() + RTModuleHeaderByteCount,
                        (std::streamsize)(bytes.size() - RTModuleHeaderByteCount));
    }

    {
        std::string runtimeError;
        if(!runModule(versionedFile,true,RuntimeExecutionMode::V1,runtimeError)) {
            cleanup();
            std::cerr << runtimeError << '\n';
            return fail("versioned module execution failed");
        }
    }

    {
        std::string runtimeError;
        if(!runModule(legacyFile,false,RuntimeExecutionMode::V1,runtimeError)) {
            cleanup();
            std::cerr << runtimeError << '\n';
            return fail("legacy module execution failed");
        }
    }

    {
        std::ifstream legacyIn(legacyFile,std::ios::in | std::ios::binary);
        if(!legacyIn.is_open()) {
            cleanup();
            return fail("unable to reopen legacy module for disasm");
        }
        std::ostringstream disasmOut;
        runDisasm(legacyIn,disasmOut);
        auto text = disasmOut.str();
        if(!contains(text,"CODE_RTFUNC") || !contains(text,"CODE_RTCALL_DIRECT")) {
            cleanup();
            return fail("legacy module disassembly lost function contents");
        }
    }

    cleanup();
    return 0;
}
