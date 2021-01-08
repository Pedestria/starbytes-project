#include "starbytes/Base/Base.h"

using namespace Starbytes;

int main(int argc,char *argv[]){
    std::string str = "World!";
    Foundation::StrRef data(str);
    const char *s = "OtherShit!";
    char *ptr = const_cast<char *>(s);
    std::cout << Foundation::fstring("Hello @r Message 2: @s",data,ptr);
    
    return 0;
};
