//
// Created by alex on 6/21/21.
//

#include <stdlib.h>
#include <stdint.h>

#ifndef STARBYTES_INTEROP_H
#define STARBYTES_INTEROP_H

#ifdef __cplusplus
namespace starbytes::Runtime {
    struct RTFuncTemplate;
}
typedef starbytes::Runtime::RTFuncTemplate StarbytesFuncTemplate;
#else
typedef struct _RTFuncTemplateInternal StarbytesFuncTemplate;
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef const char * CString;

CString CStringMake(const char *buf);
void CStringFree(const char *buf);

typedef size_t StarbytesClassType;

StarbytesClassType StarbytesStrType();
StarbytesClassType StarbytesArrayType();
StarbytesClassType StarbytesNumType();
StarbytesClassType StarbytesDictType();
StarbytesClassType StarbytesBoolType();
StarbytesClassType StarbytesFuncRefType();
StarbytesClassType StarbytesRegexType();
StarbytesClassType StarbytesTaskType();


typedef struct _StarbytesObject * StarbytesObject;

typedef struct  {
    char name[100];
    StarbytesObject data;
} StarbytesObjectProperty;

typedef struct _StarbytesFuncArgs *StarbytesFuncArgs;

#define STARBYTES_FUNC(name) StarbytesObject name(StarbytesFuncArgs args)

int StarbytesObjectIs(StarbytesObject obj);
StarbytesObject StarbytesObjectNew(StarbytesClassType type);

int StarbytesObjectTypecheck(StarbytesObject object,StarbytesClassType type);

unsigned int StarbytesObjectGetPropertyCount(StarbytesObject obj);

StarbytesObjectProperty *StarbytesObjectIndexProperty(StarbytesObject obj,unsigned int idx);

void StarbytesObjectAddProperty(StarbytesObject obj,char *name,StarbytesObject data);

StarbytesObject StarbytesObjectGetProperty(StarbytesObject obj,const char * name);

void StarbytesObjectReference(StarbytesObject obj);

void StarbytesObjectRelease(StarbytesObject obj);

#define COMPARE_EQUAL 0x00
#define COMPARE_LESS 0x01
#define COMPARE_GREATER 0x02
#define COMPARE_NOTEQUAL (COMPARE_LESS | COMPARE_GREATER)

/// @name Starbytes Internal Class Types

typedef StarbytesObject StarbytesStr;
typedef StarbytesObject StarbytesArray;
typedef StarbytesObject StarbytesDict;
typedef StarbytesObject StarbytesBool;
typedef StarbytesObject StarbytesNum;
typedef StarbytesObject StarbytesFuncRef;
typedef StarbytesObject StarbytesTask;

/// @}
///
typedef StarbytesObject StarbytesClassObject;

StarbytesClassType StarbytesMakeClass(const char *name);
StarbytesObject StarbytesClassObjectNew(StarbytesClassType type);
StarbytesClassType StarbytesClassObjectGetClass(StarbytesObject);



/// @name Starbytes String Methods
/// @{
///

typedef enum : int {
    StrEncodingUTF8,
    StrEncodingUTF16,
    StrEncodingUTF32,
} StarbytesStrEncoding;

/// Creates a String and Returns it
StarbytesStr StarbytesStrCreate(StarbytesStrEncoding enc);
StarbytesStr StarbytesStrNewWithData(const char * data);
StarbytesStr StarbytesStrCopy(StarbytesStr);
int StarbytesStrCompare(StarbytesStr lhs,StarbytesStr rhs);
char *StarbytesStrGetBuffer(StarbytesStr);
unsigned StarbytesStrLength(StarbytesStr);
void StarbytesStrDestroy(StarbytesStr );
/// @}


/// @name Starbytes Array Methods
/// @{
StarbytesArray  StarbytesArrayNew();
StarbytesArray StarbytesArrayCopy(StarbytesArray);
void StarbytesArrayPush(StarbytesArray array,StarbytesObject obj);
void StarbytesArrayPop(StarbytesArray array);
unsigned int StarbytesArrayGetLength(StarbytesArray array);
StarbytesObject StarbytesArrayIndex(StarbytesArray array,unsigned int index);
void StarbytesArraySet(StarbytesArray array,unsigned int index,StarbytesObject obj);
/// @}

/// @name Starbytes Number Methods

typedef enum : int {
    NumTypeFloat,
    NumTypeInt
} StarbytesNumT;

