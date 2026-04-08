#include "starbytes/compiler/Gen.h"
#include "starbytes/compiler/Parser.h"
#include "starbytes/compiler/RTCode.h"
#include "starbytes/runtime/RTDisasm.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

int fail(const char *message) {
    std::cerr << "SlotLocalsPhase2Test failure: " << message << '\n';
    return 1;
}

std::string rtidToString(const starbytes::Runtime::RTID &id) {
    return std::string(id.value,id.len);
}

bool contains(const std::string &text,const char *needle) {
    return text.find(needle) != std::string::npos;
}

}

int main() {
    using namespace starbytes;
    using namespace starbytes::Runtime;

    const char *source = R"starb(
func slotDemo(seed:Int) Int {
    decl total:Int = seed
    decl present:Int? = seed
    decl idx:Int = 0
    while(idx < 3){
        total += idx
        idx += 1
    }
    secure(decl kept = present) catch(hit:String) {
        total = -1
    }
    decl missing:Int?
    secure(decl fallback = missing) catch(miss:String) {
        total += 1
    }
    return total + kept
}
)starb";

    const auto outputFile = std::filesystem::current_path() / "slot_locals_phase2_test.stbxm";
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
        auto genContext = ModuleGenContext::Create("SlotLocalsPhase2",out,currentDir);
        gen.setContext(&genContext);

        Parser parser(gen);
        ModuleParseContext parseContext = ModuleParseContext::Create("SlotLocalsPhase2");
        std::istringstream in(source);
        parser.parseFromStream(in,parseContext);
        if(!parser.finish()) {
            cleanup();
            return fail("parser failed to compile slot locals source");
        }
        gen.finish();
    }

    std::ifstream moduleIn(outputFile,std::ios::in | std::ios::binary);
    if(!moduleIn.is_open()) {
        cleanup();
        return fail("unable to reopen generated module");
    }

    RTCode code = CODE_MODULE_END;
    auto moduleHeader = prepareRTModuleStream(moduleIn);
    if(!moduleHeader.hasExplicitHeader || moduleHeader.bytecodeVersion != RTBYTECODE_VERSION_V1) {
        cleanup();
        return fail("expected explicit Bytecode V1 module header");
    }
    if(!moduleIn.read((char *)&code,sizeof(code)) || code != CODE_RTFUNC) {
        cleanup();
        return fail("expected runtime function template at module start");
    }

    RTFuncTemplate funcTemplate;
    moduleIn >> &funcTemplate;

    if(rtidToString(funcTemplate.name) != "slotDemo") {
        cleanup();
        return fail("unexpected function template name");
    }
    if(funcTemplate.argsTemplate.size() != 1 || rtidToString(funcTemplate.argsTemplate.front()) != "seed") {
        cleanup();
        return fail("function argument slots were not preserved");
    }

    const std::vector<std::string> expectedLocalNames = {
        "total",
        "present",
        "idx",
        "kept",
        "hit",
        "missing",
        "fallback",
        "miss"
    };
    if(funcTemplate.localSlotNames.size() != expectedLocalNames.size()) {
        cleanup();
        return fail("unexpected number of local slot names");
    }
    for(size_t i = 0;i < expectedLocalNames.size();++i) {
        if(rtidToString(funcTemplate.localSlotNames[i]) != expectedLocalNames[i]) {
            cleanup();
            return fail("local slot names were emitted in the wrong order");
        }
    }

    std::ifstream disasmIn(outputFile,std::ios::in | std::ios::binary);
    std::ostringstream disasmOut;
    runDisasm(disasmIn,disasmOut);
    auto disasmText = disasmOut.str();
    if(!contains(disasmText,"CODE_RTLOCAL_DECL")) {
        cleanup();
        return fail("disassembly is missing local declaration opcode");
    }
    if(!contains(disasmText,"CODE_RTLOCAL_REF") && !contains(disasmText,"CODE_RTTYPED_LOCAL_REF")) {
        cleanup();
        return fail("disassembly is missing local reference opcode");
    }
    if(!contains(disasmText,"CODE_RTLOCAL_SET")) {
        cleanup();
        return fail("disassembly is missing local set opcode");
    }
    if(!contains(disasmText,"CODE_RTSECURE_LOCAL_DECL")) {
        cleanup();
        return fail("disassembly is missing secure local declaration opcode");
    }

    cleanup();
    return 0;
}
