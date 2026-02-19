#ifndef STARBYTES_BASE_ADT_H
#define STARBYTES_BASE_ADT_H

#include <algorithm>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <ostream>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace starbytes {

template<typename T>
class StringRefT {
public:
    using value_type = T;
    using pointer = const T *;
    using const_pointer = const T *;
    using iterator = const T *;
    using const_iterator = const T *;
    using size_type = uint32_t;
    static constexpr size_type npos = static_cast<size_type>(-1);

private:
    const_pointer buffer = nullptr;
    size_type len = 0;

    static size_type clampSize(std::size_t size) {
        constexpr std::size_t maxSize = static_cast<std::size_t>(static_cast<size_type>(-1));
        return static_cast<size_type>(std::min(size, maxSize));
    }

    static size_type charLength(const_pointer str) {
        if(!str) {
            return 0;
        }
        return clampSize(std::char_traits<T>::length(str));
    }

    bool equalsRaw(const_pointer other, size_type otherLen) const {
        if(len != otherLen) {
            return false;
        }
        if(len == 0) {
            return true;
        }
        if(!buffer || !other) {
            return false;
        }
        return std::char_traits<T>::compare(buffer, other, len) == 0;
    }

public:
    constexpr StringRefT() noexcept = default;

    StringRefT(const_pointer str): buffer(str), len(charLength(str)) {}

    constexpr StringRefT(const_pointer str, size_type size) noexcept:
    buffer(str),
    len(str ? size : 0) {}

    StringRefT(const std::basic_string<T> &str):
    buffer(str.data()),
    len(clampSize(str.size())) {}

    iterator begin() const noexcept {
        return buffer;
    }

    iterator end() const noexcept {
        return buffer ? (buffer + len) : buffer;
    }

    bool empty() const noexcept {
        return len == 0;
    }

    size_type size() const noexcept {
        return len;
    }

    const_pointer data() const noexcept {
        return buffer;
    }

    const_pointer getBuffer() const noexcept {
        return buffer;
    }

    const T &operator[](size_type index) const {
        return buffer[index];
    }

    bool equals(const StringRefT &str) const {
        return equalsRaw(str.buffer, str.len);
    }

    bool equals(const std::basic_string<T> &str) const {
        return equalsRaw(str.data(), clampSize(str.size()));
    }

    bool equals(const_pointer str) const {
        return equalsRaw(str, charLength(str));
    }

    int compare(const StringRefT &other) const {
        const size_type minLen = std::min(len, other.len);
        if(minLen > 0 && buffer && other.buffer) {
            const int cmp = std::char_traits<T>::compare(buffer, other.buffer, minLen);
            if(cmp != 0) {
                return cmp;
            }
        }
        if(len == other.len) {
            return 0;
        }
        return len < other.len ? -1 : 1;
    }

    bool startsWith(const StringRefT &prefix) const {
        if(prefix.len > len) {
            return false;
        }
        if(prefix.len == 0) {
            return true;
        }
        if(!buffer || !prefix.buffer) {
            return false;
        }
        return std::char_traits<T>::compare(buffer, prefix.buffer, prefix.len) == 0;
    }

    bool endsWith(const StringRefT &suffix) const {
        if(suffix.len > len) {
            return false;
        }
        if(suffix.len == 0) {
            return true;
        }
        if(!buffer || !suffix.buffer) {
            return false;
        }
        return std::char_traits<T>::compare(buffer + (len - suffix.len), suffix.buffer, suffix.len) == 0;
    }

    size_type find(T ch, size_type start = 0) const {
        if(start >= len || !buffer) {
            return npos;
        }
        const auto *it = std::find(buffer + start, buffer + len, ch);
        if(it == (buffer + len)) {
            return npos;
        }
        return static_cast<size_type>(it - buffer);
    }

    StringRefT substr_ref(size_type start, size_type endExclusive) const {
        if(start >= len || start >= endExclusive || !buffer) {
            return StringRefT();
        }
        const size_type clampedEnd = std::min(endExclusive, len);
        return StringRefT(buffer + start, clampedEnd - start);
    }

    StringRefT slice(size_type start, size_type count) const {
        if(start >= len || !buffer) {
            return StringRefT();
        }
        const size_type clampedCount = std::min(count, static_cast<size_type>(len - start));
        return StringRefT(buffer + start, clampedCount);
    }

    std::basic_string<T> str() const {
        if(empty() || !buffer) {
            return std::basic_string<T>();
        }
        return std::basic_string<T>(buffer, len);
    }

    std::basic_string_view<T> view() const noexcept {
        if(!buffer || len == 0) {
            return std::basic_string_view<T>();
        }
        return std::basic_string_view<T>(buffer, len);
    }

    operator std::basic_string<T>() const {
        return str();
    }
};

template<typename T>
inline std::basic_ostream<T> &operator<<(std::basic_ostream<T> &out, const StringRefT<T> &str) {
    if(str.size() > 0 && str.data()) {
        out.write(str.data(), str.size());
    }
    return out;
}

