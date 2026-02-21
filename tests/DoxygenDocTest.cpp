#include "starbytes/base/DoxygenDoc.h"

#include <iostream>
#include <string>

namespace {

int fail(const std::string &message) {
    std::cerr << "DoxygenDocTest failure: " << message << '\n';
    return 1;
}

bool expect(bool condition, const std::string &message) {
    if(!condition) {
        std::cerr << "DoxygenDocTest assertion failed: " << message << '\n';
        return false;
    }
    return true;
}

bool contains(const std::string &value, const std::string &needle) {
    return value.find(needle) != std::string::npos;
}

bool testAdvancedTags() {
    const std::string raw = R"(Adds two values.
@brief Adds values with advanced docs.
@param lhs Left value
@param rhs Right value
continued rhs detail
@tparam T Numeric-like type.
@return Combined value.
@throws ErrorType On overflow.
@note Stable operation.
@warning Check bounds.
@see Math.add
@since 0.8.1
@deprecated Prefer addSafe.
@example add(1,2)
@code
let x = add(1,2)
@endcode
Additional details line.)";

    auto parsed = starbytes::DoxygenDoc::parse(raw);
    if(!expect(parsed.summary == "Adds values with advanced docs.","summary parse")) {
        return false;
    }
    if(!expect(parsed.params.size() == 2,"param count")) {
        return false;
    }
    if(!expect(parsed.params[1].name == "rhs","rhs param name")) {
        return false;
    }
    if(!expect(contains(parsed.params[1].description,"continued rhs detail"),"param continuation")) {
        return false;
    }
    if(!expect(parsed.tparams.size() == 1 && parsed.tparams[0].name == "T","tparam parse")) {
        return false;
    }
    if(!expect(parsed.throwsDocs.size() == 1 && parsed.throwsDocs[0].name == "ErrorType","throws parse")) {
        return false;
    }
    if(!expect(parsed.notes.size() == 1 && parsed.warnings.size() == 1 && parsed.sees.size() == 1,"note/warning/see parse")) {
        return false;
    }
    if(!expect(parsed.since == "0.8.1","since parse")) {
        return false;
    }
    if(!expect(parsed.deprecated == "Prefer addSafe.","deprecated parse")) {
        return false;
    }
    if(!expect(parsed.examples.size() == 2,"example count")) {
        return false;
    }

    auto markdown = parsed.renderMarkdown();
    return expect(contains(markdown,"**Type Parameters**"),"markdown tparam")
        && expect(contains(markdown,"**Throws**"),"markdown throws")
        && expect(contains(markdown,"**Notes**"),"markdown notes")
        && expect(contains(markdown,"**Warnings**"),"markdown warnings")
        && expect(contains(markdown,"**See Also**"),"markdown see")
        && expect(contains(markdown,"**Since**"),"markdown since")
        && expect(contains(markdown,"**Deprecated**"),"markdown deprecated")
        && expect(contains(markdown,"**Examples**"),"markdown examples")
        && expect(contains(markdown,"```text"),"markdown code block");
}

}

int main() {
    if(!testAdvancedTags()) {
        return fail("testAdvancedTags");
    }
    return 0;
}
