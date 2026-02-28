#include "starbytes/base/CmdLine.h"
#include "starbytes/base/FileExt.def"
#include "starbytes/linguistics/CodeActionEngine.h"
#include "starbytes/linguistics/FormatterEngine.h"
#include "starbytes/linguistics/LintEngine.h"
#include "starbytes/linguistics/Serializer.h"
#include "starbytes/linguistics/SuggestionEngine.h"
#include "starbytes/linguistics/Types.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <system_error>
#include <unordered_set>
#include <vector>

#define STARBYTES_STRINGIFY_IMPL(x) #x
#define STARBYTES_STRINGIFY(x) STARBYTES_STRINGIFY_IMPL(x)

namespace {

struct ParseResult {
    bool ok = false;
    int exitCode = 1;
    std::string error;
};

struct LingOptions {
    bool showHelp = false;
    bool showVersion = false;
    bool prettyWrite = false;
    bool lint = false;
    bool suggest = false;
    bool codeActions = false;
    bool applySafeFixes = false;
    bool dryRun = false;
    bool modpathAware = false;
    size_t maxFiles = 0;
    std::vector<std::string> includeGlobs;
    std::vector<std::string> excludeGlobs;
    std::string inputPath;
};

struct GlobPattern {
    std::string raw;
    std::regex regex;
    bool pathScoped = false;
};

struct FileDiscoveryResult {
    bool ok = false;
    std::vector<std::filesystem::path> files;
    std::vector<std::string> warnings;
    std::string error;
};

struct FileAnalysisCache {
    std::filesystem::path filePath;
    std::string sourceText;
    starbytes::linguistics::LinguisticsSession session;
    bool loaded = false;

    bool lintReady = false;
    starbytes::linguistics::LintResult lintResult;

    bool suggestionsReady = false;
    starbytes::linguistics::SuggestionResult suggestionResult;

    bool codeActionsReady = false;
    starbytes::linguistics::CodeActionResult codeActionResult;

    bool safeCodeActionsReady = false;
    starbytes::linguistics::CodeActionResult safeCodeActionResult;

