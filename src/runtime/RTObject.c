#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

#include <starbytes/interop.h>

typedef struct {
    StarbytesNumT type;
    double d;
    float f;
    int64_t l;
    int i;
} StarbytesNumPriv;

typedef struct {
    StarbytesBoolVal val;
} StarbytesBoolPriv;

typedef struct {
    StarbytesFuncTemplate *ref;
} StarbytesFuncRefPriv;

typedef struct {
    StarbytesTaskState state;
    StarbytesObject value;
    char *error;
} StarbytesTaskPriv;




struct _StarbytesObject {
    
    size_t type;
    unsigned int refCount;
    
    unsigned int nProp;
    StarbytesObjectProperty *props;
    
    void *privData;
    void (*freePrivData)(void *);
    union {
        StarbytesNumPriv num;
        StarbytesBoolPriv boolean;
        StarbytesFuncRefPriv funcRef;
        StarbytesTaskPriv task;
    } inlinePayload;
    
};

struct _StarbytesFuncArgs {
    unsigned argc;
    unsigned index;
    StarbytesObject *argv;
    char *errorMessage;
};

static char *gRuntimeExecutablePath = NULL;
static char *gRuntimeScriptPath = NULL;
static char **gRuntimeScriptArgs = NULL;
static unsigned gRuntimeScriptArgCount = 0;
static int gRuntimeProfileLowLevelCountersEnabled = 0;
static StarbytesRuntimeLowLevelCounters gRuntimeLowLevelCounters;
static const char *kEnvExecutablePath = "STARBYTES_EXECUTABLE_PATH";
static const char *kEnvScriptPath = "STARBYTES_SCRIPT_PATH";
static const char *kEnvScriptArgCount = "STARBYTES_SCRIPT_ARGC";
static const char *kEnvScriptArgPrefix = "STARBYTES_SCRIPT_ARG_";

static StarbytesRuntimeObjectKind StarbytesRuntimeObjectKindFromType(StarbytesClassType type){
    if(type == StarbytesStrType()){
        return StarbytesRuntimeObjectKindString;
    }
    if(type == StarbytesArrayType()){
        return StarbytesRuntimeObjectKindArray;
    }
    if(type == StarbytesDictType()){
        return StarbytesRuntimeObjectKindDict;
    }
    if(type == StarbytesNumType()){
        return StarbytesRuntimeObjectKindNumber;
    }
    if(type == StarbytesBoolType()){
        return StarbytesRuntimeObjectKindBool;
    }
    if(type == StarbytesFuncRefType()){
        return StarbytesRuntimeObjectKindFuncRef;
    }
    if(type == StarbytesRegexType()){
        return StarbytesRuntimeObjectKindRegex;
    }
    if(type == StarbytesTaskType()){
        return StarbytesRuntimeObjectKindTask;
    }
    return StarbytesRuntimeObjectKindCustomClass;
}

static void StarbytesRuntimeProfileRecordAllocation(StarbytesClassType type){
    if(!gRuntimeProfileLowLevelCountersEnabled){
        return;
    }
    gRuntimeLowLevelCounters.objectAllocations[StarbytesRuntimeObjectKindFromType(type)] += 1;
}

static void StarbytesRuntimeProfileRecordDeallocation(StarbytesClassType type){
    if(!gRuntimeProfileLowLevelCountersEnabled){
        return;
    }
    gRuntimeLowLevelCounters.objectDeallocations[StarbytesRuntimeObjectKindFromType(type)] += 1;
}

static char *StarbytesDupString(const char *value){
    if(value == NULL){
        return NULL;
    }
    size_t len = strlen(value);
    char *copy = (char *)malloc(len + 1);
    if(copy == NULL){
        return NULL;
    }
    memcpy(copy,value,len + 1);
    return copy;
}

static void StarbytesSetOwnedString(char **slot,const char *value){
    if(slot == NULL){
        return;
    }
    if(*slot != NULL){
        free(*slot);
        *slot = NULL;
    }
    *slot = StarbytesDupString(value);
}

static unsigned utf8_scalar_length(const char *data){
    if(data == NULL){
        return 0;
    }
    const unsigned char *ptr = (const unsigned char *)data;
    unsigned count = 0;
    while(*ptr != '\0'){
        unsigned char lead = *ptr;
        size_t width = 1;
        if((lead & 0x80u) == 0x00u){
            width = 1;
        }
        else if((lead & 0xE0u) == 0xC0u && ptr[1] != '\0'){
            width = 2;
        }
        else if((lead & 0xF0u) == 0xE0u && ptr[1] != '\0' && ptr[2] != '\0'){
            width = 3;
        }
        else if((lead & 0xF8u) == 0xF0u && ptr[1] != '\0' && ptr[2] != '\0' && ptr[3] != '\0'){
            width = 4;
        }
        ptr += width;
        ++count;
    }
    return count;
}

static void StarbytesSetEnvVar(const char *name,const char *value){
    if(name == NULL){
        return;
    }
#if defined(_WIN32)
    _putenv_s(name,value == NULL ? "" : value);
#else
    if(value == NULL){
        unsetenv(name);
    }
    else {
        setenv(name,value,1);
    }
#endif
}

static void StarbytesUnsetEnvVar(const char *name){
    if(name == NULL){
        return;
    }
#if defined(_WIN32)
    _putenv_s(name,"");
#else
    unsetenv(name);
#endif
}

static unsigned StarbytesReadEnvUnsigned(const char *name){
    if(name == NULL){
        return 0;
    }
    const char *raw = getenv(name);
    if(raw == NULL || *raw == '\0'){
        return 0;
    }
    return (unsigned)strtoul(raw,NULL,10);
}

static void StarbytesBuildArgEnvName(unsigned index,char *buffer,size_t bufferSize){
    if(buffer == NULL || bufferSize == 0){
        return;
    }
    snprintf(buffer,bufferSize,"%s%u",kEnvScriptArgPrefix,index);
}

static void _StarbytesPrivDataFreeDefault(void *data){
    free(data);
}

static StarbytesNumPriv *StarbytesNumGetPriv(StarbytesNum num){
    return num ? &num->inlinePayload.num : NULL;
}

static StarbytesBoolPriv *StarbytesBoolGetPriv(StarbytesBool boolean){
    return boolean ? &boolean->inlinePayload.boolean : NULL;
}

static StarbytesFuncRefPriv *StarbytesFuncRefGetPriv(StarbytesFuncRef ref){
    return ref ? &ref->inlinePayload.funcRef : NULL;
}

static StarbytesTaskPriv *StarbytesTaskGetPriv(StarbytesTask task){
    return task ? &task->inlinePayload.task : NULL;
}

static void StarbytesReleaseInlinePayload(StarbytesObject obj){
    if(obj == NULL){
        return;
    }
    if(obj->type == StarbytesTaskType()){
        if(obj->inlinePayload.task.value != NULL){
            StarbytesObjectRelease(obj->inlinePayload.task.value);
            obj->inlinePayload.task.value = NULL;
        }
        if(obj->inlinePayload.task.error != NULL){
            free(obj->inlinePayload.task.error);
            obj->inlinePayload.task.error = NULL;
        }
        obj->inlinePayload.task.state = StarbytesTaskRejected;
    }
}

typedef struct {
    unsigned int fieldCount;
    const char **fieldNames;
    StarbytesObject *fieldValues;
} StarbytesClassObjectPriv;

static void _StarbytesClassObjectFree(void *data);

static int StarbytesObjectHasClassFieldLayout(StarbytesObject obj){
    return obj != NULL
        && !StarbytesObjectIs(obj)
        && obj->privData != NULL
        && obj->freePrivData == _StarbytesClassObjectFree;
}

