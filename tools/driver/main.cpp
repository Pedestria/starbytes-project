#include "starbytes/base/FileExt.def"
#include "starbytes/base/CmdLine.h"
#include "starbytes/compiler/AST.h"
#include "starbytes/compiler/Parser.h"
#include "starbytes/compiler/Gen.h"
#include "starbytes/interop.h"
#include "starbytes/runtime/RTEngine.h"
#include "profile/CompileProfile.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
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
    std::vector<std::string> scriptArgs;
    std::string moduleName;
    std::string outputPath;
    std::string outDir = ".starbytes";
    bool showHelp = false;
    bool showVersion = false;
    bool executeAfterCompile = false;
    bool cleanModule = false;
    bool printModulePath = false;
    bool profileCompile = false;
    std::string profileCompileOutPath;
    bool logDiagnostics = true;
    bool autoLoadNative = true;
    std::vector<std::string> nativeModules;
    std::vector<std::string> nativeSearchDirs;
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

struct ModuleSource {
    std::filesystem::path filePath;
    std::string fileText;
    uint64_t contentHash = 0;
    bool isInterfaceFile = false;
};

struct ModuleBuildUnit {
    std::filesystem::path moduleDir;
    std::vector<ModuleSource> sources;
    std::vector<std::string> imports;
    bool hasMainSource = false;
    bool hasInterfaceSource = false;
};

struct ModuleGraph {
    std::unordered_map<std::string, ModuleBuildUnit> unitsByKey;
    std::vector<std::string> buildOrder;
    std::string rootKey;
    bool rootIsDirectory = false;
    bool rootHasMainSource = false;
};

struct ModuleAnalysisEntry {
    uint64_t contentHash = 0;
    std::string compilerVersion;
    uint64_t flagsHash = 0;
    std::vector<std::string> imports;
};

struct ModuleAnalysisCache {
    std::unordered_map<std::string, ModuleAnalysisEntry> entriesBySourcePath;
    bool dirty = false;
};

struct ResolverContext {
    std::vector<std::filesystem::path> moduleSearchDirs;
};

using CompileProfileData = starbytes::driver::profile::CompileProfileData;

CompileProfileData *gActiveCompileProfile = nullptr;

std::string makeAbsolutePathString(const std::filesystem::path &path) {
    std::error_code ec;
    auto weak = std::filesystem::weakly_canonical(path, ec);
    if(ec) {
        return std::filesystem::absolute(path).string();
    }
    return weak.string();
}

std::string compilerVersionString() {
#ifdef STARBYTES_VERSION
    return STARBYTES_STRINGIFY(STARBYTES_VERSION);
#else
    return "unknown";
#endif
}

uint64_t fnv1a64(const char *data, size_t size) {
    uint64_t hash = 1469598103934665603ULL;
    for(size_t i = 0; i < size; ++i) {
        hash ^= static_cast<uint8_t>(data[i]);
        hash *= 1099511628211ULL;
    }
    return hash;
}

uint64_t hashString64(const std::string &value) {
    return fnv1a64(value.data(), value.size());
}

bool loadModuleAnalysisCache(const std::filesystem::path &cachePath,
                             ModuleAnalysisCache &cache,
                             std::string &warning) {
    cache.entriesBySourcePath.clear();
    cache.dirty = false;

    std::ifstream in(cachePath, std::ios::in);
    if(!in.is_open()) {
        return true;
    }

    std::string header;
    if(!std::getline(in, header)) {
        return true;
    }
    if(header != "STARBYTES_MODULE_ANALYSIS_CACHE_V1") {
        warning = "Ignoring incompatible module analysis cache file: " + cachePath.string();
        return true;
    }

    while(in) {
        std::string tag;
        in >> tag;
        if(!in) {
            break;
        }
        if(tag != "ENTRY") {
            std::string discard;
            std::getline(in, discard);
            continue;
        }

        std::string sourcePath;
        std::string compilerVersion;
        uint64_t contentHash = 0;
        uint64_t flagsHash = 0;
        size_t importCount = 0;

        in >> std::quoted(sourcePath) >> contentHash >> std::quoted(compilerVersion) >> flagsHash >> importCount;
        if(!in) {
            warning = "Ignoring malformed module analysis cache file: " + cachePath.string();
            cache.entriesBySourcePath.clear();
            return true;
        }

        ModuleAnalysisEntry entry;
        entry.contentHash = contentHash;
        entry.compilerVersion = std::move(compilerVersion);
        entry.flagsHash = flagsHash;
        entry.imports.reserve(importCount);
        for(size_t i = 0; i < importCount; ++i) {
            std::string importName;
            in >> std::quoted(importName);
            if(!in) {
                warning = "Ignoring malformed module analysis cache file: " + cachePath.string();
                cache.entriesBySourcePath.clear();
                return true;
            }
            entry.imports.push_back(std::move(importName));
        }
        cache.entriesBySourcePath[sourcePath] = std::move(entry);
    }

    return true;
}

bool saveModuleAnalysisCache(const std::filesystem::path &cachePath,
                             const ModuleAnalysisCache &cache,
                             std::string &error) {
    auto parent = cachePath.parent_path();
    if(!parent.empty()) {
        std::error_code dirErr;
        std::filesystem::create_directories(parent, dirErr);
        if(dirErr) {
            error = "Failed to create module analysis cache directory '" + parent.string() + "': " + dirErr.message();
            return false;
        }
    }

    std::ofstream out(cachePath, std::ios::out | std::ios::trunc);
    if(!out.is_open()) {
        error = "Failed to write module analysis cache file: " + cachePath.string();
        return false;
    }

    out << "STARBYTES_MODULE_ANALYSIS_CACHE_V1\n";
    for(const auto &pair : cache.entriesBySourcePath) {
        const auto &sourcePath = pair.first;
        const auto &entry = pair.second;
        out << "ENTRY "
            << std::quoted(sourcePath) << " "
            << entry.contentHash << " "
            << std::quoted(entry.compilerVersion) << " "
            << entry.flagsHash << " "
            << entry.imports.size();
        for(const auto &importName : entry.imports) {
            out << " " << std::quoted(importName);
        }
        out << "\n";
    }
    return true;
}

