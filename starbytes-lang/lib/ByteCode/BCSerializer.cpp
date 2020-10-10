#include "starbytes/ByteCode/BCSerializer.h"

STARBYTES_BYTECODE_NAMESPACE

class BCSerializer {
    private:
        BCProgram *& program;
        std::ostringstream *& stream;
    public:
        BCSerializer(BCProgram *& _program,std::ostringstream *& _os):stream(_os),program(_program){};
        void serialize();
        ~BCSerializer(){};
};

void serializeBCProgram(std::ostringstream * output_stream,BCProgram *program){
    BCSerializer(program,output_stream).serialize();
}

NAMESPACE_END