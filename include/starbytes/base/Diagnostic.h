#include <deque>
#include <cstdint>
#include <iostream>
#include <memory>
#include <sstream>
#include <vector>

#include "ADT.h"

#ifndef STARBYTES_BASE_DIAGNOSTIC_H
#define STARBYTES_BASE_DIAGNOSTIC_H

namespace starbytes {

#define LOGGER_COLOR_RESET 0x00
#define LOGGER_COLOR_RED 31
#define LOGGER_COLOR_BLUE 34
#define LOGGER_COLOR_GREEN 32
#define LOGGER_COLOR_YELLOW 33

struct Region{
    unsigned startCol = 0;
    unsigned startLine = 0;
    unsigned endCol = 0;
    unsigned endLine = 0;
};

 
class CodeView;

   class StreamLogger {

       std::ostream & out;
       public:

        StreamLogger(std::ostream & out);

       void open();
       void string(string_ref s);
       void integer(const int & i);
       void floatPoint(const float & f);
       void lineBreak(unsigned count = 1);
       void color(unsigned color);
       void close();
   };

struct Diagnostic {
    enum class Phase : int {
        Unknown,
        Parser,
        Semantic,
        Runtime,
        Lsp
    };

    struct RelatedSpan {
        Region span;
        std::string message;
    };

    struct FixIt {
        Region span;
        std::string replacement;
        std::string message;
    };

    std::string message;
    optional<Region> location;
    std::string id;
    std::string code;
    Phase phase = Phase::Unknown;
    std::vector<RelatedSpan> relatedSpans;
    std::vector<std::string> notes;
    std::vector<FixIt> fixits;
    std::string sourceName;
    typedef enum : int {
        Error,
        Warning,
    } Type;
    Type t;
    bool resolved = false;

    Type getType();
    bool isError();
    std::string phaseString() const;
    
    virtual void send(StreamLogger & os,const CodeView *view = nullptr) {};
    virtual ~Diagnostic() = default;
};

typedef std::shared_ptr<Diagnostic> DiagnosticPtr;

struct DiagnosticRecord {
    std::string message;
    optional<Region> location;
    std::string id;
    std::string code;
    Diagnostic::Phase phase = Diagnostic::Phase::Unknown;
    std::vector<Diagnostic::RelatedSpan> relatedSpans;
    std::vector<std::string> notes;
    std::vector<Diagnostic::FixIt> fixits;
    std::string sourceName;
    Diagnostic::Type severity = Diagnostic::Error;

    bool isError() const {
        return severity == Diagnostic::Error;
    }
};

struct StandardDiagnostic : public Diagnostic {
    static DiagnosticPtr createError(string_ref message);
    static DiagnosticPtr createError(string_ref message,const Region &region);
    static DiagnosticPtr createWarning(string_ref message);
    static DiagnosticPtr createWarning(string_ref message,const Region &region);
    void send(StreamLogger & os,const CodeView *view = nullptr) override;
};

class DiagnosticHandler {
public:
    enum class OutputMode : int {
        Human,
        MachineJson,
        Machine = MachineJson,
        Lsp
    };

    struct Metrics {
        uint64_t pushedCount = 0;
        uint64_t renderedCount = 0;
        uint64_t errorCount = 0;
        uint64_t warningCount = 0;
        uint64_t withLocationCount = 0;
        uint64_t skippedResolvedCount = 0;
        uint64_t pushTimeNs = 0;
        uint64_t renderTimeNs = 0;
        uint64_t hasErroredTimeNs = 0;
        uint64_t maxBufferedCount = 0;
        uint64_t deduplicatedCount = 0;
        uint64_t cascadeSuppressedCount = 0;
    };

    protected:
    std::deque<DiagnosticPtr> buffer;
    StreamLogger logger;
    std::unique_ptr<CodeView> codeView;
    std::string defaultSourceName;
    Diagnostic::Phase defaultPhase = Diagnostic::Phase::Unknown;
    OutputMode outputMode = OutputMode::Human;
    bool deduplicationEnabled = true;
    bool cascadeCollapseEnabled = true;
    Metrics metrics;

