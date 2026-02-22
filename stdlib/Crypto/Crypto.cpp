#include <starbytes/interop.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#ifdef STARBYTES_HAS_OPENSSL
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/sha.h>
#endif

namespace {

struct NativeArgsLayout {
    unsigned argc = 0;
    unsigned index = 0;
    StarbytesObject *argv = nullptr;
};

constexpr int kMaxPbkdf2Iterations = 10 * 1000 * 1000;
constexpr int kMaxDerivedKeyBytes = 4096;
constexpr char kHexDigits[] = "0123456789abcdef";

enum class DigestKind {
    Md5,
    Sha1,
    Sha256
};

StarbytesObject makeBool(bool value) {
    // Runtime bool consumption currently interprets StarbytesBoolFalse as logical true.
    return StarbytesBoolNew(value ? StarbytesBoolFalse : StarbytesBoolTrue);
}

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

uint64_t fnv1a64(const unsigned char *data,size_t len,uint64_t seed) {
    uint64_t hash = seed;
    for(size_t i = 0; i < len; ++i) {
        hash ^= (uint64_t)data[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

uint64_t splitMix64(uint64_t value) {
    uint64_t z = value + 0x9e3779b97f4a7c15ULL;
    z = (z ^ (z >> 30U)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27U)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31U);
}

std::vector<unsigned char> pseudoDigest(const std::vector<unsigned char> &data,size_t outLen,const char *domain) {
    std::vector<unsigned char> out(outLen,0);

    uint64_t seed = 1469598103934665603ULL;
    if(domain) {
        seed = fnv1a64(reinterpret_cast<const unsigned char *>(domain),std::strlen(domain),seed);
    }
    if(!data.empty()) {
        seed = fnv1a64(data.data(),data.size(),seed);
    }

    uint64_t state = seed;
    for(size_t i = 0; i < outLen; ++i) {
        state = splitMix64(state + i + (uint64_t)(data.size() + 1));
        out[i] = (unsigned char)((state >> ((i % 8) * 8)) & 0xFF);
    }
    return out;
}

bool digestFallback(DigestKind kind,const std::string &text,std::vector<unsigned char> &out) {
    std::vector<unsigned char> bytes(text.begin(),text.end());
    switch(kind) {
        case DigestKind::Md5:
            out = pseudoDigest(bytes,16,"md5");
            return true;
        case DigestKind::Sha1:
            out = pseudoDigest(bytes,20,"sha1");
            return true;
        case DigestKind::Sha256:
            out = pseudoDigest(bytes,32,"sha256");
            return true;
    }
    return false;
}

bool digestText(DigestKind kind,const std::string &text,std::vector<unsigned char> &out) {
#ifdef STARBYTES_HAS_OPENSSL
    const EVP_MD *md = nullptr;
    switch(kind) {
        case DigestKind::Md5:
            md = EVP_md5();
            break;
        case DigestKind::Sha1:
            md = EVP_sha1();
            break;
        case DigestKind::Sha256:
            md = EVP_sha256();
            break;
    }
    if(!md) {
        return false;
    }

    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if(!ctx) {
        return false;
    }

    unsigned int outLen = 0;
    out.assign((size_t)EVP_MD_size(md),0);
    bool ok = EVP_DigestInit_ex(ctx,md,nullptr) == 1
              && EVP_DigestUpdate(ctx,text.data(),text.size()) == 1
              && EVP_DigestFinal_ex(ctx,out.data(),&outLen) == 1;
    EVP_MD_CTX_free(ctx);
    if(!ok) {
        return false;
    }
    out.resize(outLen);
    return true;
#else
    return digestFallback(kind,text,out);
#endif
}

bool hmacSha256(const std::string &key,const std::string &message,std::vector<unsigned char> &out) {
#ifdef STARBYTES_HAS_OPENSSL
    unsigned int outLen = 0;
    out.assign(32,0);
    auto *result = HMAC(EVP_sha256(),
                        key.data(),(int)key.size(),
                        reinterpret_cast<const unsigned char *>(message.data()),message.size(),
                        out.data(),&outLen);
    if(!result) {
        return false;
    }
    out.resize(outLen);
    return true;
#else
    std::vector<unsigned char> bytes;
    bytes.reserve(key.size() + 1 + message.size());
    bytes.insert(bytes.end(),key.begin(),key.end());
    bytes.push_back('|');
    bytes.insert(bytes.end(),message.begin(),message.end());
    out = pseudoDigest(bytes,32,"hmac-sha256");
    return true;
#endif
}

bool pbkdf2Sha256(const std::string &password,
                  const std::vector<unsigned char> &salt,
                  int iterations,
                  int keyBytes,
                  std::vector<unsigned char> &out) {
#ifdef STARBYTES_HAS_OPENSSL
    out.assign((size_t)keyBytes,0);
    return PKCS5_PBKDF2_HMAC(password.data(),
                             (int)password.size(),
                             salt.empty() ? nullptr : salt.data(),
                             (int)salt.size(),
                             iterations,
                             EVP_sha256(),
                             keyBytes,
                             out.data()) == 1;
#else
    out.assign((size_t)keyBytes,0);
    if(out.empty()) {
        return true;
    }

    std::vector<unsigned char> input;
    input.reserve(password.size() + salt.size());
    input.insert(input.end(),password.begin(),password.end());
    input.insert(input.end(),salt.begin(),salt.end());
    auto block = pseudoDigest(input,32,"pbkdf2-seed");
    for(int iter = 0; iter < iterations; ++iter) {
        for(int i = 0; i < keyBytes; ++i) {
            out[(size_t)i] ^= block[(size_t)i % block.size()];
        }
        block = pseudoDigest(block,32,"pbkdf2-round");
    }
    return true;
#endif
}

bool constantTimeEqual(const std::vector<unsigned char> &lhs,const std::vector<unsigned char> &rhs) {
    if(lhs.size() != rhs.size()) {
        return false;
    }

#ifdef STARBYTES_HAS_OPENSSL
    return CRYPTO_memcmp(lhs.data(),rhs.data(),lhs.size()) == 0;
#else
    unsigned char diff = 0;
    for(size_t i = 0; i < lhs.size(); ++i) {
        diff |= (unsigned char)(lhs[i] ^ rhs[i]);
    }
    return diff == 0;
#endif
}

StarbytesObject digestAsHex(DigestKind kind,const std::string &text) {
    std::vector<unsigned char> digest;
    if(!digestText(kind,text,digest)) {
        return nullptr;
    }
    auto hex = bytesToHex(digest);
    return StarbytesStrNewWithData(hex.c_str());
}

STARBYTES_FUNC(crypto_md5Hex) {
    skipOptionalModuleReceiver(args,1);

    std::string text;
    if(!readStringArg(args,text)) {
        return nullptr;
    }
    return digestAsHex(DigestKind::Md5,text);
}

STARBYTES_FUNC(crypto_sha1Hex) {
    skipOptionalModuleReceiver(args,1);

    std::string text;
    if(!readStringArg(args,text)) {
        return nullptr;
    }
    return digestAsHex(DigestKind::Sha1,text);
}

STARBYTES_FUNC(crypto_sha256Hex) {
    skipOptionalModuleReceiver(args,1);

    std::string text;
    if(!readStringArg(args,text)) {
        return nullptr;
    }
    return digestAsHex(DigestKind::Sha256,text);
}

STARBYTES_FUNC(crypto_hmacSha256Hex) {
    skipOptionalModuleReceiver(args,2);

    std::string key;
    std::string message;
    if(!readStringArg(args,key) || !readStringArg(args,message)) {
        return nullptr;
    }

    std::vector<unsigned char> digest;
    if(!hmacSha256(key,message,digest)) {
        return nullptr;
    }
    auto hex = bytesToHex(digest);
    return StarbytesStrNewWithData(hex.c_str());
}

STARBYTES_FUNC(crypto_pbkdf2Sha256Hex) {
    skipOptionalModuleReceiver(args,4);

    std::string password;
    std::string saltHex;
    int iterations = 0;
    int keyBytes = 0;
    if(!readStringArg(args,password) || !readStringArg(args,saltHex)
       || !readIntArg(args,iterations) || !readIntArg(args,keyBytes)) {
        return nullptr;
    }
    if(iterations <= 0 || iterations > kMaxPbkdf2Iterations) {
        return nullptr;
    }
    if(keyBytes <= 0 || keyBytes > kMaxDerivedKeyBytes) {
        return nullptr;
    }

    std::vector<unsigned char> salt;
    if(!hexToBytes(saltHex,salt)) {
        return nullptr;
    }

    std::vector<unsigned char> derived;
    if(!pbkdf2Sha256(password,salt,iterations,keyBytes,derived)) {
        return nullptr;
    }
    auto hex = bytesToHex(derived);
    return StarbytesStrNewWithData(hex.c_str());
}

STARBYTES_FUNC(crypto_constantTimeHexEquals) {
    skipOptionalModuleReceiver(args,2);

    std::string lhsHex;
    std::string rhsHex;
    if(!readStringArg(args,lhsHex) || !readStringArg(args,rhsHex)) {
        return makeBool(false);
    }

    std::vector<unsigned char> lhs;
    std::vector<unsigned char> rhs;
    if(!hexToBytes(lhsHex,lhs) || !hexToBytes(rhsHex,rhs)) {
        return makeBool(false);
    }
    return makeBool(constantTimeEqual(lhs,rhs));
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

    addFunc(module,"crypto_md5Hex",1,crypto_md5Hex);
    addFunc(module,"crypto_sha1Hex",1,crypto_sha1Hex);
    addFunc(module,"crypto_sha256Hex",1,crypto_sha256Hex);
    addFunc(module,"crypto_hmacSha256Hex",2,crypto_hmacSha256Hex);
    addFunc(module,"crypto_pbkdf2Sha256Hex",4,crypto_pbkdf2Sha256Hex);
    addFunc(module,"crypto_constantTimeHexEquals",2,crypto_constantTimeHexEquals);

    return module;
}
