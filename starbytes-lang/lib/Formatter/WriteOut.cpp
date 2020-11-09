#include "starbytes/Formatter/WriteOut.h"
#include "starbytes/Core/Core.h"
#include <fstream>
#include <sstream>

STARBYTES_STD_NAMESPACE

class STBFormatter {
    private:
        std::ofstream & output;
        AbstractSyntaxTree *& tree;
    public:
        STBFormatter(std::ofstream & o,AbstractSyntaxTree *& _tree):output(o),tree(_tree){};
        void format(){

        };
};

void format_source_file(std::string & file_to_format){
    std::string * src_code = Foundation::readFile(file_to_format);
    AST::AbstractSyntaxTree * src_ast = parseCode(*src_code);
    std::ofstream out;
    out.open(file_to_format);
    STBFormatter(out,src_ast).format();
};

NAMESPACE_END