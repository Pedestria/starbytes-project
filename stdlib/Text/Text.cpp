#include <starbytes/interop.h>
#include "starbytes/base/ADT.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <string>
#include <vector>

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

#ifdef STARBYTES_HAS_ICU
#include <unicode/brkiter.h>
#include <unicode/locid.h>
#include <unicode/unistr.h>
#endif

namespace {

using starbytes::string_set;

struct NativeArgsLayout {
    unsigned argc = 0;
    unsigned index = 0;
    StarbytesObject *argv = nullptr;
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
    outValue = StarbytesStrGetBuffer(arg);
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

StarbytesObject stringListToArray(const std::vector<std::string> &values) {
    auto out = StarbytesArrayNew();
    for(const auto &value : values) {
        StarbytesArrayPush(out,StarbytesStrNewWithData(value.c_str()));
    }
    return out;
}

bool extractRegexPatternAndFlags(StarbytesObject regexObject,std::string &patternOut,std::string &flagsOut) {
    if(!regexObject || !StarbytesObjectTypecheck(regexObject,StarbytesRegexType())) {
        return false;
    }

    auto pattern = StarbytesObjectGetProperty(regexObject,"pattern");
    auto flags = StarbytesObjectGetProperty(regexObject,"flags");
    if(!pattern || !StarbytesObjectTypecheck(pattern,StarbytesStrType())) {
        return false;
    }

    patternOut = StarbytesStrGetBuffer(pattern);
    flagsOut = (flags && StarbytesObjectTypecheck(flags,StarbytesStrType())) ? StarbytesStrGetBuffer(flags) : "";
    return true;
}

uint32_t regexCompileOptionsFromFlags(const std::string &flags) {
    uint32_t options = 0;
    for(char flag : flags) {
        switch(flag) {
            case 'i':
                options |= PCRE2_CASELESS;
                break;
            case 'm':
                options |= PCRE2_MULTILINE;
                break;
            case 's':
                options |= PCRE2_DOTALL;
                break;
            case 'u':
                options |= PCRE2_UTF;
                break;
            default:
                break;
        }
    }
    return options;
}

pcre2_code *compileRegex(const std::string &pattern,const std::string &flags) {
    int errorCode = 0;
    PCRE2_SIZE errorOffset = 0;
    return pcre2_compile((PCRE2_SPTR)pattern.c_str(),
                         PCRE2_ZERO_TERMINATED,
                         regexCompileOptionsFromFlags(flags),
                         &errorCode,
                         &errorOffset,
                         nullptr);
}

std::string caseFoldAscii(const std::string &value) {
    std::string out = value;
    std::transform(out.begin(),out.end(),out.begin(),[](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

#ifdef STARBYTES_HAS_ICU
std::string caseFoldUnicode(const std::string &value) {
    auto text = icu::UnicodeString::fromUTF8(value);
    text.foldCase();
    std::string out;
    text.toUTF8String(out);
    return out;
}

std::vector<std::string> splitWithBreakIterator(const std::string &text,
                                                const std::string &localeName,
                                                bool wordsMode) {
    std::vector<std::string> out;

    UErrorCode status = U_ZERO_ERROR;
    std::unique_ptr<icu::BreakIterator> iterator;
    icu::Locale locale = localeName.empty() ? icu::Locale::getDefault() : icu::Locale(localeName.c_str());
    if(wordsMode) {
        iterator.reset(icu::BreakIterator::createWordInstance(locale,status));
    }
    else {
        iterator.reset(icu::BreakIterator::createLineInstance(locale,status));
    }
    if(U_FAILURE(status) || !iterator) {
        return out;
    }

    auto unicodeText = icu::UnicodeString::fromUTF8(text);
    iterator->setText(unicodeText);

    int32_t start = iterator->first();
    for(int32_t end = iterator->next(); end != icu::BreakIterator::DONE; start = end, end = iterator->next()) {
        auto segment = unicodeText.tempSubStringBetween(start,end);
        std::string utf8;
        segment.toUTF8String(utf8);
        if(wordsMode) {
            bool whitespaceOnly = std::all_of(utf8.begin(),utf8.end(),[](unsigned char c) {
                return std::isspace(c) != 0;
            });
            if(whitespaceOnly) {
                continue;
            }
        }
        if(!utf8.empty()) {
            out.push_back(std::move(utf8));
        }
    }
    return out;
}
#else
std::string caseFoldUnicode(const std::string &value) {
    return caseFoldAscii(value);
}

std::vector<std::string> splitWithBreakIterator(const std::string &text,
                                                const std::string &localeName,
                                                bool wordsMode) {
    (void)localeName;

    std::vector<std::string> out;
    if(wordsMode) {
        std::string current;
        for(unsigned char c : text) {
            if(std::isspace(c) != 0) {
                if(!current.empty()) {
                    out.push_back(current);
                    current.clear();
                }
            }
            else {
                current.push_back((char)c);
            }
        }
        if(!current.empty()) {
            out.push_back(current);
        }
        return out;
    }

    size_t start = 0;
    while(start < text.size()) {
        auto pos = text.find('\n',start);
        if(pos == std::string::npos) {
            out.push_back(text.substr(start));
            break;
        }
        out.push_back(text.substr(start,pos - start));
        start = pos + 1;
    }
    if(text.empty()) {
        out.push_back("");
    }
    return out;
}
#endif

STARBYTES_FUNC(text_regexMatch) {
    skipOptionalModuleReceiver(args,2);

    auto regexObject = StarbytesFuncArgsGetArg(args);
    std::string text;
    if(!readStringArg(args,text)) {
        return nullptr;
    }

    std::string pattern;
    std::string flags;
    if(!extractRegexPatternAndFlags(regexObject,pattern,flags)) {
        return nullptr;
    }

    auto *compiled = compileRegex(pattern,flags);
    if(!compiled) {
        return nullptr;
    }

    auto *matchData = pcre2_match_data_create_from_pattern(compiled,nullptr);
    auto matchRc = pcre2_match(compiled,
                               (PCRE2_SPTR)text.c_str(),
                               text.size(),
                               0,
                               0,
                               matchData,
                               nullptr);

    pcre2_match_data_free(matchData);
    pcre2_code_free(compiled);

    return makeBool(matchRc >= 0);
}

STARBYTES_FUNC(text_regexFindAll) {
    skipOptionalModuleReceiver(args,2);

    auto regexObject = StarbytesFuncArgsGetArg(args);
    std::string text;
    if(!readStringArg(args,text)) {
        return nullptr;
    }

    std::string pattern;
    std::string flags;
    if(!extractRegexPatternAndFlags(regexObject,pattern,flags)) {
        return nullptr;
    }

    auto *compiled = compileRegex(pattern,flags);
    if(!compiled) {
        return nullptr;
    }

    std::vector<std::string> matches;
    string_set dedupGuard;
    auto *matchData = pcre2_match_data_create_from_pattern(compiled,nullptr);

    size_t offset = 0;
    while(offset <= text.size()) {
        auto matchRc = pcre2_match(compiled,
                                   (PCRE2_SPTR)text.c_str(),
                                   text.size(),
                                   offset,
                                   0,
                                   matchData,
                                   nullptr);
        if(matchRc < 0) {
            break;
        }

        auto *ovector = pcre2_get_ovector_pointer(matchData);
        size_t start = ovector[0];
        size_t end = ovector[1];
        if(end < start || start > text.size() || end > text.size()) {
            break;
        }

        auto matchText = text.substr(start,end - start);
        if(dedupGuard.insert(matchText + "@" + std::to_string(start)).second) {
            matches.push_back(std::move(matchText));
        }

        if(end == offset) {
            ++offset;
        }
        else {
            offset = end;
        }
    }

    pcre2_match_data_free(matchData);
    pcre2_code_free(compiled);

    return stringListToArray(matches);
}

STARBYTES_FUNC(text_regexReplace) {
    skipOptionalModuleReceiver(args,3);

    auto regexObject = StarbytesFuncArgsGetArg(args);
    std::string text;
    std::string replacement;
    if(!readStringArg(args,text) || !readStringArg(args,replacement)) {
        return nullptr;
    }

    std::string pattern;
    std::string flags;
    if(!extractRegexPatternAndFlags(regexObject,pattern,flags)) {
        return nullptr;
    }

    auto *compiled = compileRegex(pattern,flags);
    if(!compiled) {
        return nullptr;
    }

    auto *matchData = pcre2_match_data_create_from_pattern(compiled,nullptr);

    std::string out;
    size_t offset = 0;
    size_t consumed = 0;
    while(offset <= text.size()) {
        auto matchRc = pcre2_match(compiled,
                                   (PCRE2_SPTR)text.c_str(),
                                   text.size(),
                                   offset,
                                   0,
                                   matchData,
                                   nullptr);
        if(matchRc < 0) {
            break;
        }

        auto *ovector = pcre2_get_ovector_pointer(matchData);
        size_t start = ovector[0];
        size_t end = ovector[1];
        if(end < start || start > text.size() || end > text.size()) {
            break;
        }

        out.append(text.substr(consumed,start - consumed));
        out.append(replacement);

        consumed = end;
        if(end == offset) {
            ++offset;
        }
        else {
            offset = end;
        }
    }

    out.append(text.substr(consumed));

    pcre2_match_data_free(matchData);
    pcre2_code_free(compiled);

    return StarbytesStrNewWithData(out.c_str());
}

STARBYTES_FUNC(text_caseFold) {
    skipOptionalModuleReceiver(args,2);

    std::string text;
    std::string locale;
    if(!readStringArg(args,text) || !readStringArg(args,locale)) {
        return nullptr;
    }

    auto folded = caseFoldUnicode(text);
    return StarbytesStrNewWithData(folded.c_str());
}

STARBYTES_FUNC(text_equalsFold) {
    skipOptionalModuleReceiver(args,3);

    std::string lhs;
    std::string rhs;
    std::string locale;
    if(!readStringArg(args,lhs) || !readStringArg(args,rhs) || !readStringArg(args,locale)) {
        return makeBool(false);
    }

    auto lhsFold = caseFoldUnicode(lhs);
    auto rhsFold = caseFoldUnicode(rhs);
    return makeBool(lhsFold == rhsFold);
}

STARBYTES_FUNC(text_words) {
    skipOptionalModuleReceiver(args,2);

    std::string text;
    std::string locale;
    if(!readStringArg(args,text) || !readStringArg(args,locale)) {
        return nullptr;
    }

    auto segments = splitWithBreakIterator(text,locale,true);
    return stringListToArray(segments);
}

STARBYTES_FUNC(text_lines) {
    skipOptionalModuleReceiver(args,2);

    std::string text;
    std::string locale;
    if(!readStringArg(args,text) || !readStringArg(args,locale)) {
        return nullptr;
    }

    auto segments = splitWithBreakIterator(text,locale,false);
    return stringListToArray(segments);
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

    addFunc(module,"text_regexMatch",2,text_regexMatch);
    addFunc(module,"text_regexFindAll",2,text_regexFindAll);
    addFunc(module,"text_regexReplace",3,text_regexReplace);
    addFunc(module,"text_caseFold",2,text_caseFold);
    addFunc(module,"text_equalsFold",3,text_equalsFold);
    addFunc(module,"text_words",2,text_words);
    addFunc(module,"text_lines",2,text_lines);

    return module;
}