StarbytesNum StarbytesNumNew(StarbytesNumT type,...);
StarbytesNum StarbytesNumCopy(StarbytesNum);
int StarbytesNumCompare(StarbytesNum lhs,StarbytesNum rhs);
void StarbytesNumAssign(StarbytesNum obj,StarbytesNumT type,...);
StarbytesNum StarbytesNumConvertTo(StarbytesNum num,StarbytesNumT type);
StarbytesNumT StarbytesNumGetType(StarbytesNum obj);
int StarbytesNumGetIntValue(StarbytesNum obj);
float StarbytesNumGetFloatValue(StarbytesNum obj);
StarbytesNum StarbytesNumAdd(StarbytesNum a,StarbytesNum b);
StarbytesNum StarbytesNumSub(StarbytesNum a,StarbytesNum b);
/// @}
///
///
///
/// @name Starbytes Dictionary Methods
StarbytesDict StarbytesDictNew();
StarbytesDict StarbytesDictCopy(StarbytesDict);
void StarbytesDictSet(StarbytesDict dict,StarbytesObject key,StarbytesObject val);
StarbytesObject StarbytesDictGet(StarbytesDict dict,StarbytesObject key);
/// @}
///
///@name Starbytes Boolean Methods
///

typedef enum : uint8_t {
    StarbytesBoolTrue = 0x0,
    StarbytesBoolFalse = 0x1
} StarbytesBoolVal;

StarbytesBool StarbytesBoolNew(StarbytesBoolVal val);
StarbytesBoolVal StarbytesBoolValue(StarbytesBool);

///@}
///
/// @name Starbytes FunctionRef Methods



StarbytesFuncRef StarbytesFuncRefNew(StarbytesFuncTemplate *funcTemplate);
StarbytesFuncTemplate * StarbytesFuncRefGetPtr(StarbytesFuncRef);
///@}

/// @name Starbytes Task Methods
typedef enum : uint8_t {
    StarbytesTaskPending = 0x0,
    StarbytesTaskResolved = 0x1,
    StarbytesTaskRejected = 0x2
} StarbytesTaskState;

StarbytesTask StarbytesTaskNew();
StarbytesTaskState StarbytesTaskGetState(StarbytesTask task);
void StarbytesTaskResolve(StarbytesTask task,StarbytesObject value);
void StarbytesTaskReject(StarbytesTask task,const char *error);
StarbytesObject StarbytesTaskGetValue(StarbytesTask task);
CString StarbytesTaskGetError(StarbytesTask task);
///@}

StarbytesObject StarbytesFuncArgsGetArg(StarbytesFuncArgs args);

/// Runtime command-line context (set by host/driver, read by stdlib modules)
void StarbytesRuntimeSetExecutablePath(const char *path);
void StarbytesRuntimeSetScriptPath(const char *path);
void StarbytesRuntimeClearScriptArgs();
void StarbytesRuntimePushScriptArg(const char *arg);
CString StarbytesRuntimeGetExecutablePath();
CString StarbytesRuntimeGetScriptPath();
unsigned StarbytesRuntimeGetScriptArgCount();
CString StarbytesRuntimeGetScriptArg(unsigned index);


typedef StarbytesObject (*StarbytesFuncCallback)(StarbytesFuncArgs args);

typedef struct {
  CString name;
  StarbytesFuncCallback callback;
  unsigned argCount;
}  StarbytesFuncDesc;

typedef struct __StarbytesNativeModule StarbytesNativeModule;

StarbytesNativeModule *StarbytesNativeModuleCreate();
void StarbytesNativeModuleAddDesc(StarbytesNativeModule *module,StarbytesFuncDesc *desc);


#define STARBYTES_INSTANCE_FUNC(class,name) #class"_"#name

#define STARBYTES_MAKE_CLASS_NAME(scope,name) #scope""

#define __STARBYTES_NATIVE_MODULE_MAIN_FUNC starbytesModuleMain
#define STR_WRAP(n) #n
#ifdef __cplusplus
#define STARBYTES_NATIVE_MOD_MAIN() extern "C" StarbytesNativeModule * __STARBYTES_NATIVE_MODULE_MAIN_FUNC()
#else
#define STARBYTES_NATIVE_MOD_MAIN() StarbytesNativeModule * __STARBYTES_NATIVE_MODULE_MAIN_FUNC()
#endif




#ifdef __cplusplus
}
#endif



#endif // STARBYTES_INTEROP_H
