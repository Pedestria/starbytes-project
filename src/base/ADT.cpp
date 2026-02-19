#include "starbytes/base/ADT.h"

#include <algorithm>

namespace starbytes {

static uint32_t clampTwineSize(std::size_t size) {
    constexpr std::size_t maxSize = static_cast<std::size_t>(static_cast<uint32_t>(-1));
    return static_cast<uint32_t>(std::min(size, maxSize));
}

Twine::Twine(const char *other) {
    append(other);
}

Twine::Twine(const StringRef &other) {
    append(other);
}

Twine::Twine(const std::string &other):
buffer(other) {}

Twine &Twine::append(const char *other) {
    if(other) {
        buffer.append(other);
    }
    return *this;
}

Twine &Twine::append(const StringRef &other) {
    if(other.size() > 0 && other.data()) {
        buffer.append(other.data(), other.size());
    }
    return *this;
}

Twine &Twine::append(const std::string &other) {
    buffer.append(other);
    return *this;
}

Twine &Twine::operator+(const char *other) {
    return append(other);
}

Twine &Twine::operator+(const StringRef &other) {
    return append(other);
}

Twine &Twine::operator+(const std::string &other) {
    return append(other);
}

bool Twine::empty() const noexcept {
    return buffer.empty();
}

Twine::size_type Twine::size() const noexcept {
    return clampTwineSize(buffer.size());
}

const char *Twine::c_str() const noexcept {
    return buffer.c_str();
}

const std::string &Twine::str() const noexcept {
    return buffer;
}

void Twine::clear() noexcept {
    buffer.clear();
}

StringRef StringInterner::intern(const StringRef &value) {
    auto inserted = pool.emplace(value.str());
    return StringRef(inserted.first->data(), static_cast<uint32_t>(inserted.first->size()));
}

StringRef StringInterner::intern(const std::string &value) {
    auto inserted = pool.emplace(value);
    return StringRef(inserted.first->data(), static_cast<uint32_t>(inserted.first->size()));
}

StringRef StringInterner::intern(const char *value) {
    if(!value) {
        return StringRef();
    }
    auto inserted = pool.emplace(value);
    return StringRef(inserted.first->data(), static_cast<uint32_t>(inserted.first->size()));
}

size_t StringInterner::size() const noexcept {
    return pool.size();
}

void StringInterner::clear() noexcept {
    pool.clear();
}

}