std::optional<std::vector<std::string>> resolveImportsFromCache(const ModuleAnalysisCache &cache,
                                                                const ModuleSource &source,
                                                                const std::string &compilerVersion,
                                                                uint64_t flagsHash) {
    auto sourceKey = makeAbsolutePathString(source.filePath);
    auto it = cache.entriesBySourcePath.find(sourceKey);
    if(it == cache.entriesBySourcePath.end()) {
        return std::nullopt;
    }
    const auto &entry = it->second;
    if(entry.contentHash != source.contentHash) {
        return std::nullopt;
    }
    if(entry.compilerVersion != compilerVersion) {
        return std::nullopt;
    }
    if(entry.flagsHash != flagsHash) {
        return std::nullopt;
    }
    return entry.imports;
}

void updateImportCache(ModuleAnalysisCache &cache,
                       const ModuleSource &source,
                       const std::string &compilerVersion,
                       uint64_t flagsHash,
                       const std::vector<std::string> &imports) {
    auto sourceKey = makeAbsolutePathString(source.filePath);
    auto &entry = cache.entriesBySourcePath[sourceKey];
    if(entry.contentHash != source.contentHash ||
       entry.compilerVersion != compilerVersion ||
       entry.flagsHash != flagsHash ||
       entry.imports != imports) {
        entry.contentHash = source.contentHash;
        entry.compilerVersion = compilerVersion;
        entry.flagsHash = flagsHash;
        entry.imports = imports;
        cache.dirty = true;
    }
}

std::string normalizeImportModuleName(const std::string &name) {
    std::string out = name;
    while(!out.empty() && (out.front() == '"' || out.front() == '\'')) {
        out.erase(out.begin());
    }
    while(!out.empty() && (out.back() == '"' || out.back() == '\'')) {
        out.pop_back();
    }
    return out;
}

bool lineIsCommentOrEmpty(const std::string &line) {
    size_t index = 0;
    while(index < line.size() && std::isspace(static_cast<unsigned char>(line[index]))) {
        ++index;
    }
    if(index >= line.size()) {
        return true;
    }
    if(line[index] == '#') {
        return true;
    }
    if(line[index] == '/' && (index + 1) < line.size() && line[index + 1] == '/') {
        return true;
    }
    return false;
}

bool loadModulePathFile(const std::filesystem::path &path,
                        std::vector<std::filesystem::path> &outDirs,
                        std::unordered_set<std::string> &seenDirs,
                        std::vector<std::string> &warnings) {
    std::ifstream in(path, std::ios::in);
    if(!in.is_open()) {
        return true;
    }

    const auto baseDir = path.parent_path();
    std::string rawLine;
    unsigned lineNo = 0;
    while(std::getline(in, rawLine)) {
        ++lineNo;
        if(lineIsCommentOrEmpty(rawLine)) {
            continue;
        }

        auto begin = rawLine.find_first_not_of(" \t\r\n");
        auto end = rawLine.find_last_not_of(" \t\r\n");
        if(begin == std::string::npos || end == std::string::npos) {
            continue;
        }
        auto entry = rawLine.substr(begin, end - begin + 1);
        std::filesystem::path entryPath(entry);
        if(entryPath.is_relative()) {
            entryPath = baseDir / entryPath;
        }

        std::error_code ec;
        if(!std::filesystem::exists(entryPath, ec) || ec || !std::filesystem::is_directory(entryPath, ec) || ec) {
            warnings.push_back("Ignoring invalid .starbmodpath entry `" + entry + "` at " +
                               path.string() + ":" + std::to_string(lineNo));
            continue;
        }

        auto normalized = makeAbsolutePathString(entryPath);
        if(seenDirs.insert(normalized).second) {
            outDirs.emplace_back(normalized);
        }
    }
    return true;
}

std::filesystem::path executableDirectoryFromArgv0(const char *argv0) {
    if(!argv0 || std::string(argv0).empty()) {
        return std::filesystem::current_path();
    }
    std::filesystem::path p(argv0);
    std::error_code ec;
    if(p.is_relative()) {
        p = std::filesystem::absolute(p, ec);
        if(ec) {
            p = std::filesystem::current_path() / p;
        }
    }
    auto dir = p.parent_path();
    if(dir.empty()) {
        dir = std::filesystem::current_path();
    }
    return std::filesystem::path(makeAbsolutePathString(dir));
}

ResolverContext buildResolverContext(const std::filesystem::path &inputPath,
                                     const std::filesystem::path &workspaceRoot,
                                     const char *argv0,
                                     std::vector<std::string> &warnings) {
    ResolverContext context;
    std::unordered_set<std::string> seenDirs;

    auto appendDir = [&](const std::filesystem::path &dir) {
        auto normalized = makeAbsolutePathString(dir);
        if(seenDirs.insert(normalized).second) {
            context.moduleSearchDirs.emplace_back(normalized);
        }
    };

    auto loadAtRoot = [&](const std::filesystem::path &rootDir) {
        std::error_code ec;
        if(!std::filesystem::exists(rootDir, ec) || ec || !std::filesystem::is_directory(rootDir, ec) || ec) {
            return;
        }
        loadModulePathFile(rootDir / ".starbmodpath", context.moduleSearchDirs, seenDirs, warnings);
        auto parent = rootDir.parent_path();
        if(!parent.empty()) {
            loadModulePathFile(parent / ".starbmodpath", context.moduleSearchDirs, seenDirs, warnings);
        }
    };

    appendDir(workspaceRoot);
    appendDir(workspaceRoot / "modules");
    appendDir(workspaceRoot / "stdlib");

    auto exeDir = executableDirectoryFromArgv0(argv0);
    loadAtRoot(exeDir);

    std::error_code ec;
    auto inputRoot = std::filesystem::is_directory(inputPath, ec) && !ec ? inputPath : inputPath.parent_path();
    if(inputRoot.empty()) {
        inputRoot = workspaceRoot;
    }
    inputRoot = std::filesystem::path(makeAbsolutePathString(inputRoot));
    loadAtRoot(inputRoot);

    return context;
}

uint64_t computeModuleAnalysisFlagsHash(const DriverOptions &opts,
                                        const ResolverContext &resolverContext) {
    std::ostringstream key;
    key << "auto_native=" << (opts.autoLoadNative ? "1" : "0") << ";";
    key << "native_dirs=";
    for(const auto &dir : opts.nativeSearchDirs) {
        key << makeAbsolutePathString(dir) << ";";
    }
    key << "resolver_dirs=";
    for(const auto &dir : resolverContext.moduleSearchDirs) {
        key << makeAbsolutePathString(dir) << ";";
    }
    return hashString64(key.str());
}