static StarbytesClassObjectPriv *StarbytesClassObjectGetPriv(StarbytesObject obj){
    if(!StarbytesObjectHasClassFieldLayout(obj)){
        return NULL;
    }
    return (StarbytesClassObjectPriv *)obj->privData;
}

static void _StarbytesClassObjectFree(void *data){
    StarbytesClassObjectPriv *priv = (StarbytesClassObjectPriv *)data;
    if(priv == NULL){
        return;
    }
    if(priv->fieldValues != NULL){
        for(unsigned int i = 0; i < priv->fieldCount; ++i){
            if(priv->fieldValues[i] != NULL){
                StarbytesObjectRelease(priv->fieldValues[i]);
            }
        }
        free(priv->fieldValues);
    }
    free((void *)priv->fieldNames);
    free(priv);
}

StarbytesObject StarbytesFuncArgsGetArg(StarbytesFuncArgs args){
    if(args == NULL || args->argv == NULL || args->index >= args->argc){
        return NULL;
    }
    return args->argv[args->index++];
}

void StarbytesFuncArgsSetError(StarbytesFuncArgs args,const char *message){
    if(args == NULL){
        return;
    }
    StarbytesSetOwnedString(&args->errorMessage,message);
}

void StarbytesFuncArgsClearError(StarbytesFuncArgs args){
    if(args == NULL){
        return;
    }
    StarbytesSetOwnedString(&args->errorMessage,NULL);
}

CString StarbytesFuncArgsGetError(StarbytesFuncArgs args){
    if(args == NULL){
        return NULL;
    }
    return args->errorMessage;
}

void StarbytesRuntimeSetExecutablePath(const char *path){
    StarbytesSetOwnedString(&gRuntimeExecutablePath,path);
    StarbytesSetEnvVar(kEnvExecutablePath,path == NULL ? "" : path);
}

void StarbytesRuntimeSetScriptPath(const char *path){
    StarbytesSetOwnedString(&gRuntimeScriptPath,path);
    StarbytesSetEnvVar(kEnvScriptPath,path == NULL ? "" : path);
}

void StarbytesRuntimeClearScriptArgs(){
    unsigned previousCount = gRuntimeScriptArgCount;
    if(previousCount == 0){
        previousCount = StarbytesReadEnvUnsigned(kEnvScriptArgCount);
    }
    for(unsigned i = 0; i < previousCount; ++i){
        char key[64];
        StarbytesBuildArgEnvName(i,key,sizeof(key));
        StarbytesUnsetEnvVar(key);
    }
    StarbytesSetEnvVar(kEnvScriptArgCount,"0");

    if(gRuntimeScriptArgs != NULL){
        for(unsigned i = 0; i < gRuntimeScriptArgCount; ++i){
            free(gRuntimeScriptArgs[i]);
        }
        free(gRuntimeScriptArgs);
    }
    gRuntimeScriptArgs = NULL;
    gRuntimeScriptArgCount = 0;
}

void StarbytesRuntimePushScriptArg(const char *arg){
    char *copiedArg = StarbytesDupString(arg == NULL ? "" : arg);
    char **newArgs = (char **)realloc(gRuntimeScriptArgs,sizeof(char *) * (gRuntimeScriptArgCount + 1));
    if(newArgs == NULL){
        free(copiedArg);
        return;
    }
    gRuntimeScriptArgs = newArgs;
    gRuntimeScriptArgs[gRuntimeScriptArgCount] = copiedArg;
    gRuntimeScriptArgCount += 1;

    char key[64];
    char countBuffer[32];
    StarbytesBuildArgEnvName(gRuntimeScriptArgCount - 1,key,sizeof(key));
    StarbytesSetEnvVar(key,arg == NULL ? "" : arg);
    snprintf(countBuffer,sizeof(countBuffer),"%u",gRuntimeScriptArgCount);
    StarbytesSetEnvVar(kEnvScriptArgCount,countBuffer);
}

void StarbytesRuntimeProfileSetLowLevelCountersEnabled(int enabled){
    gRuntimeProfileLowLevelCountersEnabled = enabled ? 1 : 0;
}

void StarbytesRuntimeProfileResetLowLevelCounters(){
    memset(&gRuntimeLowLevelCounters,0,sizeof(gRuntimeLowLevelCounters));
}

void StarbytesRuntimeProfileGetLowLevelCounters(StarbytesRuntimeLowLevelCounters *outCounters){
    if(outCounters == NULL){
        return;
    }
    *outCounters = gRuntimeLowLevelCounters;
}

CString StarbytesRuntimeGetExecutablePath(){
    if(gRuntimeExecutablePath == NULL){
        const char *fromEnv = getenv(kEnvExecutablePath);
        if(fromEnv != NULL){
            return fromEnv;
        }
    }
    return gRuntimeExecutablePath;
}

CString StarbytesRuntimeGetScriptPath(){
    if(gRuntimeScriptPath == NULL){
        const char *fromEnv = getenv(kEnvScriptPath);
        if(fromEnv != NULL){
            return fromEnv;
        }
    }
    return gRuntimeScriptPath;
}

unsigned StarbytesRuntimeGetScriptArgCount(){
    if(gRuntimeScriptArgCount == 0){
        return StarbytesReadEnvUnsigned(kEnvScriptArgCount);
    }
    return gRuntimeScriptArgCount;
}

CString StarbytesRuntimeGetScriptArg(unsigned index){
    if(index >= gRuntimeScriptArgCount || gRuntimeScriptArgs == NULL){
        char key[64];
        StarbytesBuildArgEnvName(index,key,sizeof(key));
        return getenv(key);
    }
    return gRuntimeScriptArgs[index];
}

size_t StarbytesStrType(){
    return 0;
}

size_t StarbytesArrayType(){
    return 1;
}

size_t StarbytesDictType(){
    return 2;
}

size_t StarbytesNumType(){
    return 3;
}

size_t StarbytesBoolType(){
    return 4;
}

size_t StarbytesFuncRefType(){
    return 5;
}

size_t StarbytesCustomClassType(){
    return 6;
}

size_t StarbytesRegexType(){
    return 7;
}

size_t StarbytesTaskType(){
    return 8;
}

int StarbytesObjectIs(StarbytesObject obj){
    if(obj == NULL){
        return 0;
    }
    return (obj->type == StarbytesStrType())
        || (obj->type == StarbytesArrayType())
        || (obj->type == StarbytesDictType())
        || (obj->type == StarbytesNumType())
        || (obj->type == StarbytesBoolType())
        || (obj->type == StarbytesFuncRefType())
        || (obj->type == StarbytesRegexType())
        || (obj->type == StarbytesTaskType());
};


StarbytesObject StarbytesObjectNew(StarbytesClassType type){
    struct _StarbytesObject obj;
    memset(&obj,0,sizeof(struct _StarbytesObject));
    obj.refCount = 1;
    obj.type = type;
    obj.freePrivData = _StarbytesPrivDataFreeDefault;
    
    StarbytesObject mem = (StarbytesObject)malloc(sizeof(struct _StarbytesObject));
    memcpy(mem,&obj,sizeof(struct _StarbytesObject));
    StarbytesRuntimeProfileRecordAllocation(type);
    return mem;
}

int StarbytesObjectTypecheck(StarbytesObject object,size_t type){
    return object->type == type;
}

unsigned int StarbytesObjectGetPropertyCount(StarbytesObject obj){
    return obj->nProp;
}

