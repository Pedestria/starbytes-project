#include "starbytes/runtime/RegexSupport.h"

#include "starbytes/base/ADT.h"

#include <string>
#include <utility>
#include <vector>

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

namespace starbytes::Runtime::regex {
namespace {

using starbytes::string_set;

StarbytesObject makeRuntimeBool(bool value){
    return StarbytesBoolNew((StarbytesBoolVal)value);
}

uint32_t regexCompileOptionsFromFlags(const std::string &flags){
    uint32_t options = 0;
    for(char flag : flags){
        switch(flag){
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

pcre2_code *compileRegex(const std::string &pattern,const std::string &flags){
    int errorCode = 0;
    PCRE2_SIZE errorOffset = 0;
    return pcre2_compile((PCRE2_SPTR)pattern.c_str(),
                         PCRE2_ZERO_TERMINATED,
                         regexCompileOptionsFromFlags(flags),
                         &errorCode,
                         &errorOffset,
                         nullptr);
}

StarbytesObject stringListToArray(const std::vector<std::string> &values){
    auto out = StarbytesArrayNew();
    for(const auto &value : values){
        auto strObj = StarbytesStrNewWithData(value.c_str());
        StarbytesArrayPush(out,strObj);
        StarbytesObjectRelease(strObj);
    }
    return out;
}

}

bool extractRegexPatternAndFlags(StarbytesObject regexObject,std::string &patternOut,std::string &flagsOut){
    if(!regexObject || !StarbytesObjectTypecheck(regexObject,StarbytesRegexType())){
        return false;
    }

    auto pattern = StarbytesObjectGetProperty(regexObject,"pattern");
    auto flags = StarbytesObjectGetProperty(regexObject,"flags");
    if(!pattern || !StarbytesObjectTypecheck(pattern,StarbytesStrType())){
        return false;
    }

    patternOut = StarbytesStrGetBuffer(pattern);
    flagsOut = (flags && StarbytesObjectTypecheck(flags,StarbytesStrType())) ? StarbytesStrGetBuffer(flags) : "";
    return true;
}

StarbytesObject match(StarbytesObject regexObject,const std::string &text,std::string &errorOut){
    errorOut.clear();

    std::string pattern;
    std::string flags;
    if(!extractRegexPatternAndFlags(regexObject,pattern,flags)){
        errorOut = "Regex.match requires a Regex value";
        return nullptr;
    }

    auto *compiled = compileRegex(pattern,flags);
    if(!compiled){
        errorOut = "Regex.match failed to compile regex";
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

    return makeRuntimeBool(matchRc >= 0);
}

StarbytesObject findAll(StarbytesObject regexObject,const std::string &text,std::string &errorOut){
    errorOut.clear();

    std::string pattern;
    std::string flags;
    if(!extractRegexPatternAndFlags(regexObject,pattern,flags)){
        errorOut = "Regex.findAll requires a Regex value";
        return nullptr;
    }

    auto *compiled = compileRegex(pattern,flags);
    if(!compiled){
        errorOut = "Regex.findAll failed to compile regex";
        return nullptr;
    }

    std::vector<std::string> matches;
    string_set dedupGuard;
    auto *matchData = pcre2_match_data_create_from_pattern(compiled,nullptr);

    size_t offset = 0;
    while(offset <= text.size()){
        auto matchRc = pcre2_match(compiled,
                                   (PCRE2_SPTR)text.c_str(),
                                   text.size(),
                                   offset,
                                   0,
                                   matchData,
                                   nullptr);
        if(matchRc < 0){
            break;
        }

        auto *ovector = pcre2_get_ovector_pointer(matchData);
        size_t start = ovector[0];
        size_t end = ovector[1];
        if(end < start || start > text.size() || end > text.size()){
            break;
        }

        auto matchText = text.substr(start,end - start);
        if(dedupGuard.insert(matchText + "@" + std::to_string(start)).second){
            matches.push_back(std::move(matchText));
        }

        if(end == offset){
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

StarbytesObject replace(StarbytesObject regexObject,const std::string &text,const std::string &replacement,std::string &errorOut){
    errorOut.clear();

    std::string pattern;
    std::string flags;
    if(!extractRegexPatternAndFlags(regexObject,pattern,flags)){
        errorOut = "Regex.replace requires a Regex value";
        return nullptr;
    }

    auto *compiled = compileRegex(pattern,flags);
    if(!compiled){
        errorOut = "Regex.replace failed to compile regex";
        return nullptr;
    }

    auto *matchData = pcre2_match_data_create_from_pattern(compiled,nullptr);

    std::string out;
    size_t offset = 0;
    size_t consumed = 0;
    while(offset <= text.size()){
        auto matchRc = pcre2_match(compiled,
                                   (PCRE2_SPTR)text.c_str(),
                                   text.size(),
                                   offset,
                                   0,
                                   matchData,
                                   nullptr);
        if(matchRc < 0){
            break;
        }

        auto *ovector = pcre2_get_ovector_pointer(matchData);
        size_t start = ovector[0];
        size_t end = ovector[1];
        if(end < start || start > text.size() || end > text.size()){
            break;
        }

        out.append(text.substr(consumed,start - consumed));
        out.append(replacement);

        consumed = end;
        if(end == offset){
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

}
