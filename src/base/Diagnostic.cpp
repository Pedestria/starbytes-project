#include "starbytes/base/Diagnostic.h"
#include "starbytes/base/CodeView.h"
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <limits>
#include <ostream>
#include <sstream>
#include <cassert>
#include <unordered_map>
#include <unordered_set>

namespace starbytes {

static bool regionHasLocation(const Region &region){
    return region.startLine > 0 || region.endLine > 0 || region.startCol > 0 || region.endCol > 0;
}

static uint64_t regionLineStart(const optional<Region> &region){
    if(!region.has_value()){
        return std::numeric_limits<uint64_t>::max();
    }
    return region.value().startLine;
}

static uint64_t regionColStart(const optional<Region> &region){
    if(!region.has_value()){
        return std::numeric_limits<uint64_t>::max();
    }
    return region.value().startCol;
}

static uint64_t regionLineEnd(const optional<Region> &region){
    if(!region.has_value()){
        return std::numeric_limits<uint64_t>::max();
    }
    return region.value().endLine;
}

static uint64_t regionColEnd(const optional<Region> &region){
    if(!region.has_value()){
        return std::numeric_limits<uint64_t>::max();
    }
    return region.value().endCol;
}

static uint32_t stableHash32(const std::string &text){
    uint32_t hash = 2166136261u;
    for(unsigned char c : text){
        hash ^= c;
        hash *= 16777619u;
    }
    return hash;
}

static const char *phaseToCodeFamily(Diagnostic::Phase phase){
    switch(phase){
        case Diagnostic::Phase::Parser:
            return "SB-PARSE";
        case Diagnostic::Phase::Semantic:
            return "SB-SEMA";
        case Diagnostic::Phase::Runtime:
            return "SB-RUNTIME";
        case Diagnostic::Phase::Lsp:
            return "SB-LSP";
        case Diagnostic::Phase::Unknown:
        default:
            return "SB-GENERAL";
    }
}

static const char *phaseToString(Diagnostic::Phase phase){
    switch(phase){
        case Diagnostic::Phase::Parser:
            return "parser";
        case Diagnostic::Phase::Semantic:
            return "semantic";
        case Diagnostic::Phase::Runtime:
            return "runtime";
        case Diagnostic::Phase::Lsp:
            return "lsp";
        case Diagnostic::Phase::Unknown:
        default:
            return "unknown";
    }
}

static std::string regionIdentity(const optional<Region> &region){
    if(!region.has_value()){
        return "-";
    }
    const auto &r = region.value();
    std::ostringstream out;
    out << r.startLine << ":" << r.startCol << ":" << r.endLine << ":" << r.endCol;
    return out.str();
}

static bool isCascadeDiagnosticMessage(const std::string &message){
    return message.rfind("Context:",0) == 0 || message.rfind("Related:",0) == 0;
}

static DiagnosticRecord diagnosticToRecord(const Diagnostic &diag){
    DiagnosticRecord record;
    record.message = diag.message;
    record.location = diag.location;
    record.id = diag.id;
    record.code = diag.code;
    record.phase = diag.phase;
    record.relatedSpans = diag.relatedSpans;
    record.notes = diag.notes;
    record.fixits = diag.fixits;
    record.sourceName = diag.sourceName;
    record.severity = diag.t;
    return record;
}

struct DiagnosticNormalizationResult {
    std::vector<DiagnosticRecord> records;
    uint64_t deduplicatedCount = 0;
    uint64_t cascadeSuppressedCount = 0;
};

static DiagnosticNormalizationResult normalizeDiagnosticRecords(std::vector<DiagnosticRecord> records,
                                                                bool deduplicate,
                                                                bool collapseCascade){
    DiagnosticNormalizationResult result;
    if(records.empty()){
        return result;
    }

    if(deduplicate){
        std::unordered_set<std::string> seen;
        seen.reserve(records.size() * 2);
        std::vector<DiagnosticRecord> deduped;
        deduped.reserve(records.size());
        for(auto &record : records){
            std::ostringstream key;
            key << record.sourceName << "|"
                << static_cast<int>(record.phase) << "|"
                << static_cast<int>(record.severity) << "|"
                << record.code << "|"
                << regionIdentity(record.location) << "|"
                << record.message;
            auto inserted = seen.insert(key.str());
            if(!inserted.second){
                result.deduplicatedCount += 1;
                continue;
            }
            deduped.push_back(std::move(record));
        }
        records = std::move(deduped);
    }

    if(collapseCascade && !records.empty()){
        std::unordered_map<std::string,size_t> rootBySpan;
        rootBySpan.reserve(records.size());
        std::vector<DiagnosticRecord> collapsed;
        collapsed.reserve(records.size());
        for(auto &record : records){
            std::ostringstream spanKeyStream;
            spanKeyStream << record.sourceName << "|"
                          << static_cast<int>(record.phase) << "|"
                          << static_cast<int>(record.severity) << "|"
                          << regionIdentity(record.location);
            auto spanKey = spanKeyStream.str();
            bool isCascade = isCascadeDiagnosticMessage(record.message);
            auto rootIt = rootBySpan.find(spanKey);

            if(isCascade && rootIt != rootBySpan.end()){
                auto &root = collapsed[rootIt->second];
                root.notes.push_back("suppressed cascade: " + record.message);
                result.cascadeSuppressedCount += 1;
                continue;
            }

            collapsed.push_back(std::move(record));
            size_t newIndex = collapsed.size() - 1;
            if(!isCascade || rootIt == rootBySpan.end()){
                rootBySpan[spanKey] = newIndex;
            }
        }
        records = std::move(collapsed);
    }

    std::stable_sort(records.begin(),records.end(),[](const DiagnosticRecord &lhs,const DiagnosticRecord &rhs){
        if(lhs.sourceName != rhs.sourceName){
            return lhs.sourceName < rhs.sourceName;
        }
        if(regionLineStart(lhs.location) != regionLineStart(rhs.location)){
            return regionLineStart(lhs.location) < regionLineStart(rhs.location);
        }
        if(regionColStart(lhs.location) != regionColStart(rhs.location)){
            return regionColStart(lhs.location) < regionColStart(rhs.location);
        }
        if(regionLineEnd(lhs.location) != regionLineEnd(rhs.location)){
            return regionLineEnd(lhs.location) < regionLineEnd(rhs.location);
        }
        if(regionColEnd(lhs.location) != regionColEnd(rhs.location)){
            return regionColEnd(lhs.location) < regionColEnd(rhs.location);
        }
        if(lhs.severity != rhs.severity){
            return lhs.severity < rhs.severity;
        }
        if(lhs.code != rhs.code){
            return lhs.code < rhs.code;
        }
        if(lhs.message != rhs.message){
            return lhs.message < rhs.message;
        }
        return lhs.id < rhs.id;
    });

    result.records = std::move(records);
    return result;
}

static std::string jsonEscape(const std::string &text){
    std::ostringstream out;
    for(char ch : text){
        switch(ch){
            case '\"': out << "\\\""; break;
            case '\\': out << "\\\\"; break;
            case '\b': out << "\\b"; break;
            case '\f': out << "\\f"; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default:
                if(static_cast<unsigned char>(ch) < 0x20){
                    out << "\\u"
                        << std::hex << std::setw(4) << std::setfill('0')
                        << static_cast<int>(static_cast<unsigned char>(ch))
                        << std::dec << std::setfill(' ');
                }
                else {
                    out << ch;
                }
                break;
        }
    }
    return out.str();
}

static void renderHumanDiagnosticRecord(const DiagnosticRecord &record,
                                        StreamLogger &logger,
                                        const CodeView *view){
    if(record.isError()){
        logger.color(LOGGER_COLOR_RED);
        logger.string("error:");
        logger.color(LOGGER_COLOR_RESET);
        logger.lineBreak();
        logger.string(record.message);
    }
    else {
        logger.color(LOGGER_COLOR_YELLOW);
        logger.string("warning:");
        logger.color(LOGGER_COLOR_RESET);
        logger.lineBreak();
        logger.string(record.message);
    }
    if(!record.code.empty()){
        logger.lineBreak();
        std::string codeLine = "[" + record.code + "]";
        logger.string(codeLine);
    }
    for(const auto &note : record.notes){
        logger.lineBreak();
        std::string noteLine = "note: " + note;
        logger.string(noteLine);
    }
    if(record.location.has_value() && view && view->hasSource() && regionHasLocation(record.location.value())){
        auto rendered = view->renderRegion(record.location.value());
        if(!rendered.empty()){
            logger.lineBreak();
            logger.string(rendered);
        }
    }
}

static void renderMachineJsonDiagnosticRecord(const DiagnosticRecord &record,StreamLogger &logger){
    std::ostringstream out;
    out << "{\"severity\":\"" << (record.isError() ? "error" : "warning") << "\"";
    out << ",\"message\":\"" << jsonEscape(record.message) << "\"";
    if(!record.code.empty()){
        out << ",\"code\":\"" << jsonEscape(record.code) << "\"";
    }
    if(!record.id.empty()){
        out << ",\"id\":\"" << jsonEscape(record.id) << "\"";
    }
    out << ",\"phase\":\"" << phaseToString(record.phase) << "\"";
    if(!record.sourceName.empty()){
        out << ",\"source\":\"" << jsonEscape(record.sourceName) << "\"";
    }
    if(record.location.has_value() && regionHasLocation(record.location.value())){
        const auto &loc = record.location.value();
        out << ",\"location\":{\"startLine\":" << loc.startLine
            << ",\"startCol\":" << loc.startCol
            << ",\"endLine\":" << loc.endLine
            << ",\"endCol\":" << loc.endCol
            << "}";
    }
    if(!record.notes.empty()){
        out << ",\"notes\":[";
        for(size_t i = 0;i < record.notes.size();++i){
            if(i > 0){
                out << ",";
            }
            out << "\"" << jsonEscape(record.notes[i]) << "\"";
        }
        out << "]";
    }
    out << "}";
    logger.string(out.str());
}

static void applyLegacyDiagnosticSchema(Diagnostic &diag,
                                        Diagnostic::Phase defaultPhase,
                                        const std::string &defaultSourceName){
    if(diag.phase == Diagnostic::Phase::Unknown && defaultPhase != Diagnostic::Phase::Unknown){
        diag.phase = defaultPhase;
    }

    if(diag.sourceName.empty() && !defaultSourceName.empty()){
        diag.sourceName = defaultSourceName;
    }

    const bool hasPrimarySpan = diag.location.has_value() && regionHasLocation(diag.location.value());
    if(diag.code.empty()){
        std::string fingerprint = diag.message;
        if(hasPrimarySpan){
            const auto &region = diag.location.value();
            fingerprint += "|";
            fingerprint += std::to_string(region.startLine);
            fingerprint += ":";
            fingerprint += std::to_string(region.startCol);
            fingerprint += ":";
            fingerprint += std::to_string(region.endLine);
            fingerprint += ":";
            fingerprint += std::to_string(region.endCol);
        }
        auto hash = stableHash32(fingerprint);
        std::ostringstream codeBuilder;
        codeBuilder << phaseToCodeFamily(diag.phase)
                    << (diag.isError() ? "-E" : "-W")
                    << std::uppercase << std::hex << std::setw(4) << std::setfill('0')
                    << (hash & 0xFFFFu);
        diag.code = codeBuilder.str();
    }

    if(diag.id.empty()){
        std::ostringstream idBuilder;
        idBuilder << diag.code;
        if(hasPrimarySpan){
            idBuilder << "@";
            if(!diag.sourceName.empty()){
                idBuilder << diag.sourceName << ":";
            }
            idBuilder << diag.location->startLine << ":" << diag.location->startCol;
        }
        diag.id = idBuilder.str();
    }
}

StreamLogger::StreamLogger(std::ostream & out):out(out){

}

void StreamLogger::open(){
    
}

void StreamLogger::color(unsigned color){
    #ifdef _WIN32
    out << "\033[" << color << "m";
    #else 
    out << "\e[" << color << "m";
    #endif
}

void StreamLogger::floatPoint(const float &f){
    out << std::to_string(f);
}

void StreamLogger::integer(const int &i){
    out << i;
}

void StreamLogger::string(string_ref s){
    if(s.size() > 0 && s.getBuffer()) {
        out.write(s.getBuffer(),s.size());
    }
}

void StreamLogger::lineBreak(unsigned c){
    while(c > 0){
        out << "\n";
        --c;
    }
    out.flush();
}

void StreamLogger::close(){
    out.flush();
}

DiagnosticHandler::DiagnosticHandler(std::ostream & out):logger(out),codeView(nullptr){

}

DiagnosticHandler::~DiagnosticHandler() = default;


std::unique_ptr<DiagnosticHandler> DiagnosticHandler::createDefault(std::ostream & out){
    return std::unique_ptr<DiagnosticHandler>(new DiagnosticHandler(out));
}

DiagnosticHandler & DiagnosticHandler::push(DiagnosticPtr diagnostic){
    auto start = std::chrono::steady_clock::now();
    if(diagnostic){
        applyLegacyDiagnosticSchema(*diagnostic,defaultPhase,defaultSourceName);
        metrics.pushedCount += 1;
        if(diagnostic->isError()){
            metrics.errorCount += 1;
        }
        else {
            metrics.warningCount += 1;
        }
        if(diagnostic->location.has_value() && regionHasLocation(diagnostic->location.value())){
            metrics.withLocationCount += 1;
        }
    }
    buffer.emplace_back(diagnostic);
    if(buffer.size() > metrics.maxBufferedCount){
        metrics.maxBufferedCount = buffer.size();
    }
    auto end = std::chrono::steady_clock::now();
    metrics.pushTimeNs += std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    return *this;
};

void DiagnosticHandler::setCodeViewSource(std::string sourceName,std::string sourceText){
    defaultSourceName = sourceName;
    if(outputMode != OutputMode::Human){
        return;
    }
    if(!codeView){
        codeView = std::make_unique<CodeView>();
    }
    codeView->setSource(std::move(sourceName),std::move(sourceText));
}

void DiagnosticHandler::setDefaultSourceName(std::string sourceName){
    defaultSourceName = std::move(sourceName);
}

const std::string &DiagnosticHandler::getDefaultSourceName() const{
    return defaultSourceName;
}

void DiagnosticHandler::setDefaultPhase(Diagnostic::Phase phase){
    defaultPhase = phase;
}

Diagnostic::Phase DiagnosticHandler::getDefaultPhase() const{
    return defaultPhase;
}

void DiagnosticHandler::setOutputMode(OutputMode mode){
    outputMode = mode;
    if(outputMode != OutputMode::Human){
        codeView.reset();
    }
}

DiagnosticHandler::OutputMode DiagnosticHandler::getOutputMode() const{
    return outputMode;
}

void DiagnosticHandler::setDeduplicationEnabled(bool enabled){
    deduplicationEnabled = enabled;
}

bool DiagnosticHandler::getDeduplicationEnabled() const{
    return deduplicationEnabled;
}

void DiagnosticHandler::setCascadeCollapseEnabled(bool enabled){
    cascadeCollapseEnabled = enabled;
}

bool DiagnosticHandler::getCascadeCollapseEnabled() const{
    return cascadeCollapseEnabled;
}

bool DiagnosticHandler::empty(){
    return buffer.empty();
};

bool DiagnosticHandler::hasErrored(){
    auto start = std::chrono::steady_clock::now();
    for(auto & diag : buffer){
        if(diag && diag->isError()){
            auto end = std::chrono::steady_clock::now();
            metrics.hasErroredTimeNs += std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
            return true;
            break;
        };
    };
    auto end = std::chrono::steady_clock::now();
    metrics.hasErroredTimeNs += std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    return false;
};

std::vector<DiagnosticPtr> DiagnosticHandler::snapshot(bool includeResolved) const{
    std::vector<DiagnosticPtr> out;
    out.reserve(buffer.size());
    for(const auto &diag : buffer){
        if(!diag){
            continue;
        }
        if(!includeResolved && diag->resolved){
            continue;
        }
        out.push_back(diag);
    }
    return out;
}

std::vector<DiagnosticRecord> DiagnosticHandler::collectRecords(bool includeResolved,bool applyAggregation) const{
    std::vector<DiagnosticRecord> records;
    records.reserve(buffer.size());
    for(const auto &diag : buffer){
        if(!diag){
            continue;
        }
        if(!includeResolved && diag->resolved){
            continue;
        }
        records.push_back(diagnosticToRecord(*diag));
    }
    if(!applyAggregation){
        return records;
    }
    auto normalized = normalizeDiagnosticRecords(std::move(records),deduplicationEnabled,cascadeCollapseEnabled);
    return std::move(normalized.records);
}

std::vector<DiagnosticRecord> DiagnosticHandler::collectLspRecords(bool includeResolved) const{
    return collectRecords(includeResolved,true);
}

void DiagnosticHandler::clear(){
    buffer.clear();
}

void DiagnosticHandler::logAll(){
    auto normalized = normalizeDiagnosticRecords(collectRecords(false,false),deduplicationEnabled,cascadeCollapseEnabled);
    metrics.deduplicatedCount += normalized.deduplicatedCount;
    metrics.cascadeSuppressedCount += normalized.cascadeSuppressedCount;

    for(const auto &record : normalized.records){
        auto start = std::chrono::steady_clock::now();
        if(outputMode == OutputMode::Human){
            renderHumanDiagnosticRecord(record,logger,codeView.get());
            logger.lineBreak();
        }
        else if(outputMode == OutputMode::MachineJson || outputMode == OutputMode::Machine){
            renderMachineJsonDiagnosticRecord(record,logger);
            logger.lineBreak();
        }
        metrics.renderedCount += 1;
        auto end = std::chrono::steady_clock::now();
        metrics.renderTimeNs += std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    }

    for(const auto &d : buffer){
        if(!d || d->resolved){
            metrics.skippedResolvedCount += 1;
        }
    }
    buffer.clear();
};

DiagnosticHandler::Metrics DiagnosticHandler::getMetrics() const{
    return metrics;
}

void DiagnosticHandler::resetMetrics(){
    metrics = {};
}

bool Diagnostic::isError(){
    return t == Diagnostic::Error;
}

std::string Diagnostic::phaseString() const{
    return phaseToString(phase);
}
DiagnosticPtr StandardDiagnostic::createError(string_ref message){
    StandardDiagnostic d {};
    d.message = message.str();
    d.t = Diagnostic::Error;
    return std::make_shared<StandardDiagnostic>(std::move(d));
}

DiagnosticPtr StandardDiagnostic::createError(string_ref message,const Region &region){
    StandardDiagnostic d {};
    d.message = message.str();
    d.t = Diagnostic::Error;
    d.location = region;
    return std::make_shared<StandardDiagnostic>(std::move(d));
}

DiagnosticPtr StandardDiagnostic::createWarning(string_ref message){
    StandardDiagnostic d {};
    d.message = message.str();
    d.t = Diagnostic::Warning;
    return std::make_shared<StandardDiagnostic>(std::move(d));
}

DiagnosticPtr StandardDiagnostic::createWarning(string_ref message,const Region &region){
    StandardDiagnostic d {};
    d.message = message.str();
    d.t = Diagnostic::Warning;
    d.location = region;
    return std::make_shared<StandardDiagnostic>(std::move(d));
}

auto ERROR_MESSAGE = "error:";
auto WARNING_MESSAGE = "warning:";

void StandardDiagnostic::send(StreamLogger & logger,const CodeView *view){
    if(t == Error){
        logger.color(LOGGER_COLOR_RED);
        logger.string(const_cast<char *>(ERROR_MESSAGE));
        logger.color(LOGGER_COLOR_RESET);
        logger.lineBreak();
        logger.string(message);
    }
    else {
        logger.color(LOGGER_COLOR_YELLOW);
        logger.string(const_cast<char *>(WARNING_MESSAGE));
        logger.color(LOGGER_COLOR_RESET);
        logger.lineBreak();
        logger.string(message);
    }
    if(location.has_value() && view && view->hasSource() && regionHasLocation(location.value())){
        auto rendered = view->renderRegion(location.value());
        if(!rendered.empty()){
            logger.lineBreak();
            logger.string(rendered);
        }
    }
};


inline int power_int(int & val,unsigned power){
        int result = val;
        for(;power > 0;power--){
            result *= val;
        }
        return result;
    }


