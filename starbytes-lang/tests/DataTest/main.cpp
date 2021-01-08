#include "starbytes/Base/Base.h"

using namespace Starbytes;

int main(int argc,char *argv[]){
    std::string str = "World!";
    Foundation::StrRef data(str);
    std::cout << Foundation::fstring("Hello @r",data);
    
    return 0;
};
