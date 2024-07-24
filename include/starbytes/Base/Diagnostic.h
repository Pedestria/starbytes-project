#include <deque>
#include <iostream>
#include <sstream>

#include "ADT.h"

#ifndef STARBYTES_BASE_DIAGNOSTIC_H
#define STARBYTES_BASE_DIAGNOSTIC_H

namespace starbytes {

#define LOGGER_COLOR_RESET 0x00
#define LOGGER_COLOR_RED 0x00
#define LOGGER_COLOR_BLUE 0x00
#define LOGGER_COLOR_GREEN 0x00
#define LOGGER_COLOR_YELLOW 0x00

struct Region{
    unsigned startCol;
    unsigned startLine;
    unsigned endCol;
    unsigned endLine;
};

 

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
    std::string message;
    typedef enum : int {
        Error,
        Warning,
    } Type;
    Type t;
    bool resolved = false;

    Type getType();
    bool isError();
    
    virtual void send(StreamLogger & os) {};
    virtual ~Diagnostic() = default;
};

typedef std::shared_ptr<Diagnostic> DiagnosticPtr;

struct StandardDiagnostic : public Diagnostic {
    static DiagnosticPtr createError(string_ref message);
    static DiagnosticPtr createWarning(string_ref message);
    void send(StreamLogger & os) override;
};

class DiagnosticHandler {
    protected:
    std::deque<DiagnosticPtr> buffer;
    StreamLogger logger;

    using SELF = DiagnosticHandler;

    DiagnosticHandler(std::ostream & out);

    public:
    static std::unique_ptr<DiagnosticHandler> createDefault(std::ostream & out);
    SELF & push(DiagnosticPtr diagnostic);
    bool empty();
    bool hasErrored();
    void logAll();
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