    using SELF = DiagnosticHandler;

    DiagnosticHandler(std::ostream & out);

    public:
    ~DiagnosticHandler();
    static std::unique_ptr<DiagnosticHandler> createDefault(std::ostream & out);
    SELF & push(DiagnosticPtr diagnostic);
    void setCodeViewSource(std::string sourceName,std::string sourceText);
    void setDefaultSourceName(std::string sourceName);
    const std::string &getDefaultSourceName() const;
    void setDefaultPhase(Diagnostic::Phase phase);
    Diagnostic::Phase getDefaultPhase() const;
    void setOutputMode(OutputMode mode);
    OutputMode getOutputMode() const;
    void setDeduplicationEnabled(bool enabled);
    bool getDeduplicationEnabled() const;
    void setCascadeCollapseEnabled(bool enabled);
    bool getCascadeCollapseEnabled() const;
    bool empty();
    bool hasErrored();
    std::vector<DiagnosticPtr> snapshot(bool includeResolved = false) const;
    std::vector<DiagnosticRecord> collectRecords(bool includeResolved = false,bool applyAggregation = true) const;
    std::vector<DiagnosticRecord> collectLspRecords(bool includeResolved = false) const;
    void clear();
    void logAll();
    Metrics getMetrics() const;
    void resetMetrics();
};

static std::unique_ptr<DiagnosticHandler> stdDiagnosticHandler = DiagnosticHandler::createDefault(std::cout);

typedef std::string string;

template<typename T>
    struct FormatProvider;

    template<typename T>
    using _has_format_provider = std::is_function<decltype(FormatProvider<T>::format)>;

    struct ObjectFormatProviderBase {
        virtual void insertFormattedObject(std::ostream & os) = 0;
        virtual ~ObjectFormatProviderBase() = default;
    };

    template<class T,class Pr = FormatProvider<T>>
             struct ObjectFormatProvider : public ObjectFormatProviderBase {
        T & object;
        void insertFormattedObject(std::ostream &os) override {
            Pr::format(os,object);
        };

        template<std::enable_if_t<_has_format_provider<T>::value,int> = 0>
                explicit ObjectFormatProvider(T & object):object(object){

        };
        ~ObjectFormatProvider() = default;
    };


    template<>
    struct FormatProvider<int> {
        static void format(std::ostream & os,int & object){
            os << object;
        }
    };

    template<>
    struct FormatProvider<char> {
        static void format(std::ostream & os,char & object){
            os << object;
        }
    };

    template<>
    struct FormatProvider<std::string> {
        static void format(std::ostream & os,std::string & object){
            os << object;
        }
    };

    template<>
    struct FormatProvider<string_ref> {
        static void format(std::ostream & os, string_ref & object){
            os << object;
        }
    };

    template<typename T>
    struct FormatProvider<std::shared_ptr<T>>{
         static void format(std::ostream & os,std::shared_ptr<T> & object){
            os << "SharedHandle(0x" << std::hex << object.get() << std::dec << ") : " << std::flush;
            FormatProvider<T>::format(os,*object);
        }
    };

    class Formatter;

    Formatter *createFormatter(string_ref fmt, std::ostream & out);
    void format(Formatter * formatter,array_ref<ObjectFormatProviderBase *> objectFormatProviders);
    void freeFormatter(Formatter *formatter);

    template<typename T>
     ObjectFormatProvider<T> * buildFormatProvider(T object){
        return new ObjectFormatProvider<T>(object);
    };

    template<class ..._Args>
    string fmtString(const char *fmt,_Args && ...args){
        std::ostringstream out;
//        auto t_args = std::make_tuple(std::forward<_Args>(args)...);
        std::array<ObjectFormatProviderBase *,sizeof...(args)> arrayArgs = {buildFormatProvider(std::forward<_Args>(args))...};
        Formatter * formatter = createFormatter(fmt,out);
        format(formatter,{arrayArgs.data(),arrayArgs.data() + arrayArgs.size()});
        freeFormatter(formatter);
        for(auto a : arrayArgs){
            delete a;
        }
        
        return out.str();
    };

};

#endif
