#include "starbytes/base/FileExt.def"
#include "starbytes/base/CmdLine.h"
#include "starbytes/compiler/AST.h"
#include "starbytes/compiler/Parser.h"
#include "starbytes/compiler/Gen.h"
#include "starbytes/compiler/RTCode.h"
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
#include <future>
#include <mutex>
#include <condition_variable>
#include <thread>

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
    bool infer64BitNumbers = false;
    std::vector<std::string> nativeModules;
    std::vector<std::string> nativeSearchDirs;
    unsigned jobs = 1;
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
    std::vector<std::string> dependencyKeys;
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

struct ModuleBuildCacheEntry {
    uint64_t moduleHash = 0;
    uint64_t fingerprint = 0;
    uint64_t flagsHash = 0;
    std::string compilerVersion;
    std::string segmentPath;
    std::string symbolPath;
    std::string interfacePath;
};

struct ModuleBuildCache {
    std::unordered_map<std::string, ModuleBuildCacheEntry> entriesByModuleKey;
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

void pruneStaleModuleAnalysisCacheEntries(ModuleAnalysisCache &cache){
    std::vector<std::string> staleKeys;
    staleKeys.reserve(cache.entriesBySourcePath.size());
    for(const auto &entry : cache.entriesBySourcePath){
        std::error_code ec;
        auto sourcePath = std::filesystem::path(entry.first);
        if(!std::filesystem::exists(sourcePath,ec) || ec ||
           !std::filesystem::is_regular_file(sourcePath,ec) || ec) {
            staleKeys.push_back(entry.first);
        }
    }
    if(staleKeys.empty()){
        return;
    }
    for(const auto &key : staleKeys){
        cache.entriesBySourcePath.erase(key);
    }
    cache.dirty = true;
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

uint64_t combineHash64(uint64_t seed,uint64_t value){
    uint64_t mixed = value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
    return seed ^ mixed;
}

uint64_t hashPath64(const std::filesystem::path &path){
    return hashString64(makeAbsolutePathString(path));
}

uint64_t computeModuleSourceHash(const ModuleBuildUnit &unit){
    uint64_t hash = 1469598103934665603ULL;
    hash = combineHash64(hash,hashPath64(unit.moduleDir));
    for(const auto &source : unit.sources){
        hash = combineHash64(hash,hashPath64(source.filePath));
        hash = combineHash64(hash,source.contentHash);
        hash = combineHash64(hash,source.isInterfaceFile ? 1ULL : 0ULL);
    }
    return hash;
}

std::unordered_map<std::string,uint64_t> computeModuleFingerprints(const ModuleGraph &graph){
    std::unordered_map<std::string,uint64_t> fingerprints;
    fingerprints.reserve(graph.unitsByKey.size());
    for(const auto &moduleKey : graph.buildOrder){
        auto unitIt = graph.unitsByKey.find(moduleKey);
        if(unitIt == graph.unitsByKey.end()){
            continue;
        }
        uint64_t hash = computeModuleSourceHash(unitIt->second);
        for(const auto &depKey : unitIt->second.dependencyKeys){
            auto depIt = fingerprints.find(depKey);
            if(depIt == fingerprints.end()){
                continue;
            }
            hash = combineHash64(hash,hashString64(depKey));
            hash = combineHash64(hash,depIt->second);
        }
        fingerprints[moduleKey] = hash;
    }
    return fingerprints;
}

bool loadModuleBuildCache(const std::filesystem::path &cachePath,
                          ModuleBuildCache &cache,
                          std::string &warning){
    cache.entriesByModuleKey.clear();
    cache.dirty = false;

    std::ifstream in(cachePath,std::ios::in);
    if(!in.is_open()){
        return true;
    }

    std::string header;
    if(!std::getline(in,header)){
        return true;
    }
    if(header != "STARBYTES_MODULE_BUILD_CACHE_V1"){
        warning = "Ignoring incompatible module build cache file: " + cachePath.string();
        return true;
    }

    while(in){
        std::string tag;
        in >> tag;
        if(!in){
            break;
        }
        if(tag != "ENTRY"){
            std::string discard;
            std::getline(in,discard);
            continue;
        }

        std::string moduleKey;
        ModuleBuildCacheEntry entry;
        in >> std::quoted(moduleKey)
           >> entry.moduleHash
           >> entry.fingerprint
           >> std::quoted(entry.compilerVersion)
           >> entry.flagsHash
           >> std::quoted(entry.segmentPath)
           >> std::quoted(entry.symbolPath)
           >> std::quoted(entry.interfacePath);
        if(!in){
            warning = "Ignoring malformed module build cache file: " + cachePath.string();
            cache.entriesByModuleKey.clear();
            return true;
        }
        cache.entriesByModuleKey[moduleKey] = std::move(entry);
    }
    return true;
}

void pruneStaleModuleBuildCacheEntries(ModuleBuildCache &cache){
    std::vector<std::string> staleKeys;
    staleKeys.reserve(cache.entriesByModuleKey.size());
    for(const auto &entry : cache.entriesByModuleKey){
        std::error_code ec;
        auto artifactPath = std::filesystem::path(entry.second.segmentPath);
        if(entry.second.segmentPath.empty() ||
           !std::filesystem::exists(artifactPath,ec) || ec ||
           !std::filesystem::is_regular_file(artifactPath,ec) || ec){
            staleKeys.push_back(entry.first);
        }
    }
    if(staleKeys.empty()){
        return;
    }
    for(const auto &key : staleKeys){
        cache.entriesByModuleKey.erase(key);
    }
    cache.dirty = true;
}

bool saveModuleBuildCache(const std::filesystem::path &cachePath,
                          const ModuleBuildCache &cache,
                          std::string &error){
    auto parent = cachePath.parent_path();
    if(!parent.empty()){
        std::error_code dirErr;
        std::filesystem::create_directories(parent,dirErr);
        if(dirErr){
            error = "Failed to create module build cache directory '" + parent.string() + "': " + dirErr.message();
            return false;
        }
    }

    std::ofstream out(cachePath,std::ios::out | std::ios::trunc);
    if(!out.is_open()){
        error = "Failed to write module build cache file: " + cachePath.string();
        return false;
    }

    out << "STARBYTES_MODULE_BUILD_CACHE_V1\n";
    for(const auto &pair : cache.entriesByModuleKey){
        const auto &moduleKey = pair.first;
        const auto &entry = pair.second;
        out << "ENTRY "
            << std::quoted(moduleKey) << " "
            << entry.moduleHash << " "
            << entry.fingerprint << " "
            << std::quoted(entry.compilerVersion) << " "
            << entry.flagsHash << " "
            << std::quoted(entry.segmentPath) << " "
            << std::quoted(entry.symbolPath) << " "
            << std::quoted(entry.interfacePath)
            << "\n";
    }
    return true;
}

std::vector<std::filesystem::path> generatedOutputArtifacts(const std::filesystem::path &compiledModulePath) {
    std::vector<std::filesystem::path> artifacts;
    artifacts.push_back(compiledModulePath);

    auto interfacePath = compiledModulePath;
    interfacePath.replace_extension("." STARBYTES_INTERFACEFILE_EXT);
    if(std::find(artifacts.begin(),artifacts.end(),interfacePath) == artifacts.end()) {
        artifacts.push_back(interfacePath);
    }

    auto symbolPath = compiledModulePath;
    symbolPath.replace_extension(".starbsymtb");
    if(std::find(artifacts.begin(),artifacts.end(),symbolPath) == artifacts.end()) {
        artifacts.push_back(symbolPath);
    }

    return artifacts;
}

void removeGeneratedOutputArtifacts(const std::filesystem::path &compiledModulePath) {
    auto artifacts = generatedOutputArtifacts(compiledModulePath);
    for(const auto &artifactPath : artifacts) {
        std::error_code removeErr;
        std::filesystem::remove(artifactPath, removeErr);
        if(removeErr && std::filesystem::exists(artifactPath)) {
            std::cerr << "Warning: failed to remove '" << artifactPath << "': " << removeErr.message() << std::endl;
        }
    }
}

void cleanAnalysisCacheDirectory(const std::filesystem::path &analysisCacheRoot){
    auto cacheDir = analysisCacheRoot / ".cache";
    std::error_code removeErr;
    std::filesystem::remove_all(cacheDir,removeErr);
    if(removeErr && std::filesystem::exists(cacheDir)) {
        std::cerr << "Warning: failed to remove cache directory '" << cacheDir << "': "
                  << removeErr.message() << std::endl;
    }

    std::error_code emptyErr;
    if(std::filesystem::exists(analysisCacheRoot,emptyErr) && !emptyErr &&
       std::filesystem::is_directory(analysisCacheRoot,emptyErr) && !emptyErr &&
       std::filesystem::is_empty(analysisCacheRoot,emptyErr) && !emptyErr) {
        std::error_code rootRemoveErr;
        std::filesystem::remove(analysisCacheRoot,rootRemoveErr);
        if(rootRemoveErr && std::filesystem::exists(analysisCacheRoot)) {
            std::cerr << "Warning: failed to remove empty cache root '" << analysisCacheRoot << "': "
                      << rootRemoveErr.message() << std::endl;
        }
    }
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
    key << "infer_64bit_numbers=" << (opts.infer64BitNumbers ? "1" : "0") << ";";
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
        auto dependencyKey = makeAbsolutePathString(*resolvedDir);
        if(std::find(unit.dependencyKeys.begin(),unit.dependencyKeys.end(),dependencyKey) == unit.dependencyKeys.end()){
            unit.dependencyKeys.push_back(dependencyKey);
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
        auto dependencyKey = makeAbsolutePathString(*resolvedDir);
        if(std::find(rootUnit.dependencyKeys.begin(),rootUnit.dependencyKeys.end(),dependencyKey) == rootUnit.dependencyKeys.end()){
            rootUnit.dependencyKeys.push_back(dependencyKey);
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

std::string artifactNameForModuleKey(const std::string &moduleKey){
    std::ostringstream out;
    out << std::hex << hashString64(moduleKey);
    return out.str();
}

struct ModuleCompileTaskResult {
    bool success = false;
    bool rebuilt = false;
    std::string moduleKey;
    std::string diagnostics;
    std::string error;
    std::filesystem::path segmentPath;
    std::filesystem::path symbolPath;
    std::filesystem::path interfacePath;
    std::shared_ptr<starbytes::Semantics::SymbolTable> symbols;
    starbytes::Parser::ProfileData parserProfile;
    uint64_t genFinishNs = 0;
};

class TaskLimiter {
    std::mutex mutex;
    std::condition_variable cv;
    unsigned maxActive = 1;
    unsigned active = 0;
public:
    explicit TaskLimiter(unsigned maxActive):maxActive(std::max(1u,maxActive)){}

    void acquire(){
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait(lock,[&](){ return active < maxActive; });
        ++active;
    }

    void release(){
        std::unique_lock<std::mutex> lock(mutex);
        if(active > 0){
            --active;
        }
        lock.unlock();
        cv.notify_one();
    }
};

ModuleCompileTaskResult compileModuleSymbolsOnly(const std::string &moduleKey,
                                                 const ModuleBuildUnit &unit,
                                                 const std::unordered_map<std::string,std::shared_ptr<starbytes::Semantics::SymbolTable>> &depTables,
                                                 bool profileEnabled,
                                                 bool infer64BitNumbers){
    ModuleCompileTaskResult result;
    result.moduleKey = moduleKey;

    std::ostringstream diagSink;
    auto diagnostics = starbytes::DiagnosticHandler::createDefault(diagSink);
    NullASTConsumer consumer;
    starbytes::Parser parser(consumer,std::move(diagnostics));
    parser.setProfilingEnabled(profileEnabled);
    parser.setInfer64BitNumbers(infer64BitNumbers);
    if(profileEnabled){
        parser.resetProfileData();
    }

    auto parseContext = starbytes::ModuleParseContext::Create(moduleKey);
    for(const auto &depKey : unit.dependencyKeys){
        auto depIt = depTables.find(depKey);
        if(depIt != depTables.end() && depIt->second){
            parseContext.sTableContext.otherTables.push_back(depIt->second);
        }
    }

    std::string parseError;
    for(const auto &source : unit.sources){
        parseContext.name = makeAbsolutePathString(source.filePath);
        std::istringstream in(source.fileText);
        parser.parseFromStream(in,parseContext);
    }

    auto ok = parser.finish();
    result.parserProfile = parser.getProfileData();
    result.diagnostics = diagSink.str();
    if(!ok){
        result.success = false;
        if(result.diagnostics.empty()){
            result.error = "semantic analysis failed";
        }
        return result;
    }

    result.symbols = std::shared_ptr<starbytes::Semantics::SymbolTable>(parseContext.sTableContext.main.release());
    result.success = (result.symbols != nullptr);
    if(!result.success){
        result.error = "failed to materialize symbol table";
    }
    return result;
}

ModuleCompileTaskResult compileModuleToSegment(const std::string &moduleKey,
                                               const std::string &moduleName,
                                               const ModuleBuildUnit &unit,
                                               const std::unordered_map<std::string,std::shared_ptr<starbytes::Semantics::SymbolTable>> &depTables,
                                               const std::filesystem::path &artifactDir,
                                               bool profileEnabled,
                                               bool infer64BitNumbers,
                                               bool generateInterface,
                                               const std::unordered_set<std::string> &interfaceAllowlist){
    ModuleCompileTaskResult result;
    result.moduleKey = moduleKey;
    result.rebuilt = true;

    std::error_code dirErr;
    std::filesystem::create_directories(artifactDir,dirErr);
    if(dirErr){
        result.error = "failed to create module artifact directory `" + artifactDir.string() + "`: " + dirErr.message();
        return result;
    }

    auto segmentPath = artifactDir / (moduleName + ".segment");
    std::ofstream moduleOut(segmentPath,std::ios::out | std::ios::binary);
    if(!moduleOut.is_open()){
        result.error = "failed to open module segment for writing: " + segmentPath.string();
        return result;
    }

    std::ostringstream diagSink;
    auto diagnostics = starbytes::DiagnosticHandler::createDefault(diagSink);
    starbytes::Gen gen;
    auto outputPathForGen = artifactDir;
    auto genContext = starbytes::ModuleGenContext::Create(moduleName,moduleOut,outputPathForGen);
    genContext.generateInterface = generateInterface;
    if(generateInterface){
        genContext.interfaceSourceAllowlist = interfaceAllowlist;
    }
    gen.setContext(&genContext);

    starbytes::Parser parser(gen,std::move(diagnostics));
    parser.setProfilingEnabled(profileEnabled);
    parser.setInfer64BitNumbers(infer64BitNumbers);
    if(profileEnabled){
        parser.resetProfileData();
    }

    auto parseContext = starbytes::ModuleParseContext::Create(moduleKey);
    for(const auto &depKey : unit.dependencyKeys){
        auto depIt = depTables.find(depKey);
        if(depIt != depTables.end() && depIt->second){
            parseContext.sTableContext.otherTables.push_back(depIt->second);
        }
    }

    for(const auto &source : unit.sources){
        parseContext.name = makeAbsolutePathString(source.filePath);
        std::istringstream in(source.fileText);
        parser.parseFromStream(in,parseContext);
    }

    auto ok = parser.finish();
    result.parserProfile = parser.getProfileData();
    result.diagnostics = diagSink.str();
    if(!ok){
        moduleOut.close();
        std::filesystem::remove(segmentPath,dirErr);
        result.success = false;
        if(result.diagnostics.empty()){
            result.error = "module compile failed";
        }
        return result;
    }

    auto genFinishStart = std::chrono::steady_clock::now();
    gen.finish();
    auto genFinishEnd = std::chrono::steady_clock::now();
    result.genFinishNs = std::chrono::duration_cast<std::chrono::nanoseconds>(genFinishEnd - genFinishStart).count();
    moduleOut.close();

    result.symbols = std::shared_ptr<starbytes::Semantics::SymbolTable>(parseContext.sTableContext.main.release());
    result.segmentPath = segmentPath;
    result.symbolPath = artifactDir / (moduleName + ".starbsymtb");
    result.interfacePath = artifactDir / (moduleName + "." STARBYTES_INTERFACEFILE_EXT);
    result.success = true;
    return result;
}

bool concatenateModuleSegments(const std::vector<std::string> &buildOrder,
                               const std::unordered_map<std::string,ModuleCompileTaskResult> &resultsByModule,
                               const std::filesystem::path &outputPath,
                               std::string &error){
    std::ofstream out(outputPath,std::ios::out | std::ios::binary | std::ios::trunc);
    if(!out.is_open()){
        error = "Failed to open linked output module for writing: " + outputPath.string();
        return false;
    }

    std::vector<char> buffer(64 * 1024);
    for(size_t i = 0;i < buildOrder.size();++i){
        auto it = resultsByModule.find(buildOrder[i]);
        if(it == resultsByModule.end()){
            error = "Internal driver error: missing build artifact for module `" + buildOrder[i] + "`.";
            return false;
        }
        auto segmentPath = it->second.segmentPath;
        std::ifstream in(segmentPath,std::ios::in | std::ios::binary);
        if(!in.is_open()){
            error = "Failed to open module segment `" + segmentPath.string() + "`.";
            return false;
        }

        std::error_code fileErr;
        auto fileSize = std::filesystem::file_size(segmentPath,fileErr);
        if(fileErr){
            error = "Failed to read module segment size `" + segmentPath.string() + "`: " + fileErr.message();
            return false;
        }
        std::streamsize bytesToCopy = static_cast<std::streamsize>(fileSize);
        if(i + 1 < buildOrder.size() && bytesToCopy >= static_cast<std::streamsize>(sizeof(starbytes::Runtime::RTCode))){
            bytesToCopy -= static_cast<std::streamsize>(sizeof(starbytes::Runtime::RTCode));
        }

        while(bytesToCopy > 0){
            auto chunk = static_cast<std::streamsize>(std::min<std::streamsize>(bytesToCopy,static_cast<std::streamsize>(buffer.size())));
            if(!in.read(buffer.data(),chunk)){
                error = "Failed to read module segment `" + segmentPath.string() + "`.";
                return false;
            }
            out.write(buffer.data(),chunk);
            if(!out.good()){
                error = "Failed to write linked module output `" + outputPath.string() + "`.";
                return false;
            }
            bytesToCopy -= chunk;
        }
    }

    return true;
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
    out << "      --clean                Remove generated output artifacts and compile cache on success.\n";
    out << "      --print-module-path    Print resolved module output path.\n";
    out << "      --profile-compile      Print structured compile phase timings.\n";
    out << "      --profile-compile-out <path>\n";
    out << "                              Write compile profile output to file.\n";
    out << "      --no-diagnostics       Do not print diagnostics buffered by runtime handlers.\n";
    out << "  -n, --native <path>        Load a native module binary before runtime execution (repeatable).\n";
    out << "  -L, --native-dir <dir>     Add a search directory for auto native module resolution (repeatable).\n";
    out << "  -j, --jobs <count>         Parallel module build jobs (default: CPU count).\n";
    out << "      --no-native-auto       Disable automatic native module resolution from imports.\n";
    out << "      --infer-64bit-numbers  Infer numeric literals as Long/Double by default.\n";
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
    parser.addValueOption("jobs",{"j"});
    parser.addFlagOption("run");
    parser.addFlagOption("no-run");
    parser.addFlagOption("clean");
    parser.addFlagOption("print-module-path");
    parser.addFlagOption("profile-compile");
    parser.addValueOption("profile-compile-out");
    parser.addFlagOption("no-diagnostics");
    parser.addFlagOption("no-native-auto");
    parser.addFlagOption("infer-64bit-numbers");

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
    opts.infer64BitNumbers = parsed.hasFlag("infer-64bit-numbers");
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
    const auto &jobsValues = parsed.values("jobs");
    if(!jobsValues.empty()) {
        try {
            auto parsedJobs = std::stoul(jobsValues.back());
            if(parsedJobs == 0) {
                return {false, 1, "Invalid --jobs value: must be >= 1."};
            }
            opts.jobs = static_cast<unsigned>(parsedJobs);
        }
        catch(...) {
            return {false, 1, "Invalid --jobs value: expected unsigned integer."};
        }
    }
    else {
        auto hw = std::thread::hardware_concurrency();
        opts.jobs = hw == 0 ? 1 : hw;
    }

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
    if(starbytes::stdDiagnosticHandler) {
        starbytes::stdDiagnosticHandler->setDefaultPhase(starbytes::Diagnostic::Phase::Runtime);
        starbytes::stdDiagnosticHandler->setDefaultSourceName("starbytes-runtime");
    }
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
    pruneStaleModuleAnalysisCacheEntries(analysisCache);

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
        parser.setInfer64BitNumbers(opts.infer64BitNumbers);
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
    auto moduleBuildCachePath = analysisCacheRoot / ".cache" / "module_build_cache.v1";
    ModuleBuildCache moduleBuildCache;
    std::string buildCacheWarning;
    loadModuleBuildCache(moduleBuildCachePath,moduleBuildCache,buildCacheWarning);
    if(!buildCacheWarning.empty()){
        std::cerr << "Warning: " << buildCacheWarning << std::endl;
    }
    pruneStaleModuleBuildCacheEntries(moduleBuildCache);

    auto moduleFingerprints = computeModuleFingerprints(graph);
    std::unordered_map<std::string,bool> moduleNeedsRebuild;
    moduleNeedsRebuild.reserve(graph.unitsByKey.size());
    std::unordered_map<std::string,std::filesystem::path> cachedSegmentPaths;
    cachedSegmentPaths.reserve(graph.unitsByKey.size());
    std::unordered_map<std::string,std::filesystem::path> cachedSymbolPaths;
    cachedSymbolPaths.reserve(graph.unitsByKey.size());
    std::unordered_map<std::string,std::filesystem::path> cachedInterfacePaths;
    cachedInterfacePaths.reserve(graph.unitsByKey.size());

    for(const auto &moduleKey : graph.buildOrder){
        auto unitIt = graph.unitsByKey.find(moduleKey);
        if(unitIt == graph.unitsByKey.end()){
            moduleNeedsRebuild[moduleKey] = true;
            continue;
        }
        auto moduleHash = computeModuleSourceHash(unitIt->second);
        auto fpIt = moduleFingerprints.find(moduleKey);
        auto moduleFingerprint = fpIt != moduleFingerprints.end() ? fpIt->second : moduleHash;

        bool rebuild = true;
        auto cacheIt = moduleBuildCache.entriesByModuleKey.find(moduleKey);
        if(cacheIt != moduleBuildCache.entriesByModuleKey.end()){
            const auto &entry = cacheIt->second;
            std::error_code segErr;
            auto segmentPath = std::filesystem::path(entry.segmentPath);
            bool segmentOk = !entry.segmentPath.empty()
                && std::filesystem::exists(segmentPath,segErr) && !segErr
                && std::filesystem::is_regular_file(segmentPath,segErr) && !segErr;
            if(segmentOk
               && entry.moduleHash == moduleHash
               && entry.fingerprint == moduleFingerprint
               && entry.compilerVersion == compilerVersion
               && entry.flagsHash == analysisFlagsHash){
                rebuild = false;
                cachedSegmentPaths[moduleKey] = segmentPath;
                if(!entry.symbolPath.empty()){
                    cachedSymbolPaths[moduleKey] = std::filesystem::path(entry.symbolPath);
                }
                if(!entry.interfacePath.empty()){
                    cachedInterfacePaths[moduleKey] = std::filesystem::path(entry.interfacePath);
                }
            }
        }
        moduleNeedsRebuild[moduleKey] = rebuild;
        if(profile.enabled){
            if(rebuild){
                profile.moduleCacheMisses += 1;
            }
            else {
                profile.moduleCacheHits += 1;
            }
        }
    }

    auto moduleArtifactDir = analysisCacheRoot / ".cache" / "module_artifacts";
    std::error_code artifactDirErr;
    std::filesystem::create_directories(moduleArtifactDir,artifactDirErr);
    if(artifactDirErr){
        std::cerr << "Failed to create module artifact cache directory `" << moduleArtifactDir
                  << "`: " << artifactDirErr.message() << std::endl;
        return finishWith(1);
    }

    std::unordered_map<std::string,std::shared_future<ModuleCompileTaskResult>> futuresByModule;
    futuresByModule.reserve(graph.unitsByKey.size());
    std::mutex moduleBuildCacheMutex;
    TaskLimiter limiter(opts.jobs);
    auto moduleBuildStart = std::chrono::steady_clock::now();

    for(const auto &moduleKey : graph.buildOrder){
        std::vector<std::pair<std::string,std::shared_future<ModuleCompileTaskResult>>> depFutures;
        bool missingDependencyFuture = false;
        std::string missingDependencyKey;
        auto unitIt = graph.unitsByKey.find(moduleKey);
        if(unitIt != graph.unitsByKey.end()){
            depFutures.reserve(unitIt->second.dependencyKeys.size());
            for(const auto &depKey : unitIt->second.dependencyKeys){
                auto depFutureIt = futuresByModule.find(depKey);
                if(depFutureIt == futuresByModule.end()){
                    missingDependencyFuture = true;
                    missingDependencyKey = depKey;
                    break;
                }
                depFutures.emplace_back(depKey,depFutureIt->second);
            }
        }

        if(missingDependencyFuture){
            futuresByModule[moduleKey] = std::async(std::launch::deferred,[moduleKey,missingDependencyKey]() -> ModuleCompileTaskResult {
                ModuleCompileTaskResult result;
                result.moduleKey = moduleKey;
                result.error = "Internal driver error: missing dependency future `" + missingDependencyKey + "`.";
                return result;
            }).share();
            continue;
        }

        futuresByModule[moduleKey] = std::async(std::launch::async,[&,moduleKey,depFutures = std::move(depFutures)]() -> ModuleCompileTaskResult {
            ModuleCompileTaskResult result;
            result.moduleKey = moduleKey;

            auto unitIt = graph.unitsByKey.find(moduleKey);
            if(unitIt == graph.unitsByKey.end()){
                result.error = "Internal driver error: missing module build unit `" + moduleKey + "`.";
                return result;
            }
            const auto &unit = unitIt->second;

            std::unordered_map<std::string,std::shared_ptr<starbytes::Semantics::SymbolTable>> depTables;
            depTables.reserve(depFutures.size());
            for(const auto &depFuture : depFutures){
                auto depResult = depFuture.second.get();
                if(!depResult.success){
                    result.error = "Dependency build failed for `" + depFuture.first + "`.";
                    return result;
                }
                depTables[depFuture.first] = depResult.symbols;
            }

            limiter.acquire();
            auto releaseGuard = std::shared_ptr<void>(nullptr,[&](void *){ limiter.release(); });

            bool rebuild = true;
            auto rebuildIt = moduleNeedsRebuild.find(moduleKey);
            if(rebuildIt != moduleNeedsRebuild.end()){
                rebuild = rebuildIt->second;
            }
            if(!rebuild){
                auto symbolOnly = compileModuleSymbolsOnly(moduleKey,unit,depTables,profile.enabled,opts.infer64BitNumbers);
                symbolOnly.segmentPath = cachedSegmentPaths[moduleKey];
                auto symIt = cachedSymbolPaths.find(moduleKey);
                if(symIt != cachedSymbolPaths.end()){
                    symbolOnly.symbolPath = symIt->second;
                }
                auto ifaceIt = cachedInterfacePaths.find(moduleKey);
                if(ifaceIt != cachedInterfacePaths.end()){
                    symbolOnly.interfacePath = ifaceIt->second;
                }
                return symbolOnly;
            }

            auto moduleArtifactName = artifactNameForModuleKey(moduleKey);
            bool shouldGenerateInterface = (moduleKey == graph.rootKey && graph.rootIsDirectory && !graph.rootHasMainSource);
            std::unordered_set<std::string> interfaceAllowlist;
            if(shouldGenerateInterface){
                bool hasConcreteSources = false;
                for(const auto &source : unit.sources){
                    if(!source.isInterfaceFile){
                        hasConcreteSources = true;
                        break;
                    }
                }
                for(const auto &source : unit.sources){
                    if(hasConcreteSources && source.isInterfaceFile){
                        continue;
                    }
                    interfaceAllowlist.insert(makeAbsolutePathString(source.filePath));
                }
            }

            auto compiled = compileModuleToSegment(moduleKey,
                                                   moduleArtifactName,
                                                   unit,
                                                   depTables,
                                                   moduleArtifactDir,
                                                   profile.enabled,
                                                   opts.infer64BitNumbers,
                                                   shouldGenerateInterface,
                                                   interfaceAllowlist);
            if(compiled.success){
                auto moduleHash = computeModuleSourceHash(unit);
                auto fpIt = moduleFingerprints.find(moduleKey);
                auto moduleFingerprint = fpIt != moduleFingerprints.end() ? fpIt->second : moduleHash;
                ModuleBuildCacheEntry entry;
                entry.moduleHash = moduleHash;
                entry.fingerprint = moduleFingerprint;
                entry.flagsHash = analysisFlagsHash;
                entry.compilerVersion = compilerVersion;
                entry.segmentPath = makeAbsolutePathString(compiled.segmentPath);
                entry.symbolPath = makeAbsolutePathString(compiled.symbolPath);
                std::error_code ifaceErr;
                if(!compiled.interfacePath.empty()
                   && std::filesystem::exists(compiled.interfacePath,ifaceErr) && !ifaceErr
                   && std::filesystem::is_regular_file(compiled.interfacePath,ifaceErr) && !ifaceErr){
                    entry.interfacePath = makeAbsolutePathString(compiled.interfacePath);
                }
                {
                    std::lock_guard<std::mutex> lock(moduleBuildCacheMutex);
                    moduleBuildCache.entriesByModuleKey[moduleKey] = std::move(entry);
                    moduleBuildCache.dirty = true;
                }
            }
            return compiled;
        }).share();
    }

    std::unordered_map<std::string,ModuleCompileTaskResult> moduleResults;
    moduleResults.reserve(graph.unitsByKey.size());
    bool moduleBuildFailed = false;
    for(const auto &moduleKey : graph.buildOrder){
        auto result = futuresByModule[moduleKey].get();
        moduleResults[moduleKey] = result;
        if(profile.enabled){
            profile.parseTotalNs += result.parserProfile.totalNs;
            profile.lexNs += result.parserProfile.lexNs;
            profile.syntaxNs += result.parserProfile.syntaxNs;
            profile.semanticNs += result.parserProfile.semanticNs;
            profile.consumerNs += result.parserProfile.consumerNs;
            profile.parserSourceBytes += result.parserProfile.sourceBytes;
            profile.parserTokenCount += result.parserProfile.tokenCount;
            profile.parserStatementCount += result.parserProfile.statementCount;
            profile.parserFileCount += result.parserProfile.fileCount;
            profile.genFinishNs += result.genFinishNs;
        }
        if(!result.success){
            moduleBuildFailed = true;
        }
    }

    if(profile.enabled){
        auto moduleBuildEnd = std::chrono::steady_clock::now();
        profile.moduleBuildNs = std::chrono::duration_cast<std::chrono::nanoseconds>(moduleBuildEnd - moduleBuildStart).count();
    }

    if(moduleBuildFailed){
        for(const auto &moduleKey : graph.buildOrder){
            auto it = moduleResults.find(moduleKey);
            if(it == moduleResults.end() || it->second.success){
                continue;
            }
            if(!it->second.error.empty()){
                std::cerr << "Error in module `" << moduleKey << "`: " << it->second.error << std::endl;
            }
            if(!it->second.diagnostics.empty()){
                std::cerr << it->second.diagnostics;
            }
        }
        maybeLogRuntimeDiagnostics(opts);
        return finishWith(1);
    }

    if(moduleBuildCache.dirty){
        std::string saveError;
        if(!saveModuleBuildCache(moduleBuildCachePath,moduleBuildCache,saveError)){
            std::cerr << "Warning: " << saveError << std::endl;
        }
    }

    auto linkStart = std::chrono::steady_clock::now();
    std::string linkError;
    if(!concatenateModuleSegments(graph.buildOrder,moduleResults,compiledModulePath,linkError)){
        std::cerr << linkError << std::endl;
        removeGeneratedOutputArtifacts(compiledModulePath);
        maybeLogRuntimeDiagnostics(opts);
        return finishWith(1);
    }
    if(profile.enabled){
        auto linkEnd = std::chrono::steady_clock::now();
        profile.moduleLinkNs = std::chrono::duration_cast<std::chrono::nanoseconds>(linkEnd - linkStart).count();
    }

    auto rootResultIt = moduleResults.find(graph.rootKey);
    if(rootResultIt != moduleResults.end()){
        auto finalSymPath = compiledModulePath;
        finalSymPath.replace_extension(".starbsymtb");
        if(!rootResultIt->second.symbolPath.empty()){
            std::error_code copyErr;
            std::filesystem::copy_file(rootResultIt->second.symbolPath,finalSymPath,std::filesystem::copy_options::overwrite_existing,copyErr);
            if(copyErr){
                std::cerr << "Warning: failed to copy root symbol table to `" << finalSymPath
                          << "`: " << copyErr.message() << std::endl;
            }
        }

        auto finalInterfacePath = compiledModulePath;
        finalInterfacePath.replace_extension("." STARBYTES_INTERFACEFILE_EXT);
        std::error_code ifaceErr;
        if(!rootResultIt->second.interfacePath.empty()
           && std::filesystem::exists(rootResultIt->second.interfacePath,ifaceErr) && !ifaceErr
           && std::filesystem::is_regular_file(rootResultIt->second.interfacePath,ifaceErr) && !ifaceErr){
            std::error_code copyErr;
            std::filesystem::copy_file(rootResultIt->second.interfacePath,finalInterfacePath,std::filesystem::copy_options::overwrite_existing,copyErr);
            if(copyErr){
                std::cerr << "Warning: failed to copy root interface file to `" << finalInterfacePath
                          << "`: " << copyErr.message() << std::endl;
            }
        }
    }

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
        removeGeneratedOutputArtifacts(compiledModulePath);
        cleanAnalysisCacheDirectory(analysisCacheRoot);
    }

    maybeLogRuntimeDiagnostics(opts);

    return finishWith((starbytes::stdDiagnosticHandler && starbytes::stdDiagnosticHandler->hasErrored()) ? 1 : 0);
}