void StarbytesObjectAddProperty(StarbytesObject obj,char *name,StarbytesObject data){
    int classFieldIndex = StarbytesClassObjectFindField(obj,name);
    if(classFieldIndex >= 0){
        StarbytesClassObjectSetField(obj,(unsigned int)classFieldIndex,data);
        return;
    }
    
    StarbytesObjectProperty prop;
    strcpy(prop.name,name);
    prop.data = data;
    
    if(obj->props == NULL){
        obj->props = (StarbytesObjectProperty *)malloc(sizeof(StarbytesObjectProperty));
    }
    else {
        obj->props = (StarbytesObjectProperty *)realloc(obj->props,sizeof(StarbytesObjectProperty) *( obj->nProp + 1));
    }
    
    memcpy(obj->props + obj->nProp,&prop,sizeof(StarbytesObjectProperty));
    obj->nProp += 1;
}

StarbytesObject StarbytesObjectGetProperty(StarbytesObject obj,const char * name){
    int classFieldIndex = StarbytesClassObjectFindField(obj,name);
    if(classFieldIndex >= 0){
        return StarbytesClassObjectGetField(obj,(unsigned int)classFieldIndex);
    }
    unsigned prop_c = obj->nProp;
    StarbytesObjectProperty *prop_ptr = obj->props;
    StarbytesObject rc = NULL;
    while(prop_c > 0){
        if(strcmp(prop_ptr->name,name) == 0){
            rc = prop_ptr->data;
            break;
        }
        ++prop_ptr;
        --prop_c;
    }
    return rc;
}

StarbytesObjectProperty * StarbytesObjectIndexProperty(StarbytesObject obj,unsigned int idx){
    assert(idx < obj->nProp);
    return &obj->props[idx];
}

void StarbytesObjectReference(StarbytesObject obj){
    if(gRuntimeProfileLowLevelCountersEnabled){
        gRuntimeLowLevelCounters.refCountIncrements += 1;
    }
    obj->refCount += 1;
}

void StarbytesObjectRelease(StarbytesObject obj){
    if(gRuntimeProfileLowLevelCountersEnabled){
        gRuntimeLowLevelCounters.refCountDecrements += 1;
    }
    obj->refCount -= 1;
    if(obj->refCount == 0){
        StarbytesRuntimeProfileRecordDeallocation(obj->type);
        unsigned prop_c = obj->nProp;
        StarbytesObjectProperty *prop_ptr = obj->props;
        for(;prop_c > 0;prop_c--){
            StarbytesObjectRelease(prop_ptr->data);
            ++prop_ptr;
        }
        free(obj->props);
        StarbytesReleaseInlinePayload(obj);
        if(obj->freePrivData){
            obj->freePrivData(obj->privData);
        }
        free(obj);
    }
}


/// Custom Class

StarbytesObject StarbytesClassObjectNew(StarbytesClassType type){
    return StarbytesObjectNew(type);
}

StarbytesObject StarbytesClassObjectNewWithFields(StarbytesClassType type,unsigned int fieldCount,const char *const *fieldNames){
    StarbytesObject obj = StarbytesObjectNew(type);
    StarbytesClassObjectPriv privData;
    privData.fieldCount = fieldCount;
    privData.fieldNames = NULL;
    privData.fieldValues = NULL;

    if(fieldCount > 0){
        privData.fieldNames = (const char **)calloc(fieldCount,sizeof(char *));
        privData.fieldValues = (StarbytesObject *)calloc(fieldCount,sizeof(StarbytesObject));
        if(privData.fieldNames == NULL || privData.fieldValues == NULL){
            free((void *)privData.fieldNames);
            free(privData.fieldValues);
            free(obj);
            return NULL;
        }
        for(unsigned int i = 0; i < fieldCount; ++i){
            privData.fieldNames[i] = fieldNames != NULL ? fieldNames[i] : NULL;
        }
    }

    obj->freePrivData = _StarbytesClassObjectFree;
    obj->privData = malloc(sizeof(StarbytesClassObjectPriv));
    if(obj->privData == NULL){
        free((void *)privData.fieldNames);
        free(privData.fieldValues);
        free(obj);
        return NULL;
    }
    memcpy(obj->privData,&privData,sizeof(StarbytesClassObjectPriv));
    return obj;
}

StarbytesClassType StarbytesClassObjectGetClass(StarbytesObject obj){
    return obj->type;
}

unsigned int StarbytesClassObjectGetFieldCount(StarbytesObject obj){
    StarbytesClassObjectPriv *priv = StarbytesClassObjectGetPriv(obj);
    return priv ? priv->fieldCount : 0u;
}

const char *StarbytesClassObjectGetFieldName(StarbytesObject obj,unsigned int idx){
    StarbytesClassObjectPriv *priv = StarbytesClassObjectGetPriv(obj);
    if(priv == NULL || idx >= priv->fieldCount){
        return NULL;
    }
    return priv->fieldNames[idx];
}

StarbytesObject StarbytesClassObjectGetField(StarbytesObject obj,unsigned int idx){
    StarbytesClassObjectPriv *priv = StarbytesClassObjectGetPriv(obj);
    if(priv == NULL || idx >= priv->fieldCount){
        return NULL;
    }
    return priv->fieldValues[idx];
}

void StarbytesClassObjectSetField(StarbytesObject obj,unsigned int idx,StarbytesObject value){
    StarbytesClassObjectPriv *priv = StarbytesClassObjectGetPriv(obj);
    if(priv == NULL || idx >= priv->fieldCount){
        return;
    }
    if(priv->fieldValues[idx] != NULL){
        StarbytesObjectRelease(priv->fieldValues[idx]);
    }
    priv->fieldValues[idx] = value;
}

int StarbytesClassObjectFindField(StarbytesObject obj,const char *name){
    StarbytesClassObjectPriv *priv = StarbytesClassObjectGetPriv(obj);
    if(priv == NULL || name == NULL){
        return -1;
    }
    for(unsigned int i = 0; i < priv->fieldCount; ++i){
        const char *fieldName = priv->fieldNames[i];
        if(fieldName != NULL && strcmp(fieldName,name) == 0){
            return (int)i;
        }
    }
    return -1;
}



/// Number Class

static int StarbytesNumIsIntegralType(StarbytesNumT type){
    return type == NumTypeInt || type == NumTypeLong;
}

static int StarbytesNumIsFloatingType(StarbytesNumT type){
    return type == NumTypeFloat || type == NumTypeDouble;
}

static long double StarbytesNumAsLongDouble(const StarbytesNumPriv *priv){
    switch(priv->type){
        case NumTypeFloat:
            return (long double)priv->f;
        case NumTypeDouble:
            return (long double)priv->d;
        case NumTypeLong:
            return (long double)priv->l;
        case NumTypeInt:
        default:
            return (long double)priv->i;
    }
}

static int64_t StarbytesNumAsInt64(const StarbytesNumPriv *priv){
    switch(priv->type){
        case NumTypeLong:
            return priv->l;
        case NumTypeFloat:
            return (int64_t)priv->f;
        case NumTypeDouble:
            return (int64_t)priv->d;
        case NumTypeInt:
        default:
            return (int64_t)priv->i;
    }
}

static StarbytesNumT StarbytesPromotedNumericType(StarbytesNumT lhs,StarbytesNumT rhs){
    if(lhs == NumTypeDouble || rhs == NumTypeDouble){
        return NumTypeDouble;
    }
    if(lhs == NumTypeFloat || rhs == NumTypeFloat){
        return NumTypeFloat;
    }
    if(lhs == NumTypeLong || rhs == NumTypeLong){
        return NumTypeLong;
    }
    return NumTypeInt;
}

