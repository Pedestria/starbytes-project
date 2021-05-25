#define __BUILD__ 1
#include "include/starbytes.h"

#include <iterator>

#include <starbytes/Parser/Parser.h>
#include <starbytes/Gen/Gen.h>
#include <starbytes/RT/RTEngine.h>

namespace starbytes {

    Byte * ByteBuffer::pointer(){
        return data;
    };

    void ByteBuffer::copy(Byte **data_dest, size_t bytes_n){
        *data_dest = new Byte[bytes_n];
        std::copy(data,data + size,*data_dest);
    };

    ByteBuffer::ByteBuffer(Byte *data,size_t size):data(data),size(size){

    };

    size_t & ByteBuffer::length(){
        return size;
    };

    ByteBuffer::ByteBuffer(ByteBuffer & other){
       other.copy(&data,other.size);
       size = other.size;  
    };

    // std::filebuf ByteBuffer::filebuf(){
    //     std::filebuf buffer;
    //     buffer.pubsetbuf((char *)data,size);
    //     return buffer;
    // };

    ByteBuffer::~ByteBuffer(){
        delete data;
    };

    ArcPtr<Module> CompilationContext::compileModule(ModuleCompilationParams &params){

        Gen gen;

        Parser parser(gen);
        ModuleParseContext ctxt = ModuleParseContext::Create(params.moduleName);

        for(unsigned i = 0;i < params.srcCount;i++){
            auto & src = params.srcs[i];
            auto & buffer = src.srcCode;
            std::ifstream in;
            in.rdbuf()->pubsetbuf((char *)buffer.pointer(),buffer.length());
            parser.parseFromStream(in,ctxt);
        };

        
    };
}


