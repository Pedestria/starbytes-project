#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <sys/wait.h>
#include <ctime>

namespace {

int fail(const std::string &message) {
    std::cerr << "LinguistWorkspacePhase6Test failure: " << message << '\n';
    return 1;
}

bool expect(bool condition, const std::string &message) {
    if(!condition) {
        std::cerr << "LinguistWorkspacePhase6Test assertion failed: " << message << '\n';
        return false;
    }
    return true;
}

std::string shellQuote(const std::string &value) {
    std::string out = "'";
    for(char ch : value) {
        if(ch == '\'') {
            out += "'\"'\"'";
        }
        else {
            out.push_back(ch);
        }
    }
    out += "'";
    return out;
}

struct CommandResult {
    int exitCode = 1;
    std::string output;
};

CommandResult runAndCapture(const std::string &command) {
    CommandResult result;
    auto fullCommand = command + " 2>&1";
    FILE *pipe = popen(fullCommand.c_str(), "r");
    if(!pipe) {
        return result;
    }

    std::array<char, 512> buffer{};
    while(fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        result.output += buffer.data();
    }
    int rawCode = pclose(pipe);
    if(rawCode >= 0) {
        result.exitCode = WEXITSTATUS(rawCode);
    }
    return result;
}

std::filesystem::path resolveLinguistBinaryPath(const char *argv0) {
    std::filesystem::path exe = argv0 ? argv0 : "";
    std::error_code ec;
    if(exe.is_relative()) {
        exe = std::filesystem::absolute(exe, ec);
    }
    if(ec || exe.empty()) {
        exe = std::filesystem::current_path();
    }

    auto candidate = exe.parent_path().parent_path() / "bin" / "starbytes-ling";
    if(std::filesystem::exists(candidate, ec) && !ec) {
        return candidate;
    }
    candidate = std::filesystem::current_path() / "bin" / "starbytes-ling";
    return candidate;
}

bool writeFile(const std::filesystem::path &path, const std::string &text) {
    std::ofstream out(path, std::ios::out | std::ios::trunc);
    if(!out.is_open()) {
        return false;
    }
    out << text;
    return true;
}

std::filesystem::path createTempWorkspaceRoot() {
    auto tempRoot = std::filesystem::temp_directory_path();
    auto seed = static_cast<unsigned>(std::time(nullptr));
    for(unsigned attempt = 0; attempt < 512; ++attempt) {
        auto candidate = tempRoot / ("starbytes-ling-phase6-" + std::to_string(seed + attempt));
        std::error_code ec;
        if(std::filesystem::create_directory(candidate, ec) && !ec) {
            return candidate;
        }
    }
    return {};
}

} // namespace

int main(int argc, char *argv[]) {
    (void)argc;

    auto toolPath = resolveLinguistBinaryPath(argv[0]);
    if(!std::filesystem::exists(toolPath)) {
        std::cerr << "LinguistWorkspacePhase6Test skipped: starbytes-ling binary not found.\n";
        return 0;
    }

    auto root = createTempWorkspaceRoot();
    if(!expect(!root.empty(), "failed to create temporary workspace")) {
        return fail("mkdtemp");
    }
    std::filesystem::create_directories(root / "sub");
    std::filesystem::create_directories(root / "vendor" / "modlib");

    if(!writeFile(root / ".starbmodpath", "./vendor/modlib\n")) {
        return fail("write-modpath");
    }
    if(!writeFile(root / "main.starb", "decl main = 1\n")) {
        return fail("write-main");
    }
    if(!writeFile(root / "sub" / "included.starb", "decl included = 2\n")) {
        return fail("write-included");
    }
    if(!writeFile(root / "sub" / "excluded.starb", "decl excluded = 3\n")) {
        return fail("write-excluded");
    }
    if(!writeFile(root / "vendor" / "modlib" / "ext.starb", "decl ext = 4\n")) {
        return fail("write-ext");
    }

    auto lintCommand = shellQuote(toolPath.string()) + " --lint --modpath-aware " + shellQuote(root.string());
    auto lintRun = runAndCapture(lintCommand);
    if(!expect(lintRun.exitCode == 0, "lint workspace command should succeed")) {
        return fail("lint-command");
    }
    if(!expect(lintRun.output.find("ext.starb") != std::string::npos,
               "modpath-aware traversal should include files from .starbmodpath roots")) {
        return fail("modpath-aware-traversal");
    }

    auto filteredCommand =
        shellQuote(toolPath.string()) + " --lint --include " + shellQuote("sub/*") +
        " --exclude " + shellQuote("*excluded.starb") + " " + shellQuote(root.string());
    auto filteredRun = runAndCapture(filteredCommand);
    if(!expect(filteredRun.exitCode == 0, "filtered workspace command should succeed")) {
        return fail("filtered-command");
    }
    if(!expect(filteredRun.output.find("excluded.starb") == std::string::npos,
               "exclude glob should suppress excluded file")) {
        return fail("exclude-filter");
    }
    if(!expect(filteredRun.output.find("main.starb") == std::string::npos,
               "include glob should remove non-matching root files")) {
        return fail("include-scope");
    }

    std::error_code cleanupEc;
    std::filesystem::remove_all(root, cleanupEc);
    return 0;
}
