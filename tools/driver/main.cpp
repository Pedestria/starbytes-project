#include "starbytes/base/FileExt.def"
#include "starbytes/compiler/AST.h"
#include "starbytes/compiler/Parser.h"
#include "starbytes/compiler/Gen.h"
#include "starbytes/runtime/RTEngine.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

#define STARBYTES_STRINGIFY_IMPL(x) #x
#define STARBYTES_STRINGIFY(x) STARBYTES_STRINGIFY_IMPL(x)

namespace {

enum class DriverCommand {
    Run,
    Compile,
    Check,
    Help
};

struct DriverOptions {
    DriverCommand command = DriverCommand::Run;
    std::string scriptPath;
    std::string moduleName;
    std::string outputPath;
    std::string outDir = ".starbytes";
    bool showHelp = false;
    bool showVersion = false;
    bool executeAfterCompile = false;
    bool cleanModule = false;
    bool printModulePath = false;
    bool logDiagnostics = true;
};

struct ParseResult {
    bool ok = false;
    int exitCode = 1;
    std::string error;
};

class NullASTConsumer final : public starbytes::ASTStreamConsumer {
public:
    bool acceptsSymbolTableContext() override {
        return false;
    }

    void consumeDecl(starbytes::ASTDecl *stmt) override {
        (void)stmt;
    }