StarbytesNum StarbytesNumNew(StarbytesNumT type,...){
    StarbytesObject obj = StarbytesObjectNew(StarbytesNumType());
    StarbytesNumPriv *priv = StarbytesNumGetPriv(obj);
    priv->type = type;
    priv->d = 0.0;
    priv->f = 0.0f;
    priv->l = 0;
    priv->i = 0;

    va_list args;
    va_start(args,type);
    switch(type){
        case NumTypeInt:
            priv->i = va_arg(args,int);
            break;
        case NumTypeLong:
            priv->l = va_arg(args,int64_t);
            break;
        case NumTypeFloat:
            priv->f = (float)va_arg(args,double);
            break;
        case NumTypeDouble:
        default:
            priv->d = va_arg(args,double);
            break;
    }
    va_end(args);
    return obj;
}

StarbytesNum StarbytesNumCopy(StarbytesNum num){
    StarbytesNumPriv *priv = StarbytesNumGetPriv(num);
    switch(priv->type){
        case NumTypeInt:
            return StarbytesNumNew(priv->type,priv->i);
        case NumTypeLong:
            return StarbytesNumNew(priv->type,priv->l);
        case NumTypeFloat:
            return StarbytesNumNew(priv->type,priv->f);
        case NumTypeDouble:
        default:
            return StarbytesNumNew(priv->type,priv->d);
    }
}

StarbytesNum StarbytesNumConvertTo(StarbytesNum num,StarbytesNumT type){
    StarbytesNumPriv *priv = StarbytesNumGetPriv(num);
    if(priv->type == type){
        return StarbytesNumCopy(num);
    }

    switch(type){
        case NumTypeInt:
            return StarbytesNumNew(NumTypeInt,(int)StarbytesNumAsInt64(priv));
        case NumTypeLong:
            return StarbytesNumNew(NumTypeLong,StarbytesNumAsInt64(priv));
        case NumTypeFloat:
            return StarbytesNumNew(NumTypeFloat,(float)StarbytesNumAsLongDouble(priv));
        case NumTypeDouble:
        default:
            return StarbytesNumNew(NumTypeDouble,(double)StarbytesNumAsLongDouble(priv));
    }
}

int StarbytesNumCompare(StarbytesNum lhs,StarbytesNum rhs){
    StarbytesNumPriv *privLhs = StarbytesNumGetPriv(lhs);
    StarbytesNumPriv *privRhs = StarbytesNumGetPriv(rhs);

    if(StarbytesNumIsIntegralType(privLhs->type) && StarbytesNumIsIntegralType(privRhs->type)){
        int64_t lhsVal = StarbytesNumAsInt64(privLhs);
        int64_t rhsVal = StarbytesNumAsInt64(privRhs);

        if(lhsVal == rhsVal){
            return COMPARE_EQUAL;
        }
        if(lhsVal > rhsVal){
            return COMPARE_GREATER;
        }
        return COMPARE_LESS;
    }

    long double lhsVal = StarbytesNumAsLongDouble(privLhs);
    long double rhsVal = StarbytesNumAsLongDouble(privRhs);
    if(lhsVal == rhsVal){
        return COMPARE_EQUAL;
    }
    if(lhsVal > rhsVal){
        return COMPARE_GREATER;
    }
    return COMPARE_LESS;
}

void StarbytesNumAssign(StarbytesNum num,StarbytesNumT type,...){
    StarbytesNumPriv *priv = StarbytesNumGetPriv(num);
    priv->type = type;
    priv->d = 0.0;
    priv->f = 0.0f;
    priv->l = 0;
    priv->i = 0;
    va_list args;
    va_start(args,type);
    switch(type){
        case NumTypeInt:
            priv->i = va_arg(args,int);
            break;
        case NumTypeLong:
            priv->l = va_arg(args,int64_t);
            break;
        case NumTypeFloat:
            priv->f = (float)va_arg(args,double);
            break;
        case NumTypeDouble:
        default:
            priv->d = va_arg(args,double);
            break;
    }
    va_end(args);
}

StarbytesNum StarbytesNumAdd(StarbytesNum a,StarbytesNum b){
    StarbytesNumPriv *a_priv = StarbytesNumGetPriv(a);
    StarbytesNumPriv *b_priv = StarbytesNumGetPriv(b);

    if(StarbytesNumIsIntegralType(a_priv->type) && StarbytesNumIsIntegralType(b_priv->type)){
        if(a_priv->type == NumTypeLong || b_priv->type == NumTypeLong){
            return StarbytesNumNew(NumTypeLong,StarbytesNumAsInt64(a_priv) + StarbytesNumAsInt64(b_priv));
        }
        return StarbytesNumNew(NumTypeInt,a_priv->i + b_priv->i);
    }
    StarbytesNumT outType = StarbytesPromotedNumericType(a_priv->type,b_priv->type);
    long double result = StarbytesNumAsLongDouble(a_priv) + StarbytesNumAsLongDouble(b_priv);
    if(outType == NumTypeDouble){
        return StarbytesNumNew(NumTypeDouble,(double)result);
    }
    return StarbytesNumNew(NumTypeFloat,(float)result);
}

StarbytesNum StarbytesNumSub(StarbytesNum a,StarbytesNum b){
    StarbytesNumPriv *a_priv = StarbytesNumGetPriv(a);
    StarbytesNumPriv *b_priv = StarbytesNumGetPriv(b);

    if(StarbytesNumIsIntegralType(a_priv->type) && StarbytesNumIsIntegralType(b_priv->type)){
        if(a_priv->type == NumTypeLong || b_priv->type == NumTypeLong){
            return StarbytesNumNew(NumTypeLong,StarbytesNumAsInt64(a_priv) - StarbytesNumAsInt64(b_priv));
        }
        return StarbytesNumNew(NumTypeInt,a_priv->i - b_priv->i);
    }
    StarbytesNumT outType = StarbytesPromotedNumericType(a_priv->type,b_priv->type);
    long double result = StarbytesNumAsLongDouble(a_priv) - StarbytesNumAsLongDouble(b_priv);
    if(outType == NumTypeDouble){
        return StarbytesNumNew(NumTypeDouble,(double)result);
    }
    return StarbytesNumNew(NumTypeFloat,(float)result);
}

StarbytesNumT StarbytesNumGetType(StarbytesNum num){
    StarbytesNumPriv *priv = StarbytesNumGetPriv(num);
    return priv->type;
}

int StarbytesNumGetIntValue(StarbytesNum num){
    StarbytesNumPriv *priv = StarbytesNumGetPriv(num);
    assert(priv->type == NumTypeInt);
    return priv->i;
}

int64_t StarbytesNumGetLongValue(StarbytesNum num){
    StarbytesNumPriv *priv = StarbytesNumGetPriv(num);
    assert(priv->type == NumTypeLong);
    return priv->l;
}

float StarbytesNumGetFloatValue(StarbytesNum num){
    StarbytesNumPriv *priv = StarbytesNumGetPriv(num);
    assert(priv->type == NumTypeFloat);
    return priv->f;
}

double StarbytesNumGetDoubleValue(StarbytesNum num){
    StarbytesNumPriv *priv = StarbytesNumGetPriv(num);
    assert(priv->type == NumTypeDouble);
    return priv->d;
}

/// String Class

typedef struct {
    StarbytesStrEncoding encoding;
    void *data;
    unsigned int length;
} StarbytesStrPriv;

void _StarbytesStrFree(void * obj){
    StarbytesStrPriv *data = (StarbytesStrPriv *)obj;
    free(data->data);
    free(data);
}

