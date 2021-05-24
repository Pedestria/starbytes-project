#include "starbytes/Base/CodeView.h"
#include <sstream>

namespace starbytes {

void generateCodeView(CodeViewSrc &src,SrcLoc & loc,llvm::Optional<SrcLoc> highlightLoc){
    std::istringstream codein(src.code);
    std::string temp;
    std::getline(codein,temp,'\n');
};

};
