#ifndef STARBYTES_RUNTIME_NATIVE_MODULE_SUPPORT_H
#define STARBYTES_RUNTIME_NATIVE_MODULE_SUPPORT_H

#include "starbytes/interop.h"

#include <cerrno>
#include <cstring>
#include <string>
#include <system_error>

namespace starbytes::Runtime::stdlib {

inline void setNativeError(StarbytesFuncArgs args,const std::string &message) {
    StarbytesFuncArgsSetError(args,message.c_str());
}

inline void setNativeErrorIfEmpty(StarbytesFuncArgs args,const std::string &message) {
    auto *existing = StarbytesFuncArgsGetError(args);
    if(existing != nullptr && existing[0] != '\0') {
        return;
    }
    StarbytesFuncArgsSetError(args,message.c_str());
}

inline StarbytesObject failNative(StarbytesFuncArgs args,const std::string &message) {
    setNativeError(args,message);
    return nullptr;
}

inline StarbytesObject failNativeIfEmpty(StarbytesFuncArgs args,const std::string &message) {
    setNativeErrorIfEmpty(args,message);
    return nullptr;
}

inline std::string systemErrorMessage(const std::string &context,const std::error_code &error) {
    if(error) {
        return context + ": " + error.message();
    }
    return context;
}

inline std::string errnoMessage(const std::string &context) {
    const int currentErrno = errno;
    if(currentErrno == 0) {
        return context;
    }
    return context + ": " + std::strerror(currentErrno);
}

}

#endif