    bool formatReady = false;
    starbytes::linguistics::FormatResult formatResult;
};

void printUsage(std::ostream &out) {
    out << "Usage:\n";
    out << "  starbytes-ling --pretty-write <source." << STARBYTES_SRCFILE_EXT << "|dir>\n";
    out << "  starbytes-ling --lint <source." << STARBYTES_SRCFILE_EXT << "|dir>\n";
    out << "  starbytes-ling --suggest <source." << STARBYTES_SRCFILE_EXT << "|dir>\n";
    out << "  starbytes-ling --code-actions <source." << STARBYTES_SRCFILE_EXT << "|dir>\n";
    out << "  starbytes-ling --apply-safe-fixes [--dry-run] <source." << STARBYTES_SRCFILE_EXT << "|dir>\n";
    out << "  starbytes-ling --help\n";
    out << "  starbytes-ling --version\n";
}

void printHelp(std::ostream &out) {
    out << "Starbytes Linguist Tool\n\n";
    printUsage(out);
    out << "\nOptions:\n";
    out << "  -h, --help              Show help.\n";
    out << "  -V, --version           Show tool version.\n";
    out << "      --pretty-write      Emit formatted source for file(s) to stdout.\n";
    out << "      --lint              Run lint engine and print findings.\n";
    out << "      --suggest           Run suggestion engine and print suggestions.\n";
    out << "      --code-actions      Build code actions from lint + suggestions.\n";
    out << "      --apply-safe-fixes  Apply safe actions in-place.\n";
    out << "      --dry-run           Preview safe-fix output without writing files.\n";
    out << "      --modpath-aware     Load additional roots from .starbmodpath.\n";
    out << "      --include <glob>    Include glob (repeatable).\n";
    out << "      --exclude <glob>    Exclude glob (repeatable).\n";
    out << "      --max-files <n>     Cap discovered file count (0 = unlimited).\n";
}

void printVersion(std::ostream &out) {
#ifdef STARBYTES_VERSION
    out << "starbytes-ling " << STARBYTES_STRINGIFY(STARBYTES_VERSION) << '\n';
#else
    out << "starbytes-ling (version unknown)" << '\n';
#endif
}

ParseResult parseArgs(int argc, const char *argv[], LingOptions &opts) {
    starbytes::cl::Parser parser;
    parser.addCommand("help");

    parser.addFlagOption("help", {"h"});
    parser.addFlagOption("version", {"V"});
    parser.addFlagOption("pretty-write");
    parser.addFlagOption("lint");
    parser.addFlagOption("suggest");
    parser.addFlagOption("code-actions");
    parser.addFlagOption("apply-safe-fixes");
    parser.addFlagOption("dry-run");
    parser.addFlagOption("modpath-aware");
    parser.addMultiValueOption("include");
    parser.addMultiValueOption("exclude");
    parser.addValueOption("max-files");

    auto parsed = parser.parse(argc, argv);
    if(!parsed.ok) {
        return {false, parsed.exitCode, parsed.error};
    }

    opts.showHelp = parsed.command == "help" || parsed.hasFlag("help");
    opts.showVersion = parsed.hasFlag("version");
    opts.prettyWrite = parsed.hasFlag("pretty-write");
    opts.lint = parsed.hasFlag("lint");
    opts.suggest = parsed.hasFlag("suggest");
    opts.codeActions = parsed.hasFlag("code-actions");
    opts.applySafeFixes = parsed.hasFlag("apply-safe-fixes");
    opts.dryRun = parsed.hasFlag("dry-run");
    opts.modpathAware = parsed.hasFlag("modpath-aware");
    opts.includeGlobs = parsed.values("include");
    opts.excludeGlobs = parsed.values("exclude");

    if(opts.showHelp || opts.showVersion) {
        return {true, 0, ""};
    }

    if(opts.dryRun && !opts.applySafeFixes) {
        return {false, 1, "--dry-run requires --apply-safe-fixes."};
    }

    if(!opts.prettyWrite && !opts.lint && !opts.suggest && !opts.codeActions && !opts.applySafeFixes) {
        return {false, 1, "No operation selected. Use --pretty-write, --lint, --suggest, --code-actions, or --apply-safe-fixes."};
    }

    if(auto maxFiles = parsed.firstValue("max-files"); maxFiles.has_value()) {
        try {
            opts.maxFiles = static_cast<size_t>(std::stoull(*maxFiles));
        }
        catch(...) {
            return {false, 1, "Invalid value for --max-files."};
        }
    }

    if(parsed.positionals.empty()) {
        return {false, 1, "Missing input path."};
    }

    if(parsed.positionals.size() > 1) {
        return {false, 1, "Too many positional arguments. Expected exactly one input path."};
    }

    opts.inputPath = parsed.positionals.front();
    return {true, 0, ""};
}

std::string trimCopy(std::string value) {
    size_t start = 0;
    while(start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0) {
        ++start;
    }
    size_t end = value.size();
    while(end > start && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
        --end;
    }
    return value.substr(start, end - start);
}

bool lineIsCommentOrEmpty(const std::string &line) {
    auto trimmed = trimCopy(line);
    if(trimmed.empty()) {
        return true;
    }
    if(trimmed[0] == '#') {
        return true;
    }
    return trimmed.size() >= 2 && trimmed[0] == '/' && trimmed[1] == '/';
}

std::string canonicalPathString(const std::filesystem::path &path) {
    std::error_code ec;
    auto absolute = std::filesystem::absolute(path, ec);
    if(ec) {
        return path.lexically_normal().generic_string();
    }
    auto canonical = absolute.lexically_normal();
    return canonical.generic_string();
}

void appendRootDir(const std::filesystem::path &path,
                   std::vector<std::filesystem::path> &roots,
                   std::unordered_set<std::string> &seenRoots) {
    std::error_code ec;
    if(!std::filesystem::exists(path, ec) || ec || !std::filesystem::is_directory(path, ec) || ec) {
        return;
    }
    auto normalized = canonicalPathString(path);
    if(seenRoots.insert(normalized).second) {
        roots.emplace_back(normalized);
    }
}

void loadModulePathFile(const std::filesystem::path &filePath,
                        std::vector<std::filesystem::path> &roots,
                        std::unordered_set<std::string> &seenRoots,
                        std::vector<std::string> &warnings) {
    std::ifstream in(filePath, std::ios::in);
    if(!in.is_open()) {
        return;
    }

    auto baseDir = filePath.parent_path();
    std::string rawLine;
    unsigned lineNo = 0;
    while(std::getline(in, rawLine)) {
        ++lineNo;
        if(lineIsCommentOrEmpty(rawLine)) {
            continue;
        }
        auto entry = trimCopy(rawLine);
        if(entry.empty()) {
            continue;
        }

        std::filesystem::path entryPath(entry);
        if(entryPath.is_relative()) {
            entryPath = baseDir / entryPath;
        }

        std::error_code ec;
        if(!std::filesystem::exists(entryPath, ec) || ec || !std::filesystem::is_directory(entryPath, ec) || ec) {
            warnings.push_back("Ignoring invalid .starbmodpath entry `" + entry + "` at " +
                               filePath.string() + ":" + std::to_string(lineNo));
            continue;
        }
        appendRootDir(entryPath, roots, seenRoots);
    }
}

std::vector<std::filesystem::path> buildSearchRoots(const std::filesystem::path &inputPath,
                                                    bool modpathAware,
                                                    std::vector<std::string> &warnings) {
    std::vector<std::filesystem::path> roots;
    std::unordered_set<std::string> seenRoots;

    std::error_code ec;
    auto baseRoot = std::filesystem::is_directory(inputPath, ec) && !ec ? inputPath : inputPath.parent_path();
    if(baseRoot.empty()) {
        baseRoot = std::filesystem::current_path();
    }
    appendRootDir(baseRoot, roots, seenRoots);

    if(!modpathAware) {
        return roots;
    }

    loadModulePathFile(baseRoot / ".starbmodpath", roots, seenRoots, warnings);
    auto parent = baseRoot.parent_path();
    if(!parent.empty()) {
        loadModulePathFile(parent / ".starbmodpath", roots, seenRoots, warnings);
    }
    return roots;
}

bool isSourceFile(const std::filesystem::path &path) {
    auto ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return ext == ".starb" || ext == ".starbint";
}

std::string globToRegexPattern(const std::string &glob) {
    std::string out = "^";
    out.reserve(glob.size() * 2 + 4);
    for(char ch : glob) {
        switch(ch) {
            case '*':
                out += ".*";
                break;
            case '?':
                out.push_back('.');
                break;
            case '.':
            case '+':
            case '(':
            case ')':
            case '[':
            case ']':
            case '{':
            case '}':
            case '^':
            case '$':
            case '|':
            case '\\':
                out.push_back('\\');
                out.push_back(ch);
                break;
            default:
                out.push_back(ch);
                break;
        }
    }
    out.push_back('$');
    return out;
}

std::vector<GlobPattern> compileGlobPatterns(const std::vector<std::string> &globs, std::string &errorOut) {
    std::vector<GlobPattern> out;
    out.reserve(globs.size());
    for(const auto &raw : globs) {
        auto trimmed = trimCopy(raw);
        if(trimmed.empty()) {
            continue;
        }
        try {
            GlobPattern pattern;
            pattern.raw = trimmed;
            pattern.pathScoped = trimmed.find('/') != std::string::npos || trimmed.find('\\') != std::string::npos;
            pattern.regex = std::regex(globToRegexPattern(trimmed), std::regex::ECMAScript);
            out.push_back(std::move(pattern));
        }
        catch(const std::regex_error &) {
            errorOut = "Invalid glob pattern: " + trimmed;
            return {};
        }
    }
    return out;
}

bool matchesAnyGlob(const std::vector<GlobPattern> &patterns,
                    const std::string &relativePath,
                    const std::string &filename) {
    for(const auto &pattern : patterns) {
        auto &value = pattern.pathScoped ? relativePath : filename;
        if(std::regex_match(value, pattern.regex)) {
            return true;
        }
    }
    return false;
}

bool shouldIncludeFile(const std::string &relativePath,
                       const std::string &filename,
                       const std::vector<GlobPattern> &includes,
                       const std::vector<GlobPattern> &excludes) {
    if(!includes.empty() && !matchesAnyGlob(includes, relativePath, filename)) {
        return false;
    }
    if(!excludes.empty() && matchesAnyGlob(excludes, relativePath, filename)) {
        return false;
    }
    return true;
}

FileDiscoveryResult discoverSourceFiles(const std::filesystem::path &inputPath, const LingOptions &opts) {
    FileDiscoveryResult result;

    std::error_code ec;
    if(!std::filesystem::exists(inputPath, ec) || ec) {
        result.error = "Input path not found: " + inputPath.string();
        return result;
    }

    std::string globError;
    auto includePatterns = compileGlobPatterns(opts.includeGlobs, globError);
    if(!globError.empty()) {
        result.error = globError;
        return result;
    }
    auto excludePatterns = compileGlobPatterns(opts.excludeGlobs, globError);
    if(!globError.empty()) {
        result.error = globError;
        return result;
    }

    if(std::filesystem::is_regular_file(inputPath, ec) && !ec) {
        result.ok = true;
        result.files.push_back(std::filesystem::path(canonicalPathString(inputPath)));
        return result;
    }

    if(!std::filesystem::is_directory(inputPath, ec) || ec) {
        result.error = "Input path is neither a file nor a directory: " + inputPath.string();
        return result;
    }

    auto roots = buildSearchRoots(inputPath, opts.modpathAware, result.warnings);
    std::unordered_set<std::string> seenFiles;
    bool hitLimit = false;

    for(const auto &root : roots) {
        std::error_code walkEc;
        for(std::filesystem::recursive_directory_iterator it(root, std::filesystem::directory_options::skip_permission_denied, walkEc), end;
            it != end; it.increment(walkEc)) {
            if(walkEc) {
                break;
            }
            if(!it->is_regular_file()) {
                continue;
            }
            if(!isSourceFile(it->path())) {
                continue;
            }

            auto absolute = std::filesystem::path(canonicalPathString(it->path()));
            auto normalized = absolute.generic_string();
            std::string relativePath = absolute.filename().generic_string();
            std::error_code relEc;
            auto rel = std::filesystem::relative(absolute, root, relEc);
            if(!relEc) {
                relativePath = rel.generic_string();
            }
            auto fileName = absolute.filename().generic_string();

            if(!shouldIncludeFile(relativePath, fileName, includePatterns, excludePatterns)) {
                continue;
            }
            if(!seenFiles.insert(normalized).second) {
                continue;
            }
            result.files.push_back(absolute);
            if(opts.maxFiles > 0 && result.files.size() >= opts.maxFiles) {
                hitLimit = true;
                break;
            }
        }
        if(hitLimit) {
            break;
        }
    }

    std::sort(result.files.begin(), result.files.end(), [](const auto &lhs, const auto &rhs) {
        return lhs.generic_string() < rhs.generic_string();
    });
    result.ok = true;
    return result;
}

bool loadFileAnalysis(FileAnalysisCache &cache, std::string &errorOut) {
    if(cache.loaded) {
        return true;
    }

    std::ifstream in(cache.filePath, std::ios::in);
    if(!in.is_open()) {
        errorOut = "Failed to open source file: " + cache.filePath.string();
        return false;
    }

    std::ostringstream buffer;
    buffer << in.rdbuf();
    cache.sourceText = buffer.str();
    cache.session.setSource(cache.filePath.string(), cache.sourceText);
    cache.loaded = true;
    return true;
}

const starbytes::linguistics::LintResult &ensureLintResult(FileAnalysisCache &cache,
                                                           const starbytes::linguistics::LinguisticsConfig &config,
                                                           starbytes::linguistics::LintEngine &engine) {
    if(!cache.lintReady) {
        cache.lintResult = engine.run(cache.session, config);
        cache.lintReady = true;
    }
    return cache.lintResult;
}

const starbytes::linguistics::SuggestionResult &ensureSuggestionResult(
    FileAnalysisCache &cache,
    const starbytes::linguistics::LinguisticsConfig &config,
    starbytes::linguistics::SuggestionEngine &engine) {
    if(!cache.suggestionsReady) {
        starbytes::linguistics::SuggestionRequest request;
        request.includeLowConfidence = false;
        cache.suggestionResult = engine.run(cache.session, config, request);
        cache.suggestionsReady = true;
    }
    return cache.suggestionResult;
}

const starbytes::linguistics::CodeActionResult &ensureCodeActionResult(
    FileAnalysisCache &cache,
    bool safeOnly,
    const starbytes::linguistics::LinguisticsConfig &config,
    starbytes::linguistics::LintEngine &lintEngine,
    starbytes::linguistics::SuggestionEngine &suggestionEngine,
    starbytes::linguistics::CodeActionEngine &actionEngine) {
    auto &targetReady = safeOnly ? cache.safeCodeActionsReady : cache.codeActionsReady;
    auto &targetResult = safeOnly ? cache.safeCodeActionResult : cache.codeActionResult;
    if(!targetReady) {
        const auto &lintResult = ensureLintResult(cache, config, lintEngine);
        const auto &suggestionResult = ensureSuggestionResult(cache, config, suggestionEngine);
        starbytes::linguistics::CodeActionRequest request;
        request.safeOnly = safeOnly;
        targetResult = actionEngine.build(lintResult.findings, suggestionResult.suggestions, config, request);
        targetReady = true;
    }
    return targetResult;
}

const starbytes::linguistics::FormatResult &ensureFormatResult(
    FileAnalysisCache &cache,
    const starbytes::linguistics::LinguisticsConfig &config,
    starbytes::linguistics::FormatterEngine &engine) {
    if(!cache.formatReady) {
        cache.formatResult = engine.format(cache.session, config);
        cache.formatReady = true;
    }
    return cache.formatResult;
}

std::vector<size_t> lineOffsets(const std::string &text) {
    std::vector<size_t> offsets;
    offsets.push_back(0);
    for(size_t i = 0; i < text.size(); ++i) {
        if(text[i] == '\n') {
            offsets.push_back(i + 1);
        }
    }
    return offsets;
}

bool offsetFromPosition(const std::string &text,
                        const std::vector<size_t> &offsets,
                        const starbytes::linguistics::TextPosition &position,
                        size_t &offsetOut) {
    if(position.line >= offsets.size()) {
        return false;
    }

    size_t lineStart = offsets[position.line];
    size_t lineEnd = text.size();
    if(position.line + 1 < offsets.size()) {
        lineEnd = offsets[position.line + 1];
        if(lineEnd > lineStart && text[lineEnd - 1] == '\n') {
            --lineEnd;
        }
    }

    if(position.character > (lineEnd - lineStart)) {
        return false;
    }

    offsetOut = lineStart + position.character;
    return true;
}

struct OffsetEdit {
    size_t start = 0;
    size_t end = 0;
    std::string replacement;
};

bool toOffsetEdits(const std::string &text,
                   const std::vector<starbytes::linguistics::TextEdit> &edits,
                   std::vector<OffsetEdit> &out) {
    auto offsets = lineOffsets(text);
    out.clear();
    out.reserve(edits.size());
    for(const auto &edit : edits) {
        if(!edit.span.isValid()) {
            return false;
        }

        size_t startOffset = 0;
        size_t endOffset = 0;
        if(!offsetFromPosition(text, offsets, edit.span.start, startOffset)) {
            return false;
        }
        if(!offsetFromPosition(text, offsets, edit.span.end, endOffset)) {
            return false;
        }
        if(startOffset > endOffset || endOffset > text.size()) {
            return false;
        }
        out.push_back(OffsetEdit{startOffset, endOffset, edit.replacement});
    }
    return true;
}

size_t applyEditsDeterministically(std::string &text, const std::vector<starbytes::linguistics::TextEdit> &edits) {
    std::vector<OffsetEdit> offsetEdits;
    if(!toOffsetEdits(text, edits, offsetEdits)) {
        return 0;
    }

    std::sort(offsetEdits.begin(), offsetEdits.end(), [](const OffsetEdit &lhs, const OffsetEdit &rhs) {
        if(lhs.start != rhs.start) {
            return lhs.start < rhs.start;
        }
        if(lhs.end != rhs.end) {
            return lhs.end < rhs.end;
        }
        return lhs.replacement < rhs.replacement;
    });

    std::vector<OffsetEdit> filtered;
    filtered.reserve(offsetEdits.size());
    size_t lastEnd = 0;
    bool hasLast = false;
    for(const auto &edit : offsetEdits) {
        if(hasLast && edit.start < lastEnd) {
            continue;
        }
        filtered.push_back(edit);
        lastEnd = edit.end;
        hasLast = true;
    }

    for(auto it = filtered.rbegin(); it != filtered.rend(); ++it) {
        text.replace(it->start, it->end - it->start, it->replacement);
    }
    return filtered.size();
}

void printSuggestions(const starbytes::linguistics::SuggestionResult &result, std::ostream &out) {
    out << "suggestions: " << result.suggestions.size() << "\n";
    for(const auto &suggestion : result.suggestions) {
        out << "[" << suggestion.id << "] "
            << "L" << (suggestion.span.start.line + 1) << ":" << suggestion.span.start.character
            << " confidence=" << suggestion.confidence
            << " " << suggestion.title << "\n";
    }
}

void printActions(const starbytes::linguistics::CodeActionResult &result, std::ostream &out) {
    out << "code-actions: " << result.actions.size() << "\n";
    for(const auto &action : result.actions) {
        out << "[" << action.id << "] "
            << action.kind
            << (action.isSafe ? " safe" : " unsafe")
            << (action.preferred ? " preferred " : " ")
            << action.title << "\n";
    }
}

bool hasOperationOutput(const LingOptions &opts) {
    return opts.prettyWrite || opts.lint || opts.suggest || opts.codeActions || opts.applySafeFixes;
}

} // namespace