    void consumeStmt(starbytes::ASTStmt *stmt) override {
        (void)stmt;
    }
};

bool startsWith(const std::string &subject, const std::string &prefix) {
    return subject.rfind(prefix, 0) == 0;
}

void printUsage(std::ostream &out) {
    out << "Usage:\n";
    out << "  starbytes [command] <script." << STARBYTES_SRCFILE_EXT << "> [options]\n";
    out << "  starbytes --help\n";
    out << "  starbytes --version\n";
}

void printHelp(std::ostream &out) {
    out << "Starbytes Driver\n\n";
    printUsage(out);
    out << "\nCommands:\n";
    out << "  run      Compile and execute a script (default command).\n";
    out << "  compile  Compile a script into a ." << STARBYTES_COMPILEDFILE_EXT << " module.\n";
    out << "  check    Parse and run semantic checks only (no module output).\n";
    out << "  help     Show this help page.\n";

    out << "\nOptions:\n";
    out << "  -h, --help                 Show help.\n";
    out << "  -V, --version              Show driver version.\n";
    out << "  -m, --modulename <name>    Override module name (default: script file stem).\n";
    out << "  -o, --output <file>        Output module path (overrides --out-dir).\n";
    out << "  -d, --out-dir <dir>        Output directory for compiled module (default: .starbytes).\n";
    out << "      --run                  Execute after compile (useful with compile command).\n";
    out << "      --no-run               Skip execution after compile.\n";
    out << "      --clean                Remove generated module file on success.\n";
    out << "      --print-module-path    Print resolved module output path.\n";
    out << "      --no-diagnostics       Do not print diagnostics buffered by runtime handlers.\n";

    out << "\nExamples:\n";
    out << "  starbytes hello." << STARBYTES_SRCFILE_EXT << "\n";
    out << "  starbytes run hello." << STARBYTES_SRCFILE_EXT << " -m HelloApp\n";
    out << "  starbytes compile hello." << STARBYTES_SRCFILE_EXT << " -o build/hello." << STARBYTES_COMPILEDFILE_EXT << "\n";
    out << "  starbytes check hello." << STARBYTES_SRCFILE_EXT << "\n";
}

void printVersion(std::ostream &out) {
#ifdef STARBYTES_VERSION
    out << "starbytes " << STARBYTES_STRINGIFY(STARBYTES_VERSION) << '\n';
#else
    out << "starbytes (version unknown)" << '\n';
#endif
}

bool consumeValueOption(const std::string &arg,
                        const std::string &longOpt,
                        const std::string &shortOpt,
                        int &index,
                        int argc,
                        const char *argv[],
                        std::string &value,
                        std::string &error) {
    if(arg == longOpt || arg == shortOpt) {
        if(index + 1 >= argc) {
            error = "Missing value for option: " + arg;
            return true;
        }
        ++index;
        value = argv[index];
        return true;
    }

    const std::string longPrefix = longOpt + "=";
    if(startsWith(arg, longPrefix)) {
        value = arg.substr(longPrefix.size());
        if(value.empty()) {
            error = "Missing value for option: " + longOpt;
        }
        return true;
    }

    const std::string shortPrefix = shortOpt + "=";
    if(startsWith(arg, shortPrefix)) {
        value = arg.substr(shortPrefix.size());
        if(value.empty()) {
            error = "Missing value for option: " + shortOpt;
        }
        return true;
    }

    return false;
}

ParseResult parseArgs(int argc, const char *argv[], DriverOptions &opts) {
    std::vector<std::string> positional;
    bool commandSet = false;
    bool forceRun = false;
    bool forceNoRun = false;

    for(int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if(arg == "--") {
            for(++i; i < argc; ++i) {
                positional.emplace_back(argv[i]);
            }
            break;
        }

        if(arg == "-h" || arg == "--help") {
            opts.showHelp = true;
            continue;
        }
        if(arg == "-V" || arg == "--version") {
            opts.showVersion = true;
            continue;
        }

        if(!commandSet && positional.empty()) {
            if(arg == "run") {
                opts.command = DriverCommand::Run;
                commandSet = true;
                continue;
            }
            if(arg == "compile") {
                opts.command = DriverCommand::Compile;
                commandSet = true;
                continue;
            }
            if(arg == "check") {
                opts.command = DriverCommand::Check;
                commandSet = true;
                continue;
            }
            if(arg == "help") {
                opts.command = DriverCommand::Help;
                opts.showHelp = true;
                commandSet = true;
                continue;
            }
        }

        std::string value;
        std::string error;
        if(consumeValueOption(arg, "--modulename", "-m", i, argc, argv, value, error)) {
            if(!error.empty()) {
                return {false, 1, error};
            }
            opts.moduleName = value;
            continue;
        }
        if(consumeValueOption(arg, "--output", "-o", i, argc, argv, value, error)) {
            if(!error.empty()) {
                return {false, 1, error};
            }
            opts.outputPath = value;
            continue;
        }
        if(consumeValueOption(arg, "--out-dir", "-d", i, argc, argv, value, error)) {
            if(!error.empty()) {
                return {false, 1, error};
            }
            opts.outDir = value;
            continue;
        }

        if(arg == "--run") {
            forceRun = true;
            continue;
        }
        if(arg == "--no-run") {
            forceNoRun = true;
            continue;
        }
        if(arg == "--clean") {
            opts.cleanModule = true;
            continue;
        }
        if(arg == "--print-module-path") {
            opts.printModulePath = true;
            continue;
        }
        if(arg == "--no-diagnostics") {
            opts.logDiagnostics = false;
            continue;
        }

        if(!arg.empty() && arg[0] == '-') {
            return {false, 1, "Unknown option: " + arg};
        }

        positional.push_back(arg);
    }

    if(opts.showHelp || opts.command == DriverCommand::Help || opts.showVersion) {
        return {true, 0, ""};
    }

    if(forceRun && forceNoRun) {
        return {false, 1, "Conflicting options: --run and --no-run cannot be used together."};
    }

    if(positional.empty()) {
        return {false, 1, "Missing input script."};
    }
    if(positional.size() > 1) {
        return {false, 1, "Too many positional arguments. Expected exactly one script path."};
    }

    opts.scriptPath = positional.front();

    opts.executeAfterCompile = (opts.command == DriverCommand::Run);
    if(opts.command == DriverCommand::Compile) {
        opts.executeAfterCompile = false;
    }
    if(opts.command == DriverCommand::Check) {
        opts.executeAfterCompile = false;
        if(forceRun || forceNoRun) {
            return {false, 1, "--run/--no-run are not valid with the check command."};
        }
    }

    if(forceRun) {
        opts.executeAfterCompile = true;
    }
    if(forceNoRun) {
        opts.executeAfterCompile = false;
    }

    return {true, 0, ""};
}

std::filesystem::path resolveCompiledModulePath(const DriverOptions &opts) {
    if(!opts.outputPath.empty()) {
        return std::filesystem::path(opts.outputPath);
    }

    std::filesystem::path outDir = opts.outDir.empty() ? std::filesystem::path(".starbytes")
                                                       : std::filesystem::path(opts.outDir);
    return outDir / (opts.moduleName + "." + STARBYTES_COMPILEDFILE_EXT);
}

bool ensureOutputParentDir(const std::filesystem::path &outputPath, std::string &error) {
    auto parent = outputPath.parent_path();
    if(parent.empty()) {
        return true;
    }

    std::error_code ec;
    std::filesystem::create_directories(parent, ec);
    if(ec) {
        error = "Failed to create output directory '" + parent.string() + "': " + ec.message();
        return false;
    }
    return true;
}

void maybeLogRuntimeDiagnostics(const DriverOptions &opts) {
    if(!opts.logDiagnostics || !starbytes::stdDiagnosticHandler) {
        return;
    }
    if(!starbytes::stdDiagnosticHandler->empty()) {
        starbytes::stdDiagnosticHandler->logAll();
    }
}

}