StarbytesStr StarbytesStrNew(StarbytesStrEncoding enc){
    StarbytesObject obj = StarbytesObjectNew(StarbytesStrType());
    StarbytesStrPriv privData;
    privData.encoding = enc;
    privData.data = NULL;
    privData.length = 0;
    obj->freePrivData = _StarbytesStrFree;
    obj->privData = malloc(sizeof(StarbytesStrPriv));
    memcpy(obj->privData,&privData,sizeof(StarbytesStrPriv));
    
    return obj;
}

StarbytesStr StarbytesStrCopy(StarbytesStr str){
    StarbytesStrPriv *privData = (StarbytesStrPriv *)str->privData;
    return StarbytesStrNewWithData((const char *)privData->data);
}

StarbytesStr StarbytesStrNewWithData(const char * data){
    StarbytesStr str = StarbytesStrNew(StrEncodingUTF8);
    unsigned int dataLen = (unsigned int)strlen(data);
    StarbytesStrPriv *privData = (StarbytesStrPriv *)str->privData;
    privData->length = utf8_scalar_length(data);
    privData->data = malloc(sizeof(char) * (dataLen + 1));
    memcpy(privData->data,data,dataLen);
    ((char *)privData->data)[dataLen] = '\0';
    return str;
}

unsigned StarbytesStrLength(StarbytesStr str){
    StarbytesStrPriv *privData = (StarbytesStrPriv *)str->privData;
    return privData ? privData->length : 0u;
}

int StarbytesStrCompare(StarbytesStr lhs,StarbytesStr rhs){
    StarbytesStrPriv *privDataLhs = (StarbytesStrPriv *)lhs->privData;
    StarbytesStrPriv *privDataRhs = (StarbytesStrPriv *)rhs->privData;
    
    if(privDataLhs->encoding == privDataRhs->encoding){
        if(privDataRhs->encoding == StrEncodingUTF8){
            int cmp = strcmp((const char *)privDataLhs->data,(const char *)privDataRhs->data);
            if(cmp == 0){
                return COMPARE_EQUAL;
            }
            return cmp < 0 ? COMPARE_LESS : COMPARE_GREATER;
        }
        uint32_t lhs_size = StarbytesStrLength(lhs);
        uint32_t rhs_size = StarbytesStrLength(rhs);
        if(lhs_size == rhs_size){
            if(privDataRhs->encoding == StrEncodingUTF16){
                short *lhs_it = (short *)privDataLhs->data;
                short *rhs_it = (short *)privDataRhs->data;
                
                for(unsigned i = 0;i < lhs_size;i++){
                    if(*lhs_it != *rhs_it){
                        return COMPARE_NOTEQUAL;
                    }
                    ++lhs_it;
                    ++rhs_it;
                }
                
                return COMPARE_EQUAL;
            }
            else {
                int *lhs_it = (int *)privDataLhs->data;
                int *rhs_it = (int *)privDataRhs->data;
                
                for(unsigned i = 0;i < lhs_size;i++){
                    if(*lhs_it != *rhs_it){
                        return COMPARE_NOTEQUAL;
                    }
                    ++lhs_it;
                    ++rhs_it;
                }
                
                return COMPARE_EQUAL;
            }
        }
        else if(lhs_size > rhs_size){
            return COMPARE_GREATER;
        }
        else {
            return COMPARE_LESS;
        }
    }
    else {
        return COMPARE_NOTEQUAL;
    }
}

char * StarbytesStrGetBuffer(StarbytesStr str){
    StarbytesStrPriv *privData = (StarbytesStrPriv *)str->privData;
    return (char *)privData->data;
}

unsigned int StarbytesStrGetLength(StarbytesStr str){
    return (uint32_t)StarbytesStrLength(str);
}

unsigned StarbytesStrByteLength(StarbytesStr str){
    StarbytesStrPriv *privData = (StarbytesStrPriv *)str->privData;
    if(privData == NULL || privData->data == NULL){
        return 0;
    }
    return (unsigned)strlen((const char *)privData->data);
}

/// Array Class

typedef enum {
    StarbytesArrayStorageBoxed = 0,
    StarbytesArrayStorageInt = 1,
    StarbytesArrayStorageLong = 2,
    StarbytesArrayStorageFloat = 3,
    StarbytesArrayStorageDouble = 4
} StarbytesArrayStorageKind;

typedef struct {
    unsigned int length;
    unsigned int capacity;
    StarbytesArrayStorageKind storageKind;
    void *data;
    StarbytesObject *cache;
} StarbytesArrayPriv;

static void StarbytesArraySyncLength(StarbytesArray array,unsigned int length){
    StarbytesArrayPriv *priv = (StarbytesArrayPriv *)array->privData;
    priv->length = length;
}

static size_t StarbytesArrayElementSize(StarbytesArrayStorageKind kind){
    switch(kind){
        case StarbytesArrayStorageInt:
            return sizeof(int);
        case StarbytesArrayStorageLong:
            return sizeof(int64_t);
        case StarbytesArrayStorageFloat:
            return sizeof(float);
        case StarbytesArrayStorageDouble:
            return sizeof(double);
        case StarbytesArrayStorageBoxed:
        default:
            return sizeof(StarbytesObject);
    }
}

static int StarbytesArrayKindIsNumeric(StarbytesArrayStorageKind kind){
    return kind != StarbytesArrayStorageBoxed;
}

static StarbytesArrayStorageKind StarbytesArrayKindFromNumType(StarbytesNumT type){
    switch(type){
        case NumTypeLong:
            return StarbytesArrayStorageLong;
        case NumTypeFloat:
            return StarbytesArrayStorageFloat;
        case NumTypeDouble:
            return StarbytesArrayStorageDouble;
        case NumTypeInt:
        default:
            return StarbytesArrayStorageInt;
    }
}

static StarbytesNumT StarbytesNumTypeFromArrayKind(StarbytesArrayStorageKind kind){
    switch(kind){
        case StarbytesArrayStorageLong:
            return NumTypeLong;
        case StarbytesArrayStorageFloat:
            return NumTypeFloat;
        case StarbytesArrayStorageDouble:
            return NumTypeDouble;
        case StarbytesArrayStorageInt:
        default:
            return NumTypeInt;
    }
}

static int StarbytesArrayKindRank(StarbytesArrayStorageKind kind){
    switch(kind){
        case StarbytesArrayStorageInt:
            return 0;
        case StarbytesArrayStorageLong:
            return 1;
        case StarbytesArrayStorageFloat:
            return 2;
        case StarbytesArrayStorageDouble:
            return 3;
        case StarbytesArrayStorageBoxed:
        default:
            return -1;
    }
}

static StarbytesArrayStorageKind StarbytesPromotedArrayKind(StarbytesArrayStorageKind lhs,StarbytesArrayStorageKind rhs){
    if(StarbytesArrayKindRank(lhs) >= StarbytesArrayKindRank(rhs)){
        return lhs;
    }
    return rhs;
}

static int StarbytesObjectToArrayNumeric(StarbytesObject obj,StarbytesArrayStorageKind *kindOut,long double *valueOut){
    if(obj == NULL || !StarbytesObjectTypecheck(obj,StarbytesNumType())){
        return 0;
    }
    StarbytesNumPriv *priv = StarbytesNumGetPriv(obj);
    *kindOut = StarbytesArrayKindFromNumType(priv->type);
    *valueOut = StarbytesNumAsLongDouble(priv);
    return 1;
}

