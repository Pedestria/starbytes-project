

#include "starbytes/compiler/ASTNodes.def"
#include "starbytes/compiler/RTCode.h"
#include "starbytes/runtime/RTDisasm.h"
#include "starbytes/base/ADT.h"
#include <iostream>
#include <fstream>
#include "starbytes/base/Diagnostic.h"




int main(int argc,const char *argv[]){
    // llvm::InitLLVM _llvm(argc,argv);
        
    bool okay = true;
    // auto okay = llvm::cl::ParseCommandLineOptions(argc,argv);

    if(!okay){
        return 1;
    };

    auto error_engine = starbytes::DiagnosticHandler::createDefault(std::cout);

    std::string file;
    
    std::ifstream in(file,std::ios::in | std::ios::binary);

    if(in.is_open()){

        starbytes::Runtime::runDisasm(in,std::cout);

        in.close();

    }
    else {
        error_engine->push(starbytes::StandardDiagnostic::createError("Failed to read file"));
    };

    if(error_engine->hasErrored()){
        error_engine->logAll();
        return 1;
    }

   return 0;
};
