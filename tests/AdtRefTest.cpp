#include "starbytes/base/ADT.h"

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

int fail(const std::string &message) {
    std::cerr << "AdtRefTest failure: " << message << '\n';
    return 1;
}

}

int main() {
    using namespace starbytes;

    StringRef full("Starbytes");
    if(full.size() != 9 || !full.equals("Starbytes")) {
        return fail("StringRef basic construction/equality failed");
    }

    if(full.substr_ref(0, 4).str() != "Star") {
        return fail("StringRef prefix slice mismatch");
    }
    if(full.substr_ref(4, full.size()).str() != "bytes") {
        return fail("StringRef suffix slice mismatch");
    }
    if(!full.substr_ref(99, 100).empty()) {
        return fail("StringRef out-of-range slice should be empty");
    }

    const char rawChars[] = {'A', '\0', 'B', 'C'};
    StringRef raw(rawChars, 4);
    auto rawCopy = raw.str();
    if(rawCopy.size() != 4 || rawCopy[0] != 'A' || rawCopy[1] != '\0' || rawCopy[2] != 'B' || rawCopy[3] != 'C') {
        return fail("StringRef sized construction should preserve embedded null bytes");
    }
    std::ostringstream rawOut;
    rawOut << raw;
    if(rawOut.str().size() != 4) {
        return fail("StringRef stream output should preserve explicit size");
    }

    std::vector<int> values {1, 2, 3, 4, 5};
    ArrayRef<int> arr(values);
    arr[1] = 8;
    if(values[1] != 8) {
        return fail("ArrayRef should reference backing storage");
    }
    auto tail = arr.slice(2, 10);
    if(tail.size() != 3 || tail[0] != 3 || tail[1] != 4 || tail[2] != 5) {
        return fail("ArrayRef slice clamp behavior failed");
    }

    Twine tw;
    tw + "Hello" + StringRef(", ") + std::string("World");
    if(tw.str() != "Hello, World") {
        return fail("Twine concatenation failed");
    }
    if(tw.size() != 12) {
        return fail("Twine size mismatch");
    }

    string_ref legacyRef("legacy");
    if(!legacyRef.equals("legacy")) {
        return fail("Legacy string_ref alias broke");
    }
    array_ref<int> legacyArr(values.data(), 2);
    if(legacyArr.size() != 2 || legacyArr[0] != 1 || legacyArr[1] != 8) {
        return fail("Legacy array_ref alias broke");
    }
    twine legacyTw;
    legacyTw + "ok";
    if(legacyTw.str() != "ok") {
        return fail("Legacy twine alias broke");
    }

    return 0;
}
