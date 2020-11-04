#include "starbytes/Base/ADT.h"
#include <string>
#include <fstream>
#include <sstream>

namespace Starbytes::Foundation {

    std::string & TmpString::getValue(){
        return value;
    };

    bool TmpString::exists(){
        
    };
    
    bool TmpString::operator==(TmpString &str_subject){
        return str_subject == value;
    };

    bool TmpString::operator==(std::string &str_subject){
        return str_subject == value;
    };
      
};