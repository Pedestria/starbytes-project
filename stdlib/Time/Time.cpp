#include <starbytes/interop.h>

#include <chrono>
#include <climits>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <iomanip>
#include <limits>
#include <regex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>

namespace {

struct NativeArgsLayout {
    unsigned argc = 0;
    unsigned index = 0;
    StarbytesObject *argv = nullptr;
};

struct TimeZoneState {
    std::string id;
    int offsetMinutes = 0;
};

struct DateTimeState {
    int64_t unixSeconds = 0;
    int32_t nanosecond = 0;
    int offsetMinutes = 0;
    std::string tzId = "UTC";
};

struct DateParts {
    int year = 1970;
    int month = 1;
    int day = 1;
    int hour = 0;
    int minute = 0;
    int second = 0;
};

std::unordered_map<StarbytesObject,int64_t> g_durationRegistry;
std::unordered_map<StarbytesObject,int64_t> g_instantRegistry;
std::unordered_map<StarbytesObject,TimeZoneState> g_timezoneRegistry;
std::unordered_map<StarbytesObject,DateTimeState> g_datetimeRegistry;

StarbytesObject makeBool(bool value) {
    // Runtime bool consumption currently interprets StarbytesBoolFalse as logical true.
    return StarbytesBoolNew(value ? StarbytesBoolFalse : StarbytesBoolTrue);
}

StarbytesObject makeInt64(int64_t value) {
    if(value > (int64_t)INT_MAX) {
        value = (int64_t)INT_MAX;
    }
    else if(value < (int64_t)INT_MIN) {
        value = (int64_t)INT_MIN;
    }
    return StarbytesNumNew(NumTypeInt,(int)value);
}

StarbytesObject makeFloat(double value) {
    return StarbytesNumNew(NumTypeFloat,value);
}

StarbytesObject makeString(const std::string &value) {
    return StarbytesStrNewWithData(value.c_str());
}

bool readIntArg(StarbytesFuncArgs args,int &outValue) {
    auto arg = StarbytesFuncArgsGetArg(args);
    if(!arg || !StarbytesObjectTypecheck(arg,StarbytesNumType()) || StarbytesNumGetType(arg) != NumTypeInt) {
        return false;
    }
    outValue = StarbytesNumGetIntValue(arg);
    return true;
}

bool readFloatArg(StarbytesFuncArgs args,double &outValue) {
    auto arg = StarbytesFuncArgsGetArg(args);
    if(!arg || !StarbytesObjectTypecheck(arg,StarbytesNumType())) {
        return false;
    }
    if(StarbytesNumGetType(arg) == NumTypeFloat) {
        outValue = StarbytesNumGetFloatValue(arg);
    }
    else {
        outValue = (double)StarbytesNumGetIntValue(arg);
    }
    return true;
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

bool checkedAddInt64(int64_t lhs,int64_t rhs,int64_t &outValue) {
    if((rhs > 0 && lhs > (std::numeric_limits<int64_t>::max() - rhs)) ||
       (rhs < 0 && lhs < (std::numeric_limits<int64_t>::min() - rhs))) {
        return false;
    }
    outValue = lhs + rhs;
    return true;
}

bool checkedSubInt64(int64_t lhs,int64_t rhs,int64_t &outValue) {
    if((rhs < 0 && lhs > (std::numeric_limits<int64_t>::max() + rhs)) ||
       (rhs > 0 && lhs < (std::numeric_limits<int64_t>::min() + rhs))) {
        return false;
    }
    outValue = lhs - rhs;
    return true;
}

StarbytesObject makeDurationObject(int64_t nanos) {
    auto object = StarbytesObjectNew(StarbytesMakeClass("Duration"));
    g_durationRegistry[object] = nanos;
    return object;
}

StarbytesObject makeInstantObject(int64_t ticks) {
    auto object = StarbytesObjectNew(StarbytesMakeClass("Instant"));
    g_instantRegistry[object] = ticks;
    return object;
}

StarbytesObject makeTimeZoneObject(const std::string &id,int offsetMinutes) {
    auto object = StarbytesObjectNew(StarbytesMakeClass("TimeZone"));
    g_timezoneRegistry[object] = {id,offsetMinutes};
    return object;
}

StarbytesObject makeDateTimeObject(int64_t unixSeconds,int32_t nanosecond,int offsetMinutes,const std::string &tzId) {
    auto object = StarbytesObjectNew(StarbytesMakeClass("DateTime"));
    g_datetimeRegistry[object] = {unixSeconds,nanosecond,offsetMinutes,tzId};
    return object;
}

bool getDurationNanos(StarbytesObject object,int64_t &outNanos) {
    auto it = g_durationRegistry.find(object);
    if(it == g_durationRegistry.end()) {
        return false;
    }
    outNanos = it->second;
    return true;
}

bool getInstantTicks(StarbytesObject object,int64_t &outTicks) {
    auto it = g_instantRegistry.find(object);
    if(it == g_instantRegistry.end()) {
        return false;
    }
    outTicks = it->second;
    return true;
}

bool getTimeZoneState(StarbytesObject object,TimeZoneState &outState) {
    auto it = g_timezoneRegistry.find(object);
    if(it == g_timezoneRegistry.end()) {
        return false;
    }
    outState = it->second;
    return true;
}

bool getDateTimeState(StarbytesObject object,DateTimeState &outState) {
    auto it = g_datetimeRegistry.find(object);
    if(it == g_datetimeRegistry.end()) {
        return false;
    }
    outState = it->second;
    return true;
}

bool gmtimeSafe(std::time_t value,std::tm &outTm) {
#ifdef _WIN32
    return gmtime_s(&outTm,&value) == 0;
#else
    return gmtime_r(&value,&outTm) != nullptr;
#endif
}

bool localtimeSafe(std::time_t value,std::tm &outTm) {
#ifdef _WIN32
    return localtime_s(&outTm,&value) == 0;
#else
    return localtime_r(&value,&outTm) != nullptr;
#endif
}

int localOffsetMinutesForEpoch(int64_t epochSeconds) {
    std::time_t raw = (std::time_t)epochSeconds;
    std::tm localTm = {};
    std::tm gmTm = {};
    if(!localtimeSafe(raw,localTm) || !gmtimeSafe(raw,gmTm)) {
        return 0;
    }
    std::time_t localAsEpoch = std::mktime(&localTm);
    std::time_t gmAsEpoch = std::mktime(&gmTm);
    if(localAsEpoch == (std::time_t)-1 || gmAsEpoch == (std::time_t)-1) {
        return 0;
    }
    double delta = std::difftime(localAsEpoch,gmAsEpoch);
    return (int)std::llround(delta / 60.0);
}

std::string formatOffsetCore(int offsetMinutes) {
    auto absMinutes = offsetMinutes >= 0 ? offsetMinutes : -offsetMinutes;
    int hh = absMinutes / 60;
    int mm = absMinutes % 60;
    std::ostringstream out;
    out << (offsetMinutes >= 0 ? '+' : '-')
        << std::setw(2) << std::setfill('0') << hh
        << ":"
        << std::setw(2) << std::setfill('0') << mm;
    return out.str();
}

std::string formatOffsetSuffix(int offsetMinutes,bool useZForZero) {
    if(offsetMinutes == 0 && useZForZero) {
        return "Z";
    }
    return formatOffsetCore(offsetMinutes);
}

std::string timezoneIdFromOffset(int offsetMinutes) {
    if(offsetMinutes == 0) {
        return "UTC";
    }
    return "UTC" + formatOffsetCore(offsetMinutes);
}

bool parseHHMMOffset(const std::string &text,int &outOffsetMinutes) {
    if(text.size() != 5 && text.size() != 6) {
        return false;
    }
    char sign = text[0];
    if(sign != '+' && sign != '-') {
        return false;
    }

    int hh = 0;
    int mm = 0;
    if(text.size() == 6) {
        if(text[3] != ':') {
            return false;
        }
        try {
            hh = std::stoi(text.substr(1,2));
            mm = std::stoi(text.substr(4,2));
        }
        catch(...) {
            return false;
        }
    }
    else {
        try {
            hh = std::stoi(text.substr(1,2));
            mm = std::stoi(text.substr(3,2));
        }
        catch(...) {
            return false;
        }
    }

    if(hh < 0 || hh > 23 || mm < 0 || mm > 59) {
        return false;
    }

    int total = hh * 60 + mm;
    outOffsetMinutes = (sign == '-') ? -total : total;
    return true;
}

bool parseTimeZoneIdentifier(const std::string &id,int &outOffsetMinutes,std::string &outNormalizedId) {
    if(id.empty()) {
        return false;
    }
    if(id == "UTC" || id == "Z") {
        outOffsetMinutes = 0;
        outNormalizedId = "UTC";
        return true;
    }
    if(id == "LOCAL") {
        outOffsetMinutes = localOffsetMinutesForEpoch((int64_t)std::time(nullptr));
        outNormalizedId = "LOCAL";
        return true;
    }

    if(parseHHMMOffset(id,outOffsetMinutes)) {
        outNormalizedId = timezoneIdFromOffset(outOffsetMinutes);
        return true;
    }

    if(id.rfind("UTC",0) == 0 && id.size() > 3) {
        auto tail = id.substr(3);
        if(parseHHMMOffset(tail,outOffsetMinutes)) {
            outNormalizedId = timezoneIdFromOffset(outOffsetMinutes);
            return true;
        }
    }

    return false;
}

bool datePartsFromState(const DateTimeState &state,DateParts &outParts) {
    int64_t adjustedSeconds = 0;
    if(!checkedAddInt64(state.unixSeconds,(int64_t)state.offsetMinutes * 60,adjustedSeconds)) {
        return false;
    }

    std::time_t raw = (std::time_t)adjustedSeconds;
    std::tm tmValue = {};
    if(!gmtimeSafe(raw,tmValue)) {
        return false;
    }

    outParts.year = tmValue.tm_year + 1900;
    outParts.month = tmValue.tm_mon + 1;
    outParts.day = tmValue.tm_mday;
    outParts.hour = tmValue.tm_hour;
    outParts.minute = tmValue.tm_min;
    outParts.second = tmValue.tm_sec;
    return true;
}

bool civilToUnixUTC(int year,int month,int day,int hour,int minute,int second,int64_t &outUnixSeconds) {
    std::tm tmValue = {};
    tmValue.tm_year = year - 1900;
    tmValue.tm_mon = month - 1;
    tmValue.tm_mday = day;
    tmValue.tm_hour = hour;
    tmValue.tm_min = minute;
    tmValue.tm_sec = second;

#ifdef _WIN32
    auto seconds = _mkgmtime(&tmValue);
#else
    auto seconds = timegm(&tmValue);
#endif
    if(seconds == (std::time_t)-1) {
        return false;
    }
    outUnixSeconds = (int64_t)seconds;
    return true;
}

int64_t steadyNowNanos() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

bool readDateTimeAndTargetTz(StarbytesFuncArgs args,DateTimeState &dateTime,TimeZoneState &tzState) {
    auto dateTimeObj = StarbytesFuncArgsGetArg(args);
    auto tzObj = StarbytesFuncArgsGetArg(args);
    if(!dateTimeObj || !tzObj) {
        return false;
    }
    return getDateTimeState(dateTimeObj,dateTime) && getTimeZoneState(tzObj,tzState);
}

STARBYTES_FUNC(Time_Duration_nanoseconds) {
    auto self = StarbytesFuncArgsGetArg(args);
    int64_t nanos = 0;
    if(!getDurationNanos(self,nanos)) {
        return nullptr;
    }
    return makeInt64(nanos);
}

STARBYTES_FUNC(Time_Duration_milliseconds) {
    auto self = StarbytesFuncArgsGetArg(args);
    int64_t nanos = 0;
    if(!getDurationNanos(self,nanos)) {
        return nullptr;
    }
    return makeInt64(nanos / 1000000);
}

STARBYTES_FUNC(Time_Duration_secondsFloat) {
    auto self = StarbytesFuncArgsGetArg(args);
    int64_t nanos = 0;
    if(!getDurationNanos(self,nanos)) {
        return nullptr;
    }
    return makeFloat((double)nanos / 1000000000.0);
}

STARBYTES_FUNC(Time_Instant_ticks) {
    auto self = StarbytesFuncArgsGetArg(args);
    int64_t ticks = 0;
    if(!getInstantTicks(self,ticks)) {
        return nullptr;
    }
    return makeInt64(ticks);
}

STARBYTES_FUNC(Time_TimeZone_id) {
    auto self = StarbytesFuncArgsGetArg(args);
    TimeZoneState state;
    if(!getTimeZoneState(self,state)) {
        return nullptr;
    }
    return makeString(state.id);
}

STARBYTES_FUNC(Time_TimeZone_offsetMinutes) {
    auto self = StarbytesFuncArgsGetArg(args);
    TimeZoneState state;
    if(!getTimeZoneState(self,state)) {
        return nullptr;
    }
    return makeInt64(state.offsetMinutes);
}

STARBYTES_FUNC(Time_DateTime_unixSeconds) {
    auto self = StarbytesFuncArgsGetArg(args);
    DateTimeState state;
    if(!getDateTimeState(self,state)) {
        return nullptr;
    }
    return makeInt64(state.unixSeconds);
}

STARBYTES_FUNC(Time_DateTime_nanosecond) {
    auto self = StarbytesFuncArgsGetArg(args);
    DateTimeState state;
    if(!getDateTimeState(self,state)) {
        return nullptr;
    }
    return makeInt64(state.nanosecond);
}

STARBYTES_FUNC(Time_DateTime_year) {
    auto self = StarbytesFuncArgsGetArg(args);
    DateTimeState state;
    if(!getDateTimeState(self,state)) {
        return nullptr;
    }
    DateParts parts;
    if(!datePartsFromState(state,parts)) {
        return nullptr;
    }
    return makeInt64(parts.year);
}

STARBYTES_FUNC(Time_DateTime_month) {
    auto self = StarbytesFuncArgsGetArg(args);
    DateTimeState state;
    if(!getDateTimeState(self,state)) {
        return nullptr;
    }
    DateParts parts;
    if(!datePartsFromState(state,parts)) {
        return nullptr;
    }
    return makeInt64(parts.month);
}

STARBYTES_FUNC(Time_DateTime_day) {
    auto self = StarbytesFuncArgsGetArg(args);
    DateTimeState state;
    if(!getDateTimeState(self,state)) {
        return nullptr;
    }
    DateParts parts;
    if(!datePartsFromState(state,parts)) {
        return nullptr;
    }
    return makeInt64(parts.day);
}

STARBYTES_FUNC(Time_DateTime_hour) {
    auto self = StarbytesFuncArgsGetArg(args);
    DateTimeState state;
    if(!getDateTimeState(self,state)) {
        return nullptr;
    }
    DateParts parts;
    if(!datePartsFromState(state,parts)) {
        return nullptr;
    }
    return makeInt64(parts.hour);
}

STARBYTES_FUNC(Time_DateTime_minute) {
    auto self = StarbytesFuncArgsGetArg(args);
    DateTimeState state;
    if(!getDateTimeState(self,state)) {
        return nullptr;
    }
    DateParts parts;
    if(!datePartsFromState(state,parts)) {
        return nullptr;
    }
    return makeInt64(parts.minute);
}

STARBYTES_FUNC(Time_DateTime_second) {
    auto self = StarbytesFuncArgsGetArg(args);
    DateTimeState state;
    if(!getDateTimeState(self,state)) {
        return nullptr;
    }
    DateParts parts;
    if(!datePartsFromState(state,parts)) {
        return nullptr;
    }
    return makeInt64(parts.second);
}

STARBYTES_FUNC(Time_DateTime_timezone) {
    auto self = StarbytesFuncArgsGetArg(args);
    DateTimeState state;
    if(!getDateTimeState(self,state)) {
        return nullptr;
    }
    return makeTimeZoneObject(state.tzId,state.offsetMinutes);
}

STARBYTES_FUNC(Time_durationZero) {
    skipOptionalModuleReceiver(args,0);
    return makeDurationObject(0);
}

STARBYTES_FUNC(Time_durationFromNanos) {
    skipOptionalModuleReceiver(args,1);
    int nanos = 0;
    if(!readIntArg(args,nanos)) {
        return nullptr;
    }
    return makeDurationObject((int64_t)nanos);
}

STARBYTES_FUNC(Time_durationFromMillis) {
    skipOptionalModuleReceiver(args,1);
    int millis = 0;
    if(!readIntArg(args,millis)) {
        return nullptr;
    }
    int64_t nanos = 0;
    if(!checkedAddInt64(0,(int64_t)millis * 1000000LL,nanos)) {
        return nullptr;
    }
    return makeDurationObject(nanos);
}

STARBYTES_FUNC(Time_durationFromSeconds) {
    skipOptionalModuleReceiver(args,1);
    double seconds = 0;
    if(!readFloatArg(args,seconds) || !std::isfinite(seconds)) {
        return nullptr;
    }
    auto nanosDouble = seconds * 1000000000.0;
    if(nanosDouble > (double)std::numeric_limits<int64_t>::max() ||
       nanosDouble < (double)std::numeric_limits<int64_t>::min()) {
        return nullptr;
    }
    return makeDurationObject((int64_t)std::llround(nanosDouble));
}

STARBYTES_FUNC(Time_durationAdd) {
    skipOptionalModuleReceiver(args,2);
    auto lhsObj = StarbytesFuncArgsGetArg(args);
    auto rhsObj = StarbytesFuncArgsGetArg(args);

    int64_t lhs = 0;
    int64_t rhs = 0;
    if(!getDurationNanos(lhsObj,lhs) || !getDurationNanos(rhsObj,rhs)) {
        return nullptr;
    }

    int64_t sum = 0;
    if(!checkedAddInt64(lhs,rhs,sum)) {
        return nullptr;
    }
    return makeDurationObject(sum);
}

STARBYTES_FUNC(Time_durationSub) {
    skipOptionalModuleReceiver(args,2);
    auto lhsObj = StarbytesFuncArgsGetArg(args);
    auto rhsObj = StarbytesFuncArgsGetArg(args);

    int64_t lhs = 0;
    int64_t rhs = 0;
    if(!getDurationNanos(lhsObj,lhs) || !getDurationNanos(rhsObj,rhs)) {
        return nullptr;
    }

    int64_t diff = 0;
    if(!checkedSubInt64(lhs,rhs,diff)) {
        return nullptr;
    }
    return makeDurationObject(diff);
}

STARBYTES_FUNC(Time_durationMul) {
    skipOptionalModuleReceiver(args,2);
    auto durationObj = StarbytesFuncArgsGetArg(args);
    double factor = 0;
    if(!durationObj || !readFloatArg(args,factor) || !std::isfinite(factor)) {
        return nullptr;
    }

    int64_t nanos = 0;
    if(!getDurationNanos(durationObj,nanos)) {
        return nullptr;
    }

    auto value = (double)nanos * factor;
    if(value > (double)std::numeric_limits<int64_t>::max() ||
       value < (double)std::numeric_limits<int64_t>::min()) {
        return nullptr;
    }
    return makeDurationObject((int64_t)std::llround(value));
}

STARBYTES_FUNC(Time_durationDiv) {
    skipOptionalModuleReceiver(args,2);
    auto durationObj = StarbytesFuncArgsGetArg(args);
    double divisor = 0;
    if(!durationObj || !readFloatArg(args,divisor) || !std::isfinite(divisor) || divisor == 0.0) {
        return nullptr;
    }

    int64_t nanos = 0;
    if(!getDurationNanos(durationObj,nanos)) {
        return nullptr;
    }

    auto value = (double)nanos / divisor;
    if(value > (double)std::numeric_limits<int64_t>::max() ||
       value < (double)std::numeric_limits<int64_t>::min()) {
        return nullptr;
    }
    return makeDurationObject((int64_t)std::llround(value));
}

STARBYTES_FUNC(Time_durationCompare) {
    skipOptionalModuleReceiver(args,2);
    auto lhsObj = StarbytesFuncArgsGetArg(args);
    auto rhsObj = StarbytesFuncArgsGetArg(args);

    int64_t lhs = 0;
    int64_t rhs = 0;
    if(!getDurationNanos(lhsObj,lhs) || !getDurationNanos(rhsObj,rhs)) {
        return nullptr;
    }
    if(lhs < rhs) {
        return makeInt64(-1);
    }
    if(lhs > rhs) {
        return makeInt64(1);
    }
    return makeInt64(0);
}

STARBYTES_FUNC(Time_sleep) {
    skipOptionalModuleReceiver(args,1);
    auto durationObj = StarbytesFuncArgsGetArg(args);
    int64_t nanos = 0;
    if(!getDurationNanos(durationObj,nanos) || nanos < 0) {
        return nullptr;
    }
    std::this_thread::sleep_for(std::chrono::nanoseconds(nanos));
    return makeBool(true);
}

STARBYTES_FUNC(Time_monotonicNow) {
    skipOptionalModuleReceiver(args,0);
    return makeInstantObject(steadyNowNanos());
}

STARBYTES_FUNC(Time_elapsedSince) {
    skipOptionalModuleReceiver(args,1);
    auto startObj = StarbytesFuncArgsGetArg(args);
    int64_t startTicks = 0;
    if(!getInstantTicks(startObj,startTicks)) {
        return nullptr;
    }

    auto nowTicks = steadyNowNanos();
    int64_t elapsed = 0;
    if(!checkedSubInt64(nowTicks,startTicks,elapsed)) {
        return nullptr;
    }
    return makeDurationObject(elapsed);
}

STARBYTES_FUNC(Time_utcNow) {
    skipOptionalModuleReceiver(args,0);
    auto now = std::chrono::system_clock::now();
    auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch());
    auto sec = std::chrono::duration_cast<std::chrono::seconds>(nanos);
    auto sub = nanos - sec;
    return makeDateTimeObject((int64_t)sec.count(),(int32_t)sub.count(),0,"UTC");
}

STARBYTES_FUNC(Time_localNow) {
    skipOptionalModuleReceiver(args,0);
    auto now = std::chrono::system_clock::now();
    auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch());
    auto sec = std::chrono::duration_cast<std::chrono::seconds>(nanos);
    auto sub = nanos - sec;
    auto epochSec = (int64_t)sec.count();
    auto offset = localOffsetMinutesForEpoch(epochSec);
    return makeDateTimeObject(epochSec,(int32_t)sub.count(),offset,"LOCAL");
}

