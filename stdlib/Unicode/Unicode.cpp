#include <starbytes/interop.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef STARBYTES_HAS_ICU
#include <unicode/brkiter.h>
#include <unicode/coll.h>
#include <unicode/locid.h>
#include <unicode/normalizer2.h>
#include <unicode/uchar.h>
#include <unicode/unistr.h>
#include <unicode/uscript.h>
#include <unicode/utf16.h>
#endif

namespace {

struct NativeArgsLayout {
    unsigned argc = 0;
    unsigned index = 0;
    StarbytesObject *argv = nullptr;
};

std::unordered_map<StarbytesObject,std::string> g_localeRegistry;

#ifdef STARBYTES_HAS_ICU
struct CollatorState {
    std::unique_ptr<icu::Collator> collator;
};

struct BreakIterState {
    std::unique_ptr<icu::BreakIterator> iterator;
    icu::UnicodeString text;
};

std::unordered_map<StarbytesObject,std::unique_ptr<CollatorState>> g_collatorRegistry;
std::unordered_map<StarbytesObject,std::unique_ptr<BreakIterState>> g_breakRegistry;
#endif

std::unordered_map<StarbytesObject,int32_t> g_scalarInfoRegistry;

StarbytesObject makeBool(bool value) {
    // Runtime bool consumption currently interprets StarbytesBoolFalse as logical true.
    return StarbytesBoolNew(value ? StarbytesBoolFalse : StarbytesBoolTrue);
}

StarbytesObject makeInt(int value) {
    return StarbytesNumNew(NumTypeInt,value);
}

StarbytesObject makeString(const std::string &value) {
    return StarbytesStrNewWithData(value.c_str());
}

bool readStringArg(StarbytesFuncArgs args,std::string &outValue) {
    auto arg = StarbytesFuncArgsGetArg(args);
    if(!arg || !StarbytesObjectTypecheck(arg,StarbytesStrType())) {
        return false;
    }
    outValue = StarbytesStrGetBuffer(arg);
    return true;
}

bool readIntArg(StarbytesFuncArgs args,int &outValue) {
    auto arg = StarbytesFuncArgsGetArg(args);
    if(!arg || !StarbytesObjectTypecheck(arg,StarbytesNumType()) || StarbytesNumGetType(arg) != NumTypeInt) {
        return false;
    }
    outValue = StarbytesNumGetIntValue(arg);
    return true;
}

bool readLocaleArg(StarbytesFuncArgs args,std::string &outLocaleId) {
    auto localeObj = StarbytesFuncArgsGetArg(args);
    if(!localeObj) {
        return false;
    }
    auto it = g_localeRegistry.find(localeObj);
    if(it == g_localeRegistry.end()) {
        return false;
    }
    outLocaleId = it->second;
    return true;
}

bool validNormalizationForm(int form) {
    return form >= 0 && form <= 3;
}

bool validBreakKind(int kind) {
    return kind >= 0 && kind <= 3;
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

StarbytesObject makeIntArray(const std::vector<int> &values) {
    auto out = StarbytesArrayNew();
    for(auto value : values) {
        StarbytesArrayPush(out,makeInt(value));
    }
    return out;
}

StarbytesObject makeStringArray(const std::vector<std::string> &values) {
    auto out = StarbytesArrayNew();
    for(const auto &value : values) {
        StarbytesArrayPush(out,makeString(value));
    }
    return out;
}

std::string localeIdFromObject(StarbytesObject localeObject) {
    if(!localeObject) {
        return "en_US";
    }
    auto it = g_localeRegistry.find(localeObject);
    if(it == g_localeRegistry.end()) {
        return "en_US";
    }
    return it->second;
}

StarbytesObject makeLocaleObject(const std::string &localeId) {
    auto localeObject = StarbytesObjectNew(StarbytesMakeClass("Locale"));
    g_localeRegistry[localeObject] = localeId;
    return localeObject;
}

#ifdef STARBYTES_HAS_ICU
icu::UnicodeString toUnicode(const std::string &value) {
    return icu::UnicodeString::fromUTF8(value);
}

std::string toUtf8(const icu::UnicodeString &value) {
    std::string out;
    value.toUTF8String(out);
    return out;
}

const icu::Normalizer2 *normalizerForForm(int form,UErrorCode &status) {
    switch(form) {
        case 0:
            return icu::Normalizer2::getNFCInstance(status);
        case 1:
            return icu::Normalizer2::getNFDInstance(status);
        case 2:
            return icu::Normalizer2::getNFKCInstance(status);
        case 3:
            return icu::Normalizer2::getNFKDInstance(status);
        default:
            return nullptr;
    }
}

icu::Locale localeFromId(const std::string &localeId) {
    if(localeId.empty() || localeId == "root") {
        return icu::Locale::getRoot();
    }
    return icu::Locale(localeId.c_str());
}

static bool isScalarValid(int cp) {
    return cp >= 0 && cp <= 0x10FFFF && !(cp >= 0xD800 && cp <= 0xDFFF);
}

std::string scalarCategoryName(int32_t cp) {
    switch(u_charType(cp)) {
        case U_UPPERCASE_LETTER: return "Uppercase_Letter";
        case U_LOWERCASE_LETTER: return "Lowercase_Letter";
        case U_TITLECASE_LETTER: return "Titlecase_Letter";
        case U_MODIFIER_LETTER: return "Modifier_Letter";
        case U_OTHER_LETTER: return "Other_Letter";
        case U_NON_SPACING_MARK: return "Nonspacing_Mark";
        case U_ENCLOSING_MARK: return "Enclosing_Mark";
        case U_COMBINING_SPACING_MARK: return "Spacing_Mark";
        case U_DECIMAL_DIGIT_NUMBER: return "Decimal_Number";
        case U_LETTER_NUMBER: return "Letter_Number";
        case U_OTHER_NUMBER: return "Other_Number";
        case U_SPACE_SEPARATOR: return "Space_Separator";
        case U_LINE_SEPARATOR: return "Line_Separator";
        case U_PARAGRAPH_SEPARATOR: return "Paragraph_Separator";
        case U_CONTROL_CHAR: return "Control";
        case U_FORMAT_CHAR: return "Format";
        case U_PRIVATE_USE_CHAR: return "Private_Use";
        case U_SURROGATE: return "Surrogate";
        case U_DASH_PUNCTUATION: return "Dash_Punctuation";
        case U_START_PUNCTUATION: return "Open_Punctuation";
        case U_END_PUNCTUATION: return "Close_Punctuation";
        case U_CONNECTOR_PUNCTUATION: return "Connector_Punctuation";
        case U_OTHER_PUNCTUATION: return "Other_Punctuation";
        case U_MATH_SYMBOL: return "Math_Symbol";
        case U_CURRENCY_SYMBOL: return "Currency_Symbol";
        case U_MODIFIER_SYMBOL: return "Modifier_Symbol";
        case U_OTHER_SYMBOL: return "Other_Symbol";
        case U_INITIAL_PUNCTUATION: return "Initial_Punctuation";
        case U_FINAL_PUNCTUATION: return "Final_Punctuation";
        default: return "Unassigned";
    }
}

std::string foldedUtf8(const std::string &value) {
    auto u = toUnicode(value);
    u.foldCase();
    return toUtf8(u);
}

bool whitespaceOnly(const std::string &value) {
    return std::all_of(value.begin(),value.end(),[](unsigned char c) {
        return std::isspace(c) != 0;
    });
}

std::vector<std::string> splitWithBreakIterator(const std::string &text,
                                                const std::string &localeId,
                                                int kind,
                                                bool skipWhitespace) {
    std::vector<std::string> out;
    UErrorCode status = U_ZERO_ERROR;
    std::unique_ptr<icu::BreakIterator> iterator;
    auto locale = localeFromId(localeId);

    switch(kind) {
        case 3:
            iterator.reset(icu::BreakIterator::createCharacterInstance(locale,status));
            break;
        case 1:
            iterator.reset(icu::BreakIterator::createSentenceInstance(locale,status));
            break;
        case 2:
            iterator.reset(icu::BreakIterator::createLineInstance(locale,status));
            break;
        default:
            iterator.reset(icu::BreakIterator::createWordInstance(locale,status));
            break;
    }
    if(U_FAILURE(status) || !iterator) {
        return out;
    }

    auto unicodeText = toUnicode(text);
    iterator->setText(unicodeText);

    int32_t start = iterator->first();
    for(int32_t end = iterator->next(); end != icu::BreakIterator::DONE; start = end, end = iterator->next()) {
        auto segment = unicodeText.tempSubStringBetween(start,end);
        auto utf8 = toUtf8(segment);
        if(skipWhitespace && whitespaceOnly(utf8)) {
            continue;
        }
        if(utf8.empty()) {
            continue;
        }
        out.push_back(std::move(utf8));
    }
    return out;
}

#else
std::string foldedUtf8(const std::string &value) {
    std::string out = value;
    std::transform(out.begin(),out.end(),out.begin(),[](unsigned char c) {
        return (char)std::tolower(c);
    });
    return out;
}
#endif

STARBYTES_FUNC(Unicode_Locale_current) {
    skipOptionalModuleReceiver(args,0);

#ifdef STARBYTES_HAS_ICU
    return makeLocaleObject(icu::Locale::getDefault().getName());
#else
    return makeLocaleObject("en_US");
#endif
}

STARBYTES_FUNC(Unicode_Locale_root) {
    skipOptionalModuleReceiver(args,0);
    return makeLocaleObject("root");
}

STARBYTES_FUNC(Unicode_Locale_from) {
    skipOptionalModuleReceiver(args,1);

    std::string identifier;
    if(!readStringArg(args,identifier) || identifier.empty()) {
        return nullptr;
    }
    return makeLocaleObject(identifier);
}

STARBYTES_FUNC(Unicode_Locale_id) {
    auto self = StarbytesFuncArgsGetArg(args);
    return makeString(localeIdFromObject(self));
}

STARBYTES_FUNC(Unicode_Collator_create) {
    skipOptionalModuleReceiver(args,1);

    std::string localeId;
    if(!readLocaleArg(args,localeId)) {
        return nullptr;
    }

#ifdef STARBYTES_HAS_ICU
    UErrorCode status = U_ZERO_ERROR;
    auto collator = std::unique_ptr<icu::Collator>(icu::Collator::createInstance(localeFromId(localeId),status));
    if(U_FAILURE(status) || !collator) {
        return nullptr;
    }
#endif

    auto object = StarbytesObjectNew(StarbytesMakeClass("Collator"));
#ifdef STARBYTES_HAS_ICU
    auto state = std::make_unique<CollatorState>();
    state->collator = std::move(collator);
    g_collatorRegistry[object] = std::move(state);
#else
    (void)localeId;
#endif
    return object;
}

STARBYTES_FUNC(Unicode_Collator_compare) {
    auto self = StarbytesFuncArgsGetArg(args);
    std::string lhs;
    std::string rhs;
    if(!readStringArg(args,lhs) || !readStringArg(args,rhs)) {
        return nullptr;
    }

#ifdef STARBYTES_HAS_ICU
    auto it = g_collatorRegistry.find(self);
    if(it != g_collatorRegistry.end() && it->second && it->second->collator) {
        UErrorCode status = U_ZERO_ERROR;
        auto result = it->second->collator->compare(toUnicode(lhs),toUnicode(rhs),status);
        if(U_FAILURE(status)) {
            return nullptr;
        }
        if(result == UCOL_LESS) {
            return makeInt(-1);
        }
        if(result == UCOL_GREATER) {
            return makeInt(1);
        }
        return makeInt(0);
    }
#endif

    if(lhs < rhs) {
        return makeInt(-1);
    }
    if(lhs > rhs) {
        return makeInt(1);
    }
    return makeInt(0);
}

STARBYTES_FUNC(Unicode_Collator_sortKey) {
    auto self = StarbytesFuncArgsGetArg(args);
    std::string text;
    if(!readStringArg(args,text)) {
        return nullptr;
    }

#ifdef STARBYTES_HAS_ICU
    auto it = g_collatorRegistry.find(self);
    if(it != g_collatorRegistry.end() && it->second && it->second->collator) {
        auto unicodeText = toUnicode(text);
        int32_t needed = it->second->collator->getSortKey(unicodeText,nullptr,0);
        if(needed <= 0) {
            return makeIntArray({});
        }
        std::vector<uint8_t> buffer((size_t)needed);
        it->second->collator->getSortKey(unicodeText,buffer.data(),needed);
        if(!buffer.empty() && buffer.back() == 0) {
            buffer.pop_back();
        }
        std::vector<int> ints;
        ints.reserve(buffer.size());
        for(auto byte : buffer) {
            ints.push_back((int)byte);
        }
        return makeIntArray(ints);
    }
#endif

    std::vector<int> ints;
    ints.reserve(text.size());
    for(unsigned char c : text) {
        ints.push_back((int)c);
    }
    return makeIntArray(ints);
}

STARBYTES_FUNC(Unicode_BreakIterator_create) {
    skipOptionalModuleReceiver(args,3);

    int kind = 0;
    if(!readIntArg(args,kind)) {
        return nullptr;
    }
    if(!validBreakKind(kind)) {
        return nullptr;
    }
    std::string localeId;
    if(!readLocaleArg(args,localeId)) {
        return nullptr;
    }
    std::string text;
    if(!readStringArg(args,text)) {
        return nullptr;
    }

#ifdef STARBYTES_HAS_ICU
    UErrorCode status = U_ZERO_ERROR;
    std::unique_ptr<icu::BreakIterator> iterator;
    auto locale = localeFromId(localeId);
    switch(kind) {
        case 3:
            iterator.reset(icu::BreakIterator::createCharacterInstance(locale,status));
            break;
        case 1:
            iterator.reset(icu::BreakIterator::createSentenceInstance(locale,status));
            break;
        case 2:
            iterator.reset(icu::BreakIterator::createLineInstance(locale,status));
            break;
        default:
            iterator.reset(icu::BreakIterator::createWordInstance(locale,status));
            break;
    }
    if(U_FAILURE(status) || !iterator) {
        return nullptr;
    }

    auto object = StarbytesObjectNew(StarbytesMakeClass("BreakIterator"));
    auto state = std::make_unique<BreakIterState>();
    state->iterator = std::move(iterator);
    state->text = toUnicode(text);
    state->iterator->setText(state->text);
    g_breakRegistry[object] = std::move(state);
    return object;
#else
    (void)kind;
    auto object = StarbytesObjectNew(StarbytesMakeClass("BreakIterator"));
    return object;
#endif
}

STARBYTES_FUNC(Unicode_BreakIterator_boundaries) {
    auto self = StarbytesFuncArgsGetArg(args);

#ifdef STARBYTES_HAS_ICU
    auto it = g_breakRegistry.find(self);
    if(it == g_breakRegistry.end() || !it->second || !it->second->iterator) {
        return nullptr;
    }

    std::vector<int> boundaries;
    it->second->iterator->setText(it->second->text);
    boundaries.push_back(it->second->iterator->first());
    for(int32_t b = it->second->iterator->next(); b != icu::BreakIterator::DONE; b = it->second->iterator->next()) {
        boundaries.push_back(b);
    }
    return makeIntArray(boundaries);
#else
    (void)self;
    return makeIntArray({0});
#endif
}

STARBYTES_FUNC(Unicode_normalize) {
    skipOptionalModuleReceiver(args,2);

    std::string text;
    int form = 0;
    if(!readStringArg(args,text) || !readIntArg(args,form)) {
        return nullptr;
    }
    if(!validNormalizationForm(form)) {
        return nullptr;
    }

#ifdef STARBYTES_HAS_ICU
    UErrorCode status = U_ZERO_ERROR;
    auto normalizer = normalizerForForm(form,status);
    if(U_FAILURE(status) || !normalizer) {
        return nullptr;
    }

    icu::UnicodeString out;
    normalizer->normalize(toUnicode(text),out,status);
    if(U_FAILURE(status)) {
        return nullptr;
    }
    return makeString(toUtf8(out));
#else
    return makeString(text);
#endif
}

STARBYTES_FUNC(Unicode_isNormalized) {
    skipOptionalModuleReceiver(args,2);

    std::string text;
    int form = 0;
    if(!readStringArg(args,text) || !readIntArg(args,form)) {
        return nullptr;
    }
    if(!validNormalizationForm(form)) {
        return nullptr;
    }

#ifdef STARBYTES_HAS_ICU
    UErrorCode status = U_ZERO_ERROR;
    auto normalizer = normalizerForForm(form,status);
    if(U_FAILURE(status) || !normalizer) {
        return nullptr;
    }
    auto isNorm = normalizer->isNormalized(toUnicode(text),status);
    if(U_FAILURE(status)) {
        return nullptr;
    }
    return makeBool(isNorm);
#else
    return makeBool(true);
#endif
}

STARBYTES_FUNC(Unicode_caseFold) {
    skipOptionalModuleReceiver(args,1);

    std::string text;
    if(!readStringArg(args,text)) {
        return nullptr;
    }
#ifdef STARBYTES_HAS_ICU
    auto unicode = toUnicode(text);
    unicode.foldCase();
    return makeString(toUtf8(unicode));
#else
    return makeString(foldedUtf8(text));
#endif
}

STARBYTES_FUNC(Unicode_toUpper) {
    skipOptionalModuleReceiver(args,2);

    std::string text;
    if(!readStringArg(args,text)) {
        return nullptr;
    }
    std::string localeId;
    if(!readLocaleArg(args,localeId)) {
        return nullptr;
    }

#ifdef STARBYTES_HAS_ICU
    auto unicode = toUnicode(text);
    unicode.toUpper(localeFromId(localeId));
    return makeString(toUtf8(unicode));
#else
    std::transform(text.begin(),text.end(),text.begin(),[](unsigned char c) {
        return (char)std::toupper(c);
    });
    return makeString(text);
#endif
}

STARBYTES_FUNC(Unicode_toLower) {
    skipOptionalModuleReceiver(args,2);

    std::string text;
    if(!readStringArg(args,text)) {
        return nullptr;
    }
    std::string localeId;
    if(!readLocaleArg(args,localeId)) {
        return nullptr;
    }

#ifdef STARBYTES_HAS_ICU
    auto unicode = toUnicode(text);
    unicode.toLower(localeFromId(localeId));
    return makeString(toUtf8(unicode));
#else
    std::transform(text.begin(),text.end(),text.begin(),[](unsigned char c) {
        return (char)std::tolower(c);
    });
    return makeString(text);
#endif
}

STARBYTES_FUNC(Unicode_toTitle) {
    skipOptionalModuleReceiver(args,2);

    std::string text;
    if(!readStringArg(args,text)) {
        return nullptr;
    }
    std::string localeId;
    if(!readLocaleArg(args,localeId)) {
        return nullptr;
    }

#ifdef STARBYTES_HAS_ICU
    auto unicode = toUnicode(text);
    unicode.toTitle(nullptr,localeFromId(localeId));
    return makeString(toUtf8(unicode));
#else
    if(!text.empty()) {
        text[0] = (char)std::toupper((unsigned char)text[0]);
    }
    return makeString(text);
#endif
}

STARBYTES_FUNC(Unicode_graphemes) {
    skipOptionalModuleReceiver(args,2);

    std::string text;
    if(!readStringArg(args,text)) {
        return nullptr;
    }
    std::string localeId;
    if(!readLocaleArg(args,localeId)) {
        return nullptr;
    }
#ifdef STARBYTES_HAS_ICU
    return makeStringArray(splitWithBreakIterator(text,localeId,3,false));
#else
    std::vector<std::string> chars;
    for(char c : text) {
        chars.push_back(std::string(1,c));
    }
    return makeStringArray(chars);
#endif
}

STARBYTES_FUNC(Unicode_words) {
    skipOptionalModuleReceiver(args,2);

    std::string text;
    if(!readStringArg(args,text)) {
        return nullptr;
    }
    std::string localeId;
    if(!readLocaleArg(args,localeId)) {
        return nullptr;
    }
#ifdef STARBYTES_HAS_ICU
    return makeStringArray(splitWithBreakIterator(text,localeId,0,true));
#else
    std::vector<std::string> out;
    std::string current;
    for(char c : text) {
        if(std::isspace((unsigned char)c)) {
            if(!current.empty()) {
                out.push_back(current);
                current.clear();
            }
        }
        else {
            current.push_back(c);
        }
    }
    if(!current.empty()) {
        out.push_back(current);
    }
    return makeStringArray(out);
#endif
}

STARBYTES_FUNC(Unicode_sentences) {
    skipOptionalModuleReceiver(args,2);

    std::string text;
    if(!readStringArg(args,text)) {
        return nullptr;
    }
    std::string localeId;
    if(!readLocaleArg(args,localeId)) {
        return nullptr;
    }
#ifdef STARBYTES_HAS_ICU
    return makeStringArray(splitWithBreakIterator(text,localeId,1,true));
#else
    return makeStringArray({text});
#endif
}

STARBYTES_FUNC(Unicode_lines) {
    skipOptionalModuleReceiver(args,2);

    std::string text;
    if(!readStringArg(args,text)) {
        return nullptr;
    }
    std::string localeId;
    if(!readLocaleArg(args,localeId)) {
        return nullptr;
    }
#ifdef STARBYTES_HAS_ICU
    return makeStringArray(splitWithBreakIterator(text,localeId,2,false));
#else
    std::vector<std::string> out;
    std::string line;
    for(char c : text) {
        if(c == '\n') {
            out.push_back(line);
            line.clear();
        }
        else {
            line.push_back(c);
        }
    }
    out.push_back(line);
    return makeStringArray(out);
#endif
}

STARBYTES_FUNC(Unicode_codepoints) {
    skipOptionalModuleReceiver(args,1);

    std::string text;
    if(!readStringArg(args,text)) {
        return nullptr;
    }

#ifdef STARBYTES_HAS_ICU
    std::vector<int> out;
    auto unicode = toUnicode(text);
    for(int32_t i = 0; i < unicode.length();) {
        UChar32 cp = unicode.char32At(i);
        out.push_back((int)cp);
        i += U16_LENGTH(cp);
    }
    return makeIntArray(out);
#else
    std::vector<int> out;
    for(unsigned char c : text) {
        out.push_back((int)c);
    }
    return makeIntArray(out);
#endif
}

STARBYTES_FUNC(Unicode_fromCodepoints) {
    skipOptionalModuleReceiver(args,1);

    auto values = StarbytesFuncArgsGetArg(args);
    if(!values || !StarbytesObjectTypecheck(values,StarbytesArrayType())) {
        return nullptr;
    }

#ifdef STARBYTES_HAS_ICU
    icu::UnicodeString out;
    auto len = StarbytesArrayGetLength(values);
    for(unsigned i = 0; i < len; ++i) {
        auto value = StarbytesArrayIndex(values,i);
        if(!value || !StarbytesObjectTypecheck(value,StarbytesNumType()) || StarbytesNumGetType(value) != NumTypeInt) {
            return nullptr;
        }
        int cp = StarbytesNumGetIntValue(value);
        if(!isScalarValid(cp)) {
            return nullptr;
        }
        out.append((UChar32)cp);
    }
    return makeString(toUtf8(out));
#else
    std::string out;
    auto len = StarbytesArrayGetLength(values);
    for(unsigned i = 0; i < len; ++i) {
        auto value = StarbytesArrayIndex(values,i);
        if(!value || !StarbytesObjectTypecheck(value,StarbytesNumType()) || StarbytesNumGetType(value) != NumTypeInt) {
            return nullptr;
        }
        int cp = StarbytesNumGetIntValue(value);
        if(cp < 0 || cp > 255) {
            return nullptr;
        }
        out.push_back((char)cp);
    }
    return makeString(out);
#endif
}

STARBYTES_FUNC(Unicode_scalarInfo) {
    skipOptionalModuleReceiver(args,1);

    int codepoint = 0;
    if(!readIntArg(args,codepoint)) {
        return nullptr;
    }

#ifdef STARBYTES_HAS_ICU
    if(!isScalarValid(codepoint)) {
        return nullptr;
    }
#endif

    auto infoObject = StarbytesObjectNew(StarbytesMakeClass("UnicodeScalarInfo"));
    g_scalarInfoRegistry[infoObject] = (int32_t)codepoint;
    return infoObject;
}

STARBYTES_FUNC(Unicode_ScalarInfo_codepoint) {
    auto self = StarbytesFuncArgsGetArg(args);
    auto it = g_scalarInfoRegistry.find(self);
    if(it == g_scalarInfoRegistry.end()) {
        return nullptr;
    }
    return makeInt((int)it->second);
}

STARBYTES_FUNC(Unicode_ScalarInfo_name) {
    auto self = StarbytesFuncArgsGetArg(args);
    auto it = g_scalarInfoRegistry.find(self);
    if(it == g_scalarInfoRegistry.end()) {
        return nullptr;
    }
#ifdef STARBYTES_HAS_ICU
    char buffer[256] = {0};
    UErrorCode status = U_ZERO_ERROR;
    auto len = u_charName(it->second,U_EXTENDED_CHAR_NAME,buffer,sizeof(buffer),&status);
    if(U_FAILURE(status) || len <= 0) {
        return makeString("<unknown>");
    }
    return makeString(std::string(buffer,(size_t)len));
#else
    return makeString("<unknown>");
#endif
}

STARBYTES_FUNC(Unicode_ScalarInfo_category) {
    auto self = StarbytesFuncArgsGetArg(args);
    auto it = g_scalarInfoRegistry.find(self);
    if(it == g_scalarInfoRegistry.end()) {
        return nullptr;
    }
#ifdef STARBYTES_HAS_ICU
    return makeString(scalarCategoryName(it->second));
#else
    return makeString("Unknown");
#endif
}

STARBYTES_FUNC(Unicode_ScalarInfo_script) {
    auto self = StarbytesFuncArgsGetArg(args);
    auto it = g_scalarInfoRegistry.find(self);
    if(it == g_scalarInfoRegistry.end()) {
        return nullptr;
    }
#ifdef STARBYTES_HAS_ICU
    UErrorCode status = U_ZERO_ERROR;
    auto code = uscript_getScript(it->second,&status);
    if(U_FAILURE(status)) {
        return makeString("Unknown");
    }
    auto name = uscript_getName(code);
    if(!name) {
        return makeString("Unknown");
    }
    return makeString(name);
#else
    return makeString("Unknown");
#endif
}

STARBYTES_FUNC(Unicode_containsFolded) {
    skipOptionalModuleReceiver(args,3);

    std::string haystack;
    std::string needle;
    if(!readStringArg(args,haystack) || !readStringArg(args,needle)) {
        return nullptr;
    }
    std::string localeId;
    if(!readLocaleArg(args,localeId)) {
        return nullptr;
    }
    (void)localeId;
    auto lhs = foldedUtf8(haystack);
    auto rhs = foldedUtf8(needle);
    return makeBool(lhs.find(rhs) != std::string::npos);
}

STARBYTES_FUNC(Unicode_startsWithFolded) {
    skipOptionalModuleReceiver(args,3);

    std::string text;
    std::string prefix;
    if(!readStringArg(args,text) || !readStringArg(args,prefix)) {
        return nullptr;
    }
    std::string localeId;
    if(!readLocaleArg(args,localeId)) {
        return nullptr;
    }
    (void)localeId;
    auto lhs = foldedUtf8(text);
    auto rhs = foldedUtf8(prefix);
    if(lhs.size() < rhs.size()) {
        return makeBool(false);
    }
    return makeBool(lhs.compare(0,rhs.size(),rhs) == 0);
}

STARBYTES_FUNC(Unicode_endsWithFolded) {
    skipOptionalModuleReceiver(args,3);

    std::string text;
    std::string suffix;
    if(!readStringArg(args,text) || !readStringArg(args,suffix)) {
        return nullptr;
    }
    std::string localeId;
    if(!readLocaleArg(args,localeId)) {
        return nullptr;
    }
    (void)localeId;
    auto lhs = foldedUtf8(text);
    auto rhs = foldedUtf8(suffix);
    if(lhs.size() < rhs.size()) {
        return makeBool(false);
    }
    return makeBool(lhs.compare(lhs.size() - rhs.size(),rhs.size(),rhs) == 0);
}

STARBYTES_FUNC(Unicode_displayWidth) {
    skipOptionalModuleReceiver(args,2);

    std::string text;
    if(!readStringArg(args,text)) {
        return nullptr;
    }
    std::string localeId;
    if(!readLocaleArg(args,localeId)) {
        return nullptr;
    }
    (void)localeId;

#ifdef STARBYTES_HAS_ICU
    auto unicode = toUnicode(text);
    int width = 0;
    for(int32_t i = 0; i < unicode.length();) {
        UChar32 cp = unicode.char32At(i);
        i += U16_LENGTH(cp);

        auto category = u_charType(cp);
        if(category == U_NON_SPACING_MARK || category == U_ENCLOSING_MARK || category == U_COMBINING_SPACING_MARK) {
            continue;
        }

        auto east = u_getIntPropertyValue(cp,UCHAR_EAST_ASIAN_WIDTH);
        if(east == U_EA_WIDE || east == U_EA_FULLWIDTH) {
            width += 2;
        }
        else {
            width += 1;
        }
    }
    return makeInt(width);
#else
    return makeInt((int)text.size());
#endif
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

    addFunc(module,"Unicode_Locale_current",0,Unicode_Locale_current);
    addFunc(module,"Unicode_Locale_root",0,Unicode_Locale_root);
    addFunc(module,"Unicode_Locale_from",1,Unicode_Locale_from);
    addFunc(module,"Unicode_Locale_id",1,Unicode_Locale_id);

    addFunc(module,"Unicode_Collator_create",1,Unicode_Collator_create);
    addFunc(module,"Unicode_Collator_compare",3,Unicode_Collator_compare);
    addFunc(module,"Unicode_Collator_sortKey",2,Unicode_Collator_sortKey);

    addFunc(module,"Unicode_BreakIterator_create",3,Unicode_BreakIterator_create);
    addFunc(module,"Unicode_BreakIterator_boundaries",1,Unicode_BreakIterator_boundaries);

    addFunc(module,"Unicode_ScalarInfo_codepoint",1,Unicode_ScalarInfo_codepoint);
    addFunc(module,"Unicode_ScalarInfo_name",1,Unicode_ScalarInfo_name);
    addFunc(module,"Unicode_ScalarInfo_category",1,Unicode_ScalarInfo_category);
    addFunc(module,"Unicode_ScalarInfo_script",1,Unicode_ScalarInfo_script);

    addFunc(module,"Unicode_normalize",2,Unicode_normalize);
    addFunc(module,"Unicode_isNormalized",2,Unicode_isNormalized);
    addFunc(module,"Unicode_caseFold",1,Unicode_caseFold);
    addFunc(module,"Unicode_toUpper",2,Unicode_toUpper);
    addFunc(module,"Unicode_toLower",2,Unicode_toLower);
    addFunc(module,"Unicode_toTitle",2,Unicode_toTitle);

    addFunc(module,"Unicode_graphemes",2,Unicode_graphemes);
    addFunc(module,"Unicode_words",2,Unicode_words);
    addFunc(module,"Unicode_sentences",2,Unicode_sentences);
    addFunc(module,"Unicode_lines",2,Unicode_lines);
    addFunc(module,"Unicode_codepoints",1,Unicode_codepoints);
    addFunc(module,"Unicode_fromCodepoints",1,Unicode_fromCodepoints);
    addFunc(module,"Unicode_scalarInfo",1,Unicode_scalarInfo);

    addFunc(module,"Unicode_containsFolded",3,Unicode_containsFolded);
    addFunc(module,"Unicode_startsWithFolded",3,Unicode_startsWithFolded);
    addFunc(module,"Unicode_endsWithFolded",3,Unicode_endsWithFolded);
    addFunc(module,"Unicode_displayWidth",2,Unicode_displayWidth);

    return module;
}
