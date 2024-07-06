#include <deque>
#include <iostream>

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
    
    virtual void send(StreamLogger & os);
    virtual ~Diagnostic() = default;
};

typedef std::unique_ptr<Diagnostic> DiagnosticPtr;

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
    SELF & operator<<(DiagnosticPtr diagnostic);
    bool empty();
    bool hasErrored();
    void logAll();
};

static std::unique_ptr<DiagnosticHandler> stdDiagnosticHandler = DiagnosticHandler::createDefault(std::cout);

};

#endif