STARBYTES_FUNC(Time_timezoneUTC) {
    skipOptionalModuleReceiver(args,0);
    return makeTimeZoneObject("UTC",0);
}

STARBYTES_FUNC(Time_timezoneLocal) {
    skipOptionalModuleReceiver(args,0);
    auto offset = localOffsetMinutesForEpoch((int64_t)std::time(nullptr));
    return makeTimeZoneObject("LOCAL",offset);
}

STARBYTES_FUNC(Time_timezoneFromID) {
    skipOptionalModuleReceiver(args,1);

    std::string id;
    if(!readStringArg(args,id)) {
        return nullptr;
    }

    int offset = 0;
    std::string normalized;
    if(!parseTimeZoneIdentifier(id,offset,normalized)) {
        return nullptr;
    }
    return makeTimeZoneObject(normalized,offset);
}

STARBYTES_FUNC(Time_parseISO8601) {
    skipOptionalModuleReceiver(args,1);

    std::string text;
    if(!readStringArg(args,text)) {
        return nullptr;
    }

    static const std::regex pattern(
        R"(^([0-9]{4})-([0-9]{2})-([0-9]{2})[Tt ]([0-9]{2}):([0-9]{2}):([0-9]{2})(?:\.([0-9]{1,9}))?(Z|[+\-][0-9]{2}:[0-9]{2}|[+\-][0-9]{4})$)");

    std::smatch match;
    if(!std::regex_match(text,match,pattern)) {
        return nullptr;
    }

    int year = std::stoi(match[1].str());
    int month = std::stoi(match[2].str());
    int day = std::stoi(match[3].str());
    int hour = std::stoi(match[4].str());
    int minute = std::stoi(match[5].str());
    int second = std::stoi(match[6].str());

    if(month < 1 || month > 12 || day < 1 || day > 31 || hour > 23 || minute > 59 || second > 60) {
        return nullptr;
    }

    int32_t nanos = 0;
    if(match[7].matched) {
        auto fraction = match[7].str();
        while(fraction.size() < 9) {
            fraction.push_back('0');
        }
        nanos = (int32_t)std::stoi(fraction.substr(0,9));
    }

    int offset = 0;
    auto zone = match[8].str();
    if(zone != "Z") {
        if(!parseHHMMOffset(zone,offset)) {
            return nullptr;
        }
    }

    int64_t wallEpoch = 0;
    if(!civilToUnixUTC(year,month,day,hour,minute,second,wallEpoch)) {
        return nullptr;
    }

    int64_t unixSeconds = 0;
    if(!checkedSubInt64(wallEpoch,(int64_t)offset * 60,unixSeconds)) {
        return nullptr;
    }

    return makeDateTimeObject(unixSeconds,nanos,offset,timezoneIdFromOffset(offset));
}