    class ScalableInteger {
        std::vector<int> buffer;
    public:
        void addPlace(int val){
            for(auto & d : buffer){
                d *= power_int(d,buffer.size());
            }
            buffer.push_back(val);
        }
        int val(){
            int _v = 0;
            for(auto & d : buffer){
                _v += d;
            }
            return _v;
        };
    };

    class Formatter {
        string_ref fmt;
        std::ostream & out;
    public:
        Formatter(string_ref & fmt, std::ostream & out): fmt(fmt), out(out){};
        void format(array_ref<ObjectFormatProviderBase *> & objectFormatProviders){
            std::istringstream in(fmt);

            auto getChar = [&](){
                return (char) in.get();
            };

            auto aheadChar = [&](){
                char c = in.get();
                in.seekg(-1,std::ios::cur);
                return c;
            };

            std::ostringstream tempBuffer;

            char c;
            while((c = getChar()) != -1){
                switch (c) {
                    case '@' : {
                        tempBuffer.str("");

                        tempBuffer << c;
                        c = getChar();
                        if(c == '{'){
                            ScalableInteger scalableInteger;
                            for(;(c = getChar()) != '}';){
                                if(!std::isdigit(c)){
                                    std::cout << "FORMATTER ERROR:" << "Character " << c << "is not a digit!" << std::endl;
                                    exit(1);
                                };
                                scalableInteger.addPlace(int(c - 0x30));
                            }

                            int val = scalableInteger.val();

                            // std::cout << "Value:" << val << std::endl;

                            assert(val < objectFormatProviders.size());
                            objectFormatProviders[val]->insertFormattedObject(out);
                        }
                        else {
                            out << tempBuffer.str();
                        }
                        break;
                    }
                    default: {
                        out << c;
                        break;
                    }
                }
            }

        }
        ~Formatter()= default;
    };

    Formatter *createFormatter(string_ref fmt, std::ostream & out){
        return new Formatter(fmt,out);
    };

    void format(Formatter * formatter,array_ref<ObjectFormatProviderBase *> objectFormatProviders){
        formatter->format(objectFormatProviders);
    };

    void freeFormatter(Formatter *formatter){
        delete formatter;
    };

};