std::vector<std::string> extractImportsFromSource(const std::string &sourceText) {
    auto importScanStart = std::chrono::steady_clock::now();
    std::vector<std::string> imports;
    static const std::regex importRegex(R"(^\s*import\s+([A-Za-z_][A-Za-z0-9_]*)\s*$)");
    std::istringstream lines(sourceText);
    std::string line;
    while(std::getline(lines, line)) {
        auto commentPos = line.find("//");
        if(commentPos != std::string::npos) {
            line = line.substr(0, commentPos);
        }
        std::smatch match;
        if(!std::regex_match(line, match, importRegex)) {
            continue;
        }
        if(match.size() < 2) {
            continue;
        }
        auto name = normalizeImportModuleName(match[1].str());
        if(name.empty()) {
            continue;
        }
        if(std::find(imports.begin(), imports.end(), name) == imports.end()) {
            imports.push_back(std::move(name));
        }
    }
    if(gActiveCompileProfile && gActiveCompileProfile->enabled) {
        auto importScanEnd = std::chrono::steady_clock::now();
        gActiveCompileProfile->importScanNs += std::chrono::duration_cast<std::chrono::nanoseconds>(importScanEnd - importScanStart).count();
    }
    return imports;
}

bool readFileText(const std::filesystem::path &path, std::string &outText, std::string &error) {
    auto fileLoadStart = std::chrono::steady_clock::now();
    std::ifstream in(path, std::ios::in);
    if(!in.is_open()) {
        error = "Failed to open source file: " + path.string();
        return false;
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    outText = buffer.str();
    if(gActiveCompileProfile && gActiveCompileProfile->enabled) {
        auto fileLoadEnd = std::chrono::steady_clock::now();
        gActiveCompileProfile->moduleFileLoadNs += std::chrono::duration_cast<std::chrono::nanoseconds>(fileLoadEnd - fileLoadStart).count();
    }
    return true;
}

bool collectModuleSourcesInDirectory(const std::filesystem::path &moduleDir,
                                     ModuleBuildUnit &unit,
                                     std::string &error) {
    struct SourceCandidate {
        std::filesystem::path path;
        bool isInterface = false;
    };
    std::vector<SourceCandidate> sourceFiles;
    std::error_code iterErr;
    for(const auto &entry : std::filesystem::directory_iterator(moduleDir, iterErr)) {
        if(iterErr) {
            error = "Failed to iterate module directory '" + moduleDir.string() + "': " + iterErr.message();
            return false;
        }
        if(!entry.is_regular_file()) {
            continue;
        }
        auto path = entry.path();
        if(path.extension() == ("." STARBYTES_SRCFILE_EXT)) {
            sourceFiles.push_back({path,false});
            continue;
        }
        if(path.extension() == ("." STARBYTES_INTERFACEFILE_EXT)) {
            sourceFiles.push_back({path,true});
            unit.hasInterfaceSource = true;
        }
    }
    std::sort(sourceFiles.begin(), sourceFiles.end(), [](const SourceCandidate &lhs,const SourceCandidate &rhs) {
        if(lhs.path.filename() == rhs.path.filename()) {
            return lhs.path.string() < rhs.path.string();
        }
        return lhs.path.filename().string() < rhs.path.filename().string();
    });
    if(sourceFiles.empty()) {
        error = "No *." STARBYTES_SRCFILE_EXT " or *." STARBYTES_INTERFACEFILE_EXT
              " files found in module directory: " + moduleDir.string();
        return false;
    }

    auto mainIt = std::find_if(sourceFiles.begin(), sourceFiles.end(), [](const SourceCandidate &source) {
        return !source.isInterface && source.path.filename() == ("main." STARBYTES_SRCFILE_EXT);
    });
    if(mainIt != sourceFiles.end()) {
        auto mainPath = *mainIt;
        sourceFiles.erase(mainIt);
        sourceFiles.push_back(mainPath);
        unit.hasMainSource = true;
    }

    for(const auto &source : sourceFiles) {
        std::string sourceText;
        if(!readFileText(source.path, sourceText, error)) {
            return false;
        }
        auto contentHash = hashString64(sourceText);
        unit.sources.push_back({source.path, sourceText, contentHash, source.isInterface});
    }
    return true;
}

std::optional<std::filesystem::path> resolveImportModuleDirectory(const std::string &moduleName,
                                                                  const std::filesystem::path &currentModuleDir,
                                                                  const std::filesystem::path &workspaceRoot,
                                                                  const ResolverContext &resolverContext) {
    auto resolveStart = std::chrono::steady_clock::now();
    std::vector<std::filesystem::path> candidates;
    candidates.push_back(currentModuleDir / moduleName);
    candidates.push_back(currentModuleDir.parent_path() / moduleName);
    candidates.push_back(workspaceRoot / moduleName);
    candidates.push_back(workspaceRoot / "modules" / moduleName);
    candidates.push_back(workspaceRoot / "stdlib" / moduleName);
    for(const auto &root : resolverContext.moduleSearchDirs) {
        candidates.push_back(root / moduleName);
    }

    for(const auto &candidate : candidates) {
        std::error_code ec;
        if(std::filesystem::exists(candidate, ec) && !ec && std::filesystem::is_directory(candidate, ec) && !ec) {
            if(gActiveCompileProfile && gActiveCompileProfile->enabled) {
                auto resolveEnd = std::chrono::steady_clock::now();
                gActiveCompileProfile->importResolveNs += std::chrono::duration_cast<std::chrono::nanoseconds>(resolveEnd - resolveStart).count();
            }
            return candidate;
        }
    }
    if(gActiveCompileProfile && gActiveCompileProfile->enabled) {
        auto resolveEnd = std::chrono::steady_clock::now();
        gActiveCompileProfile->importResolveNs += std::chrono::duration_cast<std::chrono::nanoseconds>(resolveEnd - resolveStart).count();
    }
    return std::nullopt;
}

std::string joinCycle(const std::vector<std::string> &stack, const std::string &tail) {
    std::ostringstream out;
    for(size_t i = 0; i < stack.size(); ++i) {
        if(i > 0) {
            out << " -> ";
        }
        out << stack[i];
    }
    if(!stack.empty()) {
        out << " -> ";
    }
    out << tail;
    return out.str();
}

bool discoverModuleGraphByDirectory(const std::filesystem::path &moduleDir,
                                    const std::filesystem::path &workspaceRoot,
                                    const ResolverContext &resolverContext,
                                    ModuleAnalysisCache &analysisCache,
                                    const std::string &compilerVersion,
                                    uint64_t flagsHash,
                                    ModuleGraph &graph,
                                    std::unordered_set<std::string> &visiting,
                                    std::vector<std::string> &stack,
                                    std::string &error) {
    auto moduleKey = makeAbsolutePathString(moduleDir);
    if(visiting.find(moduleKey) != visiting.end()) {
        error = "Cyclic module import detected: " + joinCycle(stack, moduleKey);
        return false;
    }
    if(graph.unitsByKey.find(moduleKey) != graph.unitsByKey.end()) {
        return true;
    }

    visiting.insert(moduleKey);
    stack.push_back(moduleKey);

    ModuleBuildUnit unit;
    unit.moduleDir = moduleDir;
    if(!collectModuleSourcesInDirectory(moduleDir, unit, error)) {
        visiting.erase(moduleKey);
        stack.pop_back();
        return false;
    }

    std::set<std::string> importedNames;
    for(const auto &source : unit.sources) {
        auto cachedImports = resolveImportsFromCache(analysisCache, source, compilerVersion, flagsHash);
        auto imports = cachedImports.has_value() ? *cachedImports : extractImportsFromSource(source.fileText);
        updateImportCache(analysisCache, source, compilerVersion, flagsHash, imports);
        for(auto &name : imports) {
            if(importedNames.insert(name).second) {
                unit.imports.push_back(name);
            }
        }
    }

    for(const auto &importName : unit.imports) {
        auto resolvedDir = resolveImportModuleDirectory(importName, unit.moduleDir, workspaceRoot, resolverContext);
        if(!resolvedDir.has_value()) {
            error = "Failed to resolve imported module `" + importName + "` from `" + moduleDir.string() + "`.";
            visiting.erase(moduleKey);
            stack.pop_back();
            return false;
        }
        if(!discoverModuleGraphByDirectory(*resolvedDir,
                                           workspaceRoot,
                                           resolverContext,
                                           analysisCache,
                                           compilerVersion,
                                           flagsHash,
                                           graph,
                                           visiting,
                                           stack,
                                           error)) {
            visiting.erase(moduleKey);
            stack.pop_back();
            return false;
        }
    }

    graph.unitsByKey.insert(std::make_pair(moduleKey, std::move(unit)));
    graph.buildOrder.push_back(moduleKey);
    visiting.erase(moduleKey);
    stack.pop_back();
    return true;
}

bool discoverModuleGraphBySingleFile(const std::filesystem::path &sourceFile,
                                     const std::filesystem::path &workspaceRoot,
                                     const ResolverContext &resolverContext,
                                     ModuleAnalysisCache &analysisCache,
                                     const std::string &compilerVersion,
                                     uint64_t flagsHash,
                                     ModuleGraph &graph,
                                     std::string &error) {
    ModuleBuildUnit rootUnit;
    rootUnit.moduleDir = sourceFile.parent_path();
    std::string sourceText;
    if(!readFileText(sourceFile, sourceText, error)) {
        return false;
    }
    rootUnit.sources.push_back({sourceFile, sourceText, hashString64(sourceText), false});
    rootUnit.hasMainSource = (sourceFile.filename() == ("main." STARBYTES_SRCFILE_EXT));

    std::set<std::string> importedNames;
    auto cachedImports = resolveImportsFromCache(analysisCache, rootUnit.sources.front(), compilerVersion, flagsHash);
    auto rootImports = cachedImports.has_value() ? *cachedImports : extractImportsFromSource(sourceText);
    updateImportCache(analysisCache, rootUnit.sources.front(), compilerVersion, flagsHash, rootImports);
    for(auto &name : rootImports) {
        if(importedNames.insert(name).second) {
            rootUnit.imports.push_back(name);
        }
    }

    std::unordered_set<std::string> visiting;
    std::vector<std::string> stack;
    for(const auto &importName : rootUnit.imports) {
        auto resolvedDir = resolveImportModuleDirectory(importName, rootUnit.moduleDir, workspaceRoot, resolverContext);
        if(!resolvedDir.has_value()) {
            error = "Failed to resolve imported module `" + importName + "` from `" + sourceFile.string() + "`.";
            return false;
        }
        if(!discoverModuleGraphByDirectory(*resolvedDir,
                                           workspaceRoot,
                                           resolverContext,
                                           analysisCache,
                                           compilerVersion,
                                           flagsHash,
                                           graph,
                                           visiting,
                                           stack,
                                           error)) {
            return false;
        }
    }

    auto rootKey = makeAbsolutePathString(sourceFile);
    graph.unitsByKey.insert(std::make_pair(rootKey, std::move(rootUnit)));
    graph.buildOrder.push_back(rootKey);
    graph.rootKey = rootKey;
    graph.rootIsDirectory = false;
    graph.rootHasMainSource = graph.unitsByKey[rootKey].hasMainSource;
    return true;
}

bool discoverModuleGraph(const std::filesystem::path &inputPath,
                         const std::filesystem::path &workspaceRoot,
                         const ResolverContext &resolverContext,
                         ModuleAnalysisCache &analysisCache,
                         const std::string &compilerVersion,
                         uint64_t flagsHash,
                         ModuleGraph &graph,
                         std::string &error) {
    std::error_code ec;
    if(std::filesystem::is_directory(inputPath, ec)) {
        std::unordered_set<std::string> visiting;
        std::vector<std::string> stack;
        if(!discoverModuleGraphByDirectory(inputPath,
                                           workspaceRoot,
                                           resolverContext,
                                           analysisCache,
                                           compilerVersion,
                                           flagsHash,
                                           graph,
                                           visiting,
                                           stack,
                                           error)) {
            return false;
        }
        graph.rootKey = makeAbsolutePathString(inputPath);
        graph.rootIsDirectory = true;
        auto rootIt = graph.unitsByKey.find(graph.rootKey);
        graph.rootHasMainSource = (rootIt != graph.unitsByKey.end()) ? rootIt->second.hasMainSource : false;
        return true;
    }

    if(!std::filesystem::is_regular_file(inputPath, ec)) {
        error = "Input path is not a file or directory: " + inputPath.string();
        return false;
    }

    return discoverModuleGraphBySingleFile(inputPath,
                                           workspaceRoot,
                                           resolverContext,
                                           analysisCache,
                                           compilerVersion,
                                           flagsHash,
                                           graph,
                                           error);
}

std::string defaultModuleNameForPath(const std::filesystem::path &path) {
    auto name = path.stem().string();
    if(name.empty()) {
        name = path.filename().string();
    }
    if(name.empty()) {
        name = "module";
    }
    return name;
}

bool parseModuleGraphSources(starbytes::Parser &parser,
                             starbytes::ModuleParseContext &parseContext,
                             const ModuleGraph &graph,
                             std::string &error) {
    for(const auto &unitKey : graph.buildOrder) {
        auto unitIt = graph.unitsByKey.find(unitKey);
        if(unitIt == graph.unitsByKey.end()) {
            error = "Internal driver error: missing module build unit for key `" + unitKey + "`.";
            return false;
        }
        const auto &unit = unitIt->second;
        for(const auto &source : unit.sources) {
            parseContext.name = makeAbsolutePathString(source.filePath);
            std::istringstream stream(source.fileText);
            parser.parseFromStream(stream, parseContext);
        }
    }
    return true;
}

bool sourceDeclaresNativeSymbols(const ModuleSource &source) {
    return source.fileText.find("@native") != std::string::npos;
}

bool moduleDeclaresNativeSymbols(const ModuleBuildUnit &unit) {
    for(const auto &source : unit.sources) {
        if(sourceDeclaresNativeSymbols(source)) {
            return true;
        }
    }
    return false;
}

void appendUniquePath(const std::filesystem::path &path,
                      std::vector<std::filesystem::path> &outPaths,
                      std::unordered_set<std::string> &seen) {
    auto normalized = makeAbsolutePathString(path);
    if(seen.insert(normalized).second) {
        outPaths.emplace_back(normalized);
    }
}

std::optional<std::filesystem::path> findNativeModuleByName(const std::string &moduleName,
                                                            const std::vector<std::filesystem::path> &searchDirs) {
    std::vector<std::string> candidateNames = {
        moduleName + "." + STARBYTES_NATIVE_MODULE_EXT,
        "lib" + moduleName + "." + STARBYTES_NATIVE_MODULE_EXT
    };

    for(const auto &dir : searchDirs) {
        std::error_code dirErr;
        if(!std::filesystem::exists(dir, dirErr) || dirErr || !std::filesystem::is_directory(dir, dirErr)) {
            continue;
        }
        for(const auto &fileName : candidateNames) {
            auto candidate = dir / fileName;
            std::error_code fileErr;
            if(std::filesystem::exists(candidate, fileErr) && !fileErr &&
               std::filesystem::is_regular_file(candidate, fileErr) && !fileErr) {
                return candidate;
            }
        }
    }
    return std::nullopt;
}

std::vector<std::filesystem::path> collectAutoNativeModulePaths(const DriverOptions &opts,
                                                                const ModuleGraph &graph,
                                                                const std::filesystem::path &compiledModulePath,
                                                                const std::filesystem::path &workspaceRoot,
                                                                const ResolverContext &resolverContext,
                                                                std::vector<std::string> &warnings) {
    std::vector<std::filesystem::path> paths;
    if(!opts.autoLoadNative) {
        return paths;
    }

    std::vector<std::filesystem::path> baseSearchDirs;
    for(const auto &dir : opts.nativeSearchDirs) {
        baseSearchDirs.emplace_back(dir);
    }
    baseSearchDirs.push_back(std::filesystem::current_path());
    baseSearchDirs.push_back(std::filesystem::current_path() / "stdlib");
    baseSearchDirs.push_back(compiledModulePath.parent_path());
    baseSearchDirs.push_back(workspaceRoot / "stdlib");
    baseSearchDirs.push_back(workspaceRoot / "build" / "stdlib");
    for(const auto &root : resolverContext.moduleSearchDirs) {
        baseSearchDirs.push_back(root);
    }

    std::unordered_set<std::string> seen;
    for(const auto &unitKey : graph.buildOrder) {
        auto unitIt = graph.unitsByKey.find(unitKey);
        if(unitIt == graph.unitsByKey.end()) {
            continue;
        }
        const auto &unit = unitIt->second;
        if(!moduleDeclaresNativeSymbols(unit)) {
            continue;
        }

        auto moduleName = unit.moduleDir.filename().string();
        if(moduleName.empty()) {
            continue;
        }

        std::vector<std::filesystem::path> searchDirs = baseSearchDirs;
        searchDirs.push_back(unit.moduleDir);
        searchDirs.push_back(unit.moduleDir.parent_path());

        auto resolvedPath = findNativeModuleByName(moduleName, searchDirs);
        if(!resolvedPath.has_value()) {
            warnings.push_back("Native module binary was not found for `" + moduleName +
                               "`. Use --native <path> or --native-dir <dir>.");
            continue;
        }
        appendUniquePath(*resolvedPath, paths, seen);
    }
    return paths;
}

void printUsage(std::ostream &out) {
    out << "Usage:\n";
    out << "  starbytes [command] <script." << STARBYTES_SRCFILE_EXT << "|module_dir> [options] [-- <script args...>]\n";
    out << "  starbytes --help\n";
    out << "  starbytes --version\n";
}

void printHelp(std::ostream &out) {
    out << "Starbytes Driver\n\n";
    printUsage(out);
    out << "\nCommands:\n";
    out << "  run      Compile and execute a script or module folder (default command).\n";
    out << "  compile  Compile a script or module folder into a ." << STARBYTES_COMPILEDFILE_EXT << " module.\n";
    out << "  check    Parse and run semantic checks only (no module output).\n";
    out << "  help     Show this help page.\n";

    out << "\nOptions:\n";
    out << "  -h, --help                 Show help.\n";
    out << "  -V, --version              Show driver version.\n";
    out << "  -m, --modulename <name>    Override module name (default: input file stem or folder name).\n";
    out << "  -o, --output <file>        Output module path (overrides --out-dir).\n";
    out << "  -d, --out-dir <dir>        Output directory for compiled module (default: .starbytes).\n";
    out << "      --run                  Execute after compile (useful with compile command).\n";
    out << "      --no-run               Skip execution after compile.\n";
    out << "      --clean                Remove generated module file on success.\n";
    out << "      --print-module-path    Print resolved module output path.\n";
    out << "      --profile-compile      Print structured compile phase timings.\n";
    out << "      --profile-compile-out <path>\n";
    out << "                              Write compile profile output to file.\n";
    out << "      --no-diagnostics       Do not print diagnostics buffered by runtime handlers.\n";
    out << "  -n, --native <path>        Load a native module binary before runtime execution (repeatable).\n";
    out << "  -L, --native-dir <dir>     Add a search directory for auto native module resolution (repeatable).\n";
    out << "      --no-native-auto       Disable automatic native module resolution from imports.\n";
    out << "      -- <args...>           Forward remaining arguments to script runtime (CmdLine module).\n";

    out << "\nExamples:\n";
    out << "  starbytes hello." << STARBYTES_SRCFILE_EXT << "\n";
    out << "  starbytes run ./app\n";
    out << "  starbytes run hello." << STARBYTES_SRCFILE_EXT << " -m HelloApp\n";
    out << "  starbytes compile hello." << STARBYTES_SRCFILE_EXT << " -o build/hello." << STARBYTES_COMPILEDFILE_EXT << "\n";
    out << "  starbytes check ./libmodule\n";
    out << "  starbytes check hello." << STARBYTES_SRCFILE_EXT << "\n";
    out << "  starbytes run app." << STARBYTES_SRCFILE_EXT << " -n ./build/stdlib/libIO." << STARBYTES_NATIVE_MODULE_EXT << "\n";
}

void printVersion(std::ostream &out) {
#ifdef STARBYTES_VERSION
    out << "starbytes " << STARBYTES_STRINGIFY(STARBYTES_VERSION) << '\n';
#else
    out << "starbytes (version unknown)" << '\n';
#endif
}

std::string commandToString(DriverCommand command) {
    switch(command) {
        case DriverCommand::Run:
            return "run";
        case DriverCommand::Compile:
            return "compile";
        case DriverCommand::Check:
            return "check";
        case DriverCommand::Help:
            return "help";
    }
    return "run";
}

ParseResult parseArgs(int argc, const char *argv[], DriverOptions &opts) {
    starbytes::cl::Parser parser;
    parser.addCommand("run");
    parser.addCommand("compile");
    parser.addCommand("check");
    parser.addCommand("help");

    parser.addFlagOption("help",{"h"});
    parser.addFlagOption("version",{"V"});
    parser.addValueOption("modulename",{"m"});
    parser.addValueOption("output",{"o"});
    parser.addValueOption("out-dir",{"d"});
    parser.addMultiValueOption("native",{"n"});
    parser.addMultiValueOption("native-dir",{"L"});
    parser.addFlagOption("run");
    parser.addFlagOption("no-run");
    parser.addFlagOption("clean");
    parser.addFlagOption("print-module-path");
    parser.addFlagOption("profile-compile");
    parser.addValueOption("profile-compile-out");
    parser.addFlagOption("no-diagnostics");
    parser.addFlagOption("no-native-auto");

    auto parsed = parser.parse(argc,argv);
    if(!parsed.ok) {
        return {false, parsed.exitCode, parsed.error};
    }

    if(parsed.command == "run") {
        opts.command = DriverCommand::Run;
    }
    else if(parsed.command == "compile") {
        opts.command = DriverCommand::Compile;
    }
    else if(parsed.command == "check") {
        opts.command = DriverCommand::Check;
    }
    else if(parsed.command == "help") {
        opts.command = DriverCommand::Help;
    }

    opts.showHelp = parsed.hasFlag("help") || opts.command == DriverCommand::Help;
    opts.showVersion = parsed.hasFlag("version");
    opts.cleanModule = parsed.hasFlag("clean");
    opts.printModulePath = parsed.hasFlag("print-module-path");
    opts.profileCompile = parsed.hasFlag("profile-compile");
    opts.logDiagnostics = !parsed.hasFlag("no-diagnostics");
    opts.autoLoadNative = !parsed.hasFlag("no-native-auto");
    opts.scriptArgs = parsed.passthroughArgs;

    const auto &moduleNameValues = parsed.values("modulename");
    if(!moduleNameValues.empty()) {
        opts.moduleName = moduleNameValues.back();
    }
    const auto &outputValues = parsed.values("output");
    if(!outputValues.empty()) {
        opts.outputPath = outputValues.back();
    }
    const auto &outDirValues = parsed.values("out-dir");
    if(!outDirValues.empty()) {
        opts.outDir = outDirValues.back();
    }
    const auto &profileOutValues = parsed.values("profile-compile-out");
    if(!profileOutValues.empty()) {
        opts.profileCompileOutPath = profileOutValues.back();
        opts.profileCompile = true;
    }
    opts.nativeModules.assign(parsed.values("native").begin(),parsed.values("native").end());
    opts.nativeSearchDirs.assign(parsed.values("native-dir").begin(),parsed.values("native-dir").end());

    bool forceRun = parsed.hasFlag("run");
    bool forceNoRun = parsed.hasFlag("no-run");

    if(opts.showHelp || opts.command == DriverCommand::Help || opts.showVersion) {
        return {true, 0, ""};
    }

    if(forceRun && forceNoRun) {
        return {false, 1, "Conflicting options: --run and --no-run cannot be used together."};
    }

    if(parsed.positionals.empty()) {
        return {false, 1, "Missing input path."};
    }
    if(parsed.positionals.size() > 1) {
        return {false, 1, "Too many positional arguments. Expected exactly one input path."};
    }

    opts.scriptPath = parsed.positionals.front();

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
    auto sessionStart = std::chrono::steady_clock::now();
    CompileProfileData profile;
    std::string profileOutPath;
    auto finishWith = [&](int code) {
        gActiveCompileProfile = nullptr;
        if(profile.enabled) {
            auto sessionEnd = std::chrono::steady_clock::now();
            profile.totalNs = std::chrono::duration_cast<std::chrono::nanoseconds>(sessionEnd - sessionStart).count();
            if(!profileOutPath.empty()) {
                std::ofstream out(profileOutPath, std::ios::out | std::ios::trunc);
                if(!out.is_open()) {
                    std::cerr << "Failed to open profile output file: " << profileOutPath << std::endl;
                    starbytes::driver::profile::printCompileProfile(std::cout, profile, false);
                    return 1;
                }
                starbytes::driver::profile::printCompileProfile(out, profile, code == 0);
                out.close();
            } else {
                starbytes::driver::profile::printCompileProfile(std::cout, profile, code == 0);
            }
        }
        return code;
    };

    DriverOptions opts;
    auto parsed = parseArgs(argc, argv, opts);
    profile.enabled = opts.profileCompile;
    profileOutPath = opts.profileCompileOutPath;
    profile.command = commandToString(opts.command);
    profile.input = opts.scriptPath;
    profile.moduleName = opts.moduleName;

    if(!parsed.ok) {
        if(!parsed.error.empty()) {
            std::cerr << parsed.error << std::endl;
        }
        printUsage(std::cerr);
        return finishWith(parsed.exitCode);
    }

    if(opts.showVersion) {
        printVersion(std::cout);
    }
    if(opts.showHelp || opts.command == DriverCommand::Help) {
        printHelp(std::cout);
        return finishWith(0);
    }
    if(opts.showVersion) {
        return finishWith(0);
    }

    std::filesystem::path inputPath(opts.scriptPath);
    if(!std::filesystem::exists(inputPath)) {
        std::cerr << "Input path not found: " << opts.scriptPath << std::endl;
        return finishWith(1);
    }

    auto workspaceRoot = std::filesystem::current_path();
    auto absoluteInputPath = std::filesystem::path(makeAbsolutePathString(inputPath));
    profile.input = absoluteInputPath.string();

    StarbytesRuntimeSetExecutablePath((argc > 0 && argv[0]) ? argv[0] : "");
    StarbytesRuntimeSetScriptPath(absoluteInputPath.string().c_str());
    StarbytesRuntimeClearScriptArgs();
    for(const auto &scriptArg : opts.scriptArgs) {
        StarbytesRuntimePushScriptArg(scriptArg.c_str());
    }

    std::vector<std::string> resolverWarnings;
    auto resolverContext = buildResolverContext(absoluteInputPath, workspaceRoot, argv[0], resolverWarnings);
    for(const auto &warning : resolverWarnings) {
        std::cerr << "Warning: " << warning << std::endl;
    }

    auto analysisCacheRoot = opts.outDir.empty() ? std::filesystem::path(".starbytes")
                                                  : std::filesystem::path(opts.outDir);
    if(!opts.outputPath.empty()) {
        auto outputParent = std::filesystem::path(opts.outputPath).parent_path();
        if(!outputParent.empty()) {
            analysisCacheRoot = outputParent;
        }
    }
    auto analysisCachePath = analysisCacheRoot / ".cache" / "module_analysis_cache.v1";
    auto compilerVersion = compilerVersionString();
    auto analysisFlagsHash = computeModuleAnalysisFlagsHash(opts, resolverContext);
    ModuleAnalysisCache analysisCache;
    std::string analysisCacheWarning;
    loadModuleAnalysisCache(analysisCachePath, analysisCache, analysisCacheWarning);
    if(!analysisCacheWarning.empty()) {
        std::cerr << "Warning: " << analysisCacheWarning << std::endl;
    }

    ModuleGraph graph;
    std::string graphError;
    gActiveCompileProfile = profile.enabled ? &profile : nullptr;
    auto moduleGraphStart = std::chrono::steady_clock::now();
    if(!discoverModuleGraph(absoluteInputPath,
                            workspaceRoot,
                            resolverContext,
                            analysisCache,
                            compilerVersion,
                            analysisFlagsHash,
                            graph,
                            graphError)) {
        std::cerr << graphError << std::endl;
        if(profile.enabled) {
            auto moduleGraphEnd = std::chrono::steady_clock::now();
            profile.moduleGraphNs = std::chrono::duration_cast<std::chrono::nanoseconds>(moduleGraphEnd - moduleGraphStart).count();
        }
        return finishWith(1);
    }
    if(analysisCache.dirty) {
        std::string cacheSaveError;
        if(!saveModuleAnalysisCache(analysisCachePath, analysisCache, cacheSaveError)) {
            std::cerr << "Warning: " << cacheSaveError << std::endl;
        }
    }
    if(profile.enabled) {
        auto moduleGraphEnd = std::chrono::steady_clock::now();
        profile.moduleGraphNs = std::chrono::duration_cast<std::chrono::nanoseconds>(moduleGraphEnd - moduleGraphStart).count();
        profile.moduleCount = graph.unitsByKey.size();
        uint64_t sourceCount = 0;
        for(const auto &entry : graph.unitsByKey) {
            sourceCount += entry.second.sources.size();
        }
        profile.sourceCount = sourceCount;
    }

    if(opts.moduleName.empty()) {
        opts.moduleName = defaultModuleNameForPath(absoluteInputPath);
    }
    profile.moduleName = opts.moduleName;

    if(graph.rootIsDirectory && opts.executeAfterCompile && !graph.rootHasMainSource) {
        std::cerr << "Missing entrypoint `main." << STARBYTES_SRCFILE_EXT
                  << "` in module directory: " << absoluteInputPath << std::endl;
        return finishWith(1);
    }

    if(opts.command == DriverCommand::Check) {
        NullASTConsumer astConsumer;
        starbytes::Parser parser(astConsumer);
        parser.setProfilingEnabled(profile.enabled);
        if(profile.enabled) {
            parser.resetProfileData();
        }
        auto parseContext = starbytes::ModuleParseContext::Create(opts.moduleName);
        std::string parseError;
        if(!parseModuleGraphSources(parser, parseContext, graph, parseError)) {
            std::cerr << parseError << std::endl;
            if(profile.enabled) {
                const auto &parserProfile = parser.getProfileData();
                profile.parseTotalNs = parserProfile.totalNs;
                profile.lexNs = parserProfile.lexNs;
                profile.syntaxNs = parserProfile.syntaxNs;
                profile.semanticNs = parserProfile.semanticNs;
                profile.consumerNs = parserProfile.consumerNs;
                profile.parserSourceBytes = parserProfile.sourceBytes;
                profile.parserTokenCount = parserProfile.tokenCount;
                profile.parserStatementCount = parserProfile.statementCount;
                profile.parserFileCount = parserProfile.fileCount;
            }
            return finishWith(1);
        }
        auto ok = parser.finish();
        if(profile.enabled) {
            const auto &parserProfile = parser.getProfileData();
            profile.parseTotalNs = parserProfile.totalNs;
            profile.lexNs = parserProfile.lexNs;
            profile.syntaxNs = parserProfile.syntaxNs;
            profile.semanticNs = parserProfile.semanticNs;
            profile.consumerNs = parserProfile.consumerNs;
            profile.parserSourceBytes = parserProfile.sourceBytes;
            profile.parserTokenCount = parserProfile.tokenCount;
            profile.parserStatementCount = parserProfile.statementCount;
            profile.parserFileCount = parserProfile.fileCount;
        }
        maybeLogRuntimeDiagnostics(opts);
        return finishWith(ok ? 0 : 1);
    }

    auto compiledModulePath = resolveCompiledModulePath(opts);

    std::string outputDirError;
    if(!ensureOutputParentDir(compiledModulePath, outputDirError)) {
        std::cerr << outputDirError << std::endl;
        return finishWith(1);
    }

    std::ofstream moduleOut(compiledModulePath, std::ios::out | std::ios::binary);
    if(!moduleOut.is_open()) {
        std::cerr << "Failed to open output module for writing: " << compiledModulePath << std::endl;
        return finishWith(1);
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
    parser.setProfilingEnabled(profile.enabled);
    if(profile.enabled) {
        parser.resetProfileData();
    }
    std::string parseError;
    if(!parseModuleGraphSources(parser, parseContext, graph, parseError)) {
        moduleOut.close();
        std::error_code removeErr;
        std::filesystem::remove(compiledModulePath, removeErr);
        if(removeErr) {
            std::cerr << "Warning: failed to remove '" << compiledModulePath << "': " << removeErr.message() << std::endl;
        }
        std::cerr << parseError << std::endl;
        if(profile.enabled) {
            const auto &parserProfile = parser.getProfileData();
            profile.parseTotalNs = parserProfile.totalNs;
            profile.lexNs = parserProfile.lexNs;
            profile.syntaxNs = parserProfile.syntaxNs;
            profile.semanticNs = parserProfile.semanticNs;
            profile.consumerNs = parserProfile.consumerNs;
            profile.parserSourceBytes = parserProfile.sourceBytes;
            profile.parserTokenCount = parserProfile.tokenCount;
            profile.parserStatementCount = parserProfile.statementCount;
            profile.parserFileCount = parserProfile.fileCount;
        }
        maybeLogRuntimeDiagnostics(opts);
        return finishWith(1);
    }
    if(!parser.finish()) {
        moduleOut.close();
        std::error_code removeErr;
        std::filesystem::remove(compiledModulePath, removeErr);
        if(removeErr) {
            std::cerr << "Warning: failed to remove '" << compiledModulePath << "': " << removeErr.message() << std::endl;
        }
        if(profile.enabled) {
            const auto &parserProfile = parser.getProfileData();
            profile.parseTotalNs = parserProfile.totalNs;
            profile.lexNs = parserProfile.lexNs;
            profile.syntaxNs = parserProfile.syntaxNs;
            profile.semanticNs = parserProfile.semanticNs;
            profile.consumerNs = parserProfile.consumerNs;
            profile.parserSourceBytes = parserProfile.sourceBytes;
            profile.parserTokenCount = parserProfile.tokenCount;
            profile.parserStatementCount = parserProfile.statementCount;
            profile.parserFileCount = parserProfile.fileCount;
        }
        maybeLogRuntimeDiagnostics(opts);
        return finishWith(1);
    }

    if(profile.enabled) {
        const auto &parserProfile = parser.getProfileData();
        profile.parseTotalNs = parserProfile.totalNs;
        profile.lexNs = parserProfile.lexNs;
        profile.syntaxNs = parserProfile.syntaxNs;
        profile.semanticNs = parserProfile.semanticNs;
        profile.consumerNs = parserProfile.consumerNs;
        profile.parserSourceBytes = parserProfile.sourceBytes;
        profile.parserTokenCount = parserProfile.tokenCount;
        profile.parserStatementCount = parserProfile.statementCount;
        profile.parserFileCount = parserProfile.fileCount;
    }

    auto genFinishStart = std::chrono::steady_clock::now();
    gen.finish();
    if(profile.enabled) {
        auto genFinishEnd = std::chrono::steady_clock::now();
        profile.genFinishNs = std::chrono::duration_cast<std::chrono::nanoseconds>(genFinishEnd - genFinishStart).count();
    }
    moduleOut.close();

    if(opts.printModulePath) {
        std::cout << compiledModulePath << std::endl;
    }

    if(opts.executeAfterCompile) {
        auto runtimeStart = std::chrono::steady_clock::now();
        std::ifstream rtcodeIn(compiledModulePath, std::ios::in | std::ios::binary);
        if(!rtcodeIn.is_open()) {
            std::cerr << "Failed to open compiled module: " << compiledModulePath << std::endl;
            return finishWith(1);
        }

        auto interp = starbytes::Runtime::Interp::Create();
        std::unordered_set<std::string> loadedNativePaths;
        auto tryLoadNativeModule = [&](const std::filesystem::path &nativePath, bool required) -> bool {
            auto normalizedPath = makeAbsolutePathString(nativePath);
            if(loadedNativePaths.find(normalizedPath) != loadedNativePaths.end()) {
                return true;
            }

            std::error_code statErr;
            if(!std::filesystem::exists(normalizedPath, statErr) || statErr ||
               !std::filesystem::is_regular_file(normalizedPath, statErr) || statErr) {
                if(required) {
                    std::cerr << "Native module not found: " << normalizedPath << std::endl;
                    return false;
                }
                return true;
            }

            if(!interp->addExtension(normalizedPath)) {
                if(required) {
                    std::cerr << "Failed to load native module: " << normalizedPath << std::endl;
                    return false;
                }
                std::cerr << "Warning: failed to load native module: " << normalizedPath << std::endl;
                return true;
            }
            loadedNativePaths.insert(normalizedPath);
            return true;
        };

        for(const auto &nativePath : opts.nativeModules) {
            if(!tryLoadNativeModule(std::filesystem::path(nativePath), true)) {
                return finishWith(1);
            }
        }

        std::vector<std::string> autoNativeWarnings;
        auto autoNativePaths = collectAutoNativeModulePaths(opts, graph, compiledModulePath, workspaceRoot, resolverContext, autoNativeWarnings);
        for(const auto &warning : autoNativeWarnings) {
            std::cerr << "Warning: " << warning << std::endl;
        }
        for(const auto &autoPath : autoNativePaths) {
            if(!tryLoadNativeModule(autoPath, false)) {
                return finishWith(1);
            }
        }

        interp->exec(rtcodeIn);
        if(profile.enabled) {
            auto runtimeEnd = std::chrono::steady_clock::now();
            profile.runtimeExecNs = std::chrono::duration_cast<std::chrono::nanoseconds>(runtimeEnd - runtimeStart).count();
        }
    }

    if(opts.cleanModule) {
        std::error_code removeErr;
        std::filesystem::remove(compiledModulePath, removeErr);
        if(removeErr && std::filesystem::exists(compiledModulePath)) {
            std::cerr << "Warning: failed to remove '" << compiledModulePath << "': " << removeErr.message() << std::endl;
        }
    }

    maybeLogRuntimeDiagnostics(opts);

    return finishWith((starbytes::stdDiagnosticHandler && starbytes::stdDiagnosticHandler->hasErrored()) ? 1 : 0);
}
