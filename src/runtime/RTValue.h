#ifndef STARBYTES_RT_RTVALUE_H
#define STARBYTES_RT_RTVALUE_H

#include "starbytes/interop.h"

#include <cstddef>
#include <string>

namespace starbytes::Runtime {

// Object-level value helpers
std::string objectToString(StarbytesObject object);
bool        runtimeObjectEquals(StarbytesObject lhs, StarbytesObject rhs);
bool        isDictKeyObject(StarbytesObject key);

// Index/slice helpers
int clampSliceBound(int bound, int len);

// Parsing
std::string stringTrim(const std::string &input);
bool parseIntStrict(const std::string &text, int &outValue);
bool parseFloatStrict(const std::string &text, float &outValue);
bool parseDoubleStrict(const std::string &text, double &outValue);

// UTF-8 string helpers (operating on UTF-8-encoded std::string)
size_t utf8ScalarWidth(const std::string &text, size_t byteOffset);
int    utf8ScalarCount(const std::string &text);
bool   utf8ScalarSlice(const std::string &text, int startScalar, int endScalar, std::string &out);
bool   utf8ScalarAt(const std::string &text, int scalarIndex, std::string &out);
int    utf8ScalarIndexForByteOffset(const std::string &text, size_t byteOffset);

}

#endif
