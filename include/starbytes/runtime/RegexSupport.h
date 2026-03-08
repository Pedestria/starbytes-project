#ifndef STARBYTES_RUNTIME_REGEXSUPPORT_H
#define STARBYTES_RUNTIME_REGEXSUPPORT_H

#include "starbytes/interop.h"

#include <string>

namespace starbytes::Runtime::regex {

bool extractRegexPatternAndFlags(StarbytesObject regexObject,std::string &patternOut,std::string &flagsOut);

StarbytesObject match(StarbytesObject regexObject,const std::string &text,std::string &errorOut);
StarbytesObject findAll(StarbytesObject regexObject,const std::string &text,std::string &errorOut);
StarbytesObject replace(StarbytesObject regexObject,const std::string &text,const std::string &replacement,std::string &errorOut);

}

#endif
