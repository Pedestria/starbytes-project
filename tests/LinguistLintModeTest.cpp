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
    std::cerr << "LinguistLintModeTest failure: " << message << '\n';
    return 1;
}

bool expect(bool condition, const std::string &message) {
    if(!condition) {
        std::cerr << "LinguistLintModeTest assertion failed: " << message << '\n';
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
        auto candidate = tempRoot / ("starbytes-ling-lint-mode-" + std::to_string(seed + attempt));
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
        std::cerr << "LinguistLintModeTest skipped: starbytes-ling binary not found.\n";
        return 0;
    }

    auto root = createTempWorkspaceRoot();
    if(!expect(!root.empty(), "failed to create temporary workspace")) {
        return fail("mkdtemp");
    }

    auto validFile = root / "valid_lint.starb";
    auto invalidFile = root / "invalid_syntax.starb";
    if(!writeFile(validFile,
                  "func undocumented(value:Int) Int {   \n"
                  "    return value\n"
                  "}\n")) {
        return fail("write-valid-file");
    }
    if(!writeFile(invalidFile, "decl broken = 1.0e\n")) {
        return fail("write-invalid-file");
    }

    auto defaultRun = runAndCapture(shellQuote(toolPath.string()) + " --lint " + shellQuote(validFile.string()));
    if(!expect(defaultRun.exitCode == 0, "default lint run should succeed")) {
        return fail("default-run-exit");
    }
    if(!expect(defaultRun.output.find("syntax-diagnostics: 0") != std::string::npos,
               "default lint should print syntax diagnostics section")) {
        return fail("default-syntax-section");
    }
    if(!expect(defaultRun.output.find("lint-findings:") != std::string::npos,
               "default lint should print semantic lint section")) {
        return fail("default-lint-section");
    }

    auto semanticOnlyRun = runAndCapture(shellQuote(toolPath.string()) + " --lint --semantic-only " +
                                         shellQuote(validFile.string()));
    if(!expect(semanticOnlyRun.exitCode == 0, "semantic-only lint run should succeed")) {
        return fail("semantic-only-exit");
    }
    if(!expect(semanticOnlyRun.output.find("syntax-diagnostics:") == std::string::npos,
               "semantic-only lint should suppress syntax diagnostics")) {
        return fail("semantic-only-syntax");
    }
    if(!expect(semanticOnlyRun.output.find("lint-findings:") != std::string::npos,
               "semantic-only lint should still print lint findings")) {
        return fail("semantic-only-lint");
    }

    auto syntaxOnlyRun = runAndCapture(shellQuote(toolPath.string()) + " --lint --syntax-only " +
                                       shellQuote(invalidFile.string()));
    if(!expect(syntaxOnlyRun.exitCode != 0, "syntax-only lint run should fail on syntax error")) {
        return fail("syntax-only-exit");
    }
    if(!expect(syntaxOnlyRun.output.find("syntax-diagnostics:") != std::string::npos,
               "syntax-only lint should print syntax diagnostics")) {
        return fail("syntax-only-section");
    }
    if(!expect(syntaxOnlyRun.output.find("lint-findings:") == std::string::npos,
               "syntax-only lint should suppress semantic lint output")) {
        return fail("syntax-only-lint");
    }

    auto invalidUsageRun = runAndCapture(shellQuote(toolPath.string()) + " --syntax-only " +
                                         shellQuote(validFile.string()));
    if(!expect(invalidUsageRun.exitCode != 0, "syntax-only without lint should fail")) {
        return fail("invalid-usage-exit");
    }
    if(!expect(invalidUsageRun.output.find("--syntax-only/--semantic-only require --lint.") != std::string::npos,
               "invalid usage should explain lint requirement")) {
        return fail("invalid-usage-message");
    }

    std::error_code cleanupEc;
    std::filesystem::remove_all(root, cleanupEc);
    return 0;
}