static long double StarbytesArrayReadNumericValue(const StarbytesArrayPriv *priv,unsigned int index){
    switch(priv->storageKind){
        case StarbytesArrayStorageLong:
            return (long double)((int64_t *)priv->data)[index];
        case StarbytesArrayStorageFloat:
            return (long double)((float *)priv->data)[index];
        case StarbytesArrayStorageDouble:
            return (long double)((double *)priv->data)[index];
        case StarbytesArrayStorageInt:
        default:
            return (long double)((int *)priv->data)[index];
    }
}

static void StarbytesArrayWriteNumericValue(StarbytesArrayPriv *priv,unsigned int index,long double value){
    switch(priv->storageKind){
        case StarbytesArrayStorageLong:
            ((int64_t *)priv->data)[index] = (int64_t)value;
            break;
        case StarbytesArrayStorageFloat:
            ((float *)priv->data)[index] = (float)value;
            break;
        case StarbytesArrayStorageDouble:
            ((double *)priv->data)[index] = (double)value;
            break;
        case StarbytesArrayStorageInt:
        default:
            ((int *)priv->data)[index] = (int)value;
            break;
    }
}

static StarbytesObject StarbytesArrayBoxNumericValue(const StarbytesArrayPriv *priv,unsigned int index){
    switch(priv->storageKind){
        case StarbytesArrayStorageLong:
            return StarbytesNumNew(NumTypeLong,((int64_t *)priv->data)[index]);
        case StarbytesArrayStorageFloat:
            return StarbytesNumNew(NumTypeFloat,(float)((float *)priv->data)[index]);
        case StarbytesArrayStorageDouble:
            return StarbytesNumNew(NumTypeDouble,((double *)priv->data)[index]);
        case StarbytesArrayStorageInt:
        default:
            return StarbytesNumNew(NumTypeInt,((int *)priv->data)[index]);
    }
}

static void StarbytesArrayReleaseNumericCache(StarbytesArrayPriv *priv){
    unsigned int i;
    if(priv->cache == NULL){
        return;
    }
    for(i = 0;i < priv->length;i++){
        if(priv->cache[i] != NULL){
            StarbytesObjectRelease(priv->cache[i]);
            priv->cache[i] = NULL;
        }
    }
}

static int StarbytesArrayEnsureCapacity(StarbytesArrayPriv *priv,unsigned int minCapacity){
    unsigned int newCapacity;
    void *newData;
    StarbytesObject *newCache;
    if(minCapacity <= priv->capacity){
        return 1;
    }
    newCapacity = priv->capacity == 0 ? 4u : priv->capacity;
    while(newCapacity < minCapacity){
        if(newCapacity > 0x7fffffffu){
            newCapacity = minCapacity;
            break;
        }
        newCapacity *= 2u;
    }

    newData = realloc(priv->data,StarbytesArrayElementSize(priv->storageKind) * newCapacity);
    if(newData == NULL){
        return 0;
    }
    priv->data = newData;

    if(StarbytesArrayKindIsNumeric(priv->storageKind)){
        newCache = (StarbytesObject *)realloc(priv->cache,sizeof(StarbytesObject) * newCapacity);
        if(newCache == NULL){
            return 0;
        }
        if(newCapacity > priv->capacity){
            memset(newCache + priv->capacity,0,sizeof(StarbytesObject) * (newCapacity - priv->capacity));
        }
        priv->cache = newCache;
    }
    priv->capacity = newCapacity;
    return 1;
}

static void StarbytesArrayAdoptNumericKind(StarbytesArrayPriv *priv,StarbytesArrayStorageKind kind){
    if(priv->storageKind == kind){
        return;
    }
    StarbytesArrayReleaseNumericCache(priv);
    free(priv->cache);
    free(priv->data);
    priv->cache = NULL;
    priv->data = NULL;
    priv->capacity = 0;
    priv->storageKind = kind;
}

static int StarbytesArrayConvertNumericKind(StarbytesArrayPriv *priv,StarbytesArrayStorageKind newKind){
    void *newData;
    StarbytesObject *newCache;
    unsigned int i;
    if(priv->storageKind == newKind){
        return 1;
    }
    newData = NULL;
    newCache = NULL;
    if(priv->capacity > 0){
        newData = malloc(StarbytesArrayElementSize(newKind) * priv->capacity);
        if(newData == NULL){
            return 0;
        }
        for(i = 0;i < priv->length;i++){
            long double value = StarbytesArrayReadNumericValue(priv,i);
            switch(newKind){
                case StarbytesArrayStorageLong:
                    ((int64_t *)newData)[i] = (int64_t)value;
                    break;
                case StarbytesArrayStorageFloat:
                    ((float *)newData)[i] = (float)value;
                    break;
                case StarbytesArrayStorageDouble:
                    ((double *)newData)[i] = (double)value;
                    break;
                case StarbytesArrayStorageInt:
                default:
                    ((int *)newData)[i] = (int)value;
                    break;
            }
        }
        newCache = (StarbytesObject *)calloc(priv->capacity,sizeof(StarbytesObject));
        if(newCache == NULL){
            free(newData);
            return 0;
        }
    }
    StarbytesArrayReleaseNumericCache(priv);
    free(priv->cache);
    free(priv->data);
    priv->data = newData;
    priv->cache = newCache;
    priv->storageKind = newKind;
    return 1;
}

static int StarbytesArrayMaterializeBoxed(StarbytesArrayPriv *priv){
    StarbytesObject *boxedData;
    unsigned int i;
    if(priv->storageKind == StarbytesArrayStorageBoxed){
        return 1;
    }
    boxedData = NULL;
    if(priv->capacity > 0){
        boxedData = (StarbytesObject *)malloc(sizeof(StarbytesObject) * priv->capacity);
        if(boxedData == NULL){
            return 0;
        }
        for(i = 0;i < priv->length;i++){
            if(priv->cache != NULL && priv->cache[i] != NULL){
                boxedData[i] = priv->cache[i];
                priv->cache[i] = NULL;
            }
            else {
                boxedData[i] = StarbytesArrayBoxNumericValue(priv,i);
            }
        }
    }
    StarbytesArrayReleaseNumericCache(priv);
    free(priv->cache);
    free(priv->data);
    priv->cache = NULL;
    priv->data = boxedData;
    priv->storageKind = StarbytesArrayStorageBoxed;
    return 1;
}

static void StarbytesArrayInvalidateNumericCache(StarbytesArrayPriv *priv,unsigned int index){
    if(priv->cache != NULL && index < priv->capacity && priv->cache[index] != NULL){
        StarbytesObjectRelease(priv->cache[index]);
        priv->cache[index] = NULL;
    }
}

void _StarbytesArrayFree(StarbytesObject array){
    StarbytesArrayPriv *priv = (StarbytesArrayPriv *)array->privData;
    if(priv->storageKind == StarbytesArrayStorageBoxed){
        StarbytesObject *data_it = (StarbytesObject *)priv->data;
        unsigned int len = priv->length;
        for(;len > 0;len--){
            StarbytesObjectRelease(*data_it);
            ++data_it;
        }
    }
    else {
        StarbytesArrayReleaseNumericCache(priv);
        free(priv->cache);
    }
    free(priv->data);
    free(priv);
}

StarbytesArray StarbytesArrayNew(){
    StarbytesObject obj = StarbytesObjectNew(StarbytesArrayType());
    StarbytesArrayPriv privData;
    privData.length = 0;
    privData.capacity = 0;
    privData.storageKind = StarbytesArrayStorageBoxed;
    privData.data = NULL;
    privData.cache = NULL;
    obj->privData = malloc(sizeof(StarbytesArrayPriv));
    memcpy(obj->privData,&privData,sizeof(StarbytesArrayPriv));

    return obj;
}

