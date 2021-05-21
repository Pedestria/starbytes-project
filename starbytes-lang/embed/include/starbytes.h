#include <iostream>
#include <streambuf>

#ifndef _STARBYTES_H
#define _STARBYTES_H


#ifdef _WIN32

#ifdef __BUILD__ 
#define STARBYTES_EMBEDDED_API_EXPORT __declspec(dllexport)
#else 
#define STARBYTES_EMBEDDED_API_EXPORT __declspec(dllimport)
#endif
#else 
#define STARBYTES_EMBEDDED_API_EXPORT
#endif

namespace starbytes {

typedef const char * CString;

typedef unsigned char Byte;

class STARBYTES_EMBEDDED_API_EXPORT ByteBuffer {
    Byte *data;
    size_t size;
public:
    // std::filebuf filebuf();
    Byte *pointer();
    size_t & length();

    void copy(Byte **data_dest,size_t bytes_n);


    ByteBuffer(Byte *data,size_t size);

    ByteBuffer(ByteBuffer & other);
    ~ByteBuffer();
};



template<class T>
class STARBYTES_EMBEDDED_API_EXPORT ArcPtr{
    T *data;
    unsigned refCount;
public:
    operator bool(){
        return data != nullptr;
    };
    T & get(){
        return *data;
    };
    ArcPtr(T *ptr):data(ptr){
        refCount = 1;
    };
    ArcPtr(ArcPtr<T> & other_ptr):data(other_ptr.data){
        ++other_ptr.refCount;
        refCount = other_ptr.refCount;
    };
    ~ArcPtr(){
        if(refCount == 0){
            delete data;
        };
    };
};


struct STARBYTES_EMBEDDED_API_EXPORT ModuleCompilationSource {
    CString name;
    CString uri;
    ByteBuffer srcCode;
};

struct STARBYTES_EMBEDDED_API_EXPORT ModuleCompilationParams {

    CString moduleName;
    CString outputName;

    unsigned srcCount;
    ModuleCompilationSource *srcs;

    std::ostream & outputStream;
};

struct STARBYTES_EMBEDDED_API_EXPORT Module {
    ArcPtr<ByteBuffer> code;
};


class STARBYTES_EMBEDDED_API_EXPORT CompilationContext {
public:
    ArcPtr<Module> compileModule(ModuleCompilationParams &params);
};

STARBYTES_EMBEDDED_API_EXPORT ArcPtr<CompilationContext> createCompilationContext();

struct STARBYTES_EMBEDDED_API_EXPORT ModuleExecutionParams {
    ArcPtr<Module> mod;
    std::ostream & outputStream;
};

class STARBYTES_EMBEDDED_API_EXPORT ExecutionContext {
public:
    int executeModule(ModuleExecutionParams & params);
};

STARBYTES_EMBEDDED_API_EXPORT ArcPtr<ExecutionContext> createExecutionContext();




};

#endif