int main(int argc, const char *argv[]) {
    DriverOptions opts;
    auto parsed = parseArgs(argc, argv, opts);
    if(!parsed.ok) {
        if(!parsed.error.empty()) {
            std::cerr << parsed.error << std::endl;
        }
        printUsage(std::cerr);
        return parsed.exitCode;
    }

    if(opts.showVersion) {
        printVersion(std::cout);
    }
    if(opts.showHelp || opts.command == DriverCommand::Help) {
        printHelp(std::cout);
        return 0;
    }
    if(opts.showVersion) {
        return 0;
    }

    std::filesystem::path scriptPath(opts.scriptPath);
    if(!std::filesystem::exists(scriptPath)) {
        std::cerr << "Input file not found: " << opts.scriptPath << std::endl;
        return 1;
    }

    std::ifstream in(scriptPath, std::ios::in);
    if(!in.is_open()) {
        std::cerr << "Failed to open script: " << opts.scriptPath << std::endl;
        return 1;
    }

    if(opts.moduleName.empty()) {
        opts.moduleName = scriptPath.stem().string();
    }

    if(opts.command == DriverCommand::Check) {
        NullASTConsumer astConsumer;
        starbytes::Parser parser(astConsumer);
        auto parseContext = starbytes::ModuleParseContext::Create(opts.moduleName);
        parser.parseFromStream(in, parseContext);
        auto ok = parser.finish();
        maybeLogRuntimeDiagnostics(opts);
        return ok ? 0 : 1;
    }

    auto compiledModulePath = resolveCompiledModulePath(opts);

    std::string outputDirError;
    if(!ensureOutputParentDir(compiledModulePath, outputDirError)) {
        std::cerr << outputDirError << std::endl;
        return 1;
    }

    std::ofstream moduleOut(compiledModulePath, std::ios::out | std::ios::binary);
    if(!moduleOut.is_open()) {
        std::cerr << "Failed to open output module for writing: " << compiledModulePath << std::endl;
        return 1;
    }

    auto outputDirForGen = compiledModulePath.parent_path();
    if(outputDirForGen.empty()) {
        outputDirForGen = std::filesystem::current_path();
    }

    starbytes::Gen gen;
    auto moduleGenContext = starbytes::ModuleGenContext::Create(opts.moduleName, moduleOut, outputDirForGen);
    auto parseContext = starbytes::ModuleParseContext::Create(opts.moduleName);
    gen.setContext(&moduleGenContext);

    starbytes::Parser parser(gen);
    parser.parseFromStream(in, parseContext);
    if(!parser.finish()) {
        moduleOut.close();
        std::error_code removeErr;
        std::filesystem::remove(compiledModulePath, removeErr);
        if(removeErr) {
            std::cerr << "Warning: failed to remove '" << compiledModulePath << "': " << removeErr.message() << std::endl;
        }
        maybeLogRuntimeDiagnostics(opts);
        return 1;
    }

    gen.finish();
    moduleOut.close();

    if(opts.printModulePath) {
        std::cout << compiledModulePath << std::endl;
    }

    if(opts.executeAfterCompile) {
        std::ifstream rtcodeIn(compiledModulePath, std::ios::in | std::ios::binary);
        if(!rtcodeIn.is_open()) {
            std::cerr << "Failed to open compiled module: " << compiledModulePath << std::endl;
            return 1;
        }

        auto interp = starbytes::Runtime::Interp::Create();
        interp->exec(rtcodeIn);
    }

    if(opts.cleanModule) {
        std::error_code removeErr;
        std::filesystem::remove(compiledModulePath, removeErr);
        if(removeErr && std::filesystem::exists(compiledModulePath)) {
            std::cerr << "Warning: failed to remove '" << compiledModulePath << "': " << removeErr.message() << std::endl;
        }
    }

    maybeLogRuntimeDiagnostics(opts);

    return (starbytes::stdDiagnosticHandler && starbytes::stdDiagnosticHandler->hasErrored()) ? 1 : 0;
}
