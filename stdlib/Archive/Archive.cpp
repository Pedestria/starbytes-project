#include <starbytes/interop.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
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

struct EntryPayload {
    std::string name;
    std::vector<unsigned char> bytes;
};

struct DecodedEntry {
    std::string name;
    std::string value;
};

constexpr char kMagic[] = "STRBARC1";
constexpr size_t kMagicLen = sizeof(kMagic) - 1;
constexpr uint16_t kVersion = 1;
constexpr uint32_t kFlagEntryCompressed = 0x1u;
constexpr size_t kChunkSize = 16 * 1024;
constexpr size_t kMaxEntryCount = 4096;
constexpr size_t kMaxEntryNameBytes = 4096;
constexpr size_t kMaxEntryValueBytes = 16 * 1024 * 1024;
constexpr size_t kMaxArchiveBytes = 64 * 1024 * 1024;
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

bool readBoolArg(StarbytesFuncArgs args,bool &outValue) {
    auto arg = StarbytesFuncArgsGetArg(args);
    if(!arg || !StarbytesObjectTypecheck(arg,StarbytesBoolType())) {
        return false;
    }
    outValue = (StarbytesBoolValue(arg) == StarbytesBoolFalse);
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

StarbytesObject makeBool(bool value) {
    // Runtime bool consumption currently interprets StarbytesBoolFalse as logical true.
    return StarbytesBoolNew(value ? StarbytesBoolFalse : StarbytesBoolTrue);
}

void writeU16(std::vector<unsigned char> &out,uint16_t value) {
    out.push_back((unsigned char)(value & 0xFFu));
    out.push_back((unsigned char)((value >> 8u) & 0xFFu));
}

void writeU32(std::vector<unsigned char> &out,uint32_t value) {
    out.push_back((unsigned char)(value & 0xFFu));
    out.push_back((unsigned char)((value >> 8u) & 0xFFu));
    out.push_back((unsigned char)((value >> 16u) & 0xFFu));
    out.push_back((unsigned char)((value >> 24u) & 0xFFu));
}

void writeU64(std::vector<unsigned char> &out,uint64_t value) {
    out.push_back((unsigned char)(value & 0xFFu));
    out.push_back((unsigned char)((value >> 8u) & 0xFFu));
    out.push_back((unsigned char)((value >> 16u) & 0xFFu));
    out.push_back((unsigned char)((value >> 24u) & 0xFFu));
    out.push_back((unsigned char)((value >> 32u) & 0xFFu));
    out.push_back((unsigned char)((value >> 40u) & 0xFFu));
    out.push_back((unsigned char)((value >> 48u) & 0xFFu));
    out.push_back((unsigned char)((value >> 56u) & 0xFFu));
}

bool readU16(const std::vector<unsigned char> &in,size_t &index,uint16_t &outValue) {
    if(index + 2 > in.size()) {
        return false;
    }
    outValue = (uint16_t)in[index] | (uint16_t)(in[index + 1] << 8u);
    index += 2;
    return true;
}

bool readU32(const std::vector<unsigned char> &in,size_t &index,uint32_t &outValue) {
    if(index + 4 > in.size()) {
        return false;
    }
    outValue = (uint32_t)in[index]
               | ((uint32_t)in[index + 1] << 8u)
               | ((uint32_t)in[index + 2] << 16u)
               | ((uint32_t)in[index + 3] << 24u);
    index += 4;
    return true;
}

bool readU64(const std::vector<unsigned char> &in,size_t &index,uint64_t &outValue) {
    if(index + 8 > in.size()) {
        return false;
    }
    outValue = (uint64_t)in[index]
               | ((uint64_t)in[index + 1] << 8u)
               | ((uint64_t)in[index + 2] << 16u)
               | ((uint64_t)in[index + 3] << 24u)
               | ((uint64_t)in[index + 4] << 32u)
               | ((uint64_t)in[index + 5] << 40u)
               | ((uint64_t)in[index + 6] << 48u)
               | ((uint64_t)in[index + 7] << 56u);
    index += 8;
    return true;
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

bool readDictStringEntries(StarbytesObject dict,std::vector<EntryPayload> &outEntries) {
    if(!dict || !StarbytesObjectTypecheck(dict,StarbytesDictType())) {
        return false;
    }

    auto keys = StarbytesObjectGetProperty(dict,"keys");
    auto values = StarbytesObjectGetProperty(dict,"values");
    if(!keys || !values || !StarbytesObjectTypecheck(keys,StarbytesArrayType()) || !StarbytesObjectTypecheck(values,StarbytesArrayType())) {
        return false;
    }

    auto len = StarbytesArrayGetLength(keys);
    if(len != StarbytesArrayGetLength(values) || len > kMaxEntryCount) {
        return false;
    }

    outEntries.clear();
    outEntries.reserve(len);
    for(unsigned i = 0; i < len; ++i) {
        auto key = StarbytesArrayIndex(keys,i);
        auto value = StarbytesArrayIndex(values,i);
        if(!key || !value || !StarbytesObjectTypecheck(key,StarbytesStrType()) || !StarbytesObjectTypecheck(value,StarbytesStrType())) {
            return false;
        }

        auto *keyBuf = StarbytesStrGetBuffer(key);
        auto *valueBuf = StarbytesStrGetBuffer(value);
        std::string keyText = keyBuf ? keyBuf : "";
        std::string valueText = valueBuf ? valueBuf : "";
        if(keyText.empty() || keyText.size() > kMaxEntryNameBytes || valueText.size() > kMaxEntryValueBytes) {
            return false;
        }

        EntryPayload entry;
        entry.name = keyText;
        entry.bytes.assign(valueText.begin(),valueText.end());
        outEntries.push_back(std::move(entry));
    }
    return true;
}

bool compressEntry(const std::vector<unsigned char> &input,std::vector<unsigned char> &out,bool &usedCompression) {
    usedCompression = false;

#ifdef STARBYTES_HAS_ZLIB
    if(input.size() > (size_t)UINT32_MAX) {
        return false;
    }

    z_stream stream = {};
    if(deflateInit2(&stream,Z_DEFAULT_COMPRESSION,Z_DEFLATED,15,8,Z_DEFAULT_STRATEGY) != Z_OK) {
        return false;
    }

    stream.next_in = (Bytef *)(input.empty() ? nullptr : input.data());
    stream.avail_in = (uInt)input.size();
    out.clear();

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
        if(out.size() > kMaxArchiveBytes) {
            deflateEnd(&stream);
            return false;
        }
    }

    deflateEnd(&stream);
    usedCompression = out.size() < input.size();
    if(!usedCompression) {
        out = input;
    }
    return true;
#else
    out = input;
    return true;
#endif
}

bool inflateEntry(const std::vector<unsigned char> &input,uint64_t originalSize,std::vector<unsigned char> &out) {
    if(originalSize > kMaxEntryValueBytes) {
        return false;
    }

#ifdef STARBYTES_HAS_ZLIB
    if(input.empty()) {
        out.clear();
        return originalSize == 0;
    }
    if(input.size() > (size_t)UINT32_MAX) {
        return false;
    }

    z_stream stream = {};
    if(inflateInit2(&stream,15) != Z_OK) {
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
        if(out.size() > originalSize || out.size() > kMaxEntryValueBytes) {
            inflateEnd(&stream);
            return false;
        }
    }
    inflateEnd(&stream);
    return out.size() == originalSize;
#else
    (void)input;
    (void)originalSize;
    (void)out;
    return false;
#endif
}

bool parseArchive(const std::vector<unsigned char> &blob,std::vector<DecodedEntry> &entries,bool materializeValues) {
    if(blob.size() < kMagicLen + 2 + 2 + 4 || blob.size() > kMaxArchiveBytes) {
        return false;
    }
    if(std::memcmp(blob.data(),kMagic,kMagicLen) != 0) {
        return false;
    }

    size_t index = kMagicLen;
    uint16_t version = 0;
    uint16_t archiveFlags = 0;
    uint32_t entryCount = 0;
    if(!readU16(blob,index,version) || !readU16(blob,index,archiveFlags) || !readU32(blob,index,entryCount)) {
        return false;
    }
    (void)archiveFlags;
    if(version != kVersion || entryCount > kMaxEntryCount) {
        return false;
    }

    entries.clear();
    entries.reserve(entryCount);
    for(uint32_t i = 0; i < entryCount; ++i) {
        uint32_t nameLen = 0;
        uint64_t originalLen = 0;
        uint64_t storedLen = 0;
        uint32_t flags = 0;
        if(!readU32(blob,index,nameLen) || !readU64(blob,index,originalLen) || !readU64(blob,index,storedLen) || !readU32(blob,index,flags)) {
            return false;
        }

        if(nameLen == 0 || nameLen > kMaxEntryNameBytes || originalLen > kMaxEntryValueBytes || storedLen > kMaxEntryValueBytes) {
            return false;
        }
        if(index + nameLen > blob.size()) {
            return false;
        }
        std::string name(reinterpret_cast<const char *>(blob.data() + index),nameLen);
        index += nameLen;

        if(index + storedLen > blob.size()) {
            return false;
        }
        std::vector<unsigned char> stored(blob.begin() + static_cast<std::vector<unsigned char>::difference_type>(index),
                                          blob.begin() + static_cast<std::vector<unsigned char>::difference_type>(index + storedLen));
        index += (size_t)storedLen;

        std::vector<unsigned char> valueBytes;
        if((flags & kFlagEntryCompressed) != 0) {
            if(!inflateEntry(stored,originalLen,valueBytes)) {
                return false;
            }
        }
        else {
            if(originalLen != storedLen) {
                return false;
            }
            valueBytes = std::move(stored);
        }

        DecodedEntry entry;
        entry.name = std::move(name);
        if(materializeValues) {
            entry.value.assign(valueBytes.begin(),valueBytes.end());
        }
        entries.push_back(std::move(entry));
    }

    return index == blob.size();
}

STARBYTES_FUNC(archive_packTextMapHex) {
    skipOptionalModuleReceiver(args,2);

    auto dictArg = StarbytesFuncArgsGetArg(args);
    bool compress = false;
    if(!readBoolArg(args,compress)) {
        return nullptr;
    }

    std::vector<EntryPayload> entries;
    if(!readDictStringEntries(dictArg,entries)) {
        return nullptr;
    }

    std::vector<unsigned char> archiveBytes;
    archiveBytes.reserve(1024);
    archiveBytes.insert(archiveBytes.end(),kMagic,kMagic + kMagicLen);
    writeU16(archiveBytes,kVersion);
    writeU16(archiveBytes,0);
    writeU32(archiveBytes,(uint32_t)entries.size());

    for(const auto &entry : entries) {
        std::vector<unsigned char> payload = entry.bytes;
        bool usedCompression = false;
        if(compress) {
            if(!compressEntry(entry.bytes,payload,usedCompression)) {
                return nullptr;
            }
        }

        writeU32(archiveBytes,(uint32_t)entry.name.size());
        writeU64(archiveBytes,(uint64_t)entry.bytes.size());
        writeU64(archiveBytes,(uint64_t)payload.size());
        writeU32(archiveBytes,usedCompression ? kFlagEntryCompressed : 0u);
        archiveBytes.insert(archiveBytes.end(),entry.name.begin(),entry.name.end());
        archiveBytes.insert(archiveBytes.end(),payload.begin(),payload.end());

        if(archiveBytes.size() > kMaxArchiveBytes) {
            return nullptr;
        }
    }

    auto hex = bytesToHex(archiveBytes);
    return StarbytesStrNewWithData(hex.c_str());
}

STARBYTES_FUNC(archive_unpackTextMapHex) {
    skipOptionalModuleReceiver(args,1);

    std::string archiveHex;
    if(!readStringArg(args,archiveHex)) {
        return nullptr;
    }

    std::vector<unsigned char> blob;
    if(!hexToBytes(archiveHex,blob)) {
        return nullptr;
    }

    std::vector<DecodedEntry> entries;
    if(!parseArchive(blob,entries,true)) {
        return nullptr;
    }

    auto dict = StarbytesDictNew();
    for(const auto &entry : entries) {
        auto key = StarbytesStrNewWithData(entry.name.c_str());
        auto value = StarbytesStrNewWithData(entry.value.c_str());
        StarbytesDictSet(dict,key,value);
    }
    return dict;
}

STARBYTES_FUNC(archive_listEntries) {
    skipOptionalModuleReceiver(args,1);

    std::string archiveHex;
    if(!readStringArg(args,archiveHex)) {
        return nullptr;
    }

    std::vector<unsigned char> blob;
    if(!hexToBytes(archiveHex,blob)) {
        return nullptr;
    }

    std::vector<DecodedEntry> entries;
    if(!parseArchive(blob,entries,false)) {
        return nullptr;
    }

    auto out = StarbytesArrayNew();
    for(const auto &entry : entries) {
        StarbytesArrayPush(out,StarbytesStrNewWithData(entry.name.c_str()));
    }
    return out;
}

STARBYTES_FUNC(archive_isValid) {
    skipOptionalModuleReceiver(args,1);

    std::string archiveHex;
    if(!readStringArg(args,archiveHex)) {
        return makeBool(false);
    }

    std::vector<unsigned char> blob;
    if(!hexToBytes(archiveHex,blob)) {
        return makeBool(false);
    }

    std::vector<DecodedEntry> entries;
    return makeBool(parseArchive(blob,entries,false));
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

    addFunc(module,"archive_packTextMapHex",2,archive_packTextMapHex);
    addFunc(module,"archive_unpackTextMapHex",1,archive_unpackTextMapHex);
    addFunc(module,"archive_listEntries",1,archive_listEntries);
    addFunc(module,"archive_isValid",1,archive_isValid);

    return module;
}
