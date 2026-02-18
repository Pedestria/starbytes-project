#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>
#include <stdint.h>

#include <starbytes/interop.h>




struct _StarbytesObject {
    
    size_t type;
    unsigned int refCount;
    
    unsigned int nProp;
    StarbytesObjectProperty *props;
    
    void *privData;
    void (*freePrivData)(void *);
    
};

struct _StarbytesFuncArgs {
    unsigned argc;
    unsigned index;
    StarbytesObject *argv;
};

static void _StarbytesPrivDataFreeDefault(void *data){
    free(data);
}

StarbytesObject StarbytesFuncArgsGetArg(StarbytesFuncArgs args){
    if(args == NULL || args->argv == NULL || args->index >= args->argc){
        return NULL;
    }
    return args->argv[args->index++];
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
    obj.nProp = 0;
    obj.props = NULL;
    obj.refCount = 1;
    obj.type = type;
    obj.privData = NULL;
    obj.freePrivData = _StarbytesPrivDataFreeDefault;
    
    StarbytesObject mem = (StarbytesObject)malloc(sizeof(struct _StarbytesObject));
    memcpy(mem,&obj,sizeof(struct _StarbytesObject));
    return mem;
}

int StarbytesObjectTypecheck(StarbytesObject object,size_t type){
    return object->type == type;
}

unsigned int StarbytesObjectGetPropertyCount(StarbytesObject obj){
    return obj->nProp;
}

void StarbytesObjectAddProperty(StarbytesObject obj,char *name,StarbytesObject data){
    
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
    obj->refCount += 1;
}

