#include <starbytes/interop.h>

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#ifdef STARBYTES_HAS_ZLIB
#include <zlib.h>
#endif

namespace {

struct NativeArgsLayout {
    unsigned argc = 0;
    unsigned index = 0;
    StarbytesObject *argv = nullptr;
};

constexpr size_t kChunkSize = 16 * 1024;
constexpr size_t kMaxInputBytes = 32 * 1024 * 1024;
constexpr size_t kMaxOutputBytes = 64 * 1024 * 1024;
constexpr char kHexDigits[] = "0123456789abcdef";

bool readStringArg(StarbytesFuncArgs args,std::string &outValue) {
    auto arg = StarbytesFuncArgsGetArg(args);
    if(!arg || !StarbytesObjectTypecheck(arg,StarbytesStrType())) {
        return false;
    }
    auto *buf = StarbytesStrGetBuffer(arg);
    outValue = buf ? buf : "";
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

int nibbleFromHex(char c) {
    if(c >= '0' && c <= '9') {
        return c - '0';
    }
    if(c >= 'a' && c <= 'f') {
        return (c - 'a') + 10;
    }
    if(c >= 'A' && c <= 'F') {
        return (c - 'A') + 10;
    }
    return -1;
}

std::string bytesToHex(const std::vector<unsigned char> &bytes) {
    std::string out;
    out.resize(bytes.size() * 2);
    for(size_t i = 0; i < bytes.size(); ++i) {
        out[i * 2] = kHexDigits[(bytes[i] >> 4) & 0x0F];
        out[i * 2 + 1] = kHexDigits[bytes[i] & 0x0F];
    }
    return out;
}

bool hexToBytes(const std::string &hex,std::vector<unsigned char> &outBytes) {
    if(hex.size() % 2 != 0) {
        return false;
    }
    outBytes.clear();
    outBytes.reserve(hex.size() / 2);
    for(size_t i = 0; i < hex.size(); i += 2) {
        int hi = nibbleFromHex(hex[i]);
        int lo = nibbleFromHex(hex[i + 1]);
        if(hi < 0 || lo < 0) {
            return false;
        }
        outBytes.push_back((unsigned char)((hi << 4) | lo));
    }
    return true;
}

uint32_t fnv1a32(const unsigned char *data,size_t len) {
    uint32_t hash = 2166136261u;
    for(size_t i = 0; i < len; ++i) {
        hash ^= data[i];
        hash *= 16777619u;
    }
    return hash;
}

bool zlibDeflate(const std::vector<unsigned char> &input,int level,int windowBits,std::vector<unsigned char> &out) {
    if(input.size() > kMaxInputBytes) {
        return false;
    }

#ifdef STARBYTES_HAS_ZLIB
    if(level < -1 || level > 9) {
        return false;
    }
    if(input.size() > (size_t)UINT32_MAX) {
        return false;
    }

    z_stream stream = {};
    if(deflateInit2(&stream,level,Z_DEFLATED,windowBits,8,Z_DEFAULT_STRATEGY) != Z_OK) {
        return false;
    }

    out.clear();
    stream.next_in = (Bytef *)(input.empty() ? nullptr : input.data());
    stream.avail_in = (uInt)input.size();

    int ret = Z_OK;
    while(ret != Z_STREAM_END) {
        size_t prior = out.size();
        out.resize(prior + kChunkSize);

        stream.next_out = reinterpret_cast<Bytef *>(out.data() + prior);
        stream.avail_out = (uInt)kChunkSize;

        ret = deflate(&stream,Z_FINISH);
        if(ret != Z_OK && ret != Z_STREAM_END) {
            deflateEnd(&stream);
            return false;
        }

        size_t produced = kChunkSize - (size_t)stream.avail_out;
        out.resize(prior + produced);
        if(out.size() > kMaxOutputBytes) {
            deflateEnd(&stream);
            return false;
        }
    }

    deflateEnd(&stream);
    return true;
#else
    (void)level;
    (void)windowBits;
    out = input;
    return true;
#endif
}

bool zlibInflate(const std::vector<unsigned char> &input,int windowBits,std::vector<unsigned char> &out) {
    if(input.size() > kMaxInputBytes) {
        return false;
    }

#ifdef STARBYTES_HAS_ZLIB
    if(input.empty()) {
        return false;
    }
    if(input.size() > (size_t)UINT32_MAX) {
        return false;
    }

    z_stream stream = {};
    if(inflateInit2(&stream,windowBits) != Z_OK) {
        return false;
    }

    out.clear();
    stream.next_in = (Bytef *)input.data();
    stream.avail_in = (uInt)input.size();

    int ret = Z_OK;
    while(ret != Z_STREAM_END) {
        size_t prior = out.size();
        out.resize(prior + kChunkSize);

        stream.next_out = reinterpret_cast<Bytef *>(out.data() + prior);
        stream.avail_out = (uInt)kChunkSize;

        ret = inflate(&stream,Z_NO_FLUSH);
        if(ret != Z_OK && ret != Z_STREAM_END) {
            inflateEnd(&stream);
            return false;
        }

        size_t produced = kChunkSize - (size_t)stream.avail_out;
        out.resize(prior + produced);
        if(out.size() > kMaxOutputBytes) {
            inflateEnd(&stream);
            return false;
        }
    }

    inflateEnd(&stream);
    return true;
#else
    (void)windowBits;
    out = input;
    return true;
#endif
}

std::string formatHex8(uint32_t value) {
    char out[9];
    std::snprintf(out,sizeof(out),"%08x",value);
    return out;
}

STARBYTES_FUNC(compression_deflateHex) {
    skipOptionalModuleReceiver(args,2);

    std::string inputHex;
    int level = -1;
    if(!readStringArg(args,inputHex) || !readIntArg(args,level)) {
        return nullptr;
    }

    std::vector<unsigned char> input;
    if(!hexToBytes(inputHex,input)) {
        return nullptr;
    }

    std::vector<unsigned char> compressed;
    if(!zlibDeflate(input,level,15,compressed)) {
        return nullptr;
    }

    auto outHex = bytesToHex(compressed);
    return StarbytesStrNewWithData(outHex.c_str());
}

STARBYTES_FUNC(compression_inflateHex) {
    skipOptionalModuleReceiver(args,1);

    std::string compressedHex;
    if(!readStringArg(args,compressedHex)) {
        return nullptr;
    }

    std::vector<unsigned char> compressed;
    if(!hexToBytes(compressedHex,compressed)) {
        return nullptr;
    }

    std::vector<unsigned char> inflated;
    if(!zlibInflate(compressed,15,inflated)) {
        return nullptr;
    }

    auto outHex = bytesToHex(inflated);
    return StarbytesStrNewWithData(outHex.c_str());
}

STARBYTES_FUNC(compression_gzipTextHex) {
    skipOptionalModuleReceiver(args,2);

    std::string text;
    int level = -1;
    if(!readStringArg(args,text) || !readIntArg(args,level)) {
        return nullptr;
    }

    std::vector<unsigned char> input(text.begin(),text.end());
    std::vector<unsigned char> compressed;
    if(!zlibDeflate(input,level,31,compressed)) {
        return nullptr;
    }

    auto outHex = bytesToHex(compressed);
    return StarbytesStrNewWithData(outHex.c_str());
}

STARBYTES_FUNC(compression_gunzipText) {
    skipOptionalModuleReceiver(args,1);

    std::string compressedHex;
    if(!readStringArg(args,compressedHex)) {
        return nullptr;
    }

    std::vector<unsigned char> compressed;
    if(!hexToBytes(compressedHex,compressed)) {
        return nullptr;
    }

    std::vector<unsigned char> outBytes;
    if(!zlibInflate(compressed,31,outBytes)) {
        return nullptr;
    }

    std::string text(outBytes.begin(),outBytes.end());
    return StarbytesStrNewWithData(text.c_str());
}

STARBYTES_FUNC(compression_crc32Hex) {
    skipOptionalModuleReceiver(args,1);

    std::string text;
    if(!readStringArg(args,text)) {
        return nullptr;
    }

    uint32_t value = 0;
#ifdef STARBYTES_HAS_ZLIB
    value = (uint32_t)::crc32(0,(const Bytef *)text.data(),(uInt)text.size());
#else
    value = fnv1a32(reinterpret_cast<const unsigned char *>(text.data()),text.size());
#endif

    auto hex = formatHex8(value);
    return StarbytesStrNewWithData(hex.c_str());
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

    addFunc(module,"compression_deflateHex",2,compression_deflateHex);
    addFunc(module,"compression_inflateHex",1,compression_inflateHex);
    addFunc(module,"compression_gzipTextHex",2,compression_gzipTextHex);
    addFunc(module,"compression_gunzipText",1,compression_gunzipText);
    addFunc(module,"compression_crc32Hex",1,compression_crc32Hex);

    return module;
}
