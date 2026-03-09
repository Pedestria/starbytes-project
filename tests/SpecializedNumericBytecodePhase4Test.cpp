#include "starbytes/compiler/Gen.h"
#include "starbytes/compiler/Parser.h"
#include "starbytes/runtime/RTDisasm.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {

int fail(const char *message) {
    std::cerr << "SpecializedNumericBytecodePhase4Test failure: " << message << '\n';
    return 1;
}

bool contains(const std::string &text,const char *needle) {
    return text.find(needle) != std::string::npos;
}

}

int main() {
    using namespace starbytes;

    const char *source = R"starb(
func phase4Kernel(values:Float[]) Double {
    decl i:Int = 0
    decl sum:Float = 0.0
    while(i < values.length){
        sum += values[i] * values[i]
        i += 1
    }
    values[0] = Float(values[0] + 1.0)
    return sqrt(sum)
}
)starb";

    const auto outputFile = std::filesystem::current_path() / "specialized_numeric_bytecode_phase4_test.stbxm";
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
        auto genContext = ModuleGenContext::Create("SpecializedPhase4",out,currentDir);
        gen.setContext(&genContext);

        Parser parser(gen);
        ModuleParseContext parseContext = ModuleParseContext::Create("SpecializedPhase4");
        std::istringstream in(source);
        parser.parseFromStream(in,parseContext);
        if(!parser.finish()) {
            cleanup();
            return fail("parser failed to compile specialized phase 4 source");
        }
        gen.finish();
    }

    std::ifstream disasmIn(outputFile,std::ios::in | std::ios::binary);
    std::ostringstream disasmOut;
    Runtime::runDisasm(disasmIn,disasmOut);
    auto text = disasmOut.str();

    if(!contains(text,"CODE_RTTYPED_LOCAL_REF")) {
        cleanup();
        return fail("typed local reference opcode not emitted");
    }
    if(!contains(text,"CODE_RTTYPED_BINARY")) {
        cleanup();
        return fail("typed binary opcode not emitted");
    }
    if(!contains(text,"CODE_RTTYPED_COMPARE")) {
        cleanup();
        return fail("typed compare opcode not emitted");
    }
    if(!contains(text,"CODE_RTTYPED_INDEX_GET")) {
        cleanup();
        return fail("typed index get opcode not emitted");
    }
    if(!contains(text,"CODE_RTTYPED_INDEX_SET")) {
        cleanup();
        return fail("typed index set opcode not emitted");
    }
    if(!contains(text,"CODE_RTTYPED_INTRINSIC")) {
        cleanup();
        return fail("typed intrinsic opcode not emitted");
    }

    cleanup();
    return 0;
}