void StarbytesObjectRelease(StarbytesObject obj){
    obj->refCount -= 1;
    if(obj->refCount == 0){
        unsigned prop_c = obj->nProp;
        StarbytesObjectProperty *prop_ptr = obj->props;
        for(;prop_c > 0;prop_c--){
            StarbytesObjectRelease(prop_ptr->data);
            ++prop_ptr;
        }
        free(obj->props);
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

StarbytesClassType StarbytesClassObjectGetClass(StarbytesObject obj){
    return obj->type;
}



/// Number Class

typedef struct {
    StarbytesNumT type;
    float f;
    int i;
} StarbytesNumPriv;

void _StarbytesNumFree(void *data){
    StarbytesNumPriv *priv = (StarbytesNumPriv *)data;
    free(priv);
}

StarbytesNum StarbytesNumNew(StarbytesNumT type,...){
    StarbytesObject obj = StarbytesObjectNew(StarbytesNumType());
    obj->freePrivData = _StarbytesNumFree;
    StarbytesNumPriv priv;
    priv.type = type;

    va_list args;
    va_start(args,type);
    if(type == NumTypeInt){
        priv.i = va_arg(args,int);
        priv.f = 0;
    }
    else {
        priv.f = (float)va_arg(args,double);
        priv.i = 0;
    }
    va_end(args);
    
    obj->privData = malloc(sizeof(StarbytesNumPriv));
    memcpy(obj->privData,&priv,sizeof(StarbytesNumPriv));
    
    return obj;
}

StarbytesNum StarbytesNumCopy(StarbytesNum num){
    StarbytesNumPriv *priv = (StarbytesNumPriv *)num->privData;
    if(priv->type == NumTypeInt){
        return StarbytesNumNew(priv->type,priv->i);
    }
    else {
        return StarbytesNumNew(priv->type,priv->f);
    }
}

StarbytesNum StarbytesNumConvertTo(StarbytesNum num,StarbytesNumT type){
    StarbytesNumPriv *priv = (StarbytesNumPriv *)num->privData;
    if(priv->type == type){
        if(type == NumTypeFloat){
            return StarbytesNumNew(NumTypeFloat,priv->f);
        }
        else {
            return StarbytesNumNew(NumTypeInt,priv->i);
        }
    }
    else {
        if(type == NumTypeFloat){
            return StarbytesNumNew(NumTypeFloat,(float)priv->i);
        }
        else {
            return StarbytesNumNew(NumTypeInt,(int)priv->f);
        }
    }
}

int StarbytesNumCompare(StarbytesNum lhs,StarbytesNum rhs){
    StarbytesNumPriv *privLhs = (StarbytesNumPriv *)lhs->privData;
    StarbytesNumPriv *privRhs = (StarbytesNumPriv *)rhs->privData;

    float lhsVal = (privLhs->type == NumTypeFloat)? privLhs->f : (float)privLhs->i;
    float rhsVal = (privRhs->type == NumTypeFloat)? privRhs->f : (float)privRhs->i;

    if(lhsVal == rhsVal){
        return COMPARE_EQUAL;
    }
    if(lhsVal > rhsVal){
        return COMPARE_GREATER;
    }
    return COMPARE_LESS;
}

void StarbytesNumAssign(StarbytesNum num,StarbytesNumT type,...){
    StarbytesNumPriv *priv = (StarbytesNumPriv *)num->privData;
    priv->type = type;
    va_list args;
    va_start(args,type);
    if(type == NumTypeInt){
        priv->i = va_arg(args,int);
        priv->f = 0;
    }
    else {
        priv->f = (float)va_arg(args,double);
        priv->i = 0;
    }
    va_end(args);
}

StarbytesNum StarbytesNumAdd(StarbytesNum a,StarbytesNum b){
    StarbytesNumPriv *a_priv = (StarbytesNumPriv *)a->privData;
    StarbytesNumPriv *b_priv = (StarbytesNumPriv *)b->privData;

    if(a_priv->type == NumTypeInt && b_priv->type == NumTypeInt){
        int r = a_priv->i + b_priv->i;
        return StarbytesNumNew(NumTypeInt,r);
    }
    float af = (a_priv->type == NumTypeFloat)? a_priv->f : (float)a_priv->i;
    float bf = (b_priv->type == NumTypeFloat)? b_priv->f : (float)b_priv->i;
    return StarbytesNumNew(NumTypeFloat,af + bf);
}

StarbytesNum StarbytesNumSub(StarbytesNum a,StarbytesNum b){
    StarbytesNumPriv *a_priv = (StarbytesNumPriv *)a->privData;
    StarbytesNumPriv *b_priv = (StarbytesNumPriv *)b->privData;

    if(a_priv->type == NumTypeInt && b_priv->type == NumTypeInt){
        int r = a_priv->i - b_priv->i;
        return StarbytesNumNew(NumTypeInt,r);
    }
    float af = (a_priv->type == NumTypeFloat)? a_priv->f : (float)a_priv->i;
    float bf = (b_priv->type == NumTypeFloat)? b_priv->f : (float)b_priv->i;
    return StarbytesNumNew(NumTypeFloat,af - bf);
}

StarbytesNumT StarbytesNumGetType(StarbytesNum num){
    StarbytesNumPriv *priv = (StarbytesNumPriv *)num->privData;
    return priv->type;
}

int StarbytesNumGetIntValue(StarbytesNum num){
    StarbytesNumPriv *priv = (StarbytesNumPriv *)num->privData;
    assert(priv->type == NumTypeInt);
    return priv->i;
}

float StarbytesNumGetFloatValue(StarbytesNum num){
    StarbytesNumPriv *priv = (StarbytesNumPriv *)num->privData;
    assert(priv->type == NumTypeFloat);
    return priv->f;
}

/// String Class

typedef struct {
    StarbytesStrEncoding encoding;
    void *data;
} StarbytesStrPriv;

void _StarbytesStrFree(void * obj){
    StarbytesStrPriv *data = (StarbytesStrPriv *)obj;
    free(data->data);
    free(data);
}

StarbytesStr StarbytesStrNew(StarbytesStrEncoding enc){
    StarbytesObject obj = StarbytesObjectNew(StarbytesStrType());
    StarbytesObjectAddProperty(obj,"length",StarbytesNumNew(NumTypeInt,(int)0));
    StarbytesStrPriv privData;
    privData.encoding = enc;
    privData.data = NULL;
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
    StarbytesObject len = StarbytesObjectGetProperty(str,"length");
    unsigned int dataLen = (unsigned int)strlen(data);
    StarbytesNumAssign(len,NumTypeInt,dataLen);
    StarbytesStrPriv *privData = (StarbytesStrPriv *)str->privData;
    privData->data = malloc(sizeof(char) * (dataLen + 1));
    memcpy(privData->data,data,dataLen);
    ((char *)privData->data)[dataLen] = '\0';
    return str;
}

unsigned StarbytesStrLength(StarbytesStr str){
    StarbytesNum len = StarbytesObjectGetProperty(str,"length");
    return (unsigned)StarbytesNumGetIntValue(len);
}

int StarbytesStrCompare(StarbytesStr lhs,StarbytesStr rhs){
    StarbytesStrPriv *privDataLhs = (StarbytesStrPriv *)lhs->privData;
    StarbytesStrPriv *privDataRhs = (StarbytesStrPriv *)rhs->privData;
    
    if(privDataLhs->encoding == privDataRhs->encoding){
        uint32_t lhs_size = StarbytesStrLength(lhs);
        uint32_t rhs_size = StarbytesStrLength(rhs);
        if(lhs_size == rhs_size){
            if(privDataRhs->encoding == StrEncodingUTF8){
                char *lhs_it = (char *)privDataLhs->data;
                char *rhs_it = (char *)privDataRhs->data;
                
                for(unsigned i = 0;i < lhs_size;i++){
                    if(*lhs_it != *rhs_it){
                        return COMPARE_NOTEQUAL;
                    }
                    ++lhs_it;
                    ++rhs_it;
                }
                
                return COMPARE_EQUAL;
            }
            else if(privDataRhs->encoding == StrEncodingUTF16){
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
    StarbytesObject len = StarbytesObjectGetProperty(str,"length");
    return (uint32_t)StarbytesNumGetIntValue(len);
}

/// Array Class

typedef struct {
    StarbytesObject *data;
} StarbytesArrayPriv;

void _StarbytesArrayFree(StarbytesObject array){
    StarbytesArrayPriv *priv = (StarbytesArrayPriv *)array->privData;
    StarbytesObject *data_it = priv->data;
    int len = StarbytesNumGetIntValue(StarbytesObjectGetProperty(array,"length"));
    for(;len > 0;len--){
        StarbytesObjectRelease(*data_it);
        ++data_it;
    };
    free(priv->data);
    free(priv);
}

StarbytesArray StarbytesArrayNew(){
    StarbytesObject obj = StarbytesObjectNew(StarbytesArrayType());
    int len = 0;
    StarbytesObjectAddProperty(obj,"length",StarbytesNumNew(NumTypeInt,len));
    StarbytesArrayPriv privData;
    privData.data = NULL;
    obj->privData = malloc(sizeof(StarbytesArrayPriv));
    memcpy(obj->privData,&privData,sizeof(StarbytesArrayPriv));

    return obj;
}

StarbytesArray StarbytesArrayCopy(StarbytesArray array){
    StarbytesArray copy = StarbytesArrayNew();
    unsigned int len = StarbytesArrayGetLength(array);
    for(unsigned idx = 0;idx < len;idx++){
        StarbytesArrayPush(copy,StarbytesArrayIndex(array,idx));
    }
    return copy;
}

unsigned int StarbytesArrayGetLength(StarbytesArray array){
    StarbytesNum len = StarbytesObjectGetProperty(array,"length");
    return (uint32_t)StarbytesNumGetIntValue(len);
}

void StarbytesArrayPush(StarbytesArray array,StarbytesObject obj){
    StarbytesArrayPriv *priv = (StarbytesArrayPriv *)array->privData;
    StarbytesObjectReference(obj);
    StarbytesNum num = StarbytesObjectGetProperty(array,"length");
    int len = StarbytesNumGetIntValue(num);
    if(priv->data == NULL){
        priv->data = (StarbytesObject *)malloc(sizeof(StarbytesObject));
    }
    else {
        priv->data = (StarbytesObject *)realloc(priv->data,sizeof(StarbytesObject) * (len + 1));
    }
    memcpy(priv->data + len,&obj,sizeof(StarbytesObject));
    StarbytesNumAssign(num,NumTypeInt,len + 1);
    
}

void StarbytesArrayPop(StarbytesArray array){
    StarbytesArrayPriv *priv = (StarbytesArrayPriv *)array->privData;
    StarbytesNum num = StarbytesObjectGetProperty(array,"length");
    int len = StarbytesNumGetIntValue(num);
    if(len <= 0){
        return;
    }
    StarbytesObjectRelease(priv->data[len - 1]);
    if(len == 1){
        free(priv->data);
        priv->data = NULL;
    }
    else {
        priv->data = (StarbytesObject *)realloc(priv->data,sizeof(StarbytesObject) * (len - 1));
    }
    StarbytesNumAssign(num,NumTypeInt,len - 1);
};

StarbytesObject StarbytesArrayIndex(StarbytesArray array,unsigned int index){
    StarbytesArrayPriv *priv = (StarbytesArrayPriv *)array->privData;
    unsigned int len = StarbytesArrayGetLength(array);
    assert(index < len && "Cannot index object outside of bounds");
    return priv->data[index];
}

void StarbytesArraySet(StarbytesArray array,unsigned int index,StarbytesObject obj){
    StarbytesArrayPriv *priv = (StarbytesArrayPriv *)array->privData;
    unsigned int len = StarbytesArrayGetLength(array);
    assert(index < len && "Cannot index object outside of bounds");
    StarbytesObject old = priv->data[index];
    StarbytesObjectRelease(old);
    StarbytesObjectReference(obj);
    priv->data[index] = obj;
}


StarbytesDict StarbytesDictNew(){
    StarbytesObject obj = StarbytesObjectNew(StarbytesDictType());
    StarbytesObjectAddProperty(obj,"length",StarbytesNumNew(NumTypeInt,(int)0));
    StarbytesObjectAddProperty(obj,"keys",StarbytesArrayNew());
    StarbytesObjectAddProperty(obj,"values",StarbytesArrayNew());
    return obj;
}

StarbytesDict StarbytesDictCopy(StarbytesDict dict){
    StarbytesObject obj = StarbytesObjectNew(StarbytesDictType());
    StarbytesNum dictL = StarbytesObjectGetProperty(dict,"length");
    StarbytesObjectAddProperty(obj,"length",StarbytesNumCopy(dictL));
    StarbytesObjectAddProperty(obj,"keys",StarbytesArrayCopy(StarbytesObjectGetProperty(dict,"keys")));
    StarbytesObjectAddProperty(obj,"values",StarbytesArrayCopy(StarbytesObjectGetProperty(dict,"values")));
    return obj;
}

void StarbytesDictSet(StarbytesDict dict,StarbytesObject key,StarbytesObject val){
    assert(key->type == StarbytesNumType() || key->type == StarbytesStrType());
    StarbytesArray keys = StarbytesObjectGetProperty(dict,"keys");
    StarbytesArray vals = StarbytesObjectGetProperty(dict,"values");
    
    StarbytesArrayPriv *priv = (StarbytesArrayPriv *)vals->privData;
    
    unsigned int len = StarbytesArrayGetLength(keys);
    int willReturn = 0;
    for(unsigned i = 0;i < len;i++){
        StarbytesObject _key = StarbytesArrayIndex(keys,i);
        if(_key->type == StarbytesNumType()){
            if(StarbytesNumCompare(key,_key) == COMPARE_EQUAL){
                willReturn = 1;
                StarbytesObject old_val = priv->data[i];
                StarbytesObjectRelease(old_val);
                StarbytesObjectReference(val);
                memcpy(priv->data + i,&val,sizeof(StarbytesObject));
                break;
            }
        }
        else if(_key->type == StarbytesStrType()){
            if(StarbytesStrCompare(key,_key) == COMPARE_EQUAL){
                willReturn = 1;
                StarbytesObject old_val = priv->data[i];
                StarbytesObjectRelease(old_val);
                StarbytesObjectReference(val);
                memcpy(priv->data + i,&val,sizeof(StarbytesObject));
                break;
            }
        }
    }
    
    if(willReturn == 0){
    
        /// Key doesn't exist.
        StarbytesArrayPush(keys,key);
        StarbytesArrayPush(vals,val);
        
        StarbytesNumAssign(StarbytesObjectGetProperty(dict,"length"),NumTypeInt,(int)len + 1);
    }
}

StarbytesObject StarbytesDictGet(StarbytesDict dict,StarbytesObject key){
    assert(key->type == StarbytesNumType() || key->type == StarbytesStrType());
    StarbytesArray keys = StarbytesObjectGetProperty(dict,"keys");
    StarbytesArray vals = StarbytesObjectGetProperty(dict,"values");
    
    unsigned int len = StarbytesArrayGetLength(keys);
    int willReturn = 0;
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

/// Starbytes Bool
///

typedef struct {
    StarbytesBoolVal val;
} StarbytesBoolPriv;

void _StarbytesBoolFree(void *data){
    StarbytesBoolPriv *priv = (StarbytesBoolPriv *)data;
    free(priv);
}

StarbytesBool StarbytesBoolNew(StarbytesBoolVal val){
    StarbytesObject obj = StarbytesObjectNew(StarbytesBoolType());
    StarbytesBoolPriv privData;
    privData.val = val;
    obj->privData = malloc(sizeof(StarbytesBoolPriv));
    obj->freePrivData = _StarbytesBoolFree;
    memcpy(obj->privData,&privData,sizeof(StarbytesBoolPriv));
    return obj;
}

StarbytesBoolVal StarbytesBoolValue(StarbytesBool boolean){
    StarbytesBoolPriv *privData = (StarbytesBoolPriv *)boolean->privData;
    return privData->val;
}


/// Starbytes Func Ref

typedef struct {
    StarbytesFuncTemplate *ref;
} StarbytesFuncRefPriv;

void _StarbytesFuncRefFree(void *data){
    StarbytesFuncRefPriv *priv = (StarbytesFuncRefPriv *)data;
    free(priv);
}

StarbytesFuncRef StarbytesFuncRefNew(StarbytesFuncTemplate *_template){
    StarbytesObject obj = StarbytesObjectNew(StarbytesFuncRefType());
    StarbytesFuncRefPriv privData;
    privData.ref = _template;
    obj->privData = malloc(sizeof(StarbytesFuncRefPriv));
    obj->freePrivData = _StarbytesFuncRefFree;
    memcpy(obj->privData,&privData,sizeof(StarbytesFuncRefPriv));
    return obj;
}

StarbytesFuncTemplate * StarbytesFuncRefGetPtr(StarbytesFuncRef ref){
    StarbytesFuncRefPriv * privData = (StarbytesFuncRefPriv *)ref->privData;
    return privData->ref;
};

/// Starbytes Task

typedef struct {
    StarbytesTaskState state;
    StarbytesObject value;
    char *error;
} StarbytesTaskPriv;

void _StarbytesTaskFree(void *data){
    StarbytesTaskPriv *priv = (StarbytesTaskPriv *)data;
    if(priv->value){
        StarbytesObjectRelease(priv->value);
    }
    if(priv->error){
        free(priv->error);
    }
    free(priv);
}

StarbytesTask StarbytesTaskNew(){
    StarbytesObject obj = StarbytesObjectNew(StarbytesTaskType());
    StarbytesTaskPriv privData;
    privData.state = StarbytesTaskPending;
    privData.value = NULL;
    privData.error = NULL;
    obj->privData = malloc(sizeof(StarbytesTaskPriv));
    obj->freePrivData = _StarbytesTaskFree;
    memcpy(obj->privData,&privData,sizeof(StarbytesTaskPriv));
    return obj;
}

StarbytesTaskState StarbytesTaskGetState(StarbytesTask task){
    if(!task || !task->privData){
        return StarbytesTaskRejected;
    }
    StarbytesTaskPriv *privData = (StarbytesTaskPriv *)task->privData;
    return privData->state;
}

void StarbytesTaskResolve(StarbytesTask task,StarbytesObject value){
    if(!task || !task->privData){
        return;
    }
    StarbytesTaskPriv *privData = (StarbytesTaskPriv *)task->privData;
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
    if(!task || !task->privData){
        return;
    }
    StarbytesTaskPriv *privData = (StarbytesTaskPriv *)task->privData;
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
    if(!task || !task->privData){
        return NULL;
    }
    StarbytesTaskPriv *privData = (StarbytesTaskPriv *)task->privData;
    if(privData->state != StarbytesTaskResolved || !privData->value){
        return NULL;
    }
    StarbytesObjectReference(privData->value);
    return privData->value;
}

CString StarbytesTaskGetError(StarbytesTask task){
    if(!task || !task->privData){
        return NULL;
    }
    StarbytesTaskPriv *privData = (StarbytesTaskPriv *)task->privData;
    if(privData->state != StarbytesTaskRejected){
        return NULL;
    }
    return privData->error;
}
