#include "starbytes/ByteCode/BCSerializer.h"
#include "starbytes/ByteCode/BCDef.h"
#include <fstream>

STARBYTES_BYTECODE_NAMESPACE
/*Starbytes Bytecode Serializer!*/

void serializeBCProgram(std::ofstream & output_stream,BCProgram *program){
    if(output_stream.is_open()){
        for(auto & bc_unit : program->units){
            if(BC_UNIT_IS(bc_unit,BCCodeBegin)){
                BCCodeBegin * unit_ref = static_cast<BCCodeBegin *>(bc_unit);
                output_stream.write((char *)unit_ref,sizeof(*unit_ref));
            }
            else if(BC_UNIT_IS(bc_unit,BCCodeEnd)){
                BCCodeEnd * unit_ref = static_cast<BCCodeEnd *>(bc_unit);
                output_stream.write((char *)unit_ref,sizeof(*unit_ref));
            }
            else if(BC_UNIT_IS(bc_unit,BCVectorBegin)){
                BCVectorBegin * unit_ref = static_cast<BCVectorBegin *>(bc_unit);
                output_stream.write((char *)unit_ref,sizeof(*unit_ref));
            }
            else if(BC_UNIT_IS(bc_unit,BCVectorEnd)){
                BCVectorEnd * unit_ref = static_cast<BCVectorEnd*>(bc_unit);
                output_stream.write((char *)unit_ref,sizeof(*unit_ref));
            }
            else if(BC_UNIT_IS(bc_unit,BCReference)){
                BCReference * unit_ref = static_cast<BCReference *>(bc_unit);
                output_stream.write((char *)unit_ref,sizeof(*unit_ref));
            }
        }
    }
}



void deserializeBCProgram(std::ifstream & input,BCProgram *& program_loc){
    //Get first BCUnit!
    if(input.is_open()){
        while(true){
            BCCodeBegin * first_unit = new BCCodeBegin();
            input.read((char *)first_unit,sizeof(*first_unit));
            program_loc->units.push_back(first_unit);
            //Check for all cases!
            BCCodeEnd * last_unit = new BCCodeEnd();
            input.read((char *)last_unit,sizeof(*last_unit));
            program_loc->units.push_back(last_unit);
            char end = input.peek();
            if(end == EOF){
                break;
            }
        }
    }
};

NAMESPACE_END