STARBYTES_FUNC(Time_formatISO8601) {
    skipOptionalModuleReceiver(args,2);

    DateTimeState dateTime;
    TimeZoneState tzState;
    if(!readDateTimeAndTargetTz(args,dateTime,tzState)) {
        return nullptr;
    }

    DateTimeState shifted = dateTime;
    shifted.offsetMinutes = tzState.offsetMinutes;
    shifted.tzId = tzState.id;

    DateParts parts;
    if(!datePartsFromState(shifted,parts)) {
        return nullptr;
    }

    std::ostringstream out;
    out << std::setw(4) << std::setfill('0') << parts.year
        << "-"
        << std::setw(2) << std::setfill('0') << parts.month
        << "-"
        << std::setw(2) << std::setfill('0') << parts.day
        << "T"
        << std::setw(2) << std::setfill('0') << parts.hour
        << ":"
        << std::setw(2) << std::setfill('0') << parts.minute
        << ":"
        << std::setw(2) << std::setfill('0') << parts.second;

    if(shifted.nanosecond > 0) {
        std::ostringstream frac;
        frac << std::setw(9) << std::setfill('0') << shifted.nanosecond;
        auto fracStr = frac.str();
        while(!fracStr.empty() && fracStr.back() == '0') {
            fracStr.pop_back();
        }
        if(!fracStr.empty()) {
            out << "." << fracStr;
        }
    }

    out << formatOffsetSuffix(shifted.offsetMinutes,true);
    return makeString(out.str());
}

