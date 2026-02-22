#include <starbytes/interop.h>

#include <algorithm>
#include <climits>
#include <cstdint>
#include <mutex>
#include <random>
#include <string>
#include <vector>

#ifdef STARBYTES_HAS_OPENSSL
#include <openssl/rand.h>
#endif

namespace {

struct NativeArgsLayout {
    unsigned argc = 0;
    unsigned index = 0;
    StarbytesObject *argv = nullptr;
};

constexpr int kMaxByteCount = 1024 * 1024;
constexpr char kHexDigits[] = "0123456789abcdef";

std::mt19937_64 g_rng((std::random_device())());
std::mutex g_rngMutex;

StarbytesObject makeBool(bool value) {
    // Runtime bool consumption currently interprets StarbytesBoolFalse as logical true.
    return StarbytesBoolNew(value ? StarbytesBoolFalse : StarbytesBoolTrue);
}

StarbytesObject makeInt(int value) {
    return StarbytesNumNew(NumTypeInt,value);
}

StarbytesObject makeFloat(double value) {
    return StarbytesNumNew(NumTypeFloat,value);
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

StarbytesObject bytesToArray(const std::vector<unsigned char> &bytes) {
    auto out = StarbytesArrayNew();
    for(unsigned char b : bytes) {
        StarbytesArrayPush(out,makeInt((int)b));
    }
    return out;
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

bool fillDeterministicBytes(int count,std::vector<unsigned char> &outBytes) {
    if(count < 0 || count > kMaxByteCount) {
        return false;
    }
    outBytes.assign((size_t)count,0);

    std::lock_guard<std::mutex> lock(g_rngMutex);
    std::uniform_int_distribution<int> dist(0,255);
    for(int i = 0; i < count; ++i) {
        outBytes[(size_t)i] = (unsigned char)dist(g_rng);
    }
    return true;
}

bool fillSecureBytes(int count,std::vector<unsigned char> &outBytes) {
    if(count < 0 || count > kMaxByteCount) {
        return false;
    }
    outBytes.assign((size_t)count,0);
    if(count == 0) {
        return true;
    }

#ifdef STARBYTES_HAS_OPENSSL
    return RAND_bytes(outBytes.data(),count) == 1;
#else
    std::random_device rd;
    for(int i = 0; i < count; ++i) {
        outBytes[(size_t)i] = (unsigned char)(rd() & 0xFF);
    }
    return true;
#endif
}

std::string formatUuidV4(const std::vector<unsigned char> &bytes) {
    if(bytes.size() != 16) {
        return "";
    }

    auto hex = bytesToHex(bytes);
    return hex.substr(0,8) + "-" + hex.substr(8,4) + "-" + hex.substr(12,4) + "-" + hex.substr(16,4) + "-" + hex.substr(20,12);
}

STARBYTES_FUNC(random_setSeed) {
    skipOptionalModuleReceiver(args,1);

    int seed = 0;
    if(!readIntArg(args,seed)) {
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(g_rngMutex);
    g_rng.seed((uint64_t)(uint32_t)seed);
    return makeBool(true);
}

STARBYTES_FUNC(random_int) {
    skipOptionalModuleReceiver(args,2);

    int minValue = 0;
    int maxValue = 0;
    if(!readIntArg(args,minValue) || !readIntArg(args,maxValue)) {
        return nullptr;
    }
    if(minValue > maxValue) {
        std::swap(minValue,maxValue);
    }

    std::lock_guard<std::mutex> lock(g_rngMutex);
    std::uniform_int_distribution<int> dist(minValue,maxValue);
    return makeInt(dist(g_rng));
}

STARBYTES_FUNC(random_float) {
    skipOptionalModuleReceiver(args,0);

    std::lock_guard<std::mutex> lock(g_rngMutex);
    std::uniform_real_distribution<double> dist(0.0,1.0);
    return makeFloat(dist(g_rng));
}

STARBYTES_FUNC(random_bytes) {
    skipOptionalModuleReceiver(args,1);

    int count = 0;
    if(!readIntArg(args,count)) {
        return nullptr;
    }

    std::vector<unsigned char> bytes;
    if(!fillDeterministicBytes(count,bytes)) {
        return nullptr;
    }
    return bytesToArray(bytes);
}

STARBYTES_FUNC(random_secureBytes) {
    skipOptionalModuleReceiver(args,1);

    int count = 0;
    if(!readIntArg(args,count)) {
        return nullptr;
    }

    std::vector<unsigned char> bytes;
    if(!fillSecureBytes(count,bytes)) {
        return nullptr;
    }
    return bytesToArray(bytes);
}

STARBYTES_FUNC(random_secureHex) {
    skipOptionalModuleReceiver(args,1);

    int byteCount = 0;
    if(!readIntArg(args,byteCount)) {
        return nullptr;
    }

    std::vector<unsigned char> bytes;
    if(!fillSecureBytes(byteCount,bytes)) {
        return nullptr;
    }
    auto hex = bytesToHex(bytes);
    return StarbytesStrNewWithData(hex.c_str());
}

STARBYTES_FUNC(random_uuid4) {
    skipOptionalModuleReceiver(args,0);

    std::vector<unsigned char> bytes;
    if(!fillSecureBytes(16,bytes)) {
        return nullptr;
    }

    bytes[6] = (unsigned char)((bytes[6] & 0x0F) | 0x40);
    bytes[8] = (unsigned char)((bytes[8] & 0x3F) | 0x80);
    auto uuid = formatUuidV4(bytes);
    if(uuid.empty()) {
        return nullptr;
    }
    return StarbytesStrNewWithData(uuid.c_str());
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

    addFunc(module,"random_setSeed",1,random_setSeed);
    addFunc(module,"random_int",2,random_int);
    addFunc(module,"random_float",0,random_float);
    addFunc(module,"random_bytes",1,random_bytes);
    addFunc(module,"random_secureBytes",1,random_secureBytes);
    addFunc(module,"random_secureHex",1,random_secureHex);
    addFunc(module,"random_uuid4",0,random_uuid4);

    return module;
}