int main(int argc, const char *argv[]) {
    LingOptions opts;
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
        return 0;
    }
    if(opts.showHelp) {
        printHelp(std::cout);
        return 0;
    }
    if(!hasOperationOutput(opts)) {
        return 0;
    }

    std::filesystem::path inputPath(opts.inputPath);
    auto discovery = discoverSourceFiles(inputPath, opts);
    if(!discovery.ok) {
        std::cerr << discovery.error << std::endl;
        return 1;
    }
    for(const auto &warning : discovery.warnings) {
        std::cerr << warning << std::endl;
    }

    if(discovery.files.empty()) {
        std::cerr << "No source files matched." << std::endl;
        return 1;
    }

    auto config = starbytes::linguistics::LinguisticsConfig::defaults();
    starbytes::linguistics::LintEngine lintEngine;
    starbytes::linguistics::SuggestionEngine suggestionEngine;
    starbytes::linguistics::CodeActionEngine codeActionEngine;
    starbytes::linguistics::FormatterEngine formatterEngine;

    std::vector<FileAnalysisCache> fileCaches;
    fileCaches.reserve(discovery.files.size());
    for(const auto &file : discovery.files) {
        FileAnalysisCache cache;
        cache.filePath = file;
        fileCaches.push_back(std::move(cache));
    }

    std::error_code inputKindEc;
    bool inputWasDirectory = std::filesystem::is_directory(inputPath, inputKindEc) && !inputKindEc;
    bool multiFile = fileCaches.size() > 1 || inputWasDirectory;
    bool hadError = false;

    for(auto &file : fileCaches) {
        std::string loadError;
        if(!loadFileAnalysis(file, loadError)) {
            std::cerr << loadError << std::endl;
            hadError = true;
            continue;
        }

        auto printHeader = [&](const char *operationLabel) {
            if(!multiFile) {
                return;
            }
            std::cout << "== [" << operationLabel << "] " << file.filePath.generic_string() << " ==\n";
        };

        if(opts.lint) {
            printHeader("lint");
            std::cout << starbytes::linguistics::LinguisticsSerializer::toText(
                             ensureLintResult(file, config, lintEngine))
                      << '\n';
        }

        if(opts.suggest) {
            printHeader("suggest");
            printSuggestions(ensureSuggestionResult(file, config, suggestionEngine), std::cout);
        }

        if(opts.codeActions) {
            printHeader("code-actions");
            printActions(ensureCodeActionResult(file, false, config, lintEngine, suggestionEngine, codeActionEngine),
                         std::cout);
        }

        if(opts.prettyWrite) {
            printHeader("pretty-write");
            const auto &formatResult = ensureFormatResult(file, config, formatterEngine);
            if(!formatResult.ok) {
                std::cerr << "Failed to format source in --pretty-write mode: "
                          << file.filePath.generic_string() << std::endl;
                hadError = true;
            }
            else {
                std::cout << formatResult.formattedText;
                if(multiFile && !formatResult.formattedText.empty() && formatResult.formattedText.back() != '\n') {
                    std::cout << '\n';
                }
                for(const auto &note : formatResult.notes) {
                    std::cerr << file.filePath.generic_string() << ": " << note << std::endl;
                }
            }
        }

        if(opts.applySafeFixes) {
            const auto &safeActions =
                ensureCodeActionResult(file, true, config, lintEngine, suggestionEngine, codeActionEngine);

            std::vector<starbytes::linguistics::TextEdit> edits;
            edits.reserve(safeActions.actions.size());
            for(const auto &action : safeActions.actions) {
                edits.insert(edits.end(), action.edits.begin(), action.edits.end());
            }

            auto previewText = file.sourceText;
            auto appliedCount = applyEditsDeterministically(previewText, edits);

            if(opts.dryRun) {
                printHeader("apply-safe-fixes dry-run");
                std::cout << "dry-run safe-fix edits: " << appliedCount << "\n";
                std::cout << previewText;
                if(!previewText.empty() && previewText.back() != '\n') {
                    std::cout << '\n';
                }
            }
            else {
                std::ofstream outFile(file.filePath, std::ios::out | std::ios::trunc);
                if(!outFile.is_open()) {
                    std::cerr << "Failed to open file for --apply-safe-fixes: "
                              << file.filePath.generic_string() << std::endl;
                    hadError = true;
                }
                else {
                    outFile << previewText;
                    std::cerr << "Applied safe fixes: " << appliedCount
                              << " (" << file.filePath.generic_string() << ")" << std::endl;
                }
            }
        }
    }

    return hadError ? 1 : 0;
}
