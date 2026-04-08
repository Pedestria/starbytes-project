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
    std::cerr << "FeedbackVectorsPhase2Test failure: " << message << '\n';
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
class Counter {
    decl value:Int = 1

    func read() Int {
        return self.value
    }
}

func identity<T>(value:T) T {
    return value
}

func memberGetLoop(target:Counter,limit:Int) Int {
    decl i:Int = 0
    decl total:Int = 0
    while(i < limit){
        total += identity<Counter>(target).value
        i += 1
    }
    return total
}

func memberInvokeLoop(target:Counter,limit:Int) Int {
    decl i:Int = 0
    decl total:Int = 0
    while(i < limit){
        total += identity<Counter>(target).read()
        i += 1
    }
    return total
}

decl globalValue:Int = 4
decl counter = new Counter()
decl values:Int[] = [1,2,3]
decl i:Int = 0
decl lengthSum:Int = 0
while(i < 8){
    lengthSum += values.length
    lengthSum += globalValue
    i += 1
}

decl sink:Int = memberGetLoop(counter,8)
sink += memberInvokeLoop(counter,8)
print(sink + lengthSum)
)starb";

    const auto outputFile = std::filesystem::current_path() / "feedback_vectors_phase2_test.stbxm";
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
        auto genContext = ModuleGenContext::Create("FeedbackVectorsPhase2",out,currentDir);
        gen.setContext(&genContext);

        Parser parser(gen);
        ModuleParseContext parseContext = ModuleParseContext::Create("FeedbackVectorsPhase2");
        std::istringstream in(source);
        parser.parseFromStream(in,parseContext);
        if(!parser.finish()) {
            cleanup();
            return fail("parser failed to compile phase 2 source");
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
    if(profile.feedbackSitesInstalled == 0) {
        cleanup();
        return fail("expected feedback sites to be installed");
    }
    if(profile.feedbackCacheHits < 10) {
        cleanup();
        return fail("expected monomorphic feedback cache hits");
    }
    if(profile.opcodeExecutions[CODE_RTVAR_REF] == 0) {
        cleanup();
        return fail("expected scoped/global variable references");
    }
    if(profile.opcodeExecutions[CODE_RTMEMBER_GET] == 0) {
        cleanup();
        return fail("expected generic member gets");
    }
    if(profile.opcodeExecutions[CODE_RTMEMBER_IVK] == 0) {
        cleanup();
        return fail("expected generic member invokes");
    }
    if(!contains(captured.str(),"72")) {
        cleanup();
        std::cerr << captured.str() << '\n';
        return fail("unexpected phase 2 output");
    }

    cleanup();
    return 0;
}
