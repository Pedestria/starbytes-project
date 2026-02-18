#include <starbytes/interop.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

struct NativeStream {
    std::string path;
    std::fstream file;
    std::string mode;
    std::string encoding;
    std::string newline;
    bool binary = false;
    bool closed = false;
};

struct NativeArgsLayout {
    unsigned argc = 0;
    unsigned index = 0;
    StarbytesObject *argv = nullptr;
};

std::unordered_map<StarbytesObject,std::unique_ptr<NativeStream>> g_streamRegistry;

StarbytesObject makeBool(bool value) {
    return StarbytesBoolNew(value ? StarbytesBoolTrue : StarbytesBoolFalse);
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

bool readIntArg(StarbytesFuncArgs args,int &outValue) {
    auto arg = StarbytesFuncArgsGetArg(args);
    if(!arg || !StarbytesObjectTypecheck(arg,StarbytesNumType()) || StarbytesNumGetType(arg) != NumTypeInt) {
        return false;
    }
    outValue = StarbytesNumGetIntValue(arg);
    return true;
}

bool readBoolArg(StarbytesFuncArgs args,bool &outValue) {
    auto arg = StarbytesFuncArgsGetArg(args);
    if(!arg || !StarbytesObjectTypecheck(arg,StarbytesBoolType())) {
        return false;
    }
    outValue = (StarbytesBoolValue(arg) == StarbytesBoolTrue);
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

std::ios::openmode parseOpenMode(const std::string &modeText,bool binary,bool &ok,bool &exclusiveCreate) {
    ok = true;
    exclusiveCreate = false;

    std::ios::openmode mode = static_cast<std::ios::openmode>(0);
    bool hasPrimaryMode = false;

    for(char c : modeText) {
        switch(c) {
            case 'r':
                mode |= std::ios::in;
                hasPrimaryMode = true;
                break;
            case 'w':
                mode |= (std::ios::out | std::ios::trunc);
                hasPrimaryMode = true;
                break;
            case 'a':
                mode |= (std::ios::out | std::ios::app);
                hasPrimaryMode = true;
                break;
            case 'x':
                mode |= std::ios::out;
                hasPrimaryMode = true;
                exclusiveCreate = true;
                break;
            case '+':
                mode |= (std::ios::in | std::ios::out);
                break;
            case 'b':
                mode |= std::ios::binary;
                break;
            case 't':
                break;
            default:
                ok = false;
                return static_cast<std::ios::openmode>(0);
        }
    }

    if(!hasPrimaryMode) {
        mode |= std::ios::in;
    }
    if(binary) {
        mode |= std::ios::binary;
    }
    return mode;
}

std::unique_ptr<NativeStream> createNativeStream(const std::string &path,
                                                 const std::string &modeText,
                                                 bool binary,
                                                 const std::string &encoding,
                                                 const std::string &newline) {
    bool modeOk = false;
    bool exclusiveCreate = false;
    auto openMode = parseOpenMode(modeText,binary,modeOk,exclusiveCreate);
    if(!modeOk) {
        return nullptr;
    }

    if(exclusiveCreate && std::filesystem::exists(path)) {
        return nullptr;
    }

    auto stream = std::make_unique<NativeStream>();
    stream->path = path;
    stream->mode = modeText;
    stream->encoding = encoding;
    stream->newline = newline;
    stream->binary = binary;

    stream->file.open(path,openMode);
    if(!stream->file.is_open()) {
        return nullptr;
    }

    return stream;
}

StarbytesObject registerStreamObject(const char *className,std::unique_ptr<NativeStream> stream) {
    if(!stream) {
        return nullptr;
    }

    auto object = StarbytesObjectNew(StarbytesMakeClass(className));
    g_streamRegistry[object] = std::move(stream);
    return object;
}

NativeStream *requireStream(StarbytesFuncArgs args,StarbytesObject &selfOut) {
    selfOut = StarbytesFuncArgsGetArg(args);
    if(!selfOut) {
        return nullptr;
    }
    auto it = g_streamRegistry.find(selfOut);
    if(it == g_streamRegistry.end()) {
        return nullptr;
    }
    return it->second.get();
}

bool streamReadable(const NativeStream *stream) {
    if(!stream) {
        return false;
    }
    return stream->mode.find('r') != std::string::npos || stream->mode.find('+') != std::string::npos;
}

bool streamWritable(const NativeStream *stream) {
    if(!stream) {
        return false;
    }
    return stream->mode.find('w') != std::string::npos
        || stream->mode.find('a') != std::string::npos
        || stream->mode.find('x') != std::string::npos
        || stream->mode.find('+') != std::string::npos;
}

StarbytesObject streamClose(StarbytesFuncArgs args) {
    StarbytesObject self = nullptr;
    auto *stream = requireStream(args,self);
    if(!stream) {
        return nullptr;
    }

    if(!stream->closed && stream->file.is_open()) {
        stream->file.flush();
        stream->file.close();
    }
    stream->closed = true;
    g_streamRegistry.erase(self);
    return makeBool(true);
}

StarbytesObject streamIsClosed(StarbytesFuncArgs args) {
    StarbytesObject self = nullptr;
    auto *stream = requireStream(args,self);
    if(!stream) {
        return makeBool(true);
    }
    return makeBool(stream->closed || !stream->file.is_open());
}

StarbytesObject streamFlush(StarbytesFuncArgs args) {
    StarbytesObject self = nullptr;
    auto *stream = requireStream(args,self);
    if(!stream || stream->closed || !stream->file.is_open()) {
        return nullptr;
    }

    stream->file.flush();
    if(stream->file.fail()) {
        return nullptr;
    }
    return makeBool(true);
}

StarbytesObject streamSeek(StarbytesFuncArgs args) {
    StarbytesObject self = nullptr;
    auto *stream = requireStream(args,self);
    if(!stream || stream->closed || !stream->file.is_open()) {
        return nullptr;
    }

    int offset = 0;
    int origin = 0;
    if(!readIntArg(args,offset) || !readIntArg(args,origin)) {
        return nullptr;
    }

    std::ios::seekdir dir = std::ios::beg;
    if(origin == 1) {
        dir = std::ios::cur;
    }
    else if(origin == 2) {
        dir = std::ios::end;
    }

    stream->file.clear();
    stream->file.seekg(offset,dir);
    stream->file.seekp(offset,dir);
    if(stream->file.fail()) {
        return nullptr;
    }

    auto pos = stream->file.tellg();
    if(pos == std::streampos(-1)) {
        pos = stream->file.tellp();
    }
    if(pos == std::streampos(-1)) {
        return makeInt(0);
    }
    return makeInt((int)pos);
}

StarbytesObject streamPosition(StarbytesFuncArgs args) {
    StarbytesObject self = nullptr;
    auto *stream = requireStream(args,self);
    if(!stream || stream->closed || !stream->file.is_open()) {
        return nullptr;
    }

    auto pos = stream->file.tellg();
    if(pos == std::streampos(-1)) {
        pos = stream->file.tellp();
    }
    if(pos == std::streampos(-1)) {
        return nullptr;
    }
    return makeInt((int)pos);
}

StarbytesObject streamTruncate(StarbytesFuncArgs args) {
    StarbytesObject self = nullptr;
    auto *stream = requireStream(args,self);
    if(!stream || stream->closed || !stream->file.is_open()) {
        return nullptr;
    }

    int newSize = 0;
    if(!readIntArg(args,newSize) || newSize < 0) {
        return nullptr;
    }

    stream->file.flush();
    stream->file.clear();
    std::error_code ec;
    std::filesystem::resize_file(stream->path,(uintmax_t)newSize,ec);
    if(ec) {
        return nullptr;
    }
    stream->file.seekg(0,std::ios::beg);
    stream->file.seekp(0,std::ios::beg);
    return makeInt(newSize);
}

StarbytesObject textRead(StarbytesFuncArgs args) {
    StarbytesObject self = nullptr;
    auto *stream = requireStream(args,self);
    if(!stream || stream->closed || !stream->file.is_open() || !streamReadable(stream)) {
        return nullptr;
    }

    int upTo = 0;
    if(!readIntArg(args,upTo) || upTo < 0) {
        return nullptr;
    }

    std::string data;
    data.resize((size_t)upTo);

    stream->file.clear();
    stream->file.read(data.data(),(std::streamsize)upTo);
    auto count = stream->file.gcount();
    if(count < 0) {
        return nullptr;
    }
    data.resize((size_t)count);
    return StarbytesStrNewWithData(data.c_str());
}

StarbytesObject textReadToEnd(StarbytesFuncArgs args) {
    StarbytesObject self = nullptr;
    auto *stream = requireStream(args,self);
    if(!stream || stream->closed || !stream->file.is_open() || !streamReadable(stream)) {
        return nullptr;
    }

    stream->file.clear();
    std::ostringstream out;
    out << stream->file.rdbuf();
    return StarbytesStrNewWithData(out.str().c_str());
}

StarbytesObject textReadLine(StarbytesFuncArgs args) {
    StarbytesObject self = nullptr;
    auto *stream = requireStream(args,self);
    if(!stream || stream->closed || !stream->file.is_open() || !streamReadable(stream)) {
        return nullptr;
    }

    stream->file.clear();
    std::string line;
    if(!std::getline(stream->file,line)) {
        return nullptr;
    }

    if(stream->newline == "" || stream->newline == "\\n") {
        // Keep Python-like line value without trailing newline by default std::getline behavior.
    }

    return StarbytesStrNewWithData(line.c_str());
}

StarbytesObject textWrite(StarbytesFuncArgs args) {
    StarbytesObject self = nullptr;
    auto *stream = requireStream(args,self);
    if(!stream || stream->closed || !stream->file.is_open() || !streamWritable(stream)) {
        return nullptr;
    }

    std::string text;
    if(!readStringArg(args,text)) {
        return nullptr;
    }

    stream->file.clear();
    stream->file.write(text.data(),(std::streamsize)text.size());
    if(stream->file.fail()) {
        return nullptr;
    }
    return makeInt((int)text.size());
}

StarbytesObject streamReadableFlag(StarbytesFuncArgs args) {
    StarbytesObject self = nullptr;
    auto *stream = requireStream(args,self);
    return makeBool(streamReadable(stream) && stream && !stream->closed);
}

StarbytesObject streamWritableFlag(StarbytesFuncArgs args) {
    StarbytesObject self = nullptr;
    auto *stream = requireStream(args,self);
    return makeBool(streamWritable(stream) && stream && !stream->closed);
}

StarbytesObject streamSeekableFlag(StarbytesFuncArgs args) {
    StarbytesObject self = nullptr;
    auto *stream = requireStream(args,self);
    return makeBool(stream && !stream->closed && stream->file.is_open());
}

StarbytesObject textEncoding(StarbytesFuncArgs args) {
    StarbytesObject self = nullptr;
    auto *stream = requireStream(args,self);
    if(!stream) {
        return StarbytesStrNewWithData("utf8");
    }
    return StarbytesStrNewWithData(stream->encoding.c_str());
}

StarbytesObject textNewline(StarbytesFuncArgs args) {
    StarbytesObject self = nullptr;
    auto *stream = requireStream(args,self);
    if(!stream) {
        return StarbytesStrNewWithData("\\n");
    }
    return StarbytesStrNewWithData(stream->newline.c_str());
}

bool bytesFromArray(StarbytesObject arrayObj,std::vector<char> &bytes) {
    if(!arrayObj || !StarbytesObjectTypecheck(arrayObj,StarbytesArrayType())) {
        return false;
    }
    auto len = StarbytesArrayGetLength(arrayObj);
    bytes.clear();
    bytes.reserve(len);
    for(unsigned i = 0; i < len; ++i) {
        auto item = StarbytesArrayIndex(arrayObj,i);
        if(!item || !StarbytesObjectTypecheck(item,StarbytesNumType()) || StarbytesNumGetType(item) != NumTypeInt) {
            return false;
        }
        auto intVal = StarbytesNumGetIntValue(item);
        if(intVal < 0 || intVal > 255) {
            return false;
        }
        bytes.push_back((char)intVal);
    }
    return true;
}

StarbytesObject bytesToArray(const char *data,size_t len) {
    auto arr = StarbytesArrayNew();
    for(size_t i = 0; i < len; ++i) {
        StarbytesArrayPush(arr,makeInt((unsigned char)data[i]));
    }
    return arr;
}

StarbytesObject binaryReadBytes(StarbytesFuncArgs args) {
    StarbytesObject self = nullptr;
    auto *stream = requireStream(args,self);
    if(!stream || stream->closed || !stream->file.is_open() || !streamReadable(stream)) {
        return nullptr;
    }

    int upTo = 0;
    if(!readIntArg(args,upTo) || upTo < 0) {
        return nullptr;
    }

    std::vector<char> buffer((size_t)upTo);
    stream->file.clear();
    stream->file.read(buffer.data(),(std::streamsize)upTo);
    auto count = stream->file.gcount();
    if(count < 0) {
        return nullptr;
    }

    return bytesToArray(buffer.data(),(size_t)count);
}

StarbytesObject binaryReadAllBytes(StarbytesFuncArgs args) {
    StarbytesObject self = nullptr;
    auto *stream = requireStream(args,self);
    if(!stream || stream->closed || !stream->file.is_open() || !streamReadable(stream)) {
        return nullptr;
    }

    stream->file.clear();
    std::ostringstream out;
    out << stream->file.rdbuf();
    auto data = out.str();
    return bytesToArray(data.data(),data.size());
}

StarbytesObject binaryWriteBytes(StarbytesFuncArgs args) {
    StarbytesObject self = nullptr;
    auto *stream = requireStream(args,self);
    if(!stream || stream->closed || !stream->file.is_open() || !streamWritable(stream)) {
        return nullptr;
    }

    auto arrayObj = StarbytesFuncArgsGetArg(args);
    std::vector<char> bytes;
    if(!bytesFromArray(arrayObj,bytes)) {
        return nullptr;
    }

    stream->file.clear();
    if(!bytes.empty()) {
        stream->file.write(bytes.data(),(std::streamsize)bytes.size());
    }
    if(stream->file.fail()) {
        return nullptr;
    }
    return makeInt((int)bytes.size());
}

STARBYTES_FUNC(openText) {
    skipOptionalModuleReceiver(args,5);

    std::string path;
    std::string mode;
    std::string encoding;
    std::string newline;
    int buffering = 0;

    if(!readStringArg(args,path) || !readStringArg(args,mode) || !readStringArg(args,encoding)
       || !readStringArg(args,newline) || !readIntArg(args,buffering)) {
        return nullptr;
    }

    (void)buffering;

    auto stream = createNativeStream(path,mode,false,encoding,newline);
    return registerStreamObject("TextFile",std::move(stream));
}

STARBYTES_FUNC(openBinary) {
    skipOptionalModuleReceiver(args,3);

    std::string path;
    std::string mode;
    int buffering = 0;

    if(!readStringArg(args,path) || !readStringArg(args,mode) || !readIntArg(args,buffering)) {
        return nullptr;
    }

    (void)buffering;

    auto stream = createNativeStream(path,mode,true,"binary","\n");
    return registerStreamObject("BinaryFile",std::move(stream));
}

STARBYTES_FUNC(openFile) {
    skipOptionalModuleReceiver(args,2);

    std::string path;
    std::string mode;

    if(!readStringArg(args,path) || !readStringArg(args,mode)) {
        return nullptr;
    }

    auto stream = createNativeStream(path,mode,false,"utf8","\n");
    return registerStreamObject("Stream",std::move(stream));
}

STARBYTES_FUNC(TextFile_close) { return streamClose(args); }
STARBYTES_FUNC(TextFile_isClosed) { return streamIsClosed(args); }
STARBYTES_FUNC(TextFile_read) { return textRead(args); }
STARBYTES_FUNC(TextFile_readToEnd) { return textReadToEnd(args); }
STARBYTES_FUNC(TextFile_readLine) { return textReadLine(args); }
STARBYTES_FUNC(TextFile_write) { return textWrite(args); }
STARBYTES_FUNC(TextFile_seek) { return streamSeek(args); }
STARBYTES_FUNC(TextFile_position) { return streamPosition(args); }
STARBYTES_FUNC(TextFile_truncate) { return streamTruncate(args); }
STARBYTES_FUNC(TextFile_flush) { return streamFlush(args); }
STARBYTES_FUNC(TextFile_readable) { return streamReadableFlag(args); }
STARBYTES_FUNC(TextFile_writable) { return streamWritableFlag(args); }
STARBYTES_FUNC(TextFile_seekable) { return streamSeekableFlag(args); }
STARBYTES_FUNC(TextFile_encoding) { return textEncoding(args); }
STARBYTES_FUNC(TextFile_newline) { return textNewline(args); }

STARBYTES_FUNC(BinaryFile_close) { return streamClose(args); }
STARBYTES_FUNC(BinaryFile_isClosed) { return streamIsClosed(args); }
STARBYTES_FUNC(BinaryFile_readBytes) { return binaryReadBytes(args); }
STARBYTES_FUNC(BinaryFile_readAllBytes) { return binaryReadAllBytes(args); }
STARBYTES_FUNC(BinaryFile_writeBytes) { return binaryWriteBytes(args); }
STARBYTES_FUNC(BinaryFile_seek) { return streamSeek(args); }
STARBYTES_FUNC(BinaryFile_position) { return streamPosition(args); }
STARBYTES_FUNC(BinaryFile_truncate) { return streamTruncate(args); }
STARBYTES_FUNC(BinaryFile_flush) { return streamFlush(args); }
STARBYTES_FUNC(BinaryFile_readable) { return streamReadableFlag(args); }
STARBYTES_FUNC(BinaryFile_writable) { return streamWritableFlag(args); }
STARBYTES_FUNC(BinaryFile_seekable) { return streamSeekableFlag(args); }

STARBYTES_FUNC(io_exists) {
    skipOptionalModuleReceiver(args,1);

    std::string path;
    if(!readStringArg(args,path)) {
        return makeBool(false);
    }
    return makeBool(std::filesystem::exists(path));
}

STARBYTES_FUNC(io_remove) {
    skipOptionalModuleReceiver(args,1);

    std::string path;
    if(!readStringArg(args,path)) {
        return nullptr;
    }
    std::error_code ec;
    auto removed = std::filesystem::remove(path,ec);
    if(ec) {
        return nullptr;
    }
    return makeBool(removed);
}

STARBYTES_FUNC(io_rename) {
    skipOptionalModuleReceiver(args,2);

    std::string src;
    std::string dst;
    if(!readStringArg(args,src) || !readStringArg(args,dst)) {
        return nullptr;
    }
    std::error_code ec;
    std::filesystem::rename(src,dst,ec);
    if(ec) {
        return nullptr;
    }
    return makeBool(true);
}

STARBYTES_FUNC(io_mkdir) {
    skipOptionalModuleReceiver(args,2);

    std::string path;
    bool recursive = false;
    if(!readStringArg(args,path) || !readBoolArg(args,recursive)) {
        return nullptr;
    }

    std::error_code ec;
    bool rc = false;
    if(recursive) {
        rc = std::filesystem::create_directories(path,ec);
    }
    else {
        rc = std::filesystem::create_directory(path,ec);
    }
    if(ec) {
        return nullptr;
    }
    return makeBool(rc || std::filesystem::exists(path));
}

STARBYTES_FUNC(io_listdir) {
    skipOptionalModuleReceiver(args,1);

    std::string path;
    if(!readStringArg(args,path)) {
        return nullptr;
    }

    std::error_code ec;
    if(!std::filesystem::is_directory(path,ec) || ec) {
        return nullptr;
    }

    auto out = StarbytesArrayNew();
    for(const auto &entry : std::filesystem::directory_iterator(path,ec)) {
        if(ec) {
            return nullptr;
        }
        auto name = entry.path().filename().string();
        StarbytesArrayPush(out,StarbytesStrNewWithData(name.c_str()));
    }
    return out;
}

STARBYTES_FUNC(io_readText) {
    skipOptionalModuleReceiver(args,2);

    std::string path;
    std::string encoding;
    if(!readStringArg(args,path) || !readStringArg(args,encoding)) {
        return nullptr;
    }
    (void)encoding;

    std::ifstream in(path,std::ios::in);
    if(!in.is_open()) {
        return nullptr;
    }
    std::ostringstream out;
    out << in.rdbuf();
    return StarbytesStrNewWithData(out.str().c_str());
}

STARBYTES_FUNC(io_writeText) {
    skipOptionalModuleReceiver(args,3);

    std::string path;
    std::string text;
    std::string encoding;
    if(!readStringArg(args,path) || !readStringArg(args,text) || !readStringArg(args,encoding)) {
        return nullptr;
    }
    (void)encoding;

    std::ofstream out(path,std::ios::out | std::ios::trunc);
    if(!out.is_open()) {
        return nullptr;
    }
    out.write(text.data(),(std::streamsize)text.size());
    if(out.fail()) {
        return nullptr;
    }
    return makeBool(true);
}

STARBYTES_FUNC(io_readBytes) {
    skipOptionalModuleReceiver(args,1);

    std::string path;
    if(!readStringArg(args,path)) {
        return nullptr;
    }

    std::ifstream in(path,std::ios::in | std::ios::binary);
    if(!in.is_open()) {
        return nullptr;
    }

    std::ostringstream out;
    out << in.rdbuf();
    auto data = out.str();
    return bytesToArray(data.data(),data.size());
}

STARBYTES_FUNC(io_writeBytes) {
    skipOptionalModuleReceiver(args,2);

    std::string path;
    if(!readStringArg(args,path)) {
        return nullptr;
    }

    auto arrayObj = StarbytesFuncArgsGetArg(args);
    std::vector<char> bytes;
    if(!bytesFromArray(arrayObj,bytes)) {
        return nullptr;
    }

    std::ofstream out(path,std::ios::out | std::ios::binary | std::ios::trunc);
    if(!out.is_open()) {
        return nullptr;
    }
    if(!bytes.empty()) {
        out.write(bytes.data(),(std::streamsize)bytes.size());
    }
    if(out.fail()) {
        return nullptr;
    }
    return makeBool(true);
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

    addFunc(module,"openText",5,openText);
    addFunc(module,"openBinary",3,openBinary);
    addFunc(module,"openFile",2,openFile);

    addFunc(module,"TextFile_close",1,TextFile_close);
    addFunc(module,"TextFile_isClosed",1,TextFile_isClosed);
    addFunc(module,"TextFile_read",2,TextFile_read);
    addFunc(module,"TextFile_readToEnd",1,TextFile_readToEnd);
    addFunc(module,"TextFile_readLine",1,TextFile_readLine);
    addFunc(module,"TextFile_write",2,TextFile_write);
    addFunc(module,"TextFile_seek",3,TextFile_seek);
    addFunc(module,"TextFile_position",1,TextFile_position);
    addFunc(module,"TextFile_truncate",2,TextFile_truncate);
    addFunc(module,"TextFile_flush",1,TextFile_flush);
    addFunc(module,"TextFile_readable",1,TextFile_readable);
    addFunc(module,"TextFile_writable",1,TextFile_writable);
    addFunc(module,"TextFile_seekable",1,TextFile_seekable);
    addFunc(module,"TextFile_encoding",1,TextFile_encoding);
    addFunc(module,"TextFile_newline",1,TextFile_newline);

    addFunc(module,"BinaryFile_close",1,BinaryFile_close);
    addFunc(module,"BinaryFile_isClosed",1,BinaryFile_isClosed);
    addFunc(module,"BinaryFile_readBytes",2,BinaryFile_readBytes);
    addFunc(module,"BinaryFile_readAllBytes",1,BinaryFile_readAllBytes);
    addFunc(module,"BinaryFile_writeBytes",2,BinaryFile_writeBytes);
    addFunc(module,"BinaryFile_seek",3,BinaryFile_seek);
    addFunc(module,"BinaryFile_position",1,BinaryFile_position);
    addFunc(module,"BinaryFile_truncate",2,BinaryFile_truncate);
    addFunc(module,"BinaryFile_flush",1,BinaryFile_flush);
    addFunc(module,"BinaryFile_readable",1,BinaryFile_readable);
    addFunc(module,"BinaryFile_writable",1,BinaryFile_writable);
    addFunc(module,"BinaryFile_seekable",1,BinaryFile_seekable);

    addFunc(module,"io_exists",1,io_exists);
    addFunc(module,"io_remove",1,io_remove);
    addFunc(module,"io_rename",2,io_rename);
    addFunc(module,"io_mkdir",2,io_mkdir);
    addFunc(module,"io_listdir",1,io_listdir);
    addFunc(module,"io_readText",2,io_readText);
    addFunc(module,"io_writeText",3,io_writeText);
    addFunc(module,"io_readBytes",1,io_readBytes);
    addFunc(module,"io_writeBytes",2,io_writeBytes);

    return module;
}