StarbytesArray StarbytesArrayCopy(StarbytesArray array){
    StarbytesArray copy = StarbytesArrayNew();
    StarbytesArrayPriv *src = (StarbytesArrayPriv *)array->privData;
    StarbytesArrayPriv *dst = (StarbytesArrayPriv *)copy->privData;
    unsigned int idx;
    if(src->length == 0){
        return copy;
    }
    dst->storageKind = src->storageKind;
    if(!StarbytesArrayEnsureCapacity(dst,src->length)){
        return copy;
    }
    if(src->storageKind == StarbytesArrayStorageBoxed){
        for(idx = 0;idx < src->length;idx++){
            StarbytesObject element = ((StarbytesObject *)src->data)[idx];
            StarbytesObjectReference(element);
            ((StarbytesObject *)dst->data)[idx] = element;
        }
    }
    else {
        memcpy(dst->data,src->data,StarbytesArrayElementSize(src->storageKind) * src->length);
    }
    StarbytesArraySyncLength(copy,src->length);
    return copy;
}

void StarbytesArrayReserve(StarbytesArray array,unsigned int capacity){
    StarbytesArrayPriv *priv = (StarbytesArrayPriv *)array->privData;
    if(capacity <= priv->capacity){
        return;
    }
    (void)StarbytesArrayEnsureCapacity(priv,capacity);
}

unsigned int StarbytesArrayGetLength(StarbytesArray array){
    StarbytesArrayPriv *priv = (StarbytesArrayPriv *)array->privData;
    return priv->length;
}

void StarbytesArrayPush(StarbytesArray array,StarbytesObject obj){
    StarbytesArrayPriv *priv = (StarbytesArrayPriv *)array->privData;
    unsigned int len = priv->length;
    StarbytesArrayStorageKind incomingKind;
    long double incomingValue;
    if(StarbytesObjectToArrayNumeric(obj,&incomingKind,&incomingValue)){
        if(priv->length == 0){
            StarbytesArrayAdoptNumericKind(priv,incomingKind);
        }
        if(StarbytesArrayKindIsNumeric(priv->storageKind)){
            if(StarbytesArrayKindRank(incomingKind) != StarbytesArrayKindRank(priv->storageKind)){
                if(!StarbytesArrayConvertNumericKind(priv,StarbytesPromotedArrayKind(priv->storageKind,incomingKind))){
                    return;
                }
            }
            if(!StarbytesArrayEnsureCapacity(priv,len + 1)){
                return;
            }
            StarbytesArrayWriteNumericValue(priv,len,incomingValue);
            StarbytesArrayInvalidateNumericCache(priv,len);
            StarbytesArraySyncLength(array,len + 1);
            return;
        }
    }

    if(priv->storageKind != StarbytesArrayStorageBoxed){
        if(!StarbytesArrayMaterializeBoxed(priv)){
            return;
        }
    }
    if(!StarbytesArrayEnsureCapacity(priv,len + 1)){
        return;
    }
    StarbytesObjectReference(obj);
    ((StarbytesObject *)priv->data)[len] = obj;
    StarbytesArraySyncLength(array,len + 1);
}

void StarbytesArrayPop(StarbytesArray array){
    StarbytesArrayPriv *priv = (StarbytesArrayPriv *)array->privData;
    unsigned int len = priv->length;
    if(len <= 0){
        return;
    }
    if(priv->storageKind == StarbytesArrayStorageBoxed){
        StarbytesObjectRelease(((StarbytesObject *)priv->data)[len - 1]);
    }
    else {
        StarbytesArrayInvalidateNumericCache(priv,len - 1);
    }
    StarbytesArraySyncLength(array,len - 1);
}

StarbytesObject StarbytesArrayIndex(StarbytesArray array,unsigned int index){
    StarbytesArrayPriv *priv = (StarbytesArrayPriv *)array->privData;
    unsigned int len = priv->length;
    assert(index < len && "Cannot index object outside of bounds");
    if(priv->storageKind == StarbytesArrayStorageBoxed){
        return ((StarbytesObject *)priv->data)[index];
    }
    if(priv->cache[index] == NULL){
        priv->cache[index] = StarbytesArrayBoxNumericValue(priv,index);
    }
    return priv->cache[index];
}

void StarbytesArraySet(StarbytesArray array,unsigned int index,StarbytesObject obj){
    StarbytesArrayPriv *priv = (StarbytesArrayPriv *)array->privData;
    unsigned int len = priv->length;
    StarbytesArrayStorageKind incomingKind;
    long double incomingValue;
    assert(index < len && "Cannot index object outside of bounds");
    if(StarbytesObjectToArrayNumeric(obj,&incomingKind,&incomingValue) && StarbytesArrayKindIsNumeric(priv->storageKind)){
        if(StarbytesArrayKindRank(incomingKind) != StarbytesArrayKindRank(priv->storageKind)){
            if(!StarbytesArrayConvertNumericKind(priv,StarbytesPromotedArrayKind(priv->storageKind,incomingKind))){
                return;
            }
        }
        StarbytesArrayWriteNumericValue(priv,index,incomingValue);
        StarbytesArrayInvalidateNumericCache(priv,index);
        return;
    }
    if(priv->storageKind != StarbytesArrayStorageBoxed){
        if(!StarbytesArrayMaterializeBoxed(priv)){
            return;
        }
    }
    {
        StarbytesObject old = ((StarbytesObject *)priv->data)[index];
        StarbytesObjectRelease(old);
        StarbytesObjectReference(obj);
        ((StarbytesObject *)priv->data)[index] = obj;
    }
}

int StarbytesArrayTryGetNumeric(StarbytesArray array,unsigned int index,StarbytesNumT outType,long double *valueOut){
    StarbytesArrayPriv *priv = (StarbytesArrayPriv *)array->privData;
    long double value;
    if(index >= priv->length){
        return 0;
    }
    if(priv->storageKind == StarbytesArrayStorageBoxed){
        StarbytesObject valueObj = ((StarbytesObject *)priv->data)[index];
        StarbytesArrayStorageKind valueKind;
        if(!StarbytesObjectToArrayNumeric(valueObj,&valueKind,&value)){
            return 0;
        }
    }
    else {
        value = StarbytesArrayReadNumericValue(priv,index);
    }
    if(valueOut != NULL){
        switch(outType){
            case NumTypeLong:
                *valueOut = (long double)((int64_t)value);
                break;
            case NumTypeFloat:
                *valueOut = (long double)((float)value);
                break;
            case NumTypeDouble:
                *valueOut = (long double)((double)value);
                break;
            case NumTypeInt:
            default:
                *valueOut = (long double)((int)value);
                break;
        }
    }
    return 1;
}

int StarbytesArrayTrySetNumeric(StarbytesArray array,unsigned int index,StarbytesNumT valueType,long double value){
    StarbytesArrayPriv *priv = (StarbytesArrayPriv *)array->privData;
    StarbytesArrayStorageKind incomingKind = StarbytesArrayKindFromNumType(valueType);
    if(index >= priv->length){
        return 0;
    }
    if(!StarbytesArrayKindIsNumeric(priv->storageKind)){
        return 0;
    }
    if(StarbytesArrayKindRank(incomingKind) != StarbytesArrayKindRank(priv->storageKind)){
        if(!StarbytesArrayConvertNumericKind(priv,StarbytesPromotedArrayKind(priv->storageKind,incomingKind))){
            return 0;
        }
    }
    StarbytesArrayWriteNumericValue(priv,index,value);
    StarbytesArrayInvalidateNumericCache(priv,index);
    return 1;
}


typedef struct {
    unsigned int length;
    StarbytesArray keys;
    StarbytesArray values;
} StarbytesDictPriv;

