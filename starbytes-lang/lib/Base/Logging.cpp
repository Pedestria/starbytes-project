#include "starbytes/Base/Logging.h"
#include "starbytes/Base/ADT.h"
#include <cstring>

STARBYTES_FOUNDATION_NAMESPACE
/// Format a string and return a cpp string!
/// Example:
/// fstring("Error Message: @r")
std::string fstring(const char *format,...){
    char buffer[500];
    char * bufptr = buffer;
    va_list args;
    va_start(args,format);
    char c = *format;
    while(c != '\0'){
        c = *format;
        if(c == '@'){
            ++format;
            c = *format;
            if(c == 'r'){
                Foundation::StrRef str = va_arg(args,Foundation::StrRef);
                std::cout << str.size() << std::endl;
                memcpy(bufptr,str.begin(),str.size());
            }
            else if(c == 's'){
                char *str = va_arg(args,char *);
                memcpy(bufptr,str,strlen(str));
            }
            else if(c == 'i'){
                int i = va_arg(args,int);
                std::string res = std::to_string(i);
                
//                memcpy(bufptr,res.begin(),res.size());
            };
        }
        else {
            *bufptr = c;
        }
        
        ++bufptr;
        ++format;
    };
    va_end(args);
    return buffer;
};

NAMESPACE_END
