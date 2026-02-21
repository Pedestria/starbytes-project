#include <starbytes/interop.h>

#include <algorithm>
#include <chrono>
#include <climits>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

namespace {

struct NativeArgsLayout {
    unsigned argc = 0;
    unsigned index = 0;
    StarbytesObject *argv = nullptr;
};

StarbytesObject makeBool(bool value) {
    // Runtime bool consumption currently interprets StarbytesBoolFalse as logical true.
    return StarbytesBoolNew(value ? StarbytesBoolFalse : StarbytesBoolTrue);
}

StarbytesObject makeInt(int value) {
    return StarbytesNumNew(NumTypeInt,value);
}

bool readStringArg(StarbytesFuncArgs args,std::string &outValue) {
    auto arg = StarbytesFuncArgsGetArg(args);
    if(!arg || !StarbytesObjectTypecheck(arg,StarbytesStrType())) {
        return false;
    }
    outValue = StarbytesStrGetBuffer(arg);
    return true;
}

bool readBoolArg(StarbytesFuncArgs args,bool &outValue) {
    auto arg = StarbytesFuncArgsGetArg(args);
    if(!arg || !StarbytesObjectTypecheck(arg,StarbytesBoolType())) {
        return false;
    }
    outValue = (StarbytesBoolValue(arg) == StarbytesBoolFalse);
    return true;
}

bool readStringArrayArg(StarbytesFuncArgs args,std::vector<std::string> &outValues) {
    auto arg = StarbytesFuncArgsGetArg(args);
    if(!arg || !StarbytesObjectTypecheck(arg,StarbytesArrayType())) {
        return false;
    }

    auto len = StarbytesArrayGetLength(arg);
    outValues.clear();
    outValues.reserve(len);
    for(unsigned i = 0; i < len; ++i) {
        auto item = StarbytesArrayIndex(arg,i);
        if(!item || !StarbytesObjectTypecheck(item,StarbytesStrType())) {
            return false;
        }
        outValues.emplace_back(StarbytesStrGetBuffer(item));
    }
    return true;
}

void skipOptionalModuleReceiver(StarbytesFuncArgs args,unsigned expectedUserArgs) {
    auto *raw = reinterpret_cast<NativeArgsLayout *>(args);
    if(!raw || raw->argc < raw->index) {
        return;
    }
    auto remaining = raw->argc - raw->index;
    if(remaining == expectedUserArgs + 1) {
        (void)StarbytesFuncArgsGetArg(args);
    }
}

std::string pathToString(const std::filesystem::path &path) {
    return path.generic_string();
}

StarbytesObject makePathString(const std::filesystem::path &path) {
    auto text = pathToString(path);
    return StarbytesStrNewWithData(text.c_str());
}

bool isIntRange(long long value) {
    return value >= INT_MIN && value <= INT_MAX;
}

STARBYTES_FUNC(fs_currentDirectory) {
    skipOptionalModuleReceiver(args,0);
    std::error_code ec;
    auto cwd = std::filesystem::current_path(ec);
    if(ec) {
        return nullptr;
    }
    return makePathString(cwd);
}

STARBYTES_FUNC(fs_changeDirectory) {
    skipOptionalModuleReceiver(args,1);
    std::string path;
    if(!readStringArg(args,path)) {
        return nullptr;
    }
    std::error_code ec;
    std::filesystem::current_path(std::filesystem::path(path),ec);
    if(ec) {
        return nullptr;
    }
    return makeBool(true);
}

STARBYTES_FUNC(fs_pathAbsolute) {
    skipOptionalModuleReceiver(args,1);
    std::string path;
    if(!readStringArg(args,path)) {
        return nullptr;
    }
    std::error_code ec;
    auto absolutePath = std::filesystem::absolute(std::filesystem::path(path),ec);
    if(ec) {
        return nullptr;
    }
    return makePathString(absolutePath);
}

STARBYTES_FUNC(fs_pathNormalize) {
    skipOptionalModuleReceiver(args,1);
    std::string path;
    if(!readStringArg(args,path)) {
        return nullptr;
    }
    auto normalized = std::filesystem::path(path).lexically_normal();
    return makePathString(normalized);
}

STARBYTES_FUNC(fs_pathCanonical) {
    skipOptionalModuleReceiver(args,1);
    std::string path;
    if(!readStringArg(args,path)) {
        return nullptr;
    }
    std::error_code ec;
    auto canonical = std::filesystem::weakly_canonical(std::filesystem::path(path),ec);
    if(ec) {
        return nullptr;
    }
    return makePathString(canonical);
}

STARBYTES_FUNC(fs_pathJoin) {
    skipOptionalModuleReceiver(args,1);
    std::vector<std::string> parts;
    if(!readStringArrayArg(args,parts) || parts.empty()) {
        return nullptr;
    }
    std::filesystem::path joined(parts[0]);
    for(size_t i = 1; i < parts.size(); ++i) {
        joined /= std::filesystem::path(parts[i]);
    }
    return makePathString(joined);
}

STARBYTES_FUNC(fs_pathParent) {
    skipOptionalModuleReceiver(args,1);
    std::string path;
    if(!readStringArg(args,path)) {
        return StarbytesStrNewWithData("");
    }
    return makePathString(std::filesystem::path(path).parent_path());
}

STARBYTES_FUNC(fs_pathFilename) {
    skipOptionalModuleReceiver(args,1);
    std::string path;
    if(!readStringArg(args,path)) {
        return StarbytesStrNewWithData("");
    }
    return makePathString(std::filesystem::path(path).filename());
}

STARBYTES_FUNC(fs_pathStem) {
    skipOptionalModuleReceiver(args,1);
    std::string path;
    if(!readStringArg(args,path)) {
        return StarbytesStrNewWithData("");
    }
    return makePathString(std::filesystem::path(path).stem());
}

STARBYTES_FUNC(fs_pathExtension) {
    skipOptionalModuleReceiver(args,1);
    std::string path;
    if(!readStringArg(args,path)) {
        return StarbytesStrNewWithData("");
    }
    return makePathString(std::filesystem::path(path).extension());
}

STARBYTES_FUNC(fs_pathIsAbsolute) {
    skipOptionalModuleReceiver(args,1);
    std::string path;
    if(!readStringArg(args,path)) {
        return makeBool(false);
    }
    return makeBool(std::filesystem::path(path).is_absolute());
}

STARBYTES_FUNC(fs_isFile) {
    skipOptionalModuleReceiver(args,1);
    std::string path;
    if(!readStringArg(args,path)) {
        return makeBool(false);
    }
    std::error_code ec;
    auto result = std::filesystem::is_regular_file(std::filesystem::path(path),ec);
    if(ec) {
        return makeBool(false);
    }
    return makeBool(result);
}

STARBYTES_FUNC(fs_isDirectory) {
    skipOptionalModuleReceiver(args,1);
    std::string path;
    if(!readStringArg(args,path)) {
        return makeBool(false);
    }
    std::error_code ec;
    auto result = std::filesystem::is_directory(std::filesystem::path(path),ec);
    if(ec) {
        return makeBool(false);
    }
    return makeBool(result);
}

STARBYTES_FUNC(fs_fileSize) {
    skipOptionalModuleReceiver(args,1);
    std::string path;
    if(!readStringArg(args,path)) {
        return nullptr;
    }
    std::error_code ec;
    auto size = std::filesystem::file_size(std::filesystem::path(path),ec);
    if(ec || size > static_cast<uintmax_t>(INT_MAX)) {
        return nullptr;
    }
    return makeInt(static_cast<int>(size));
}

STARBYTES_FUNC(fs_lastWriteEpochSeconds) {
    skipOptionalModuleReceiver(args,1);
    std::string path;
    if(!readStringArg(args,path)) {
        return nullptr;
    }
    std::error_code ec;
    auto fileTime = std::filesystem::last_write_time(std::filesystem::path(path),ec);
    if(ec) {
        return nullptr;
    }

    auto systemTime = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        fileTime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(systemTime.time_since_epoch()).count();
    if(!isIntRange(seconds)) {
        return nullptr;
    }
    return makeInt(static_cast<int>(seconds));
}

STARBYTES_FUNC(fs_copyPath) {
    skipOptionalModuleReceiver(args,4);
    std::string src;
    std::string dst;
    bool recursive = false;
    bool overwrite = false;
    if(!readStringArg(args,src) || !readStringArg(args,dst) || !readBoolArg(args,recursive) || !readBoolArg(args,overwrite)) {
        return nullptr;
    }

    std::filesystem::copy_options options = std::filesystem::copy_options::none;
    if(overwrite) {
        options |= std::filesystem::copy_options::overwrite_existing;
    }
    if(recursive) {
        options |= std::filesystem::copy_options::recursive;
    }

    std::error_code ec;
    std::filesystem::copy(std::filesystem::path(src),std::filesystem::path(dst),options,ec);
    if(ec) {
        return nullptr;
    }
    return makeBool(true);
}

STARBYTES_FUNC(fs_removeTree) {
    skipOptionalModuleReceiver(args,1);
    std::string path;
    if(!readStringArg(args,path)) {
        return nullptr;
    }
    std::error_code ec;
    auto removed = std::filesystem::remove_all(std::filesystem::path(path),ec);
    if(ec) {
        return nullptr;
    }
    return makeBool(removed > 0);
}

STARBYTES_FUNC(fs_walkPaths) {
    skipOptionalModuleReceiver(args,3);
    std::string root;
    bool recursive = false;
    bool includeDirectories = false;
    if(!readStringArg(args,root) || !readBoolArg(args,recursive) || !readBoolArg(args,includeDirectories)) {
        return nullptr;
    }

    std::error_code ec;
    auto rootPath = std::filesystem::path(root);
    if(!std::filesystem::is_directory(rootPath,ec) || ec) {
        return nullptr;
    }

    std::vector<std::string> values;
    if(recursive) {
        for(std::filesystem::recursive_directory_iterator it(rootPath,ec), end; it != end && !ec; it.increment(ec)) {
            auto candidate = it->path();
            bool isDir = it->is_directory(ec);
            if(ec) {
                return nullptr;
            }
            if(includeDirectories || !isDir) {
                values.emplace_back(pathToString(candidate));
            }
        }
    }
    else {
        for(std::filesystem::directory_iterator it(rootPath,ec), end; it != end && !ec; it.increment(ec)) {
            auto candidate = it->path();
            bool isDir = it->is_directory(ec);
            if(ec) {
                return nullptr;
            }
            if(includeDirectories || !isDir) {
                values.emplace_back(pathToString(candidate));
            }
        }
    }

    if(ec) {
        return nullptr;
    }

    std::sort(values.begin(),values.end());
    auto result = StarbytesArrayNew();
    for(const auto &item : values) {
        StarbytesArrayPush(result,StarbytesStrNewWithData(item.c_str()));
    }
    return result;
}

STARBYTES_FUNC(fs_tempDirectory) {
    skipOptionalModuleReceiver(args,0);
    std::error_code ec;
    auto tempPath = std::filesystem::temp_directory_path(ec);
    if(ec) {
        return nullptr;
    }
    return makePathString(tempPath);
}

STARBYTES_FUNC(fs_homeDirectory) {
    skipOptionalModuleReceiver(args,0);

    const char *home = std::getenv("HOME");
    if(!home || !*home) {
        home = std::getenv("USERPROFILE");
    }

    std::string resolved;
    if(home && *home) {
        resolved = home;
    }
    else {
        const char *drive = std::getenv("HOMEDRIVE");
        const char *path = std::getenv("HOMEPATH");
        if(drive && *drive && path && *path) {
            resolved = std::string(drive) + std::string(path);
        }
    }

    if(resolved.empty()) {
        return nullptr;
    }
    return makePathString(std::filesystem::path(resolved));
}

void addFunc(StarbytesNativeModule *module,const char *name,unsigned argCount,StarbytesFuncCallback callback) {
    StarbytesFuncDesc desc;
    desc.name = CStringMake(name);
    desc.argCount = argCount;
    desc.callback = callback;
    StarbytesNativeModuleAddDesc(module,&desc);
}

}