static void _StarbytesDictFree(void *data){
    StarbytesDictPriv *priv = (StarbytesDictPriv *)data;
    if(priv == NULL){
        return;
    }
    if(priv->keys){
        StarbytesObjectRelease(priv->keys);
    }
    if(priv->values){
        StarbytesObjectRelease(priv->values);
    }
    free(priv);
}

StarbytesDict StarbytesDictNew(){
    StarbytesObject obj = StarbytesObjectNew(StarbytesDictType());
    StarbytesDictPriv privData;
    privData.length = 0;
    privData.keys = StarbytesArrayNew();
    privData.values = StarbytesArrayNew();
    obj->freePrivData = _StarbytesDictFree;
    obj->privData = malloc(sizeof(StarbytesDictPriv));
    memcpy(obj->privData,&privData,sizeof(StarbytesDictPriv));
    return obj;
}

StarbytesDict StarbytesDictCopy(StarbytesDict dict){
    StarbytesObject obj = StarbytesObjectNew(StarbytesDictType());
    StarbytesDictPriv privData;
    privData.length = StarbytesDictGetLength(dict);
    privData.keys = StarbytesArrayCopy(StarbytesDictGetKeys(dict));
    privData.values = StarbytesArrayCopy(StarbytesDictGetValues(dict));
    obj->freePrivData = _StarbytesDictFree;
    obj->privData = malloc(sizeof(StarbytesDictPriv));
    memcpy(obj->privData,&privData,sizeof(StarbytesDictPriv));
    return obj;
}

void StarbytesDictSet(StarbytesDict dict,StarbytesObject key,StarbytesObject val){
    StarbytesDictPriv *priv = (StarbytesDictPriv *)dict->privData;
    StarbytesArray keys = priv->keys;
    StarbytesArray vals = priv->values;
    unsigned int len;
    int willReturn = 0;
    assert(key->type == StarbytesNumType() || key->type == StarbytesStrType());
    len = StarbytesArrayGetLength(keys);
    for(unsigned i = 0;i < len;i++){
        StarbytesObject _key = StarbytesArrayIndex(keys,i);
        if(_key->type == StarbytesNumType()){
            if(StarbytesNumCompare(key,_key) == COMPARE_EQUAL){
                willReturn = 1;
                StarbytesArraySet(vals,i,val);
                break;
            }
        }
        else if(_key->type == StarbytesStrType()){
            if(StarbytesStrCompare(key,_key) == COMPARE_EQUAL){
                willReturn = 1;
                StarbytesArraySet(vals,i,val);
                break;
            }
        }
    }
    
    if(willReturn == 0){
        StarbytesArrayPush(keys,key);
        StarbytesArrayPush(vals,val);
        priv->length = len + 1;
    }
}

StarbytesObject StarbytesDictGet(StarbytesDict dict,StarbytesObject key){
    StarbytesDictPriv *priv = (StarbytesDictPriv *)dict->privData;
    StarbytesArray keys = priv->keys;
    StarbytesArray vals = priv->values;
    unsigned int len;
    assert(key->type == StarbytesNumType() || key->type == StarbytesStrType());
    len = StarbytesArrayGetLength(keys);
    for(unsigned i = 0;i < len;i++){
        StarbytesObject _key = StarbytesArrayIndex(keys,i);
        if(_key->type == StarbytesNumType()){
            if(StarbytesNumCompare(key,_key) == COMPARE_EQUAL){
                return StarbytesArrayIndex(vals,i);
            }
        }
        else if(_key->type == StarbytesStrType()){
            if(StarbytesStrCompare(key,_key) == COMPARE_EQUAL){
                return StarbytesArrayIndex(vals,i);
            }
        }
    }
    return NULL;
}

unsigned int StarbytesDictGetLength(StarbytesDict dict){
    StarbytesDictPriv *priv = (StarbytesDictPriv *)dict->privData;
    return priv ? priv->length : 0u;
}

StarbytesArray StarbytesDictGetKeys(StarbytesDict dict){
    StarbytesDictPriv *priv = (StarbytesDictPriv *)dict->privData;
    return priv ? priv->keys : NULL;
}

StarbytesArray StarbytesDictGetValues(StarbytesDict dict){
    StarbytesDictPriv *priv = (StarbytesDictPriv *)dict->privData;
    return priv ? priv->values : NULL;
}

void StarbytesDictSetLength(StarbytesDict dict,unsigned int length){
    StarbytesDictPriv *priv = (StarbytesDictPriv *)dict->privData;
    if(priv){
        priv->length = length;
    }
}

/// Starbytes Bool
///

StarbytesBool StarbytesBoolNew(StarbytesBoolVal val){
    StarbytesObject obj = StarbytesObjectNew(StarbytesBoolType());
    obj->inlinePayload.boolean.val = val;
    return obj;
}

StarbytesBoolVal StarbytesBoolValue(StarbytesBool boolean){
    StarbytesBoolPriv *privData = StarbytesBoolGetPriv(boolean);
    return privData->val;
}


/// Starbytes Func Ref

StarbytesFuncRef StarbytesFuncRefNew(StarbytesFuncTemplate *_template){
    StarbytesObject obj = StarbytesObjectNew(StarbytesFuncRefType());
    obj->inlinePayload.funcRef.ref = _template;
    return obj;
}

StarbytesFuncTemplate * StarbytesFuncRefGetPtr(StarbytesFuncRef ref){
    StarbytesFuncRefPriv * privData = StarbytesFuncRefGetPriv(ref);
    return privData->ref;
};

/// Starbytes Task

StarbytesTask StarbytesTaskNew(){
    StarbytesObject obj = StarbytesObjectNew(StarbytesTaskType());
    obj->inlinePayload.task.state = StarbytesTaskPending;
    obj->inlinePayload.task.value = NULL;
    obj->inlinePayload.task.error = NULL;
    return obj;
}

StarbytesTaskState StarbytesTaskGetState(StarbytesTask task){
    if(!task){
        return StarbytesTaskRejected;
    }
    StarbytesTaskPriv *privData = StarbytesTaskGetPriv(task);
    return privData->state;
}

void StarbytesTaskResolve(StarbytesTask task,StarbytesObject value){
    if(!task){
        return;
    }
    StarbytesTaskPriv *privData = StarbytesTaskGetPriv(task);
    if(privData->state != StarbytesTaskPending){
        return;
    }
    privData->state = StarbytesTaskResolved;
    if(value){
        StarbytesObjectReference(value);
    }
    privData->value = value;
}

void StarbytesTaskReject(StarbytesTask task,const char *error){
    if(!task){
        return;
    }
    StarbytesTaskPriv *privData = StarbytesTaskGetPriv(task);
    if(privData->state != StarbytesTaskPending){
        return;
    }
    privData->state = StarbytesTaskRejected;
    if(error){
        size_t len = strlen(error);
        privData->error = (char *)malloc(len + 1);
        memcpy(privData->error,error,len + 1);
    }
}

StarbytesObject StarbytesTaskGetValue(StarbytesTask task){
    if(!task){
        return NULL;
    }
    StarbytesTaskPriv *privData = StarbytesTaskGetPriv(task);
    if(privData->state != StarbytesTaskResolved || !privData->value){
        return NULL;
    }
    StarbytesObjectReference(privData->value);
    return privData->value;
}

CString StarbytesTaskGetError(StarbytesTask task){
    if(!task){
        return NULL;
    }
    StarbytesTaskPriv *privData = StarbytesTaskGetPriv(task);
    if(privData->state != StarbytesTaskRejected){
        return NULL;
    }
    return privData->error;
}
