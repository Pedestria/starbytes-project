#include "starbytes/base/Diagnostic.h"
#include <iostream>
#include <ostream>
#include <sstream>
#include <cassert>

namespace starbytes {

inline void makeCarrots(unsigned start,unsigned length,std::ostream & out){
    out << "\x1b[31m";
    while(start > 0){
        out << " " << std::endl;
        --start;
    }
    while(length > 0){
        out << "^" << std::flush;
        --length;
    }
    out << "\x1b[0m" << std::endl;
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

void StreamLogger::string(string_ref s){
    out.write(s.getBuffer(),s.size());
}

void StreamLogger::lineBreak(unsigned c){
    if(c > 0){
        out << "\n";
        out.flush();
        --c;
    }
}

void StreamLogger::close(){
    out.flush();
}

DiagnosticHandler::DiagnosticHandler(std::ostream & out):logger(out){

}


std::unique_ptr<DiagnosticHandler> DiagnosticHandler::createDefault(std::ostream & out){
    return std::make_unique<DiagnosticHandler>(DiagnosticHandler(out));
}

DiagnosticHandler & DiagnosticHandler::push(DiagnosticPtr diagnostic){
    buffer.emplace_back(diagnostic);
    return *this;
};

bool DiagnosticHandler::empty(){
    return buffer.empty();
};

bool DiagnosticHandler::hasErrored(){
    for(auto & diag : buffer){
        if(diag->isError()){
            return true;
            break;
        };
    };
    return false;
};

void DiagnosticHandler::logAll(){
    while(!buffer.empty()){
        auto & d = buffer.front();
        if(!d->resolved){
            d->send(logger);
        };
        buffer.pop_front();
    };
};

bool Diagnostic::isError(){
    return t == Diagnostic::Error;
}
DiagnosticPtr StandardDiagnostic::createError(string_ref message){
    StandardDiagnostic d {};
    d.message = message.getBuffer();
    d.t = Diagnostic::Error;
    return std::make_shared<StandardDiagnostic>(std::move(d));
}

DiagnosticPtr StandardDiagnostic::createWarning(string_ref message){
    StandardDiagnostic d {};
    d.message = message.getBuffer();
    d.t = Diagnostic::Warning;
    return std::make_shared<StandardDiagnostic>(std::move(d));
}

auto ERROR_MESSAGE = "error:";

void StandardDiagnostic::send(StreamLogger & logger){
    if(t == Error){
        logger.color(LOGGER_COLOR_RED);
        logger.string(const_cast<char *>(ERROR_MESSAGE));
        logger.color(LOGGER_COLOR_RESET);
        logger.lineBreak();
        logger.string(message);
    }
    else {

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