STARBYTES_FUNC(Time_fromUnix) {
    skipOptionalModuleReceiver(args,3);

    int seconds = 0;
    int nanos = 0;
    if(!readIntArg(args,seconds) || !readIntArg(args,nanos)) {
        return nullptr;
    }
    auto tzObj = StarbytesFuncArgsGetArg(args);
    TimeZoneState tz;
    if(!tzObj || !getTimeZoneState(tzObj,tz)) {
        return nullptr;
    }
    if(nanos < 0 || nanos > 999999999) {
        return nullptr;
    }

    return makeDateTimeObject((int64_t)seconds,(int32_t)nanos,tz.offsetMinutes,tz.id);
}

STARBYTES_FUNC(Time_convertTimeZone) {
    skipOptionalModuleReceiver(args,2);

    auto dtObj = StarbytesFuncArgsGetArg(args);
    auto tzObj = StarbytesFuncArgsGetArg(args);

    DateTimeState dt;
    TimeZoneState tz;
    if(!dtObj || !tzObj || !getDateTimeState(dtObj,dt) || !getTimeZoneState(tzObj,tz)) {
        return nullptr;
    }

    return makeDateTimeObject(dt.unixSeconds,dt.nanosecond,tz.offsetMinutes,tz.id);
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

    addFunc(module,"Time_Duration_nanoseconds",1,Time_Duration_nanoseconds);
    addFunc(module,"Time_Duration_milliseconds",1,Time_Duration_milliseconds);
    addFunc(module,"Time_Duration_secondsFloat",1,Time_Duration_secondsFloat);

    addFunc(module,"Time_Instant_ticks",1,Time_Instant_ticks);

    addFunc(module,"Time_TimeZone_id",1,Time_TimeZone_id);
    addFunc(module,"Time_TimeZone_offsetMinutes",1,Time_TimeZone_offsetMinutes);

    addFunc(module,"Time_DateTime_unixSeconds",1,Time_DateTime_unixSeconds);
    addFunc(module,"Time_DateTime_nanosecond",1,Time_DateTime_nanosecond);
    addFunc(module,"Time_DateTime_year",1,Time_DateTime_year);
    addFunc(module,"Time_DateTime_month",1,Time_DateTime_month);
    addFunc(module,"Time_DateTime_day",1,Time_DateTime_day);
    addFunc(module,"Time_DateTime_hour",1,Time_DateTime_hour);
    addFunc(module,"Time_DateTime_minute",1,Time_DateTime_minute);
    addFunc(module,"Time_DateTime_second",1,Time_DateTime_second);
    addFunc(module,"Time_DateTime_timezone",1,Time_DateTime_timezone);

    addFunc(module,"Time_durationZero",0,Time_durationZero);
    addFunc(module,"Time_durationFromNanos",1,Time_durationFromNanos);
    addFunc(module,"Time_durationFromMillis",1,Time_durationFromMillis);
    addFunc(module,"Time_durationFromSeconds",1,Time_durationFromSeconds);
    addFunc(module,"Time_durationAdd",2,Time_durationAdd);
    addFunc(module,"Time_durationSub",2,Time_durationSub);
    addFunc(module,"Time_durationMul",2,Time_durationMul);
    addFunc(module,"Time_durationDiv",2,Time_durationDiv);
    addFunc(module,"Time_durationCompare",2,Time_durationCompare);
    addFunc(module,"Time_sleep",1,Time_sleep);

    addFunc(module,"Time_monotonicNow",0,Time_monotonicNow);
    addFunc(module,"Time_elapsedSince",1,Time_elapsedSince);

    addFunc(module,"Time_utcNow",0,Time_utcNow);
    addFunc(module,"Time_localNow",0,Time_localNow);

    addFunc(module,"Time_timezoneUTC",0,Time_timezoneUTC);
    addFunc(module,"Time_timezoneLocal",0,Time_timezoneLocal);
    addFunc(module,"Time_timezoneFromID",1,Time_timezoneFromID);

    addFunc(module,"Time_parseISO8601",1,Time_parseISO8601);
    addFunc(module,"Time_formatISO8601",2,Time_formatISO8601);
    addFunc(module,"Time_fromUnix",3,Time_fromUnix);
    addFunc(module,"Time_convertTimeZone",2,Time_convertTimeZone);

    return module;
}
