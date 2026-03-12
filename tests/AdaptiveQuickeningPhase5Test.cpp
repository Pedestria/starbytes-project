#include "starbytes/compiler/Gen.h"
#include "starbytes/compiler/Parser.h"
#include "starbytes/runtime/RTEngine.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {

int fail(const char *message) {
    std::cerr << "AdaptiveQuickeningPhase5Test failure: " << message << '\n';
    return 1;
}

}

int main() {
    using namespace starbytes;

    const char *source = R"starb(
func adaptiveKernel(values:Any[]) Double {
    decl left:Any = values[0]
    decl right:Any = values[1]
    decl zero:Any = 0.0
    decl total:Any = left + right
    decl totalNum:Double = Double(total)
    if(total > zero){
        return sqrt(totalNum)
    }
    return 0.0
}

func typedArrayKernel(values:Double[]) Double {
    decl i:Int = 0
    while(i < values.length){
        values[i] = values[i] + 1.0
        i += 1
    }
    return values[0]
}

decl dynamicValues:Any[] = [9.0,7.0]
decl typedValues:Double[] = [1.0,2.0,3.0]
decl iteration:Int = 0
decl sink:Double = 0.0
while(iteration < 8){
    sink += adaptiveKernel(dynamicValues)
    sink += typedArrayKernel(typedValues)
    iteration += 1
}
)starb";

    const auto outputFile = std::filesystem::current_path() / "adaptive_quickening_phase5_test.stbxm";
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
        auto genContext = ModuleGenContext::Create("AdaptiveQuickeningPhase5",out,currentDir);
        gen.setContext(&genContext);

        Parser parser(gen);
        ModuleParseContext parseContext = ModuleParseContext::Create("AdaptiveQuickeningPhase5");
        std::istringstream in(source);
        parser.parseFromStream(in,parseContext);
        if(!parser.finish()) {
            cleanup();
            return fail("parser failed to compile phase 5 source");
        }
        gen.finish();
    }

    auto interp = Runtime::Interp::Create();
    std::ifstream in(outputFile,std::ios::in | std::ios::binary);
    if(!in.is_open()) {
        cleanup();
        return fail("unable to open compiled module file");
    }
    interp->exec(in);
    if(interp->hasRuntimeError()) {
        cleanup();
        return fail(interp->takeRuntimeError().c_str());
    }

    auto profile = interp->getProfileData();
    if(profile.quickenedSitesInstalled == 0) {
        cleanup();
        return fail("expected quickened sites to be installed");
    }
    if(profile.quickenedExecutions == 0) {
        cleanup();
        return fail("expected quickened expressions to execute");
    }
    if(profile.quickenedSpecializations == 0) {
        cleanup();
        return fail("expected adaptive specializations to be recorded");
    }

    cleanup();
    return 0;
}
