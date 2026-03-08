#include <starbytes/interop.h>
#include "starbytes/base/ADT.h"
#include "starbytes/runtime/NativeModuleSupport.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

#ifdef STARBYTES_HAS_ICU
#include <unicode/brkiter.h>
#include <unicode/locid.h>
#include <unicode/unistr.h>
#endif

namespace {

using starbytes::Runtime::stdlib::failNativeIfEmpty;
using starbytes::Runtime::stdlib::setNativeErrorIfEmpty;

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
        setNativeErrorIfEmpty(args,"expected String argument");
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

    addFunc(module,"text_caseFold",2,text_caseFold);
    addFunc(module,"text_equalsFold",3,text_equalsFold);
    addFunc(module,"text_words",2,text_words);
    addFunc(module,"text_lines",2,text_lines);

    return module;
}
