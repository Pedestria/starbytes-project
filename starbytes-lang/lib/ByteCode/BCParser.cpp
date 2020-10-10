#include "starbytes/ByteCode/BCParser.h"
#include <vector>

STARBYTES_BYTECODE_NAMESPACE

TYPED_ENUM BCTokType:int {
    ID,OpenParen,CloseParen,Number
};
struct BCToken {
    BCTokType type;
    std::string content;
};

class BCFileLexer {
    private:
        std::string code;
        std::vector<BCToken> *toks;
    public:
        BCFileLexer(std::string _code):code(_code){};
        ~BCFileLexer(){};
        std::vector<BCToken> * init(){
            std::vector<BCToken> * vec = new std::vector<BCToken>();
            toks = vec;
            
            return vec;
        };
};

class BCFileParser {
    private:
        std::vector<BCToken> *toks;
    public:
        BCFileParser(std::vector<BCToken> *_toks):toks(_toks){};
        ~BCFileParser(){};
        BCProgram * parse();
};

BCProgram * parseToProgram(std::string &file) {
    auto file_buf = Foundation::readFile(file);
    auto toks = BCFileLexer(*file_buf).init();
    auto result = BCFileParser(toks).parse();
    return result;
};

NAMESPACE_END