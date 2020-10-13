#include "starbytes/ByteCode/BCSerializer.h"
#include "starbytes/ByteCode/BCDef.h"

STARBYTES_BYTECODE_NAMESPACE
/*Starbytes Bytecode Serializer!*/
class BCSerializer {
    private:
        BCProgram *& program;
        std::ostringstream *& stream;
    public:
        BCSerializer(BCProgram *& _program,std::ostringstream *& _os):stream(_os),program(_program){};
        void serialize();
        template<typename Type>
        void write(Type t);
        ~BCSerializer(){};
};

template<typename Type>
void BCSerializer::write(Type t){
    *stream << t;
};

void serializeDigit(BCSerializer *self,BCDigit *unit){
    self->write(unit->value);
};

void serializeBCUnit(BCSerializer *self,BCUnit *unit){
    if(BC_UNIT_IS(unit,BCIdentifier)){

    }
    else(BC_UNIT_IS(unit, BCDigit));{
        serializeDigit(self,ASSERT_BC_UNIT(unit,BCDigit));
    };
};

void BCSerializer::serialize() {
    for(auto u : program->units){
        serializeBCUnit(this,u);
    }
};

void serializeBCProgram(std::ostringstream * output_stream,BCProgram *program){
    BCSerializer(program,output_stream).serialize();
}

NAMESPACE_END