STARBYTES_NATIVE_MOD_MAIN() {
    auto module = StarbytesNativeModuleCreate();

    addFunc(module,"fs_currentDirectory",0,fs_currentDirectory);
    addFunc(module,"fs_changeDirectory",1,fs_changeDirectory);
    addFunc(module,"fs_pathAbsolute",1,fs_pathAbsolute);
    addFunc(module,"fs_pathNormalize",1,fs_pathNormalize);
    addFunc(module,"fs_pathCanonical",1,fs_pathCanonical);
    addFunc(module,"fs_pathJoin",1,fs_pathJoin);
    addFunc(module,"fs_pathParent",1,fs_pathParent);
    addFunc(module,"fs_pathFilename",1,fs_pathFilename);
    addFunc(module,"fs_pathStem",1,fs_pathStem);
    addFunc(module,"fs_pathExtension",1,fs_pathExtension);
    addFunc(module,"fs_pathIsAbsolute",1,fs_pathIsAbsolute);
    addFunc(module,"fs_isFile",1,fs_isFile);
    addFunc(module,"fs_isDirectory",1,fs_isDirectory);
    addFunc(module,"fs_fileSize",1,fs_fileSize);
    addFunc(module,"fs_lastWriteEpochSeconds",1,fs_lastWriteEpochSeconds);
    addFunc(module,"fs_copyPath",4,fs_copyPath);
    addFunc(module,"fs_removeTree",1,fs_removeTree);
    addFunc(module,"fs_walkPaths",3,fs_walkPaths);
    addFunc(module,"fs_tempDirectory",0,fs_tempDirectory);
    addFunc(module,"fs_homeDirectory",0,fs_homeDirectory);

    return module;
}
