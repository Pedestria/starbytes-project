#include "RTValue.h"

#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <limits>
#include <sstream>

namespace starbytes::Runtime {

namespace {

bool utf8ByteOffsetForScalarIndex(const std::string &text, int scalarIndex, size_t &byteOffsetOut){
    if(scalarIndex < 0){
        return false;
    }
    size_t offset = 0;
    int current = 0;
    while(offset < text.size()){
        if(current == scalarIndex){
            byteOffsetOut = offset;
            return true;
        }
        auto width = utf8ScalarWidth(text, offset);
        if(width == 0){
            return false;
        }
        offset += width;
        ++current;
    }
    if(current == scalarIndex){
        byteOffsetOut = offset;
        return true;
    }
    return false;
}

}

std::string stringTrim(const std::string &input){
    size_t begin = 0;
    while(begin < input.size() && std::isspace((unsigned char)input[begin])){
        ++begin;
    }
    size_t end = input.size();
    while(end > begin && std::isspace((unsigned char)input[end - 1])){
        --end;
    }
    return input.substr(begin, end - begin);
}

int utf8ScalarCount(const std::string &text){
    int count = 0;
    size_t offset = 0;
    while(offset < text.size()){
        auto width = utf8ScalarWidth(text, offset);
        if(width == 0){
            break;
        }
        offset += width;
        ++count;
    }
    return count;
}

std::string objectToString(StarbytesObject object){
    if(!object){
        return "null";
    }
    if(StarbytesObjectTypecheck(object, StarbytesStrType())){
        return StarbytesStrGetBuffer(object);
    }
    if(StarbytesObjectTypecheck(object, StarbytesBoolType())){
        return ((bool)StarbytesBoolValue(object)) ? "true" : "false";
    }
    if(StarbytesObjectTypecheck(object, StarbytesNumType())){
        auto numType = StarbytesNumGetType(object);
        if(numType == NumTypeFloat){
            std::ostringstream out;
            out << StarbytesNumGetFloatValue(object);
            return out.str();
        }
        if(numType == NumTypeDouble){
            std::ostringstream out;
            out << StarbytesNumGetDoubleValue(object);
            return out.str();
        }
        if(numType == NumTypeLong){
            return std::to_string(StarbytesNumGetLongValue(object));
        }
        return std::to_string(StarbytesNumGetIntValue(object));
    }
    if(StarbytesObjectTypecheck(object, StarbytesRegexType())){
        auto pattern = StarbytesObjectGetProperty(object, "pattern");
        auto flags = StarbytesObjectGetProperty(object, "flags");
        std::string p = pattern ? std::string(StarbytesStrGetBuffer(pattern)) : "";
        std::string f = flags ? std::string(StarbytesStrGetBuffer(flags)) : "";
        return "/" + p + "/" + f;
    }
    return "<object>";
}

bool runtimeObjectEquals(StarbytesObject lhs, StarbytesObject rhs){
    if(!lhs || !rhs){
        return lhs == rhs;
    }
    if(StarbytesObjectTypecheck(lhs, StarbytesNumType()) && StarbytesObjectTypecheck(rhs, StarbytesNumType())){
        return StarbytesNumCompare(lhs, rhs) == COMPARE_EQUAL;
    }
    if(StarbytesObjectTypecheck(lhs, StarbytesStrType()) && StarbytesObjectTypecheck(rhs, StarbytesStrType())){
        return StarbytesStrCompare(lhs, rhs) == COMPARE_EQUAL;
    }
    if(StarbytesObjectTypecheck(lhs, StarbytesBoolType()) && StarbytesObjectTypecheck(rhs, StarbytesBoolType())){
        return (bool)StarbytesBoolValue(lhs) == (bool)StarbytesBoolValue(rhs);
    }
    return lhs == rhs;
}

bool isDictKeyObject(StarbytesObject key){
    return key && (StarbytesObjectTypecheck(key, StarbytesStrType()) || StarbytesObjectTypecheck(key, StarbytesNumType()));
}

int clampSliceBound(int bound, int len){
    if(bound < 0){
        bound = len + bound;
    }
    if(bound < 0){
        return 0;
    }
    if(bound > len){
        return len;
    }
    return bound;
}

bool parseIntStrict(const std::string &text, int &outValue){
    auto trimmed = stringTrim(text);
    if(trimmed.empty()){
        return false;
    }
    errno = 0;
    char *endPtr = nullptr;
    long parsed = std::strtol(trimmed.c_str(), &endPtr, 10);
    if(errno != 0 || endPtr == nullptr || *endPtr != '\0'){
        return false;
    }
    if(parsed < std::numeric_limits<int>::min() || parsed > std::numeric_limits<int>::max()){
        return false;
    }
    outValue = (int)parsed;
    return true;
}

bool parseFloatStrict(const std::string &text, float &outValue){
    auto trimmed = stringTrim(text);
    if(trimmed.empty()){
        return false;
    }
    errno = 0;
    char *endPtr = nullptr;
    float parsed = std::strtof(trimmed.c_str(), &endPtr);
    if(errno != 0 || endPtr == nullptr || *endPtr != '\0'){
        return false;
    }
    outValue = parsed;
    return true;
}

bool parseDoubleStrict(const std::string &text, double &outValue){
    auto trimmed = stringTrim(text);
    if(trimmed.empty()){
        return false;
    }
    errno = 0;
    char *endPtr = nullptr;
    double parsed = std::strtod(trimmed.c_str(), &endPtr);
    if(errno != 0 || endPtr == nullptr || *endPtr != '\0'){
        return false;
    }
    outValue = parsed;
    return true;
}

size_t utf8ScalarWidth(const std::string &text, size_t byteOffset){
    if(byteOffset >= text.size()){
        return 0;
    }
    auto lead = (unsigned char)text[byteOffset];
    if((lead & 0x80u) == 0x00u){
        return 1;
    }
    if((lead & 0xE0u) == 0xC0u && byteOffset + 1 < text.size()){
        return 2;
    }
    if((lead & 0xF0u) == 0xE0u && byteOffset + 2 < text.size()){
        return 3;
    }
    if((lead & 0xF8u) == 0xF0u && byteOffset + 3 < text.size()){
        return 4;
    }
    return 1;
}

bool utf8ScalarSlice(const std::string &text, int startScalar, int endScalar, std::string &out){
    int scalarLength = utf8ScalarCount(text);
    startScalar = clampSliceBound(startScalar, scalarLength);
    endScalar = clampSliceBound(endScalar, scalarLength);
    if(endScalar < startScalar){
        endScalar = startScalar;
    }
    size_t startByte = 0;
    size_t endByte = 0;
    if(!utf8ByteOffsetForScalarIndex(text, startScalar, startByte)
       || !utf8ByteOffsetForScalarIndex(text, endScalar, endByte)
       || endByte < startByte){
        return false;
    }
    out = text.substr(startByte, endByte - startByte);
    return true;
}

bool utf8ScalarAt(const std::string &text, int scalarIndex, std::string &out){
    size_t startByte = 0;
    if(!utf8ByteOffsetForScalarIndex(text, scalarIndex, startByte) || startByte >= text.size()){
        return false;
    }
    auto width = utf8ScalarWidth(text, startByte);
    if(width == 0 || startByte + width > text.size()){
        return false;
    }
    out = text.substr(startByte, width);
    return true;
}

int utf8ScalarIndexForByteOffset(const std::string &text, size_t byteOffset){
    if(byteOffset == std::string::npos || byteOffset > text.size()){
        return -1;
    }
    int scalarIndex = 0;
    size_t offset = 0;
    while(offset < byteOffset && offset < text.size()){
        auto width = utf8ScalarWidth(text, offset);
        if(width == 0){
            return -1;
        }
        offset += width;
        ++scalarIndex;
    }
    return (offset == byteOffset) ? scalarIndex : -1;
}

}