template<typename T>
inline bool operator==(const StringRefT<T> &lhs, const std::basic_string<T> &rhs) {
    return lhs.equals(rhs);
}

template<typename T>
inline bool operator==(const std::basic_string<T> &lhs, const StringRefT<T> &rhs) {
    return rhs.equals(lhs);
}

template<typename T>
inline bool operator==(const StringRefT<T> &lhs, const StringRefT<T> &rhs) {
    return lhs.equals(rhs);
}

template<typename T>
inline bool operator==(const StringRefT<T> &lhs, const T *rhs) {
    return lhs.equals(rhs);
}

template<typename T>
inline bool operator==(const T *lhs, const StringRefT<T> &rhs) {
    return rhs.equals(lhs);
}

template<typename T>
inline bool operator<(const StringRefT<T> &lhs, const StringRefT<T> &rhs) {
    return lhs.compare(rhs) < 0;
}

template<typename T>
inline bool operator<(const StringRefT<T> &lhs, const std::basic_string<T> &rhs) {
    return lhs.compare(StringRefT<T>(rhs)) < 0;
}

template<typename T>
inline bool operator<(const std::basic_string<T> &lhs, const StringRefT<T> &rhs) {
    return StringRefT<T>(lhs).compare(rhs) < 0;
}

using StringRef = StringRefT<char>;
using WStringRef = StringRefT<wchar_t>;
template<typename T>
using string_ref_t = StringRefT<T>;

using string_ref = StringRef;
using wstring_ref = WStringRef;

template<typename T>
class ArrayRef {
public:
    using value_type = T;
    using pointer = T *;
    using const_pointer = const T *;
    using iterator = T *;
    using const_iterator = const T *;
    using size_type = uint32_t;

private:
    pointer buffer = nullptr;
    size_type len = 0;

    static size_type clampSize(std::size_t size) {
        constexpr std::size_t maxSize = static_cast<std::size_t>(static_cast<size_type>(-1));
        return static_cast<size_type>(std::min(size, maxSize));
    }

public:
    constexpr ArrayRef() noexcept = default;

    constexpr ArrayRef(pointer buf, size_type length) noexcept:
    buffer(buf),
    len(buf ? length : 0) {}

    ArrayRef(iterator beginIt, iterator endIt) noexcept:
    buffer(beginIt),
    len((beginIt && endIt && endIt >= beginIt) ? static_cast<size_type>(endIt - beginIt) : 0) {}

    ArrayRef(std::vector<T> &vec) noexcept:
    buffer(vec.data()),
    len(clampSize(vec.size())) {}

    iterator begin() noexcept {
        return buffer;
    }

    iterator end() noexcept {
        return buffer ? (buffer + len) : buffer;
    }

    const_iterator begin() const noexcept {
        return buffer;
    }

    const_iterator end() const noexcept {
        return buffer ? (buffer + len) : buffer;
    }

    bool empty() const noexcept {
        return len == 0;
    }

    size_type size() const noexcept {
        return len;
    }

    pointer data() noexcept {
        return buffer;
    }

    const_pointer data() const noexcept {
        return buffer;
    }

    T &operator[](size_type index) {
        return buffer[index];
    }

    const T &operator[](size_type index) const {
        return buffer[index];
    }

    ArrayRef slice(size_type start, size_type count) const noexcept {
        if(start >= len || !buffer) {
            return ArrayRef();
        }
        const size_type clampedCount = std::min(count, static_cast<size_type>(len - start));
        return ArrayRef(buffer + start, clampedCount);
    }
};

template<typename T>
using array_ref = ArrayRef<T>;

class Twine {
public:
    using size_type = uint32_t;

private:
    std::string buffer;

public:
    Twine() = default;
    explicit Twine(const char *other);
    explicit Twine(const StringRef &other);
    explicit Twine(const std::string &other);

    Twine &append(const char *other);
    Twine &append(const StringRef &other);
    Twine &append(const std::string &other);

    Twine &operator+(const char *other);
    Twine &operator+(const StringRef &other);
    Twine &operator+(const std::string &other);

    bool empty() const noexcept;
    size_type size() const noexcept;
    const char *c_str() const noexcept;
    const std::string &str() const noexcept;
    void clear() noexcept;
};

using twine = Twine;

template<typename T1, typename T2>
using map = std::map<T1, T2>;

template<typename T>
using string_map = std::map<std::string, T, std::less<>>;

using string_set = std::set<std::string, std::less<>>;

class StringInterner {
    string_set pool;
public:
    StringInterner() = default;
    StringRef intern(const StringRef &value);
    StringRef intern(const std::string &value);
    StringRef intern(const char *value);
    size_t size() const noexcept;
    void clear() noexcept;
};

template<typename T>
using optional = std::optional<T>;

}

#endif
