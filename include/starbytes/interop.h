//
// Created by alex on 6/21/21.
//



#ifndef STARBYTES_INTEROP_H
#define STARBYTES_INTEROP_H

#ifdef __cplusplus
extern "C" {
#endif

typedef const char * CString;

typedef struct __StarbytesObject StarbytesObject;


typedef struct __StarbytesNativeModule StarbytesNativeModule;

typedef StarbytesObject StarbytesStr;
typedef StarbytesObject StarbytesArray;
typedef StarbytesObject StarbytesDict;
typedef StarbytesObject StarbytesNum;

void StarbytesObjectDestroy(StarbytesObject *obj);

/// @name Starbytes String Methods
/// @{

/// Creates a String and Returns it
StarbytesStr *StarbytesStrCreate();
bool StarbytesStrCompare(StarbytesStr *lhs,StarbytesStr *rhs);
void StarbytesStrDestroy(StarbytesStr *);
/// @}


/// @name Starbytes Array Methods
/// @{
StarbytesArray  *StarbytesArrayCreate();
unsigned StarbytesArrayLen(StarbytesArray *);
StarbytesObject *StarbytesArrayIndex(StarbytesArray *);
void StarbytesArrayDestroy(StarbytesArray *);
/// @}

typedef struct __StarbytesFuncArgs StarbytesFuncArgs;
StarbytesObject * StarbytesFuncArgsGetArg(StarbytesFuncArgs *args);


typedef StarbytesObject * (*StarbytesFuncCallback)(StarbytesFuncArgs *args);

struct StarbytesFuncDesc {
  CString name;
  StarbytesFuncCallback callback;
  unsigned argCount;
};


struct StarbytesObjectDesc {
  CString name;
  unsigned propertyCount;
};

#define STARBYTES_FUNC(name) StarbytesObject * name(StarbytesFuncArgs *args)


#define STARBYTES_FUNC_DESC(func,arg_n) struct StarbytesFuncDesc  func##_desc; \
   func##_desc.name = #func; \
  func##_desc.callback = &func;                                    \
func##_desc.argCount = arg_n;

#define STARBYTES_INSTANCE_FUNC_DESC(class_name,func,arg_n) STARBYTES_FUNC(##class_name##_##func ,arg_n)

STARBYTES_FUNC(myFunction);

#define __STARBYTES_NATIVE_MODULE_MAIN_FUNC starbytesModuleMain
#define STR_WRAP(n) #n
#define STARBYTES_NATIVE_MOD_MAIN() StarbytesNativeModule * __STARBYTES_NATIVE_MODULE_MAIN_FUNC()




#ifdef __cplusplus
}
#endif



#endif // STARBYTES_INTEROP_H
