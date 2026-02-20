#include "starbytes/base/Diagnostic.h"
#include "starbytes/base/CodeView.h"
#include <chrono>
#include <iostream>
#include <ostream>
#include <sstream>
#include <cassert>

namespace starbytes {

static bool regionHasLocation(const Region &region){
    return region.startLine > 0 || region.endLine > 0 || region.startCol > 0 || region.endCol > 0;
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
        metrics.pushedCount += 1;
        if(diagnostic->isError()){
            metrics.errorCount += 1;
        }
        else {
            metrics.warningCount += 1;
        }
        if(diagnostic->location.has_value()){
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
    if(!codeView){
        codeView = std::make_unique<CodeView>();
    }
    codeView->setSource(std::move(sourceName),std::move(sourceText));
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

void DiagnosticHandler::clear(){
    buffer.clear();
}

void DiagnosticHandler::logAll(){
    while(!buffer.empty()){
        auto start = std::chrono::steady_clock::now();
        auto & d = buffer.front();
        if(!d){
            metrics.skippedResolvedCount += 1;
        }
        else if(!d->resolved){
            d->send(logger,codeView.get());
            logger.lineBreak();
            metrics.renderedCount += 1;
        }
        else {
            metrics.skippedResolvedCount += 1;
        };
        auto end = std::chrono::steady_clock::now();
        metrics.renderTimeNs += std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        buffer.pop_front();
    };
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
