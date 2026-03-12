#include "starbytes/runtime/RTEngine.h"
#include "starbytes/compiler/RTCode.h"
#include "RTStdlib.h"
#include "starbytes/runtime/RegexSupport.h"
#include "starbytes/base/ADT.h"
#include "starbytes/base/Diagnostic.h"

#include <iostream>
#include <iomanip>
#include <cstring>
#include <cassert>
#include <cmath>
#include <algorithm>
#include <exception>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <sstream>
#include <set>
#include <vector>
#include <deque>
#include <limits>

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

#include "starbytes/interop.h"

namespace starbytes::Runtime {

struct RTScope {
    std::string name;
    RTScope *parent = nullptr;
};

struct _StarbytesFuncArgs {
    unsigned argc = 0;
    unsigned index = 0;
    StarbytesObject *argv = nullptr;
    char *errorMessage = nullptr;
};

RTScope *RTSCOPE_GLOBAL = new RTScope({"__GLOBAL__"});

static std::string rtidToString(const RTID &id){
    return std::string(id.value,id.len);
}

static std::string attributeArgValueToString(const RTAttributeArg &arg){
    return std::string(arg.value.value,arg.value.len);
}

static const RTAttribute *findAttribute(const RTFuncTemplate *func,string_ref attrName){
    if(!func){
        return nullptr;
    }
    for(const auto &attr : func->attributes){
        string_ref currentName(attr.name.value, (uint32_t)attr.name.len);
        if(currentName == attrName){
            return &attr;
        }
    }
    return nullptr;
}

static const RTAttribute *findAttribute(const RTVar *var,string_ref attrName){
    if(!var){
        return nullptr;
    }
    for(const auto &attr : var->attributes){
        string_ref currentName(attr.name.value, (uint32_t)attr.name.len);
        if(currentName == attrName){
            return &attr;
        }
    }
    return nullptr;
}

static std::string resolveNativeCallbackName(const RTFuncTemplate *func){
    if(!func){
        return {};
    }
    auto defaultName = rtidToString(func->name);
    auto *nativeAttr = findAttribute(func,"native");
    if(!nativeAttr){
        return {};
    }
    if(nativeAttr->args.empty()){
        return defaultName;
    }
    for(const auto &arg : nativeAttr->args){
        if(arg.hasName && rtidToString(arg.name) != "name"){
            continue;
        }
        auto value = attributeArgValueToString(arg);
        if(!value.empty()){
            return value;
        }
    }
    return defaultName;
}

static std::string resolveNativeValueName(const RTVar *var){
    if(!var){
        return {};
    }
    auto defaultName = rtidToString(var->id);
    auto *nativeAttr = findAttribute(var,"native");
    if(!nativeAttr){
        return {};
    }
    if(nativeAttr->args.empty()){
        return defaultName;
    }
    for(const auto &arg : nativeAttr->args){
        if(arg.hasName && rtidToString(arg.name) != "name"){
            continue;
        }
        auto value = attributeArgValueToString(arg);
        if(!value.empty()){
            return value;
        }
    }
    return defaultName;
}

static std::string mangleClassMethodName(string_ref className,string_ref methodName){
    Twine out;
    out + "__";
    out + className.str();
    out + "__";
    out + methodName.str();
    return out.str();
}

static uint32_t regexCompileOptionsFromFlags(const std::string &flags){
    uint32_t options = 0;
    for(char flag : flags){
        switch(flag){
            case 'i':
                options |= PCRE2_CASELESS;
                break;
            case 'm':
                options |= PCRE2_MULTILINE;
                break;
            case 's':
                options |= PCRE2_DOTALL;
                break;
            case 'u':
                options |= PCRE2_UTF;
                break;
            default:
                break;
        }
    }
    return options;
}

static bool isIntegralNumType(StarbytesNumT numType){
    return numType == NumTypeInt || numType == NumTypeLong;
}

static bool isFloatingNumType(StarbytesNumT numType){
    return numType == NumTypeFloat || numType == NumTypeDouble;
}

static int numericTypeRank(StarbytesNumT numType){
    switch(numType){
        case NumTypeInt:
            return 0;
        case NumTypeLong:
            return 1;
        case NumTypeFloat:
            return 2;
        case NumTypeDouble:
        default:
            return 3;
    }
}

static StarbytesNumT promoteNumericType(StarbytesNumT lhs,StarbytesNumT rhs){
    return numericTypeRank(lhs) >= numericTypeRank(rhs) ? lhs : rhs;
}

static bool objectToNumber(StarbytesObject object,long double &value,StarbytesNumT &numType){
    if(!object || !StarbytesObjectTypecheck(object,StarbytesNumType())){
        return false;
    }
    numType = StarbytesNumGetType(object);
    if(numType == NumTypeFloat){
        value = StarbytesNumGetFloatValue(object);
    }
    else if(numType == NumTypeDouble){
        value = StarbytesNumGetDoubleValue(object);
    }
    else if(numType == NumTypeLong){
        value = (long double)StarbytesNumGetLongValue(object);
    }
    else {
        value = StarbytesNumGetIntValue(object);
    }
    return true;
}

static StarbytesObject makeNumber(long double value,StarbytesNumT numType){
    if(numType == NumTypeDouble){
        return StarbytesNumNew(NumTypeDouble,(double)value);
    }
    if(numType == NumTypeFloat){
        return StarbytesNumNew(NumTypeFloat,(float)value);
    }
    if(numType == NumTypeLong){
        return StarbytesNumNew(NumTypeLong,(int64_t)value);
    }
    return StarbytesNumNew(NumTypeInt,(int)value);
}

static RTTypedNumericKind typedKindFromNumType(StarbytesNumT numType){
    switch(numType){
        case NumTypeInt:
            return RTTYPED_NUM_INT;
        case NumTypeLong:
            return RTTYPED_NUM_LONG;
        case NumTypeFloat:
            return RTTYPED_NUM_FLOAT;
        case NumTypeDouble:
            return RTTYPED_NUM_DOUBLE;
        default:
            return RTTYPED_NUM_OBJECT;
    }
}

static StarbytesNumT numTypeFromTypedKind(RTTypedNumericKind kind){
    switch(kind){
        case RTTYPED_NUM_INT:
            return NumTypeInt;
        case RTTYPED_NUM_LONG:
            return NumTypeLong;
        case RTTYPED_NUM_FLOAT:
            return NumTypeFloat;
        case RTTYPED_NUM_DOUBLE:
            return NumTypeDouble;
        default:
            return NumTypeInt;
    }
}

static bool extractTypedNumericValue(StarbytesObject object,RTTypedNumericKind kind,long double &valueOut){
    StarbytesNumT actualType = NumTypeInt;
    long double rawValue = 0.0;
    if(!objectToNumber(object,rawValue,actualType)){
        return false;
    }
    switch(kind){
        case RTTYPED_NUM_INT:
            valueOut = (long double)((int)rawValue);
            return true;
        case RTTYPED_NUM_LONG:
            valueOut = (long double)((int64_t)rawValue);
            return true;
        case RTTYPED_NUM_FLOAT:
            valueOut = (long double)((float)rawValue);
            return true;
        case RTTYPED_NUM_DOUBLE:
            valueOut = (long double)((double)rawValue);
            return true;
        default:
            break;
    }
    return false;
}

static constexpr unsigned kQuickeningInvocationThreshold = 4;

static std::string objectToString(StarbytesObject object){
    if(!object){
        return "null";
    }
    if(StarbytesObjectTypecheck(object,StarbytesStrType())){
        return StarbytesStrGetBuffer(object);
    }
    if(StarbytesObjectTypecheck(object,StarbytesBoolType())){
        return ((bool)StarbytesBoolValue(object))? "true" : "false";
    }
    if(StarbytesObjectTypecheck(object,StarbytesNumType())){
        auto numType = StarbytesNumGetType(object);
        if(numType == NumTypeFloat){
            std::ostringstream out;
            out << StarbytesNumGetFloatValue(object);
            return out.str();
        }
        if(numType == NumTypeDouble){
            std::ostringstream out;
            out << StarbytesNumGetDoubleValue(object);
            return out.str();
        }
        if(numType == NumTypeLong){
            return std::to_string(StarbytesNumGetLongValue(object));
        }
        return std::to_string(StarbytesNumGetIntValue(object));
    }
    if(StarbytesObjectTypecheck(object,StarbytesRegexType())){
        auto pattern = StarbytesObjectGetProperty(object,"pattern");
        auto flags = StarbytesObjectGetProperty(object,"flags");
        std::string p = pattern? std::string(StarbytesStrGetBuffer(pattern)) : "";
        std::string f = flags? std::string(StarbytesStrGetBuffer(flags)) : "";
        return "/" + p + "/" + f;
    }
    return "<object>";
}

static bool runtimeObjectEquals(StarbytesObject lhs,StarbytesObject rhs){
    if(!lhs || !rhs){
        return lhs == rhs;
    }
    if(StarbytesObjectTypecheck(lhs,StarbytesNumType()) && StarbytesObjectTypecheck(rhs,StarbytesNumType())){
        return StarbytesNumCompare(lhs,rhs) == COMPARE_EQUAL;
    }
    if(StarbytesObjectTypecheck(lhs,StarbytesStrType()) && StarbytesObjectTypecheck(rhs,StarbytesStrType())){
        return StarbytesStrCompare(lhs,rhs) == COMPARE_EQUAL;
    }
    if(StarbytesObjectTypecheck(lhs,StarbytesBoolType()) && StarbytesObjectTypecheck(rhs,StarbytesBoolType())){
        return (bool)StarbytesBoolValue(lhs) == (bool)StarbytesBoolValue(rhs);
    }
    return lhs == rhs;
}

static bool isDictKeyObject(StarbytesObject key){
    return key && (StarbytesObjectTypecheck(key,StarbytesStrType()) || StarbytesObjectTypecheck(key,StarbytesNumType()));
}

static int clampSliceBound(int bound,int len){
    if(bound < 0){
        bound = len + bound;
    }
    if(bound < 0){
        return 0;
    }
    if(bound > len){
        return len;
    }
    return bound;
}

static std::string stringTrim(const std::string &input){
    size_t begin = 0;
    while(begin < input.size() && std::isspace((unsigned char)input[begin])){
        ++begin;
    }
    size_t end = input.size();
    while(end > begin && std::isspace((unsigned char)input[end - 1])){
        --end;
    }
    return input.substr(begin,end - begin);
}

static bool parseIntStrict(const std::string &text,int &outValue){
    auto trimmed = stringTrim(text);
    if(trimmed.empty()){
        return false;
    }
    errno = 0;
    char *endPtr = nullptr;
    long parsed = std::strtol(trimmed.c_str(),&endPtr,10);
    if(errno != 0 || endPtr == nullptr || *endPtr != '\0'){
        return false;
    }
    if(parsed < std::numeric_limits<int>::min() || parsed > std::numeric_limits<int>::max()){
        return false;
    }
    outValue = (int)parsed;
    return true;
}

static bool parseFloatStrict(const std::string &text,float &outValue){
    auto trimmed = stringTrim(text);
    if(trimmed.empty()){
        return false;
    }
    errno = 0;
    char *endPtr = nullptr;
    float parsed = std::strtof(trimmed.c_str(),&endPtr);
    if(errno != 0 || endPtr == nullptr || *endPtr != '\0'){
        return false;
    }
    outValue = parsed;
    return true;
}

static bool parseDoubleStrict(const std::string &text,double &outValue){
    auto trimmed = stringTrim(text);
    if(trimmed.empty()){
        return false;
    }
    errno = 0;
    char *endPtr = nullptr;
    double parsed = std::strtod(trimmed.c_str(),&endPtr);
    if(errno != 0 || endPtr == nullptr || *endPtr != '\0'){
        return false;
    }
    outValue = parsed;
    return true;
}



class RTAllocator {
    
   string_map<string_map<StarbytesObject>> all_var_objects;
    
public:
    std::string currentScope;

    RTAllocator():all_var_objects(),currentScope(RTSCOPE_GLOBAL->name){
        
    };

    string_map<StarbytesObject> & getObjectRegistryAtScope(string_ref name){
        return all_var_objects[name];
    };

    void setScope(string_ref scope_name){
        currentScope.assign(scope_name.getBuffer(),scope_name.size());
    };
    void setScope(const std::string &scope_name){
        currentScope = scope_name;
    };
    void allocVariable(string_ref name,StarbytesObject obj){
        auto found = all_var_objects.find(currentScope);
        if(found == all_var_objects.end()){
            string_map<StarbytesObject> vars;
            vars.insert(std::make_pair(name,obj));
            all_var_objects.insert(std::make_pair(currentScope,std::move(vars)));
        }
        else {
            auto & var_map = found->second;
            auto existing = var_map.find(std::string(name));
            if(existing != var_map.end()){
                StarbytesObjectRelease(existing->second);
                existing->second = obj;
            }
            else {
                var_map.insert(std::make_pair(name,obj));
            }
        };
    };
    void allocVariable(string_ref name,StarbytesObject obj,string_ref scope){
        auto found = all_var_objects.find(scope);
        if(found == all_var_objects.end()){
            string_map<StarbytesObject> vars;
            vars.insert(std::make_pair(name,obj));
            all_var_objects.insert(std::make_pair(scope,std::move(vars)));
        }
        else {
            auto & var_map = found->second;
            auto existing = var_map.find(std::string(name));
            if(existing != var_map.end()){
                StarbytesObjectRelease(existing->second);
                existing->second = obj;
            }
            else {
                var_map.insert(std::make_pair(name,obj));
            }
        };
    };

    StarbytesObject referenceVariable(string_ref scope,string_ref name){
        auto found = all_var_objects.find(scope);
        if(found == all_var_objects.end()){
//             std::cout << "Var NOT Found AND Scope NOT Found:" << name.data() << std::endl;
            return nullptr;
        }
        else {
            auto & var_map = found->second;
            for(auto & var : var_map){
                if(var.first.size() == name.size() &&
                   std::memcmp(var.first.data(),name.getBuffer(),name.size()) == 0){
                    /// Increase Reference count upon variable reference.
                    if(!var.second){
                        return nullptr;
                    }
                    StarbytesObjectReference(var.second);
//                     std::cout << "Found Var:" << name.data() << std::endl;
                    return var.second;
                    break;
                };
            };
//             std::cout << "Var NOT FOUND:" << name.data() << std::endl;
            return nullptr;
        }
    };
    void clearScope(){
        auto found = all_var_objects.find(currentScope);
        if(found != all_var_objects.end()){
            auto & map = found->second;
            for(auto & ent : map){
                StarbytesObjectRelease(ent.second);
            };
            all_var_objects.erase(found);
        };
    };
};

StarbytesNativeModule * starbytes_native_mod_load(string_ref path);
StarbytesFuncCallback starbytes_native_mod_load_function(StarbytesNativeModule * mod,string_ref name);
StarbytesFuncCallback starbytes_native_mod_load_value(StarbytesNativeModule * mod,string_ref name);
void starbytes_native_mod_close(StarbytesNativeModule * mod);

struct ScheduledTaskCall {
    StarbytesTask task = nullptr;
    std::string funcName;
    std::vector<StarbytesObject> args;
    StarbytesObject boundSelf = nullptr;
};

class InterpImpl final : public Interp {
    struct LocalSlot {
        RTTypedNumericKind kind = RTTYPED_NUM_OBJECT;
        bool hasNumericValue = false;
        StarbytesObject object = nullptr;
        int intValue = 0;
        int64_t longValue = 0;
        float floatValue = 0.0f;
        double doubleValue = 0.0;
    };

    struct LocalFrame {
        const RTFuncTemplate *funcTemplate = nullptr;
        std::vector<LocalSlot> slots;
    };

    std::unique_ptr<RTAllocator> allocator;
    std::vector<LocalFrame> localFrames;
    
    std::vector<RTClass> classes;
    
    std::vector<RTFuncTemplate> functions;
    std::vector<StarbytesNativeModule *> nativeModules;
    string_map<StarbytesFuncCallback> nativeCallbackCache;
    string_map<StarbytesFuncCallback> nativeValueCallbackCache;
    std::deque<ScheduledTaskCall> microtaskQueue;
    bool isDrainingMicrotasks = false;
    std::istream *activeInput = nullptr;

    map<StarbytesClassType,std::string> runtimeClassRegistry;
    string_map<StarbytesClassType> classTypeByName;
    map<StarbytesClassType,size_t> classIndexByType;
    std::string lastRuntimeError;
    RuntimeProfileData runtimeProfile;

    void execNorm(RTCode &code,std::istream &in,bool * willReturn,StarbytesObject *return_val);
    
    StarbytesObject invokeFunc(std::istream &in,RTFuncTemplate *func_temp,unsigned argCount,StarbytesObject boundSelf = nullptr);
    RTClass *findClassByName(string_ref className);
    RTClass *findClassByType(StarbytesClassType classType);
    bool buildClassHierarchy(RTClass *classMeta,std::vector<RTClass *> &hierarchy);
    RTClass *findMethodOwnerInHierarchy(RTClass *classMeta,const std::string &methodName);
    RTFuncTemplate *findFunctionByName(string_ref functionName);
    LocalFrame *currentLocalFrame();
    const LocalFrame *currentLocalFrame() const;
    void pushLocalFrame(const RTFuncTemplate *funcTemp);
    void popLocalFrame();
    StarbytesObject referenceLocalSlot(uint32_t slot);
    bool localSlotToNumber(uint32_t slot,RTTypedNumericKind kind,long double &valueOut);
    bool localSlotToIndex(uint32_t slot,int &indexOut);
    bool observedLocalSlotNumericType(uint32_t slot,StarbytesNumT &typeOut) const;
    void storeLocalSlotOwned(uint32_t slot,StarbytesObject value);
    void storeLocalSlotBorrowed(uint32_t slot,StarbytesObject value);
    bool inspectLocalRefExpr(std::istream &in,uint32_t &slotOut,RTTypedNumericKind &kindOut);
    bool inspectQuickeningCandidate(std::istream &in,std::istream::pos_type exprStart,RTCode code,RTQuickenedExpr &out);
    RTQuickenedExpr *getOrInstallQuickenedExpr(std::istream &in,std::istream::pos_type exprStart,RTCode code);
    bool tryExecuteQuickenedExpr(std::istream &in,std::istream::pos_type exprStart,RTQuickenedExpr &expr,StarbytesObject &result);
    void discardExprArgs(std::istream &in,unsigned argCount);
    void skipExpr(std::istream &in);
    void skipExprFromCode(std::istream &in,RTCode code);
    void skipRuntimeStmt(std::istream &in,RTCode code);
    void skipRuntimeBlock(std::istream &in,RTCode endCode);
    void executeRuntimeBlock(std::istream &in,RTCode endCode,bool *willReturn,StarbytesObject *return_val);
    StarbytesFuncCallback findNativeCallback(string_ref callbackName);
    StarbytesFuncCallback findNativeValueCallback(string_ref callbackName);
    StarbytesObject invokeNativeFunc(std::istream &in,StarbytesFuncCallback callback,unsigned argCount,StarbytesObject boundSelf);
    StarbytesObject invokeNativeFuncWithValues(StarbytesFuncCallback callback,ArrayRef<StarbytesObject> args,StarbytesObject boundSelf);
    StarbytesObject invokeFuncWithValues(RTFuncTemplate *func_temp,ArrayRef<StarbytesObject> args,StarbytesObject boundSelf = nullptr);
    StarbytesTask scheduleLazyCall(std::istream &in,RTFuncTemplate *func_temp,unsigned argCount,StarbytesObject boundSelf = nullptr);
    void processOneMicrotask();
    void processMicrotasks();
    
    StarbytesObject evalExpr(std::istream &in);
    
public:
    InterpImpl():allocator(std::make_unique<RTAllocator>()){
        auto addBuiltinTemplate = [&](const char *name,std::initializer_list<const char *> params){
            RTFuncTemplate funcTemplate;
            funcTemplate.name = {strlen(name),name};
            for(auto *param : params){
                funcTemplate.argsTemplate.push_back({strlen(param),param});
            }
            functions.push_back(std::move(funcTemplate));
        };
        addBuiltinTemplate("print",{"object"});
        stdlib::addMathBuiltinTemplates(functions);
    };
    ~InterpImpl() override;
    void exec(std::istream &in) override;
    bool addExtension(const std::string &path) override;
    bool hasRuntimeError() const override {
        return !lastRuntimeError.empty();
    }
    std::string takeRuntimeError() override {
        auto out = lastRuntimeError;
        lastRuntimeError.clear();
        return out;
    }
    RuntimeProfileData getProfileData() const override {
        return runtimeProfile;
    }
};

RTClass *InterpImpl::findClassByName(string_ref className){
    auto foundType = classTypeByName.find(className.view());
    if(foundType == classTypeByName.end()){
        return nullptr;
    }
    auto foundClass = classIndexByType.find(foundType->second);
    if(foundClass == classIndexByType.end()){
        return nullptr;
    }
    return &classes[foundClass->second];
}

RTClass *InterpImpl::findClassByType(StarbytesClassType classType){
    auto foundClass = classIndexByType.find(classType);
    if(foundClass == classIndexByType.end()){
        return nullptr;
    }
    return &classes[foundClass->second];
}

bool InterpImpl::buildClassHierarchy(RTClass *classMeta,std::vector<RTClass *> &hierarchy){
    hierarchy.clear();
    if(!classMeta){
        return false;
    }
    string_set visited;
    std::vector<RTClass *> reverseChain;
    for(auto *cursor = classMeta; cursor != nullptr;){
        string_ref cursorName(cursor->name.value, (uint32_t)cursor->name.len);
        if(visited.find(cursorName.view()) != visited.end()){
            return false;
        }
        visited.insert(cursorName.str());
        reverseChain.push_back(cursor);
        if(!cursor->hasSuperClass){
            break;
        }
        cursor = findClassByName(string_ref(cursor->superClassName.value, (uint32_t)cursor->superClassName.len));
    }
    hierarchy.assign(reverseChain.rbegin(),reverseChain.rend());
    return true;
}

RTClass *InterpImpl::findMethodOwnerInHierarchy(RTClass *classMeta,const std::string &methodName){
    string_set visited;
    string_ref methodNameRef(methodName);
    for(auto *cursor = classMeta; cursor != nullptr;){
        string_ref cursorName(cursor->name.value, (uint32_t)cursor->name.len);
        if(visited.find(cursorName.view()) != visited.end()){
            return nullptr;
        }
        visited.insert(cursorName.str());
        for(auto &method : cursor->methods){
            string_ref currentMethod(method.name.value, (uint32_t)method.name.len);
            if(currentMethod == methodNameRef){
                return cursor;
            }
        }
        if(!cursor->hasSuperClass){
            break;
        }
        cursor = findClassByName(string_ref(cursor->superClassName.value, (uint32_t)cursor->superClassName.len));
    }
    return nullptr;
}

RTFuncTemplate *InterpImpl::findFunctionByName(string_ref functionName){
    for(auto &funcTemplate : functions){
        string_ref tempName(funcTemplate.name.value,(uint32_t)funcTemplate.name.len);
        if(functionName == tempName){
            return &funcTemplate;
        }
    }
    return nullptr;
}

InterpImpl::LocalFrame *InterpImpl::currentLocalFrame(){
    if(localFrames.empty()){
        return nullptr;
    }
    return &localFrames.back();
}

const InterpImpl::LocalFrame *InterpImpl::currentLocalFrame() const{
    if(localFrames.empty()){
        return nullptr;
    }
    return &localFrames.back();
}

void InterpImpl::pushLocalFrame(const RTFuncTemplate *funcTemp){
    LocalFrame frame;
    frame.funcTemplate = funcTemp;
    size_t slotCount = 0;
    if(funcTemp){
        slotCount = funcTemp->argsTemplate.size() + funcTemp->localSlotNames.size();
    }
    frame.slots.resize(slotCount);
    if(funcTemp){
        for(size_t i = 0;i < slotCount;++i){
            frame.slots[i].kind = (i < funcTemp->slotKinds.size())? funcTemp->slotKinds[i] : RTTYPED_NUM_OBJECT;
        }
    }
    localFrames.push_back(std::move(frame));
}

void InterpImpl::popLocalFrame(){
    if(localFrames.empty()){
        return;
    }
    auto frame = std::move(localFrames.back());
    localFrames.pop_back();
    for(auto &slot : frame.slots){
        if(slot.object){
            StarbytesObjectRelease(slot.object);
        }
    }
}

StarbytesObject InterpImpl::referenceLocalSlot(uint32_t slot){
    auto *frame = currentLocalFrame();
    if(!frame || slot >= frame->slots.size()){
        return nullptr;
    }
    auto &slotValue = frame->slots[slot];
    if(slotValue.kind == RTTYPED_NUM_OBJECT){
        if(!slotValue.object){
            return nullptr;
        }
        StarbytesObjectReference(slotValue.object);
        return slotValue.object;
    }
    if(slotValue.object){
        StarbytesObjectReference(slotValue.object);
        return slotValue.object;
    }
    if(!slotValue.hasNumericValue){
        return nullptr;
    }
    switch(slotValue.kind){
        case RTTYPED_NUM_INT:
            return StarbytesNumNew(NumTypeInt,slotValue.intValue);
        case RTTYPED_NUM_LONG:
            return StarbytesNumNew(NumTypeLong,slotValue.longValue);
        case RTTYPED_NUM_FLOAT:
            return StarbytesNumNew(NumTypeFloat,slotValue.floatValue);
        case RTTYPED_NUM_DOUBLE:
            return StarbytesNumNew(NumTypeDouble,slotValue.doubleValue);
        default:
            return nullptr;
    }
}

bool InterpImpl::localSlotToNumber(uint32_t slot,RTTypedNumericKind kind,long double &valueOut){
    auto *frame = currentLocalFrame();
    if(!frame || slot >= frame->slots.size()){
        return false;
    }
    auto &slotValue = frame->slots[slot];
    if(slotValue.object){
        return extractTypedNumericValue(slotValue.object,kind,valueOut);
    }
    if(!slotValue.hasNumericValue){
        return false;
    }
    switch(kind){
        case RTTYPED_NUM_INT:
            if(slotValue.kind == RTTYPED_NUM_DOUBLE){
                valueOut = (long double)((int)slotValue.doubleValue);
            }
            else if(slotValue.kind == RTTYPED_NUM_FLOAT){
                valueOut = (long double)((int)slotValue.floatValue);
            }
            else if(slotValue.kind == RTTYPED_NUM_LONG){
                valueOut = (long double)((int)slotValue.longValue);
            }
            else {
                valueOut = (long double)slotValue.intValue;
            }
            return true;
        case RTTYPED_NUM_LONG:
            if(slotValue.kind == RTTYPED_NUM_DOUBLE){
                valueOut = (long double)((int64_t)slotValue.doubleValue);
            }
            else if(slotValue.kind == RTTYPED_NUM_FLOAT){
                valueOut = (long double)((int64_t)slotValue.floatValue);
            }
            else if(slotValue.kind == RTTYPED_NUM_LONG){
                valueOut = (long double)slotValue.longValue;
            }
            else {
                valueOut = (long double)((int64_t)slotValue.intValue);
            }
            return true;
        case RTTYPED_NUM_FLOAT:
            if(slotValue.kind == RTTYPED_NUM_DOUBLE){
                valueOut = (long double)((float)slotValue.doubleValue);
            }
            else if(slotValue.kind == RTTYPED_NUM_LONG){
                valueOut = (long double)((float)slotValue.longValue);
            }
            else {
                valueOut = (long double)((slotValue.kind == RTTYPED_NUM_FLOAT)? slotValue.floatValue : (float)slotValue.intValue);
            }
            return true;
        case RTTYPED_NUM_DOUBLE:
            if(slotValue.kind == RTTYPED_NUM_DOUBLE){
                valueOut = slotValue.doubleValue;
            }
            else if(slotValue.kind == RTTYPED_NUM_FLOAT){
                valueOut = slotValue.floatValue;
            }
            else if(slotValue.kind == RTTYPED_NUM_LONG){
                valueOut = (long double)slotValue.longValue;
            }
            else {
                valueOut = (long double)slotValue.intValue;
            }
            return true;
        default:
            break;
    }
    return false;
}

bool InterpImpl::localSlotToIndex(uint32_t slot,int &indexOut){
    indexOut = -1;
    auto *frame = currentLocalFrame();
    if(!frame || slot >= frame->slots.size()){
        return false;
    }
    auto &slotValue = frame->slots[slot];
    if(slotValue.object){
        if(!StarbytesObjectTypecheck(slotValue.object,StarbytesNumType())){
            return false;
        }
        auto indexType = StarbytesNumGetType(slotValue.object);
        if(indexType == NumTypeInt){
            indexOut = StarbytesNumGetIntValue(slotValue.object);
            return true;
        }
        if(indexType == NumTypeLong){
            indexOut = (int)StarbytesNumGetLongValue(slotValue.object);
            return true;
        }
        return false;
    }
    if(!slotValue.hasNumericValue){
        return false;
    }
    if(slotValue.kind == RTTYPED_NUM_INT){
        indexOut = slotValue.intValue;
        return true;
    }
    if(slotValue.kind == RTTYPED_NUM_LONG){
        indexOut = (int)slotValue.longValue;
        return true;
    }
    return false;
}

bool InterpImpl::observedLocalSlotNumericType(uint32_t slot,StarbytesNumT &typeOut) const{
    auto *frame = currentLocalFrame();
    if(!frame || slot >= frame->slots.size()){
        return false;
    }
    const auto &slotValue = frame->slots[slot];
    if(slotValue.object){
        if(!StarbytesObjectTypecheck(slotValue.object,StarbytesNumType())){
            return false;
        }
        typeOut = StarbytesNumGetType(slotValue.object);
        return true;
    }
    if(!slotValue.hasNumericValue){
        return false;
    }
    typeOut = numTypeFromTypedKind(slotValue.kind);
    return typeOut == NumTypeInt || typeOut == NumTypeLong || typeOut == NumTypeFloat || typeOut == NumTypeDouble;
}

void InterpImpl::storeLocalSlotOwned(uint32_t slot,StarbytesObject value){
    auto *frame = currentLocalFrame();
    if(!frame || slot >= frame->slots.size()){
        if(value){
            StarbytesObjectRelease(value);
        }
        return;
    }
    auto &target = frame->slots[slot];
    if(target.object){
        StarbytesObjectRelease(target.object);
        target.object = nullptr;
    }
    target.hasNumericValue = false;
    if(!value){
        return;
    }
    if(target.kind != RTTYPED_NUM_OBJECT){
        long double numericValue = 0.0;
        if(extractTypedNumericValue(value,target.kind,numericValue)){
            target.hasNumericValue = true;
            if(target.kind == RTTYPED_NUM_INT){
                target.intValue = (int)numericValue;
            }
            else if(target.kind == RTTYPED_NUM_LONG){
                target.longValue = (int64_t)numericValue;
            }
            else if(target.kind == RTTYPED_NUM_FLOAT){
                target.floatValue = (float)numericValue;
            }
            else if(target.kind == RTTYPED_NUM_DOUBLE){
                target.doubleValue = (double)numericValue;
            }
            StarbytesObjectRelease(value);
            return;
        }
    }
    target.object = value;
}

void InterpImpl::storeLocalSlotBorrowed(uint32_t slot,StarbytesObject value){
    auto *frame = currentLocalFrame();
    if(!frame || slot >= frame->slots.size()){
        return;
    }
    auto &target = frame->slots[slot];
    if(target.object){
        StarbytesObjectRelease(target.object);
        target.object = nullptr;
    }
    target.hasNumericValue = false;
    if(!value){
        return;
    }
    if(target.kind != RTTYPED_NUM_OBJECT){
        long double numericValue = 0.0;
        if(extractTypedNumericValue(value,target.kind,numericValue)){
            target.hasNumericValue = true;
            if(target.kind == RTTYPED_NUM_INT){
                target.intValue = (int)numericValue;
            }
            else if(target.kind == RTTYPED_NUM_LONG){
                target.longValue = (int64_t)numericValue;
            }
            else if(target.kind == RTTYPED_NUM_FLOAT){
                target.floatValue = (float)numericValue;
            }
            else if(target.kind == RTTYPED_NUM_DOUBLE){
                target.doubleValue = (double)numericValue;
            }
            return;
        }
    }
    StarbytesObjectReference(value);
    target.object = value;
}

bool InterpImpl::inspectLocalRefExpr(std::istream &in,uint32_t &slotOut,RTTypedNumericKind &kindOut){
    RTCode exprCode = CODE_MODULE_END;
    if(!in.read((char *)&exprCode,sizeof(exprCode))){
        return false;
    }
    if(exprCode == CODE_RTLOCAL_REF){
        kindOut = RTTYPED_NUM_OBJECT;
        in.read((char *)&slotOut,sizeof(slotOut));
        return true;
    }
    if(exprCode == CODE_RTTYPED_LOCAL_REF){
        in.read((char *)&kindOut,sizeof(kindOut));
        in.read((char *)&slotOut,sizeof(slotOut));
        return true;
    }
    return false;
}

bool InterpImpl::inspectQuickeningCandidate(std::istream &in,
                                            std::istream::pos_type exprStart,
                                            RTCode code,
                                            RTQuickenedExpr &out){
    auto payloadStart = in.tellg();
    auto restore = [&]() {
        in.clear();
        in.seekg(payloadStart);
    };

    auto finalize = [&]() -> bool {
        auto endPos = in.tellg();
        out.byteSize = (endPos == std::istream::pos_type(-1) || exprStart == std::istream::pos_type(-1))
            ? 0
            : static_cast<std::streamoff>(endPos - exprStart);
        restore();
        return out.byteSize > 0;
    };

    auto loadLocalLocalOperands = [&](uint32_t &slotA,RTTypedNumericKind &kindA,
                                      uint32_t &slotB,RTTypedNumericKind &kindB) -> bool {
        return inspectLocalRefExpr(in,slotA,kindA) && inspectLocalRefExpr(in,slotB,kindB);
    };

    if(code == CODE_BINARY_OPERATOR){
        RTCode binaryCode = BINARY_OP_PLUS;
        in.read((char *)&binaryCode,sizeof(binaryCode));
        uint32_t slotA = 0;
        uint32_t slotB = 0;
        RTTypedNumericKind kindA = RTTYPED_NUM_OBJECT;
        RTTypedNumericKind kindB = RTTYPED_NUM_OBJECT;
        if(!loadLocalLocalOperands(slotA,kindA,slotB,kindB)){
            restore();
            return false;
        }
        if(binaryCode == BINARY_OP_PLUS || binaryCode == BINARY_OP_MINUS
           || binaryCode == BINARY_OP_MUL || binaryCode == BINARY_OP_DIV
           || binaryCode == BINARY_OP_MOD){
            out.kind = RTQUICKEN_EXPR_LOCAL_LOCAL_BINARY;
            out.op = static_cast<uint8_t>(binaryCode);
        }
        else if(binaryCode == BINARY_EQUAL_EQUAL || binaryCode == BINARY_NOT_EQUAL
                || binaryCode == BINARY_LESS || binaryCode == BINARY_LESS_EQUAL
                || binaryCode == BINARY_GREATER || binaryCode == BINARY_GREATER_EQUAL){
            out.kind = RTQUICKEN_EXPR_LOCAL_LOCAL_COMPARE;
            switch(binaryCode){
                case BINARY_EQUAL_EQUAL:
                    out.op = RTTYPED_COMPARE_EQ;
                    break;
                case BINARY_NOT_EQUAL:
                    out.op = RTTYPED_COMPARE_NE;
                    break;
                case BINARY_LESS:
                    out.op = RTTYPED_COMPARE_LT;
                    break;
                case BINARY_LESS_EQUAL:
                    out.op = RTTYPED_COMPARE_LE;
                    break;
                case BINARY_GREATER:
                    out.op = RTTYPED_COMPARE_GT;
                    break;
                case BINARY_GREATER_EQUAL:
                    out.op = RTTYPED_COMPARE_GE;
                    break;
                default:
                    restore();
                    return false;
            }
        }
        else {
            restore();
            return false;
        }
        out.numericKind = RTTYPED_NUM_OBJECT;
        out.slotA = slotA;
        out.slotB = slotB;
        return finalize();
    }

    if(code == CODE_RTTYPED_BINARY){
        RTTypedNumericKind kind = RTTYPED_NUM_OBJECT;
        RTTypedBinaryOp op = RTTYPED_BINARY_ADD;
        in.read((char *)&kind,sizeof(kind));
        in.read((char *)&op,sizeof(op));
        uint32_t slotA = 0;
        uint32_t slotB = 0;
        RTTypedNumericKind kindA = RTTYPED_NUM_OBJECT;
        RTTypedNumericKind kindB = RTTYPED_NUM_OBJECT;
        if(!loadLocalLocalOperands(slotA,kindA,slotB,kindB)){
            restore();
            return false;
        }
        out.kind = RTQUICKEN_EXPR_LOCAL_LOCAL_BINARY;
        out.numericKind = kind;
        out.op = op;
        out.slotA = slotA;
        out.slotB = slotB;
        return finalize();
    }

    if(code == CODE_RTTYPED_COMPARE){
        RTTypedNumericKind kind = RTTYPED_NUM_OBJECT;
        RTTypedCompareOp op = RTTYPED_COMPARE_EQ;
        in.read((char *)&kind,sizeof(kind));
        in.read((char *)&op,sizeof(op));
        uint32_t slotA = 0;
        uint32_t slotB = 0;
        RTTypedNumericKind kindA = RTTYPED_NUM_OBJECT;
        RTTypedNumericKind kindB = RTTYPED_NUM_OBJECT;
        if(!loadLocalLocalOperands(slotA,kindA,slotB,kindB)){
            restore();
            return false;
        }
        out.kind = RTQUICKEN_EXPR_LOCAL_LOCAL_COMPARE;
        out.numericKind = kind;
        out.op = op;
        out.slotA = slotA;
        out.slotB = slotB;
        return finalize();
    }

    if(code == CODE_RTTYPED_INTRINSIC){
        RTTypedNumericKind kind = RTTYPED_NUM_OBJECT;
        RTTypedIntrinsicOp op = RTTYPED_INTRINSIC_SQRT;
        in.read((char *)&kind,sizeof(kind));
        in.read((char *)&op,sizeof(op));
        uint32_t slot = 0;
        RTTypedNumericKind operandKind = RTTYPED_NUM_OBJECT;
        if(op != RTTYPED_INTRINSIC_SQRT || !inspectLocalRefExpr(in,slot,operandKind)){
            restore();
            return false;
        }
        out.kind = RTQUICKEN_EXPR_LOCAL_INTRINSIC;
        out.numericKind = kind;
        out.op = op;
        out.slotA = slot;
        return finalize();
    }

    if(code == CODE_RTIVKFUNC){
        RTCode calleeCode = CODE_MODULE_END;
        if(!in.read((char *)&calleeCode,sizeof(calleeCode))){
            restore();
            return false;
        }
        std::string calleeName;
        if(calleeCode == CODE_RTFUNC_REF || calleeCode == CODE_RTVAR_REF){
            RTID id;
            in >> &id;
            calleeName = rtidToString(id);
        }
        else {
            restore();
            return false;
        }
        unsigned argCount = 0;
        in.read((char *)&argCount,sizeof(argCount));
        if(calleeName != "sqrt" || argCount != 1){
            restore();
            return false;
        }
        uint32_t slot = 0;
        RTTypedNumericKind operandKind = RTTYPED_NUM_OBJECT;
        if(!inspectLocalRefExpr(in,slot,operandKind)){
            restore();
            return false;
        }
        out.kind = RTQUICKEN_EXPR_LOCAL_INTRINSIC;
        out.numericKind = RTTYPED_NUM_OBJECT;
        out.op = RTTYPED_INTRINSIC_SQRT;
        out.slotA = slot;
        return finalize();
    }

    if(code == CODE_RTTYPED_INDEX_GET){
        RTTypedNumericKind kind = RTTYPED_NUM_OBJECT;
        in.read((char *)&kind,sizeof(kind));
        uint32_t collectionSlot = 0;
        uint32_t indexSlot = 0;
        RTTypedNumericKind collectionKind = RTTYPED_NUM_OBJECT;
        RTTypedNumericKind indexKind = RTTYPED_NUM_OBJECT;
        if(!loadLocalLocalOperands(collectionSlot,collectionKind,indexSlot,indexKind)){
            restore();
            return false;
        }
        out.kind = RTQUICKEN_EXPR_LOCAL_LOCAL_INDEX_GET;
        out.numericKind = kind;
        out.slotA = collectionSlot;
        out.slotB = indexSlot;
        return finalize();
    }

    if(code == CODE_RTTYPED_INDEX_SET){
        RTTypedNumericKind kind = RTTYPED_NUM_OBJECT;
        in.read((char *)&kind,sizeof(kind));
        uint32_t collectionSlot = 0;
        uint32_t indexSlot = 0;
        uint32_t valueSlot = 0;
        RTTypedNumericKind collectionKind = RTTYPED_NUM_OBJECT;
        RTTypedNumericKind indexKind = RTTYPED_NUM_OBJECT;
        RTTypedNumericKind valueKind = RTTYPED_NUM_OBJECT;
        if(!inspectLocalRefExpr(in,collectionSlot,collectionKind)
           || !inspectLocalRefExpr(in,indexSlot,indexKind)
           || !inspectLocalRefExpr(in,valueSlot,valueKind)){
            restore();
            return false;
        }
        out.kind = RTQUICKEN_EXPR_LOCAL_LOCAL_LOCAL_INDEX_SET;
        out.numericKind = kind;
        out.slotA = collectionSlot;
        out.slotB = indexSlot;
        out.slotC = valueSlot;
        return finalize();
    }

    restore();
    return false;
}

RTQuickenedExpr *InterpImpl::getOrInstallQuickenedExpr(std::istream &in,
                                                       std::istream::pos_type exprStart,
                                                       RTCode code){
    auto *frame = currentLocalFrame();
    if(!frame || !frame->funcTemplate || frame->funcTemplate->invocations < kQuickeningInvocationThreshold){
        return nullptr;
    }
    auto *funcTemplate = const_cast<RTFuncTemplate *>(frame->funcTemplate);
    if(funcTemplate->block_start_pos == std::istream::pos_type(-1)
       || exprStart == std::istream::pos_type(-1)
       || exprStart < funcTemplate->block_start_pos){
        return nullptr;
    }
    auto offset = static_cast<std::streamoff>(exprStart - funcTemplate->block_start_pos);
    auto found = funcTemplate->quickenedExprs.find(offset);
    if(found != funcTemplate->quickenedExprs.end()){
        return &found->second;
    }
    RTQuickenedExpr candidate;
    if(!inspectQuickeningCandidate(in,exprStart,code,candidate)){
        return nullptr;
    }
    auto inserted = funcTemplate->quickenedExprs.insert(std::make_pair(offset,candidate));
    if(inserted.second){
        runtimeProfile.quickenedSitesInstalled += 1;
    }
    return &inserted.first->second;
}

bool InterpImpl::tryExecuteQuickenedExpr(std::istream &in,
                                         std::istream::pos_type exprStart,
                                         RTQuickenedExpr &expr,
                                         StarbytesObject &result){
    result = nullptr;

    auto finish = [&](StarbytesObject value) -> bool {
        auto endPos = exprStart + expr.byteSize;
        in.clear();
        in.seekg(endPos);
        result = value;
        expr.executions += 1;
        runtimeProfile.quickenedExecutions += 1;
        return true;
    };

    auto fail = [&]() -> bool {
        expr.fallbackCount += 1;
        runtimeProfile.quickenedFallbacks += 1;
        return false;
    };

    auto resolveKind = [&](uint32_t slotA,uint32_t slotB = 0,bool hasRhs = false) -> RTTypedNumericKind {
        if(expr.numericKind != RTTYPED_NUM_OBJECT){
            return expr.numericKind;
        }
        StarbytesNumT lhsType = NumTypeInt;
        if(!observedLocalSlotNumericType(slotA,lhsType)){
            return RTTYPED_NUM_OBJECT;
        }
        if(!hasRhs){
            expr.numericKind = typedKindFromNumType(lhsType);
            runtimeProfile.quickenedSpecializations += 1;
            return expr.numericKind;
        }
        StarbytesNumT rhsType = NumTypeInt;
        if(!observedLocalSlotNumericType(slotB,rhsType)){
            return RTTYPED_NUM_OBJECT;
        }
        expr.numericKind = typedKindFromNumType(promoteNumericType(lhsType,rhsType));
        runtimeProfile.quickenedSpecializations += 1;
        return expr.numericKind;
    };

    if(expr.kind == RTQUICKEN_EXPR_LOCAL_LOCAL_BINARY){
        auto kind = resolveKind(expr.slotA,expr.slotB,true);
        if(kind == RTTYPED_NUM_OBJECT){
            return fail();
        }
        long double lhsVal = 0.0;
        long double rhsVal = 0.0;
        if(!localSlotToNumber(expr.slotA,kind,lhsVal) || !localSlotToNumber(expr.slotB,kind,rhsVal)){
            return fail();
        }
        if((expr.op == RTTYPED_BINARY_DIV || expr.op == RTTYPED_BINARY_MOD) && rhsVal == 0.0){
            return finish(nullptr);
        }
        switch(expr.op){
            case RTTYPED_BINARY_ADD:
                return finish(makeNumber(lhsVal + rhsVal,numTypeFromTypedKind(kind)));
            case RTTYPED_BINARY_SUB:
                return finish(makeNumber(lhsVal - rhsVal,numTypeFromTypedKind(kind)));
            case RTTYPED_BINARY_MUL:
                return finish(makeNumber(lhsVal * rhsVal,numTypeFromTypedKind(kind)));
            case RTTYPED_BINARY_DIV:
                if(kind == RTTYPED_NUM_INT){
                    return finish(makeNumber((long double)((int)lhsVal / (int)rhsVal),NumTypeInt));
                }
                if(kind == RTTYPED_NUM_LONG){
                    return finish(makeNumber((long double)((int64_t)lhsVal / (int64_t)rhsVal),NumTypeLong));
                }
                return finish(makeNumber(lhsVal / rhsVal,numTypeFromTypedKind(kind)));
            case RTTYPED_BINARY_MOD:
                if(kind == RTTYPED_NUM_INT){
                    return finish(makeNumber((long double)((int)lhsVal % (int)rhsVal),NumTypeInt));
                }
                if(kind == RTTYPED_NUM_LONG){
                    return finish(makeNumber((long double)((int64_t)lhsVal % (int64_t)rhsVal),NumTypeLong));
                }
                return finish(makeNumber(std::fmod((double)lhsVal,(double)rhsVal),numTypeFromTypedKind(kind)));
            default:
                return fail();
        }
    }

    if(expr.kind == RTQUICKEN_EXPR_LOCAL_LOCAL_COMPARE){
        auto kind = resolveKind(expr.slotA,expr.slotB,true);
        if(kind == RTTYPED_NUM_OBJECT){
            return fail();
        }
        long double lhsVal = 0.0;
        long double rhsVal = 0.0;
        if(!localSlotToNumber(expr.slotA,kind,lhsVal) || !localSlotToNumber(expr.slotB,kind,rhsVal)){
            return fail();
        }
        bool comparison = false;
        switch(expr.op){
            case RTTYPED_COMPARE_EQ:
                comparison = (lhsVal == rhsVal);
                break;
            case RTTYPED_COMPARE_NE:
                comparison = (lhsVal != rhsVal);
                break;
            case RTTYPED_COMPARE_LT:
                comparison = (lhsVal < rhsVal);
                break;
            case RTTYPED_COMPARE_LE:
                comparison = (lhsVal <= rhsVal);
                break;
            case RTTYPED_COMPARE_GT:
                comparison = (lhsVal > rhsVal);
                break;
            case RTTYPED_COMPARE_GE:
                comparison = (lhsVal >= rhsVal);
                break;
            default:
                return fail();
        }
        return finish(StarbytesBoolNew((StarbytesBoolVal)comparison));
    }

    if(expr.kind == RTQUICKEN_EXPR_LOCAL_INTRINSIC){
        auto kind = resolveKind(expr.slotA);
        if(kind == RTTYPED_NUM_OBJECT){
            return fail();
        }
        long double value = 0.0;
        if(!localSlotToNumber(expr.slotA,kind,value)){
            return fail();
        }
        if(expr.op == RTTYPED_INTRINSIC_SQRT){
            if(value < 0.0){
                lastRuntimeError = "sqrt requires non-negative numeric input";
                return finish(nullptr);
            }
            return finish(makeNumber(std::sqrt((double)value),NumTypeDouble));
        }
        return fail();
    }

    if(expr.kind == RTQUICKEN_EXPR_LOCAL_LOCAL_INDEX_GET){
        auto collection = referenceLocalSlot(expr.slotA);
        int index = -1;
        if(!collection || !localSlotToIndex(expr.slotB,index)){
            if(collection){
                StarbytesObjectRelease(collection);
            }
            return fail();
        }
        StarbytesObject value = nullptr;
        if(index >= 0 && StarbytesObjectTypecheck(collection,StarbytesArrayType())
           && (unsigned)index < StarbytesArrayGetLength(collection)){
            long double numericValue = 0.0;
            if(StarbytesArrayTryGetNumeric(collection,(unsigned)index,numTypeFromTypedKind(expr.numericKind),&numericValue)){
                value = makeNumber(numericValue,numTypeFromTypedKind(expr.numericKind));
            }
        }
        StarbytesObjectRelease(collection);
        if(!value){
            return fail();
        }
        return finish(value);
    }

    if(expr.kind == RTQUICKEN_EXPR_LOCAL_LOCAL_LOCAL_INDEX_SET){
        auto collection = referenceLocalSlot(expr.slotA);
        int index = -1;
        long double numericValue = 0.0;
        if(!collection || !localSlotToIndex(expr.slotB,index)
           || !localSlotToNumber(expr.slotC,expr.numericKind,numericValue)){
            if(collection){
                StarbytesObjectRelease(collection);
            }
            return fail();
        }
        bool success = false;
        if(index >= 0 && StarbytesObjectTypecheck(collection,StarbytesArrayType())
           && (unsigned)index < StarbytesArrayGetLength(collection)){
            success = StarbytesArrayTrySetNumeric(collection,(unsigned)index,numTypeFromTypedKind(expr.numericKind),numericValue) != 0;
        }
        StarbytesObjectRelease(collection);
        if(!success){
            return fail();
        }
        return finish(referenceLocalSlot(expr.slotC));
    }

    return fail();
}

void InterpImpl::discardExprArgs(std::istream &in,unsigned argCount){
    while(argCount > 0){
        auto arg = evalExpr(in);
        if(arg){
            StarbytesObjectRelease(arg);
        }
        --argCount;
    }
}

StarbytesFuncCallback InterpImpl::findNativeCallback(string_ref callbackName){
    auto cached = nativeCallbackCache.find(callbackName.view());
    if(cached != nativeCallbackCache.end()){
        return cached->second;
    }
    for(auto *module : nativeModules){
        auto callback = starbytes_native_mod_load_function(module,callbackName);
        if(callback){
            nativeCallbackCache.insert(std::make_pair(callbackName.str(),callback));
            return callback;
        }
    }
    nativeCallbackCache.insert(std::make_pair(callbackName.str(),nullptr));
    return nullptr;
}

StarbytesFuncCallback InterpImpl::findNativeValueCallback(string_ref callbackName){
    auto cached = nativeValueCallbackCache.find(callbackName.view());
    if(cached != nativeValueCallbackCache.end()){
        return cached->second;
    }
    for(auto *module : nativeModules){
        auto callback = starbytes_native_mod_load_value(module,callbackName);
        if(callback){
            nativeValueCallbackCache.insert(std::make_pair(callbackName.str(),callback));
            return callback;
        }
    }
    nativeValueCallbackCache.insert(std::make_pair(callbackName.str(),nullptr));
    return nullptr;
}

StarbytesObject InterpImpl::invokeNativeFunc(std::istream &in,
                                             StarbytesFuncCallback callback,
                                             unsigned argCount,
                                             StarbytesObject boundSelf){
    std::vector<StarbytesObject> args;
    args.reserve(argCount + (boundSelf ? 1u : 0u));
    if(boundSelf){
        StarbytesObjectReference(boundSelf);
        args.push_back(boundSelf);
    }
    while(argCount > 0){
        auto arg = evalExpr(in);
        args.push_back(arg);
        --argCount;
    }

    _StarbytesFuncArgs nativeArgs;
    nativeArgs.argc = (unsigned)args.size();
    nativeArgs.index = 0;
    nativeArgs.argv = args.empty()? nullptr : args.data();
    StarbytesObject result = nullptr;
    try {
        result = callback((StarbytesFuncArgs)&nativeArgs);
    }
    catch(const std::exception &ex){
        StarbytesFuncArgsSetError((StarbytesFuncArgs)&nativeArgs,ex.what());
    }
    catch(...){
        StarbytesFuncArgsSetError((StarbytesFuncArgs)&nativeArgs,"native callback threw an unknown exception");
    }
    auto nativeErrorRaw = StarbytesFuncArgsGetError((StarbytesFuncArgs)&nativeArgs);
    std::string nativeError = nativeErrorRaw == nullptr ? std::string() : std::string(nativeErrorRaw);
    StarbytesFuncArgsClearError((StarbytesFuncArgs)&nativeArgs);

    for(auto *arg : args){
        if(arg){
            StarbytesObjectRelease(arg);
        }
    }
    if(!result && !nativeError.empty()){
        lastRuntimeError = nativeError;
    }
    return result;
}

StarbytesObject InterpImpl::invokeNativeFuncWithValues(StarbytesFuncCallback callback,
                                                       ArrayRef<StarbytesObject> args,
                                                       StarbytesObject boundSelf){
    std::vector<StarbytesObject> callArgs;
    callArgs.reserve(args.size() + (boundSelf ? 1u : 0u));
    if(boundSelf){
        StarbytesObjectReference(boundSelf);
        callArgs.push_back(boundSelf);
    }
    for(auto *arg : args){
        StarbytesObjectReference(arg);
        callArgs.push_back(arg);
    }
    _StarbytesFuncArgs nativeArgs;
    nativeArgs.argc = (unsigned)callArgs.size();
    nativeArgs.index = 0;
    nativeArgs.argv = callArgs.empty()? nullptr : callArgs.data();
    StarbytesObject result = nullptr;
    try {
        result = callback((StarbytesFuncArgs)&nativeArgs);
    }
    catch(const std::exception &ex){
        StarbytesFuncArgsSetError((StarbytesFuncArgs)&nativeArgs,ex.what());
    }
    catch(...){
        StarbytesFuncArgsSetError((StarbytesFuncArgs)&nativeArgs,"native callback threw an unknown exception");
    }
    auto nativeErrorRaw = StarbytesFuncArgsGetError((StarbytesFuncArgs)&nativeArgs);
    std::string nativeError = nativeErrorRaw == nullptr ? std::string() : std::string(nativeErrorRaw);
    StarbytesFuncArgsClearError((StarbytesFuncArgs)&nativeArgs);
    for(auto *arg : callArgs){
        if(arg){
            StarbytesObjectRelease(arg);
        }
    }
    if(!result && !nativeError.empty()){
        lastRuntimeError = nativeError;
    }
    return result;
}

StarbytesObject InterpImpl::invokeFuncWithValues(RTFuncTemplate *func_temp,
                                                 ArrayRef<StarbytesObject> args,
                                                 StarbytesObject boundSelf){
    if(!func_temp){
        return nullptr;
    }
    std::string parentScope = allocator->currentScope;
    string_ref func_name = {func_temp->name.value,(uint32_t)func_temp->name.len};

    if(func_name == "print"){
        if(!args.empty()){
            if(args[0] || lastRuntimeError.empty()){
                stdlib::print(args[0],runtimeClassRegistry);
            }
        }
        return nullptr;
    }
    if(stdlib::isMathBuiltinFunction(func_name)){
        return stdlib::invokeMathBuiltinFunction(func_name,args,lastRuntimeError);
    }

    auto nativeCallbackName = resolveNativeCallbackName(func_temp);
    if(!nativeCallbackName.empty()){
        auto callback = findNativeCallback(string_ref(nativeCallbackName));
        if(callback){
            return invokeNativeFuncWithValues(callback,args,boundSelf);
        }
        if(func_temp->blockByteSize == 0){
            lastRuntimeError = "native callback `" + nativeCallbackName + "` is not loaded";
            return nullptr;
        }
    }

    Twine funcScopeBuilder;
    funcScopeBuilder + func_name.str();
    funcScopeBuilder + std::to_string(func_temp->invocations);
    std::string funcScope = funcScopeBuilder.str();
    pushLocalFrame(func_temp);
    if(boundSelf){
        StarbytesObjectReference(boundSelf);
        allocator->allocVariable(string_ref("self"),boundSelf,funcScope);
    }

    uint32_t paramSlot = 0;
    for(auto *arg : args){
        if(paramSlot < func_temp->argsTemplate.size()){
            StarbytesObjectReference(arg);
            storeLocalSlotOwned(paramSlot,arg);
            ++paramSlot;
        }
    }

    allocator->setScope(funcScope);
    auto current_pos = func_temp->block_start_pos;
    if(!activeInput){
        popLocalFrame();
        allocator->clearScope();
        allocator->setScope(parentScope);
        return nullptr;
    }
    auto &bodyIn = *activeInput;
    auto resumePos = bodyIn.tellg();
    bodyIn.clear();
    bodyIn.seekg(current_pos);

    func_temp->invocations += 1;
    RTCode code = CODE_MODULE_END;
    if(!bodyIn.read((char *)&code,sizeof(RTCode))){
        bodyIn.clear();
        bodyIn.seekg(resumePos);
        popLocalFrame();
        allocator->clearScope();
        allocator->setScope(parentScope);
        return nullptr;
    }
    bool willReturn = false;
    StarbytesObject return_val = nullptr;
    while(bodyIn.good() && code != CODE_RTFUNCBLOCK_END){
        if(!willReturn){
            execNorm(code,bodyIn,&willReturn,&return_val);
            processMicrotasks();
        }
        if(!bodyIn.read((char *)&code,sizeof(RTCode))){
            break;
        }
    }
    bodyIn.clear();
    bodyIn.seekg(resumePos);
    popLocalFrame();
    allocator->clearScope();
    allocator->setScope(parentScope);
    return return_val;
}

StarbytesTask InterpImpl::scheduleLazyCall(std::istream &in,
                                           RTFuncTemplate *func_temp,
                                           unsigned argCount,
                                           StarbytesObject boundSelf){
    auto task = StarbytesTaskNew();
    if(!task || !func_temp){
        if(task){
            StarbytesTaskReject(task,"invalid lazy function");
        }
        discardExprArgs(in,argCount);
        return task;
    }

    ScheduledTaskCall call;
    call.task = task;
    StarbytesObjectReference(task);
    call.funcName = rtidToString(func_temp->name);
    call.args.reserve(argCount);
    if(boundSelf){
        StarbytesObjectReference(boundSelf);
        call.boundSelf = boundSelf;
    }
    while(argCount > 0){
        auto arg = evalExpr(in);
        if(!arg){
            StarbytesTaskReject(task,lastRuntimeError.empty()? "lazy argument evaluation failed" : lastRuntimeError.c_str());
            lastRuntimeError.clear();
            if(call.boundSelf){
                StarbytesObjectRelease(call.boundSelf);
                call.boundSelf = nullptr;
            }
            StarbytesObjectRelease(call.task);
            call.task = nullptr;
            return task;
        }
        call.args.push_back(arg);
        --argCount;
    }
    microtaskQueue.push_back(std::move(call));
    return task;
}

void InterpImpl::processOneMicrotask(){
    if(microtaskQueue.empty()){
        return;
    }
    ScheduledTaskCall call = std::move(microtaskQueue.front());
    microtaskQueue.pop_front();
    if(!call.task){
        if(call.task){
            StarbytesObjectRelease(call.task);
        }
        if(call.boundSelf){
            StarbytesObjectRelease(call.boundSelf);
        }
        for(auto *arg : call.args){
            if(arg){
                StarbytesObjectRelease(arg);
            }
        }
        return;
    }

    auto *runtimeFunc = findFunctionByName(string_ref(call.funcName));
    if(!runtimeFunc){
        StarbytesTaskReject(call.task,"lazy function no longer exists");
        for(auto *arg : call.args){
            if(arg){
                StarbytesObjectRelease(arg);
            }
        }
        if(call.boundSelf){
            StarbytesObjectRelease(call.boundSelf);
        }
        StarbytesObjectRelease(call.task);
        return;
    }

    auto result = invokeFuncWithValues(runtimeFunc,{call.args.data(), static_cast<uint32_t>(call.args.size())},call.boundSelf);
    if(result){
        StarbytesTaskResolve(call.task,result);
        StarbytesObjectRelease(result);
        lastRuntimeError.clear();
    }
    else {
        auto message = lastRuntimeError.empty()? std::string("task failed") : lastRuntimeError;
        StarbytesTaskReject(call.task,message.c_str());
        lastRuntimeError.clear();
    }

    for(auto *arg : call.args){
        if(arg){
            StarbytesObjectRelease(arg);
        }
    }
    if(call.boundSelf){
        StarbytesObjectRelease(call.boundSelf);
    }
    StarbytesObjectRelease(call.task);
}

void InterpImpl::processMicrotasks(){
    if(isDrainingMicrotasks){
        return;
    }
    isDrainingMicrotasks = true;
    while(!microtaskQueue.empty()){
        processOneMicrotask();
    }
    isDrainingMicrotasks = false;
}


StarbytesObject InterpImpl::invokeFunc(std::istream & in,RTFuncTemplate *func_temp,unsigned argCount,StarbytesObject boundSelf){
    if(!func_temp){
        discardExprArgs(in,argCount);
        return nullptr;
    }
    std::string parentScope = allocator->currentScope;
    string_ref func_name = {func_temp->name.value,(uint32_t)func_temp->name.len};
    
    if(func_name == "print"){
        if(argCount > 0){
            auto object_to_print = evalExpr(in);
            if(object_to_print || lastRuntimeError.empty()){
                stdlib::print(object_to_print,runtimeClassRegistry);
            }
            if(object_to_print){
                StarbytesObjectRelease(object_to_print);
            }
            --argCount;
        }
        if(argCount > 0){
            discardExprArgs(in,argCount);
        }
        return nullptr;
    }
    if(stdlib::isMathBuiltinFunction(func_name)){
        std::vector<StarbytesObject> args;
        args.reserve(argCount);
        while(argCount > 0){
            args.push_back(evalExpr(in));
            --argCount;
        }
        auto result = stdlib::invokeMathBuiltinFunction(func_name,{args.data(), static_cast<uint32_t>(args.size())},lastRuntimeError);
        for(auto *arg : args){
            if(arg){
                StarbytesObjectRelease(arg);
            }
        }
        return result;
    }
    auto nativeCallbackName = resolveNativeCallbackName(func_temp);
    if(!nativeCallbackName.empty()){
        auto callback = findNativeCallback(string_ref(nativeCallbackName));
        if(callback){
            return invokeNativeFunc(in,callback,argCount,boundSelf);
        }
        if(func_temp->blockByteSize == 0){
            discardExprArgs(in,argCount);
            lastRuntimeError = "native callback `" + nativeCallbackName + "` is not loaded";
            return nullptr;
        }
    }
    Twine funcScopeBuilder;
    funcScopeBuilder + func_name.str();
    funcScopeBuilder + std::to_string(func_temp->invocations);
    std::string funcScope = funcScopeBuilder.str();
    std::vector<StarbytesObject> args;
    args.reserve(argCount);
    while(argCount > 0){
        args.push_back(evalExpr(in));
        --argCount;
    }
    auto resumePos = in.tellg();
    pushLocalFrame(func_temp);

    if(boundSelf){
        StarbytesObjectReference(boundSelf);
        allocator->allocVariable(string_ref("self"),boundSelf,funcScope);
    }

    uint32_t paramSlot = 0;
    for(auto *arg : args){
        if(paramSlot < func_temp->argsTemplate.size()){
            storeLocalSlotOwned(paramSlot,arg);
            ++paramSlot;
        }
        else if(arg){
            StarbytesObjectRelease(arg);
        }
    }
    allocator->setScope(funcScope);
    auto current_pos = func_temp->block_start_pos;
    /// Invoke
    in.seekg(current_pos);
    
    func_temp->invocations += 1;
                
    RTCode code = CODE_MODULE_END;
    if(!in.read((char *)&code,sizeof(RTCode))){
        in.clear();
        in.seekg(resumePos);
        popLocalFrame();
        allocator->clearScope();
        allocator->setScope(parentScope);
        return nullptr;
    }
    bool willReturn = false;
    StarbytesObject return_val = nullptr;
    while(in.good() && code != CODE_RTFUNCBLOCK_END){
        if(!willReturn){
            execNorm(code,in,&willReturn,&return_val);
            processMicrotasks();
        }
        if(!in.read((char *)&code,sizeof(RTCode))){
            break;
        }
    };
    in.seekg(resumePos);
    
    popLocalFrame();
    allocator->clearScope();
    allocator->setScope(parentScope);
    
    return return_val;
    
}

StarbytesObject InterpImpl::evalExpr(std::istream & in){
    auto exprStart = in.tellg();
    RTCode code;
    in.read((char *)&code,sizeof(RTCode));
    if(code != CODE_MODULE_END){
        if(auto *quickened = getOrInstallQuickenedExpr(in,exprStart,code)){
            StarbytesObject quickenedResult = nullptr;
            if(tryExecuteQuickenedExpr(in,exprStart,*quickened,quickenedResult)){
                return quickenedResult;
            }
            auto fallbackPos = exprStart;
            if(fallbackPos != std::istream::pos_type(-1)){
                fallbackPos += static_cast<std::streamoff>(sizeof(RTCode));
                in.clear();
                in.seekg(fallbackPos);
            }
        }
    }
    switch (code) {
        case CODE_RTOBJCREATE:
        {
            
            break;
        }
        case CODE_RTINTOBJCREATE :
        {
            StarbytesObject object;
            in >> &object;
            return object;
            break;
        }
        case CODE_RTVAR_REF : {
            RTID var_id;
            in >> &var_id;
            string_ref var_name (var_id.value,var_id.len);
            return allocator->referenceVariable(allocator->currentScope,var_name);
            
            break;
        }
        case CODE_RTLOCAL_REF: {
            uint32_t slot = 0;
            in.read((char *)&slot,sizeof(slot));
            return referenceLocalSlot(slot);
        }
        case CODE_RTTYPED_LOCAL_REF: {
            RTTypedNumericKind kind = RTTYPED_NUM_OBJECT;
            uint32_t slot = 0;
            in.read((char *)&kind,sizeof(kind));
            in.read((char *)&slot,sizeof(slot));
            (void)kind;
            return referenceLocalSlot(slot);
        }
        case CODE_RTFUNC_REF : {
            RTID var_id;
            in >> &var_id;
            string_ref func_name (var_id.value,var_id.len);
            auto *funcTemplate = findFunctionByName(func_name);
            if(funcTemplate){
                return StarbytesFuncRefNew(funcTemplate);
            }
            break;
        }
        case CODE_RTIVKFUNC : {
            auto calleeObject = evalExpr(in);
            unsigned argCount;
            in.read((char *)&argCount,sizeof(argCount));
            if(!calleeObject){
                discardExprArgs(in,argCount);
                return nullptr;
            }
            if(!StarbytesObjectTypecheck(calleeObject,StarbytesFuncRefType())){
                discardExprArgs(in,argCount);
                lastRuntimeError = "invocation target is not a function";
                StarbytesObjectRelease(calleeObject);
                return nullptr;
            }
            auto funcRefObject = (StarbytesFuncRef)calleeObject;
            auto *funcTemplate = StarbytesFuncRefGetPtr(funcRefObject);
            StarbytesObject returnValue = nullptr;
            if(funcTemplate && funcTemplate->isLazy){
                returnValue = scheduleLazyCall(in,funcTemplate,argCount);
            }
            else {
                returnValue = invokeFunc(in,funcTemplate,argCount);
            }
            StarbytesObjectRelease(funcRefObject);
            return returnValue;
        }
        case CODE_UNARY_OPERATOR: {
            RTCode unaryCode = UNARY_OP_NOT;
            in.read((char *)&unaryCode,sizeof(unaryCode));
            auto operand = evalExpr(in);
            if(!operand){
                return nullptr;
            }
            if(unaryCode == UNARY_OP_PLUS){
                if(!StarbytesObjectTypecheck(operand,StarbytesNumType())){
                    StarbytesObjectRelease(operand);
                    return nullptr;
                }
                auto operandType = StarbytesNumGetType(operand);
                StarbytesObject result = nullptr;
                if(operandType == NumTypeFloat){
                    result = StarbytesNumNew(NumTypeFloat,StarbytesNumGetFloatValue(operand));
                }
                else if(operandType == NumTypeDouble){
                    result = StarbytesNumNew(NumTypeDouble,StarbytesNumGetDoubleValue(operand));
                }
                else if(operandType == NumTypeLong){
                    result = StarbytesNumNew(NumTypeLong,StarbytesNumGetLongValue(operand));
                }
                else {
                    result = StarbytesNumNew(NumTypeInt,StarbytesNumGetIntValue(operand));
                }
                StarbytesObjectRelease(operand);
                return result;
            }
            if(unaryCode == UNARY_OP_MINUS){
                if(!StarbytesObjectTypecheck(operand,StarbytesNumType())){
                    StarbytesObjectRelease(operand);
                    return nullptr;
                }
                auto operandType = StarbytesNumGetType(operand);
                StarbytesObject result = nullptr;
                if(operandType == NumTypeFloat){
                    result = StarbytesNumNew(NumTypeFloat,-StarbytesNumGetFloatValue(operand));
                }
                else if(operandType == NumTypeDouble){
                    result = StarbytesNumNew(NumTypeDouble,-StarbytesNumGetDoubleValue(operand));
                }
                else if(operandType == NumTypeLong){
                    result = StarbytesNumNew(NumTypeLong,-StarbytesNumGetLongValue(operand));
                }
                else {
                    result = StarbytesNumNew(NumTypeInt,-StarbytesNumGetIntValue(operand));
                }
                StarbytesObjectRelease(operand);
                return result;
            }
            if(unaryCode == UNARY_OP_NOT){
                if(!StarbytesObjectTypecheck(operand,StarbytesBoolType())){
                    StarbytesObjectRelease(operand);
                    return nullptr;
                }
                bool current = (bool)StarbytesBoolValue(operand);
                StarbytesObjectRelease(operand);
                return StarbytesBoolNew((StarbytesBoolVal)!current);
            }
            if(unaryCode == UNARY_OP_BITWISE_NOT){
                if(!StarbytesObjectTypecheck(operand,StarbytesNumType()) || StarbytesNumGetType(operand) != NumTypeInt){
                    StarbytesObjectRelease(operand);
                    return nullptr;
                }
                auto val = StarbytesNumGetIntValue(operand);
                StarbytesObjectRelease(operand);
                return StarbytesNumNew(NumTypeInt,(starbytes_int_t)(~val));
            }
            if(unaryCode == UNARY_OP_AWAIT){
                if(!StarbytesObjectTypecheck(operand,StarbytesTaskType())){
                    StarbytesObjectRelease(operand);
                    lastRuntimeError = "`await` requires a Task value";
                    return nullptr;
                }
                while(StarbytesTaskGetState(operand) == StarbytesTaskPending){
                    if(microtaskQueue.empty()){
                        lastRuntimeError = "await stalled on unresolved task";
                        break;
                    }
                    processMicrotasks();
                }
                auto state = StarbytesTaskGetState(operand);
                if(state == StarbytesTaskResolved){
                    auto value = StarbytesTaskGetValue(operand);
                    StarbytesObjectRelease(operand);
                    return value;
                }
                auto reason = StarbytesTaskGetError(operand);
                if(reason){
                    lastRuntimeError = reason;
                }
                else if(lastRuntimeError.empty()){
                    lastRuntimeError = "task rejected";
                }
                StarbytesObjectRelease(operand);
                return nullptr;
            }
            StarbytesObjectRelease(operand);
            return nullptr;
        }
        case CODE_RTTYPED_NEGATE: {
            RTTypedNumericKind kind = RTTYPED_NUM_OBJECT;
            in.read((char *)&kind,sizeof(kind));
            auto operand = evalExpr(in);
            if(!operand){
                return nullptr;
            }
            long double value = 0.0;
            auto finish = [&](StarbytesObject result){
                StarbytesObjectRelease(operand);
                return result;
            };
            if(!extractTypedNumericValue(operand,kind,value)){
                return finish(nullptr);
            }
            return finish(makeNumber(-value,numTypeFromTypedKind(kind)));
        }
        case CODE_BINARY_OPERATOR: {
            RTCode binaryCode = BINARY_OP_PLUS;
            in.read((char *)&binaryCode,sizeof(binaryCode));
            auto lhs = evalExpr(in);
            if(!lhs){
                skipExpr(in);
                return nullptr;
            }

            auto releaseLhs = [&](){
                if(lhs){
                    StarbytesObjectRelease(lhs);
                    lhs = nullptr;
                }
            };

            if(binaryCode == BINARY_LOGIC_AND || binaryCode == BINARY_LOGIC_OR){
                if(!StarbytesObjectTypecheck(lhs,StarbytesBoolType())){
                    releaseLhs();
                    skipExpr(in);
                    return nullptr;
                }

                bool l = (bool)StarbytesBoolValue(lhs);
                if(binaryCode == BINARY_LOGIC_AND && !l){
                    releaseLhs();
                    skipExpr(in);
                    return StarbytesBoolNew((StarbytesBoolVal)false);
                }
                if(binaryCode == BINARY_LOGIC_OR && l){
                    releaseLhs();
                    skipExpr(in);
                    return StarbytesBoolNew((StarbytesBoolVal)true);
                }

                auto rhs = evalExpr(in);
                if(!rhs){
                    releaseLhs();
                    return nullptr;
                }
                if(!StarbytesObjectTypecheck(rhs,StarbytesBoolType())){
                    releaseLhs();
                    StarbytesObjectRelease(rhs);
                    return nullptr;
                }
                bool r = (bool)StarbytesBoolValue(rhs);
                StarbytesObjectRelease(rhs);
                releaseLhs();
                bool result = (binaryCode == BINARY_LOGIC_AND)? (l && r) : (l || r);
                return StarbytesBoolNew((StarbytesBoolVal)result);
            }

            auto rhs = evalExpr(in);
            if(!rhs){
                releaseLhs();
                return nullptr;
            }

            auto finish = [&](StarbytesObject result){
                releaseLhs();
                StarbytesObjectRelease(rhs);
                return result;
            };

            if(binaryCode == BINARY_OP_PLUS){
                if(StarbytesObjectTypecheck(lhs,StarbytesNumType()) && StarbytesObjectTypecheck(rhs,StarbytesNumType())){
                    return finish(StarbytesNumAdd(lhs,rhs));
                }
                if(StarbytesObjectTypecheck(lhs,StarbytesStrType()) || StarbytesObjectTypecheck(rhs,StarbytesStrType())){
                    auto leftStr = objectToString(lhs);
                    auto rightStr = objectToString(rhs);
                    return finish(StarbytesStrNewWithData((leftStr + rightStr).c_str()));
                }
                return finish(nullptr);
            }
            if(binaryCode == BINARY_OP_MINUS){
                if(StarbytesObjectTypecheck(lhs,StarbytesNumType()) && StarbytesObjectTypecheck(rhs,StarbytesNumType())){
                    return finish(StarbytesNumSub(lhs,rhs));
                }
                return finish(nullptr);
            }
            if(binaryCode == BINARY_OP_MUL || binaryCode == BINARY_OP_DIV || binaryCode == BINARY_OP_MOD){
                if(!(StarbytesObjectTypecheck(lhs,StarbytesNumType()) && StarbytesObjectTypecheck(rhs,StarbytesNumType()))){
                    return finish(nullptr);
                }
                StarbytesNumT lhsType = NumTypeInt;
                StarbytesNumT rhsType = NumTypeInt;
                long double lhsVal = 0.0;
                long double rhsVal = 0.0;
                if(!objectToNumber(lhs,lhsVal,lhsType) || !objectToNumber(rhs,rhsVal,rhsType)){
                    return finish(nullptr);
                }
                if((binaryCode == BINARY_OP_DIV || binaryCode == BINARY_OP_MOD) && rhsVal == 0.0){
                    return finish(nullptr);
                }
                StarbytesNumT resultType = promoteNumericType(lhsType,rhsType);
                if(binaryCode == BINARY_OP_MUL){
                    return finish(makeNumber(lhsVal * rhsVal,resultType));
                }
                if(binaryCode == BINARY_OP_DIV){
                    if(isFloatingNumType(resultType)){
                        return finish(makeNumber(lhsVal / rhsVal,resultType));
                    }
                    if(resultType == NumTypeLong){
                        return finish(makeNumber((long double)((int64_t)lhsVal / (int64_t)rhsVal),NumTypeLong));
                    }
                    return finish(makeNumber((long double)((int)lhsVal / (int)rhsVal),NumTypeInt));
                }
                if(isFloatingNumType(resultType)){
                    return finish(makeNumber(std::fmod((double)lhsVal,(double)rhsVal),resultType));
                }
                if(resultType == NumTypeLong){
                    return finish(makeNumber((long double)((int64_t)lhsVal % (int64_t)rhsVal),NumTypeLong));
                }
                return finish(makeNumber((long double)((int)lhsVal % (int)rhsVal),NumTypeInt));
            }
            if(binaryCode == BINARY_EQUAL_EQUAL || binaryCode == BINARY_NOT_EQUAL){
                bool equals = false;
                if(StarbytesObjectTypecheck(lhs,StarbytesNumType()) && StarbytesObjectTypecheck(rhs,StarbytesNumType())){
                    equals = (StarbytesNumCompare(lhs,rhs) == COMPARE_EQUAL);
                }
                else if(StarbytesObjectTypecheck(lhs,StarbytesStrType()) && StarbytesObjectTypecheck(rhs,StarbytesStrType())){
                    equals = (StarbytesStrCompare(lhs,rhs) == COMPARE_EQUAL);
                }
                else if(StarbytesObjectTypecheck(lhs,StarbytesBoolType()) && StarbytesObjectTypecheck(rhs,StarbytesBoolType())){
                    equals = ((bool)StarbytesBoolValue(lhs) == (bool)StarbytesBoolValue(rhs));
                }
                else {
                    equals = (lhs == rhs);
                }
                if(binaryCode == BINARY_NOT_EQUAL){
                    equals = !equals;
                }
                return finish(StarbytesBoolNew((StarbytesBoolVal)equals));
            }
            if(binaryCode == BINARY_LESS || binaryCode == BINARY_LESS_EQUAL ||
               binaryCode == BINARY_GREATER || binaryCode == BINARY_GREATER_EQUAL){
                bool result = false;
                if(StarbytesObjectTypecheck(lhs,StarbytesNumType()) && StarbytesObjectTypecheck(rhs,StarbytesNumType())){
                    int cmp = StarbytesNumCompare(lhs,rhs);
                    if(binaryCode == BINARY_LESS){
                        result = (cmp == COMPARE_LESS);
                    }
                    else if(binaryCode == BINARY_LESS_EQUAL){
                        result = (cmp == COMPARE_LESS || cmp == COMPARE_EQUAL);
                    }
                    else if(binaryCode == BINARY_GREATER){
                        result = (cmp == COMPARE_GREATER);
                    }
                    else {
                        result = (cmp == COMPARE_GREATER || cmp == COMPARE_EQUAL);
                    }
                    return finish(StarbytesBoolNew((StarbytesBoolVal)result));
                }
                if(StarbytesObjectTypecheck(lhs,StarbytesStrType()) && StarbytesObjectTypecheck(rhs,StarbytesStrType())){
                    int cmp = StarbytesStrCompare(lhs,rhs);
                    if(binaryCode == BINARY_LESS){
                        result = (cmp == COMPARE_LESS);
                    }
                    else if(binaryCode == BINARY_LESS_EQUAL){
                        result = (cmp == COMPARE_LESS || cmp == COMPARE_EQUAL);
                    }
                    else if(binaryCode == BINARY_GREATER){
                        result = (cmp == COMPARE_GREATER);
                    }
                    else {
                        result = (cmp == COMPARE_GREATER || cmp == COMPARE_EQUAL);
                    }
                    return finish(StarbytesBoolNew((StarbytesBoolVal)result));
                }
                return finish(nullptr);
            }
            if(binaryCode == BINARY_BITWISE_AND || binaryCode == BINARY_BITWISE_OR ||
               binaryCode == BINARY_BITWISE_XOR || binaryCode == BINARY_SHIFT_LEFT ||
               binaryCode == BINARY_SHIFT_RIGHT){
                if(!(StarbytesObjectTypecheck(lhs,StarbytesNumType()) && StarbytesObjectTypecheck(rhs,StarbytesNumType()))){
                    return finish(nullptr);
                }
                if(StarbytesNumGetType(lhs) != NumTypeInt || StarbytesNumGetType(rhs) != NumTypeInt){
                    return finish(nullptr);
                }
                auto lhsVal = (starbytes_int_t)StarbytesNumGetIntValue(lhs);
                auto rhsVal = (starbytes_int_t)StarbytesNumGetIntValue(rhs);
                if((binaryCode == BINARY_SHIFT_LEFT || binaryCode == BINARY_SHIFT_RIGHT) && rhsVal < 0){
                    return finish(nullptr);
                }
                if(binaryCode == BINARY_BITWISE_AND){
                    return finish(StarbytesNumNew(NumTypeInt,lhsVal & rhsVal));
                }
                if(binaryCode == BINARY_BITWISE_OR){
                    return finish(StarbytesNumNew(NumTypeInt,lhsVal | rhsVal));
                }
                if(binaryCode == BINARY_BITWISE_XOR){
                    return finish(StarbytesNumNew(NumTypeInt,lhsVal ^ rhsVal));
                }
                if(binaryCode == BINARY_SHIFT_LEFT){
                    return finish(StarbytesNumNew(NumTypeInt,(starbytes_int_t)(lhsVal << rhsVal)));
                }
                return finish(StarbytesNumNew(NumTypeInt,(starbytes_int_t)(lhsVal >> rhsVal)));
            }
            return finish(nullptr);
        }
        case CODE_RTTYPED_BINARY: {
            RTTypedNumericKind kind = RTTYPED_NUM_OBJECT;
            RTTypedBinaryOp op = RTTYPED_BINARY_ADD;
            in.read((char *)&kind,sizeof(kind));
            in.read((char *)&op,sizeof(op));
            auto lhs = evalExpr(in);
            if(!lhs){
                skipExpr(in);
                return nullptr;
            }
            auto rhs = evalExpr(in);
            if(!rhs){
                StarbytesObjectRelease(lhs);
                return nullptr;
            }
            long double lhsVal = 0.0;
            long double rhsVal = 0.0;
            auto finish = [&](StarbytesObject result){
                StarbytesObjectRelease(lhs);
                StarbytesObjectRelease(rhs);
                return result;
            };
            if(!extractTypedNumericValue(lhs,kind,lhsVal) || !extractTypedNumericValue(rhs,kind,rhsVal)){
                return finish(nullptr);
            }
            if((op == RTTYPED_BINARY_DIV || op == RTTYPED_BINARY_MOD) && rhsVal == 0.0){
                return finish(nullptr);
            }
            switch(op){
                case RTTYPED_BINARY_ADD:
                    return finish(makeNumber(lhsVal + rhsVal,numTypeFromTypedKind(kind)));
                case RTTYPED_BINARY_SUB:
                    return finish(makeNumber(lhsVal - rhsVal,numTypeFromTypedKind(kind)));
                case RTTYPED_BINARY_MUL:
                    return finish(makeNumber(lhsVal * rhsVal,numTypeFromTypedKind(kind)));
                case RTTYPED_BINARY_DIV:
                    if(kind == RTTYPED_NUM_INT){
                        return finish(makeNumber((long double)((int)lhsVal / (int)rhsVal),NumTypeInt));
                    }
                    if(kind == RTTYPED_NUM_LONG){
                        return finish(makeNumber((long double)((int64_t)lhsVal / (int64_t)rhsVal),NumTypeLong));
                    }
                    return finish(makeNumber(lhsVal / rhsVal,numTypeFromTypedKind(kind)));
                case RTTYPED_BINARY_MOD:
                    if(kind == RTTYPED_NUM_INT){
                        return finish(makeNumber((long double)((int)lhsVal % (int)rhsVal),NumTypeInt));
                    }
                    if(kind == RTTYPED_NUM_LONG){
                        return finish(makeNumber((long double)((int64_t)lhsVal % (int64_t)rhsVal),NumTypeLong));
                    }
                    return finish(makeNumber(std::fmod((double)lhsVal,(double)rhsVal),numTypeFromTypedKind(kind)));
                default:
                    return finish(nullptr);
            }
        }
        case CODE_RTTYPED_COMPARE: {
            RTTypedNumericKind kind = RTTYPED_NUM_OBJECT;
            RTTypedCompareOp op = RTTYPED_COMPARE_EQ;
            in.read((char *)&kind,sizeof(kind));
            in.read((char *)&op,sizeof(op));
            auto lhs = evalExpr(in);
            if(!lhs){
                skipExpr(in);
                return nullptr;
            }
            auto rhs = evalExpr(in);
            if(!rhs){
                StarbytesObjectRelease(lhs);
                return nullptr;
            }
            long double lhsVal = 0.0;
            long double rhsVal = 0.0;
            auto finish = [&](bool result){
                StarbytesObjectRelease(lhs);
                StarbytesObjectRelease(rhs);
                return StarbytesBoolNew((StarbytesBoolVal)result);
            };
            if(!extractTypedNumericValue(lhs,kind,lhsVal) || !extractTypedNumericValue(rhs,kind,rhsVal)){
                StarbytesObjectRelease(lhs);
                StarbytesObjectRelease(rhs);
                return nullptr;
            }
            switch(op){
                case RTTYPED_COMPARE_EQ:
                    return finish(lhsVal == rhsVal);
                case RTTYPED_COMPARE_NE:
                    return finish(lhsVal != rhsVal);
                case RTTYPED_COMPARE_LT:
                    return finish(lhsVal < rhsVal);
                case RTTYPED_COMPARE_LE:
                    return finish(lhsVal <= rhsVal);
                case RTTYPED_COMPARE_GT:
                    return finish(lhsVal > rhsVal);
                case RTTYPED_COMPARE_GE:
                    return finish(lhsVal >= rhsVal);
                default:
                    StarbytesObjectRelease(lhs);
                    StarbytesObjectRelease(rhs);
                    return nullptr;
            }
        }
        case CODE_RTVAR_SET: {
            RTID varId;
            in >> &varId;
            auto value = evalExpr(in);
            if(!value){
                value = StarbytesBoolNew((StarbytesBoolVal)false);
            }
            allocator->allocVariable(string_ref(varId.value,varId.len),value);
            StarbytesObjectReference(value);
            return value;
        }
        case CODE_RTLOCAL_SET: {
            uint32_t slot = 0;
            in.read((char *)&slot,sizeof(slot));
            auto value = evalExpr(in);
            if(!value){
                value = StarbytesBoolNew((StarbytesBoolVal)false);
            }
            storeLocalSlotBorrowed(slot,value);
            return value;
        }
        case CODE_RTARRAY_LITERAL: {
            unsigned elementCount = 0;
            in.read((char *)&elementCount,sizeof(elementCount));
            auto array = StarbytesArrayNew();
            StarbytesArrayReserve(array,elementCount);
            while(elementCount > 0){
                auto element = evalExpr(in);
                if(!element){
                    StarbytesObjectRelease(array);
                    return nullptr;
                }
                StarbytesArrayPush(array,element);
                StarbytesObjectRelease(element);
                --elementCount;
            }
            return array;
        }
        case CODE_RTDICT_LITERAL: {
            unsigned pairCount = 0;
            in.read((char *)&pairCount,sizeof(pairCount));
            auto dict = StarbytesDictNew();
            while(pairCount > 0){
                auto key = evalExpr(in);
                auto val = evalExpr(in);
                if(key && val){
                    StarbytesDictSet(dict,key,val);
                }
                if(key){
                    StarbytesObjectRelease(key);
                }
                if(val){
                    StarbytesObjectRelease(val);
                }
                --pairCount;
            }
            return dict;
        }
        case CODE_RTTYPED_INTRINSIC: {
            RTTypedNumericKind kind = RTTYPED_NUM_OBJECT;
            RTTypedIntrinsicOp op = RTTYPED_INTRINSIC_SQRT;
            in.read((char *)&kind,sizeof(kind));
            in.read((char *)&op,sizeof(op));
            auto argument = evalExpr(in);
            if(!argument){
                return nullptr;
            }
            long double value = 0.0;
            auto finish = [&](StarbytesObject result){
                StarbytesObjectRelease(argument);
                return result;
            };
            if(!extractTypedNumericValue(argument,kind,value)){
                return finish(nullptr);
            }
            switch(op){
                case RTTYPED_INTRINSIC_SQRT:
                    if(value < 0.0){
                        lastRuntimeError = "sqrt requires non-negative numeric input";
                        return finish(nullptr);
                    }
                    return finish(makeNumber(std::sqrt((double)value),NumTypeDouble));
                default:
                    return finish(nullptr);
            }
        }
        case CODE_RTTYPECHECK: {
            auto object = evalExpr(in);
            RTID typeId;
            in >> &typeId;
            std::string targetType = rtidToString(typeId);
            bool matches = false;
            if(targetType == "Any"){
                matches = (object != nullptr);
            }
            else if(object){
                if(targetType == "String"){
                    matches = StarbytesObjectTypecheck(object,StarbytesStrType());
                }
                else if(targetType == "Bool"){
                    matches = StarbytesObjectTypecheck(object,StarbytesBoolType());
                }
                else if(targetType == "Array"){
                    matches = StarbytesObjectTypecheck(object,StarbytesArrayType());
                }
                else if(targetType == "Dict"){
                    matches = StarbytesObjectTypecheck(object,StarbytesDictType());
                }
                else if(targetType == "Map"){
                    matches = StarbytesObjectTypecheck(object,StarbytesDictType());
                }
                else if(targetType == "Regex"){
                    matches = StarbytesObjectTypecheck(object,StarbytesRegexType());
                }
                else if(targetType == "Task"){
                    matches = StarbytesObjectTypecheck(object,StarbytesTaskType());
                }
                else if(targetType == "__func__"){
                    matches = StarbytesObjectTypecheck(object,StarbytesFuncRefType());
                }
                else if(targetType == "Int"){
                    matches = StarbytesObjectTypecheck(object,StarbytesNumType())
                        && StarbytesNumGetType(object) == NumTypeInt;
                }
                else if(targetType == "Float"){
                    matches = StarbytesObjectTypecheck(object,StarbytesNumType())
                        && StarbytesNumGetType(object) == NumTypeFloat;
                }
                else if(targetType == "Long"){
                    matches = StarbytesObjectTypecheck(object,StarbytesNumType())
                        && StarbytesNumGetType(object) == NumTypeLong;
                }
                else if(targetType == "Double"){
                    matches = StarbytesObjectTypecheck(object,StarbytesNumType())
                        && StarbytesNumGetType(object) == NumTypeDouble;
                }
                else if(!StarbytesObjectIs(object)){
                    auto classType = StarbytesClassObjectGetClass(object);
                    auto *classMeta = findClassByType(classType);
                    std::set<std::string> visited;
                    while(classMeta){
                        auto className = rtidToString(classMeta->name);
                        if(visited.find(className) != visited.end()){
                            break;
                        }
                        visited.insert(className);
                        if(className == targetType){
                            matches = true;
                            break;
                        }
                        if(!classMeta->hasSuperClass){
                            break;
                        }
                        auto superName = rtidToString(classMeta->superClassName);
                        classMeta = findClassByName(string_ref(superName));
                    }
                }
            }
            if(object){
                StarbytesObjectRelease(object);
            }
            return StarbytesBoolNew((StarbytesBoolVal)matches);
        }
        case CODE_RTCAST: {
            auto object = evalExpr(in);
            RTID typeId;
            in >> &typeId;
            std::string targetType = rtidToString(typeId);
            if(!object){
                return nullptr;
            }
            auto failCast = [&](const std::string &message) -> StarbytesObject {
                lastRuntimeError = message;
                StarbytesObjectRelease(object);
                return nullptr;
            };

            if(targetType == "Any"){
                return object;
            }
            if(targetType == "String"){
                auto text = objectToString(object);
                StarbytesObjectRelease(object);
                return StarbytesStrNewWithData(text.c_str());
            }
            if(targetType == "Bool"){
                bool value = true;
                if(StarbytesObjectTypecheck(object,StarbytesBoolType())){
                    value = (bool)StarbytesBoolValue(object);
                }
                else if(StarbytesObjectTypecheck(object,StarbytesNumType())){
                    auto objectType = StarbytesNumGetType(object);
                    if(objectType == NumTypeFloat){
                        value = StarbytesNumGetFloatValue(object) != 0.0f;
                    }
                    else if(objectType == NumTypeDouble){
                        value = StarbytesNumGetDoubleValue(object) != 0.0;
                    }
                    else if(objectType == NumTypeLong){
                        value = StarbytesNumGetLongValue(object) != 0;
                    }
                    else {
                        value = StarbytesNumGetIntValue(object) != 0;
                    }
                }
                else if(StarbytesObjectTypecheck(object,StarbytesStrType())){
                    auto text = stringTrim(StarbytesStrGetBuffer(object));
                    value = !text.empty();
                }
                StarbytesObjectRelease(object);
                return StarbytesBoolNew((StarbytesBoolVal)value);
            }
            if(targetType == "Int"){
                int value = 0;
                if(StarbytesObjectTypecheck(object,StarbytesNumType())){
                    auto objectType = StarbytesNumGetType(object);
                    if(objectType == NumTypeFloat){
                        value = (int)StarbytesNumGetFloatValue(object);
                    }
                    else if(objectType == NumTypeDouble){
                        value = (int)StarbytesNumGetDoubleValue(object);
                    }
                    else if(objectType == NumTypeLong){
                        value = (int)StarbytesNumGetLongValue(object);
                    }
                    else {
                        value = StarbytesNumGetIntValue(object);
                    }
                    StarbytesObjectRelease(object);
                    return StarbytesNumNew(NumTypeInt,value);
                }
                if(StarbytesObjectTypecheck(object,StarbytesBoolType())){
                    value = (bool)StarbytesBoolValue(object)? 1 : 0;
                    StarbytesObjectRelease(object);
                    return StarbytesNumNew(NumTypeInt,value);
                }
                if(StarbytesObjectTypecheck(object,StarbytesStrType())){
                    auto text = StarbytesStrGetBuffer(object);
                    if(!parseIntStrict(text? text : "",value)){
                        return failCast("Int cast failed: string is not a valid integer");
                    }
                    StarbytesObjectRelease(object);
                    return StarbytesNumNew(NumTypeInt,value);
                }
                return failCast("Int cast failed: unsupported source type");
            }
            if(targetType == "Long"){
                int64_t value = 0;
                if(StarbytesObjectTypecheck(object,StarbytesNumType())){
                    auto objectType = StarbytesNumGetType(object);
                    if(objectType == NumTypeFloat){
                        value = (int64_t)StarbytesNumGetFloatValue(object);
                    }
                    else if(objectType == NumTypeDouble){
                        value = (int64_t)StarbytesNumGetDoubleValue(object);
                    }
                    else if(objectType == NumTypeLong){
                        value = StarbytesNumGetLongValue(object);
                    }
                    else {
                        value = (int64_t)StarbytesNumGetIntValue(object);
                    }
                    StarbytesObjectRelease(object);
                    return StarbytesNumNew(NumTypeLong,value);
                }
                if(StarbytesObjectTypecheck(object,StarbytesBoolType())){
                    value = (bool)StarbytesBoolValue(object)? 1 : 0;
                    StarbytesObjectRelease(object);
                    return StarbytesNumNew(NumTypeLong,value);
                }
                if(StarbytesObjectTypecheck(object,StarbytesStrType())){
                    auto text = StarbytesStrGetBuffer(object);
                    errno = 0;
                    char *endPtr = nullptr;
                    long long parsed = std::strtoll(text ? text : "",&endPtr,10);
                    if(errno != 0 || endPtr == nullptr || *endPtr != '\0'){
                        return failCast("Long cast failed: string is not a valid integer");
                    }
                    StarbytesObjectRelease(object);
                    return StarbytesNumNew(NumTypeLong,(int64_t)parsed);
                }
                return failCast("Long cast failed: unsupported source type");
            }
            if(targetType == "Float"){
                float value = 0.0f;
                if(StarbytesObjectTypecheck(object,StarbytesNumType())){
                    auto objectType = StarbytesNumGetType(object);
                    if(objectType == NumTypeFloat){
                        value = StarbytesNumGetFloatValue(object);
                    }
                    else if(objectType == NumTypeDouble){
                        value = (float)StarbytesNumGetDoubleValue(object);
                    }
                    else if(objectType == NumTypeLong){
                        value = (float)StarbytesNumGetLongValue(object);
                    }
                    else {
                        value = (float)StarbytesNumGetIntValue(object);
                    }
                    StarbytesObjectRelease(object);
                    return StarbytesNumNew(NumTypeFloat,value);
                }
                if(StarbytesObjectTypecheck(object,StarbytesBoolType())){
                    value = (bool)StarbytesBoolValue(object)? 1.0f : 0.0f;
                    StarbytesObjectRelease(object);
                    return StarbytesNumNew(NumTypeFloat,value);
                }
                if(StarbytesObjectTypecheck(object,StarbytesStrType())){
                    auto text = StarbytesStrGetBuffer(object);
                    if(!parseFloatStrict(text? text : "",value)){
                        return failCast("Float cast failed: string is not a valid float");
                    }
                    StarbytesObjectRelease(object);
                    return StarbytesNumNew(NumTypeFloat,value);
                }
                return failCast("Float cast failed: unsupported source type");
            }
            if(targetType == "Double"){
                double value = 0.0;
                if(StarbytesObjectTypecheck(object,StarbytesNumType())){
                    auto objectType = StarbytesNumGetType(object);
                    if(objectType == NumTypeFloat){
                        value = StarbytesNumGetFloatValue(object);
                    }
                    else if(objectType == NumTypeDouble){
                        value = StarbytesNumGetDoubleValue(object);
                    }
                    else if(objectType == NumTypeLong){
                        value = (double)StarbytesNumGetLongValue(object);
                    }
                    else {
                        value = (double)StarbytesNumGetIntValue(object);
                    }
                    StarbytesObjectRelease(object);
                    return StarbytesNumNew(NumTypeDouble,value);
                }
                if(StarbytesObjectTypecheck(object,StarbytesBoolType())){
                    value = (bool)StarbytesBoolValue(object)? 1.0 : 0.0;
                    StarbytesObjectRelease(object);
                    return StarbytesNumNew(NumTypeDouble,value);
                }
                if(StarbytesObjectTypecheck(object,StarbytesStrType())){
                    auto text = StarbytesStrGetBuffer(object);
                    if(!parseDoubleStrict(text? text : "",value)){
                        return failCast("Double cast failed: string is not a valid float");
                    }
                    StarbytesObjectRelease(object);
                    return StarbytesNumNew(NumTypeDouble,value);
                }
                return failCast("Double cast failed: unsupported source type");
            }

            if(!StarbytesObjectIs(object)){
                auto classType = StarbytesClassObjectGetClass(object);
                auto *classMeta = findClassByType(classType);
                string_set visited;
                while(classMeta){
                    auto className = rtidToString(classMeta->name);
                    if(visited.find(className) != visited.end()){
                        break;
                    }
                    visited.insert(className);
                    if(className == targetType){
                        return object;
                    }
                    if(!classMeta->hasSuperClass){
                        break;
                    }
                    auto superName = rtidToString(classMeta->superClassName);
                    classMeta = findClassByName(string_ref(superName));
                }
                std::ostringstream ss;
                ss << "Class cast failed: object is not compatible with `" << targetType << "`";
                return failCast(ss.str());
            }

            std::ostringstream ss;
            ss << "Class cast failed: source value is not a class object for target `" << targetType << "`";
            return failCast(ss.str());
        }
        case CODE_RTTERNARY: {
            auto condition = evalExpr(in);
            if(!condition){
                skipExpr(in);
                skipExpr(in);
                return nullptr;
            }
            if(!StarbytesObjectTypecheck(condition,StarbytesBoolType())){
                StarbytesObjectRelease(condition);
                skipExpr(in);
                skipExpr(in);
                lastRuntimeError = "ternary condition must be Bool";
                return nullptr;
            }
            bool chooseTrue = (bool)StarbytesBoolValue(condition);
            StarbytesObjectRelease(condition);
            if(chooseTrue){
                auto trueValue = evalExpr(in);
                skipExpr(in);
                return trueValue;
            }
            skipExpr(in);
            return evalExpr(in);
        }
        case CODE_RTINDEX_GET: {
            auto collection = evalExpr(in);
            auto index = evalExpr(in);
            if(!collection || !index){
                if(collection){
                    StarbytesObjectRelease(collection);
                }
                if(index){
                    StarbytesObjectRelease(index);
                }
                return nullptr;
            }
            StarbytesObject result = nullptr;
            if(StarbytesObjectTypecheck(collection,StarbytesArrayType()) &&
               StarbytesObjectTypecheck(index,StarbytesNumType())){
                int idx = -1;
                auto indexType = StarbytesNumGetType(index);
                if(indexType == NumTypeInt){
                    idx = StarbytesNumGetIntValue(index);
                }
                else if(indexType == NumTypeLong){
                    idx = (int)StarbytesNumGetLongValue(index);
                }
                if(idx >= 0 && (unsigned)idx < StarbytesArrayGetLength(collection)){
                    result = StarbytesArrayIndex(collection,(unsigned)idx);
                }
            }
            else if(StarbytesObjectTypecheck(collection,StarbytesDictType()) &&
                    (StarbytesObjectTypecheck(index,StarbytesStrType()) || StarbytesObjectTypecheck(index,StarbytesNumType()))){
                result = StarbytesDictGet(collection,index);
            }
            if(result){
                StarbytesObjectReference(result);
            }
            StarbytesObjectRelease(collection);
            StarbytesObjectRelease(index);
            return result;
        }
        case CODE_RTTYPED_INDEX_GET: {
            RTTypedNumericKind kind = RTTYPED_NUM_OBJECT;
            in.read((char *)&kind,sizeof(kind));
            auto collection = evalExpr(in);
            auto index = evalExpr(in);
            if(!collection || !index){
                if(collection){
                    StarbytesObjectRelease(collection);
                }
                if(index){
                    StarbytesObjectRelease(index);
                }
                return nullptr;
            }
            StarbytesObject result = nullptr;
            if(StarbytesObjectTypecheck(collection,StarbytesArrayType()) &&
               StarbytesObjectTypecheck(index,StarbytesNumType())){
                int idx = -1;
                auto indexType = StarbytesNumGetType(index);
                if(indexType == NumTypeInt){
                    idx = StarbytesNumGetIntValue(index);
                }
                else if(indexType == NumTypeLong){
                    idx = (int)StarbytesNumGetLongValue(index);
                }
                if(idx >= 0 && (unsigned)idx < StarbytesArrayGetLength(collection)){
                    long double value = 0.0;
                    if(StarbytesArrayTryGetNumeric(collection,(unsigned)idx,numTypeFromTypedKind(kind),&value)){
                        result = makeNumber(value,numTypeFromTypedKind(kind));
                    }
                }
            }
            StarbytesObjectRelease(collection);
            StarbytesObjectRelease(index);
            return result;
        }
        case CODE_RTINDEX_SET: {
            auto collection = evalExpr(in);
            auto index = evalExpr(in);
            auto value = evalExpr(in);
            if(!collection || !index){
                if(collection){
                    StarbytesObjectRelease(collection);
                }
                if(index){
                    StarbytesObjectRelease(index);
                }
                if(value){
                    StarbytesObjectRelease(value);
                }
                return nullptr;
            }
            if(!value){
                value = StarbytesBoolNew((StarbytesBoolVal)false);
            }
            bool success = false;
            if(StarbytesObjectTypecheck(collection,StarbytesArrayType()) &&
               StarbytesObjectTypecheck(index,StarbytesNumType())){
                int idx = -1;
                auto indexType = StarbytesNumGetType(index);
                if(indexType == NumTypeInt){
                    idx = StarbytesNumGetIntValue(index);
                }
                else if(indexType == NumTypeLong){
                    idx = (int)StarbytesNumGetLongValue(index);
                }
                if(idx >= 0 && (unsigned)idx < StarbytesArrayGetLength(collection)){
                    StarbytesArraySet(collection,(unsigned)idx,value);
                    success = true;
                }
            }
            else if(StarbytesObjectTypecheck(collection,StarbytesDictType()) &&
                    (StarbytesObjectTypecheck(index,StarbytesStrType()) || StarbytesObjectTypecheck(index,StarbytesNumType()))){
                StarbytesDictSet(collection,index,value);
                success = true;
            }
            StarbytesObjectRelease(collection);
            StarbytesObjectRelease(index);
            if(!success){
                StarbytesObjectRelease(value);
                return nullptr;
            }
            return value;
        }
        case CODE_RTTYPED_INDEX_SET: {
            RTTypedNumericKind kind = RTTYPED_NUM_OBJECT;
            in.read((char *)&kind,sizeof(kind));
            auto collection = evalExpr(in);
            auto index = evalExpr(in);
            auto value = evalExpr(in);
            if(!collection || !index){
                if(collection){
                    StarbytesObjectRelease(collection);
                }
                if(index){
                    StarbytesObjectRelease(index);
                }
                if(value){
                    StarbytesObjectRelease(value);
                }
                return nullptr;
            }
            if(!value){
                value = StarbytesBoolNew((StarbytesBoolVal)false);
            }
            bool success = false;
            if(StarbytesObjectTypecheck(collection,StarbytesArrayType()) &&
               StarbytesObjectTypecheck(index,StarbytesNumType())){
                int idx = -1;
                auto indexType = StarbytesNumGetType(index);
                if(indexType == NumTypeInt){
                    idx = StarbytesNumGetIntValue(index);
                }
                else if(indexType == NumTypeLong){
                    idx = (int)StarbytesNumGetLongValue(index);
                }
                if(idx >= 0 && (unsigned)idx < StarbytesArrayGetLength(collection)){
                    long double numericValue = 0.0;
                    if(extractTypedNumericValue(value,kind,numericValue)){
                        success = StarbytesArrayTrySetNumeric(collection,(unsigned)idx,numTypeFromTypedKind(kind),numericValue) != 0;
                    }
                }
            }
            StarbytesObjectRelease(collection);
            StarbytesObjectRelease(index);
            if(!success){
                StarbytesObjectRelease(value);
                return nullptr;
            }
            StarbytesObjectReference(value);
            return value;
        }
        case CODE_RTNEWOBJ : {
            RTID classId;
            in >> &classId;
            std::string className = rtidToString(classId);

            unsigned argCount = 0;
            in.read((char *)&argCount,sizeof(argCount));

            auto foundClassType = classTypeByName.find(className);
            StarbytesClassType classType;
            if(foundClassType == classTypeByName.end()){
                classType = StarbytesMakeClass(className.c_str());
                classTypeByName.insert(std::make_pair(className,classType));
                runtimeClassRegistry.insert(std::make_pair(classType,className));
            }
            else {
                classType = foundClassType->second;
            }

            StarbytesObject instance = StarbytesClassObjectNew(classType);
            auto *classMeta = findClassByType(classType);
            if(classMeta){
                std::vector<RTClass *> hierarchy;
                if(!buildClassHierarchy(classMeta,hierarchy)){
                    hierarchy.push_back(classMeta);
                }

                for(auto *classInChain : hierarchy){
                    for(auto &field : classInChain->fields){
                        std::string fieldName = rtidToString(field.id);
                        auto existingProperty = StarbytesObjectGetProperty(instance,fieldName.c_str());
                        if(existingProperty){
                            continue;
                        }
                        StarbytesObjectAddProperty(instance,const_cast<char *>(fieldName.c_str()),StarbytesBoolNew(StarbytesBoolFalse));
                    }
                }

                for(auto *classInChain : hierarchy){
                    if(classInChain->hasFieldInitFunc){
                        std::string fieldInitName = rtidToString(classInChain->fieldInitFuncName);
                        auto *fieldInitFunc = findFunctionByName(string_ref(fieldInitName));
                        if(fieldInitFunc){
                            auto initResult = invokeFunc(in,fieldInitFunc,0,instance);
                            if(initResult){
                                StarbytesObjectRelease(initResult);
                            }
                        }
                    }
                }

                if(classMeta->constructors.empty()){
                    if(argCount > 0){
                        discardExprArgs(in,argCount);
                    }
                    return instance;
                }

                RTFuncTemplate *ctorMeta = nullptr;
                for(auto &ctorTemplate : classMeta->constructors){
                    if(ctorTemplate.argsTemplate.size() == argCount){
                        ctorMeta = &ctorTemplate;
                        break;
                    }
                }
                if(!ctorMeta){
                    discardExprArgs(in,argCount);
                    return instance;
                }

                std::string ctorName = rtidToString(ctorMeta->name);
                std::string mangledCtorName = mangleClassMethodName(className,ctorName);
                auto *runtimeCtor = findFunctionByName(string_ref(mangledCtorName));
                if(!runtimeCtor){
                    discardExprArgs(in,argCount);
                    return instance;
                }

                auto ctorResult = invokeFunc(in,runtimeCtor,argCount,instance);
                if(ctorResult){
                    StarbytesObjectRelease(ctorResult);
                }
                return instance;
            }
            discardExprArgs(in,argCount);
            return instance;
        }
        case CODE_RTMEMBER_GET : {
            auto object = evalExpr(in);
            RTID memberId;
            in >> &memberId;
            std::string memberName = rtidToString(memberId);

            StarbytesObject value = nullptr;
            if(object){
                value = StarbytesObjectGetProperty(object,memberName.c_str());
                if(value){
                    StarbytesObjectReference(value);
                }
                StarbytesObjectRelease(object);
            }
            if(value){
                return value;
            }
            return StarbytesBoolNew(StarbytesBoolFalse);
        }
        case CODE_RTMEMBER_SET : {
            auto object = evalExpr(in);
            RTID memberId;
            in >> &memberId;
            auto value = evalExpr(in);
            if(!value){
                value = StarbytesBoolNew(StarbytesBoolFalse);
            }
            if(!object){
                StarbytesObjectRelease(value);
                return nullptr;
            }

            std::string memberName = rtidToString(memberId);
            bool found = false;
            unsigned propCount = StarbytesObjectGetPropertyCount(object);
            for(unsigned i = 0;i < propCount;i++){
                auto *prop = StarbytesObjectIndexProperty(object,i);
                if(std::strcmp(prop->name,memberName.c_str()) == 0){
                    if(prop->data){
                        StarbytesObjectRelease(prop->data);
                    }
                    StarbytesObjectReference(value);
                    prop->data = value;
                    found = true;
                    break;
                }
            }

            if(!found){
                StarbytesObjectReference(value);
                StarbytesObjectAddProperty(object,const_cast<char *>(memberName.c_str()),value);
            }
            StarbytesObjectRelease(object);
            return value;
        }
        case CODE_RTMEMBER_IVK : {
            auto object = evalExpr(in);
            RTID memberId;
            in >> &memberId;
            unsigned argCount = 0;
            in.read((char *)&argCount,sizeof(argCount));
            if(!object){
                discardExprArgs(in,argCount);
                return nullptr;
            }

            std::string methodName = rtidToString(memberId);
            unsigned consumedArgs = 0;
            auto collectArgs = [&](std::vector<StarbytesObject> &args) -> bool {
                consumedArgs = 0;
                args.clear();
                args.reserve(argCount);
                for(unsigned i = 0;i < argCount;++i){
                    auto arg = evalExpr(in);
                    ++consumedArgs;
                    if(!arg){
                        for(auto *existing : args){
                            if(existing){
                                StarbytesObjectRelease(existing);
                            }
                        }
                        args.clear();
                        return false;
                    }
                    args.push_back(arg);
                }
                return true;
            };
            auto releaseArgs = [&](std::vector<StarbytesObject> &args){
                for(auto *arg : args){
                    if(arg){
                        StarbytesObjectRelease(arg);
                    }
                }
                args.clear();
            };
            auto discardRemainingArgs = [&](){
                if(consumedArgs < argCount){
                    discardExprArgs(in,argCount - consumedArgs);
                    consumedArgs = argCount;
                }
            };
            auto expectIntArg = [&](StarbytesObject arg,int &value) -> bool {
                if(!arg || !StarbytesObjectTypecheck(arg,StarbytesNumType())){
                    return false;
                }
                auto argType = StarbytesNumGetType(arg);
                if(argType == NumTypeInt){
                    value = StarbytesNumGetIntValue(arg);
                    return true;
                }
                if(argType == NumTypeLong){
                    value = (int)StarbytesNumGetLongValue(arg);
                    return true;
                }
                return false;
            };
            auto expectStringArg = [&](StarbytesObject arg,std::string &value) -> bool {
                if(!arg || !StarbytesObjectTypecheck(arg,StarbytesStrType())){
                    return false;
                }
                value = StarbytesStrGetBuffer(arg);
                return true;
            };

            if(StarbytesObjectTypecheck(object,StarbytesStrType()) ||
               StarbytesObjectTypecheck(object,StarbytesRegexType()) ||
               StarbytesObjectTypecheck(object,StarbytesArrayType()) ||
               StarbytesObjectTypecheck(object,StarbytesDictType())){
                std::vector<StarbytesObject> args;
                auto failWithArgs = [&](const std::string &message) -> StarbytesObject {
                    discardRemainingArgs();
                    if(!message.empty()){
                        lastRuntimeError = message;
                    }
                    releaseArgs(args);
                    StarbytesObjectRelease(object);
                    return nullptr;
                };

                if(StarbytesObjectTypecheck(object,StarbytesStrType())){
                    auto source = std::string(StarbytesStrGetBuffer(object));
                    auto length = (int)source.size();

                    if(methodName == "isEmpty"){
                        if(argCount != 0){
                            return failWithArgs("String.isEmpty expects 0 arguments");
                        }
                        StarbytesObjectRelease(object);
                        return StarbytesBoolNew((StarbytesBoolVal)source.empty());
                    }
                    if(methodName == "at"){
                        if(argCount != 1 || !collectArgs(args)){
                            return failWithArgs("String.at expects 1 argument");
                        }
                        int index = 0;
                        if(!expectIntArg(args[0],index)){
                            return failWithArgs("String.at expects Int index");
                        }
                        if(index < 0 || index >= length){
                            return failWithArgs("String.at index out of range");
                        }
                        std::string ch(1,source[(size_t)index]);
                        releaseArgs(args);
                        StarbytesObjectRelease(object);
                        return StarbytesStrNewWithData(ch.c_str());
                    }
                    if(methodName == "slice"){
                        if(argCount != 2 || !collectArgs(args)){
                            return failWithArgs("String.slice expects 2 arguments");
                        }
                        int start = 0;
                        int end = 0;
                        if(!expectIntArg(args[0],start) || !expectIntArg(args[1],end)){
                            return failWithArgs("String.slice expects Int arguments");
                        }
                        start = clampSliceBound(start,length);
                        end = clampSliceBound(end,length);
                        if(end < start){
                            end = start;
                        }
                        auto out = source.substr((size_t)start,(size_t)(end - start));
                        releaseArgs(args);
                        StarbytesObjectRelease(object);
                        return StarbytesStrNewWithData(out.c_str());
                    }
                    if(methodName == "contains" || methodName == "startsWith" || methodName == "endsWith" ||
                       methodName == "indexOf" || methodName == "lastIndexOf"){
                        if(argCount != 1 || !collectArgs(args)){
                            return failWithArgs("String method expects 1 argument");
                        }
                        std::string needle;
                        if(!expectStringArg(args[0],needle)){
                            return failWithArgs("String method expects String argument");
                        }
                        bool boolResult = false;
                        int intResult = -1;
                        if(methodName == "contains"){
                            boolResult = source.find(needle) != std::string::npos;
                        }
                        else if(methodName == "startsWith"){
                            boolResult = source.rfind(needle,0) == 0;
                        }
                        else if(methodName == "endsWith"){
                            boolResult = needle.size() <= source.size() &&
                                source.compare(source.size() - needle.size(),needle.size(),needle) == 0;
                        }
                        else if(methodName == "indexOf"){
                            auto pos = source.find(needle);
                            intResult = (pos == std::string::npos)? -1 : (int)pos;
                        }
                        else {
                            auto pos = source.rfind(needle);
                            intResult = (pos == std::string::npos)? -1 : (int)pos;
                        }
                        releaseArgs(args);
                        StarbytesObjectRelease(object);
                        if(methodName == "indexOf" || methodName == "lastIndexOf"){
                            return StarbytesNumNew(NumTypeInt,intResult);
                        }
                        return StarbytesBoolNew((StarbytesBoolVal)boolResult);
                    }
                    if(methodName == "lower" || methodName == "upper" || methodName == "trim"){
                        if(argCount != 0){
                            return failWithArgs("String method expects 0 arguments");
                        }
                        std::string out = source;
                        if(methodName == "lower"){
                            std::transform(out.begin(),out.end(),out.begin(),[](unsigned char c){ return (char)std::tolower(c); });
                        }
                        else if(methodName == "upper"){
                            std::transform(out.begin(),out.end(),out.begin(),[](unsigned char c){ return (char)std::toupper(c); });
                        }
                        else {
                            out = stringTrim(source);
                        }
                        StarbytesObjectRelease(object);
                        return StarbytesStrNewWithData(out.c_str());
                    }
                    if(methodName == "replace"){
                        if(argCount != 2 || !collectArgs(args)){
                            return failWithArgs("String.replace expects 2 arguments");
                        }
                        std::string oldValue;
                        std::string newValue;
                        if(!expectStringArg(args[0],oldValue) || !expectStringArg(args[1],newValue)){
                            return failWithArgs("String.replace expects String arguments");
                        }
                        std::string out = source;
                        if(!oldValue.empty()){
                            size_t start = 0;
                            while((start = out.find(oldValue,start)) != std::string::npos){
                                out.replace(start,oldValue.size(),newValue);
                                start += newValue.size();
                            }
                        }
                        releaseArgs(args);
                        StarbytesObjectRelease(object);
                        return StarbytesStrNewWithData(out.c_str());
                    }
                    if(methodName == "split"){
                        if(argCount != 1 || !collectArgs(args)){
                            return failWithArgs("String.split expects 1 argument");
                        }
                        std::string sep;
                        if(!expectStringArg(args[0],sep)){
                            return failWithArgs("String.split expects String separator");
                        }
                        auto out = StarbytesArrayNew();
                        if(sep.empty()){
                            for(char c : source){
                                std::string part(1,c);
                                auto strObj = StarbytesStrNewWithData(part.c_str());
                                StarbytesArrayPush(out,strObj);
                                StarbytesObjectRelease(strObj);
                            }
                        }
                        else {
                            size_t start = 0;
                            while(true){
                                auto pos = source.find(sep,start);
                                std::string part;
                                if(pos == std::string::npos){
                                    part = source.substr(start);
                                }
                                else {
                                    part = source.substr(start,pos - start);
                                }
                                auto strObj = StarbytesStrNewWithData(part.c_str());
                                StarbytesArrayPush(out,strObj);
                                StarbytesObjectRelease(strObj);
                                if(pos == std::string::npos){
                                    break;
                                }
                                start = pos + sep.size();
                            }
                        }
                        releaseArgs(args);
                        StarbytesObjectRelease(object);
                        return out;
                    }
                    if(methodName == "repeat"){
                        if(argCount != 1 || !collectArgs(args)){
                            return failWithArgs("String.repeat expects 1 argument");
                        }
                        int count = 0;
                        if(!expectIntArg(args[0],count)){
                            return failWithArgs("String.repeat expects Int count");
                        }
                        if(count < 0){
                            count = 0;
                        }
                        std::string out;
                        out.reserve(source.size() * (size_t)count);
                        for(int i = 0;i < count;++i){
                            out += source;
                        }
                        releaseArgs(args);
                        StarbytesObjectRelease(object);
                        return StarbytesStrNewWithData(out.c_str());
                    }
                    discardExprArgs(in,argCount);
                    StarbytesObjectRelease(object);
                    return nullptr;
                }

                if(StarbytesObjectTypecheck(object,StarbytesRegexType())){
                    if(methodName == "match"){
                        if(argCount != 1 || !collectArgs(args)){
                            return failWithArgs("Regex.match expects 1 argument");
                        }
                        std::string text;
                        if(!expectStringArg(args[0],text)){
                            return failWithArgs("Regex.match expects String text");
                        }
                        std::string error;
                        auto result = regex::match(object,text,error);
                        releaseArgs(args);
                        StarbytesObjectRelease(object);
                        if(!result){
                            if(!error.empty()){
                                lastRuntimeError = error;
                            }
                            return nullptr;
                        }
                        return result;
                    }
                    if(methodName == "findAll"){
                        if(argCount != 1 || !collectArgs(args)){
                            return failWithArgs("Regex.findAll expects 1 argument");
                        }
                        std::string text;
                        if(!expectStringArg(args[0],text)){
                            return failWithArgs("Regex.findAll expects String text");
                        }
                        std::string error;
                        auto result = regex::findAll(object,text,error);
                        releaseArgs(args);
                        StarbytesObjectRelease(object);
                        if(!result){
                            if(!error.empty()){
                                lastRuntimeError = error;
                            }
                            return nullptr;
                        }
                        return result;
                    }
                    if(methodName == "replace"){
                        if(argCount != 2 || !collectArgs(args)){
                            return failWithArgs("Regex.replace expects 2 arguments");
                        }
                        std::string text;
                        std::string replacement;
                        if(!expectStringArg(args[0],text) || !expectStringArg(args[1],replacement)){
                            return failWithArgs("Regex.replace expects String arguments");
                        }
                        std::string error;
                        auto result = regex::replace(object,text,replacement,error);
                        releaseArgs(args);
                        StarbytesObjectRelease(object);
                        if(!result){
                            if(!error.empty()){
                                lastRuntimeError = error;
                            }
                            return nullptr;
                        }
                        return result;
                    }
                    discardExprArgs(in,argCount);
                    StarbytesObjectRelease(object);
                    return nullptr;
                }

                if(StarbytesObjectTypecheck(object,StarbytesArrayType())){
                    auto arrayLen = StarbytesArrayGetLength(object);
                    if(methodName == "isEmpty"){
                        if(argCount != 0){
                            return failWithArgs("Array.isEmpty expects 0 arguments");
                        }
                        StarbytesObjectRelease(object);
                        return StarbytesBoolNew((StarbytesBoolVal)(arrayLen == 0));
                    }
                    if(methodName == "push"){
                        if(argCount != 1 || !collectArgs(args)){
                            return failWithArgs("Array.push expects 1 argument");
                        }
                        StarbytesArrayPush(object,args[0]);
                        auto nextLen = (int)StarbytesArrayGetLength(object);
                        releaseArgs(args);
                        StarbytesObjectRelease(object);
                        return StarbytesNumNew(NumTypeInt,nextLen);
                    }
                    if(methodName == "pop"){
                        if(argCount != 0){
                            return failWithArgs("Array.pop expects 0 arguments");
                        }
                        if(arrayLen == 0){
                            return failWithArgs("Array.pop on empty array");
                        }
                        auto value = StarbytesArrayIndex(object,arrayLen - 1);
                        if(value){
                            StarbytesObjectReference(value);
                        }
                        StarbytesArrayPop(object);
                        StarbytesObjectRelease(object);
                        return value;
                    }
                    if(methodName == "at"){
                        if(argCount != 1 || !collectArgs(args)){
                            return failWithArgs("Array.at expects 1 argument");
                        }
                        int index = 0;
                        if(!expectIntArg(args[0],index)){
                            return failWithArgs("Array.at expects Int index");
                        }
                        if(index < 0 || (unsigned)index >= arrayLen){
                            return failWithArgs("Array.at index out of range");
                        }
                        auto value = StarbytesArrayIndex(object,(unsigned)index);
                        if(value){
                            StarbytesObjectReference(value);
                        }
                        releaseArgs(args);
                        StarbytesObjectRelease(object);
                        return value;
                    }
                    if(methodName == "set"){
                        if(argCount != 2 || !collectArgs(args)){
                            return failWithArgs("Array.set expects 2 arguments");
                        }
                        int index = 0;
                        if(!expectIntArg(args[0],index)){
                            return failWithArgs("Array.set expects Int index");
                        }
                        bool ok = index >= 0 && (unsigned)index < arrayLen;
                        if(ok){
                            StarbytesArraySet(object,(unsigned)index,args[1]);
                        }
                        releaseArgs(args);
                        StarbytesObjectRelease(object);
                        return StarbytesBoolNew((StarbytesBoolVal)ok);
                    }
                    if(methodName == "insert"){
                        if(argCount != 2 || !collectArgs(args)){
                            return failWithArgs("Array.insert expects 2 arguments");
                        }
                        int index = 0;
                        if(!expectIntArg(args[0],index)){
                            return failWithArgs("Array.insert expects Int index");
                        }
                        bool ok = index >= 0 && (unsigned)index <= arrayLen;
                        if(ok){
                            StarbytesArrayPush(object,args[1]);
                            for(unsigned pos = arrayLen; pos > (unsigned)index; --pos){
                                auto shifted = StarbytesArrayIndex(object,pos - 1);
                                StarbytesArraySet(object,pos,shifted);
                            }
                            StarbytesArraySet(object,(unsigned)index,args[1]);
                        }
                        releaseArgs(args);
                        StarbytesObjectRelease(object);
                        return StarbytesBoolNew((StarbytesBoolVal)ok);
                    }
                    if(methodName == "removeAt"){
                        if(argCount != 1 || !collectArgs(args)){
                            return failWithArgs("Array.removeAt expects 1 argument");
                        }
                        int index = 0;
                        if(!expectIntArg(args[0],index)){
                            return failWithArgs("Array.removeAt expects Int index");
                        }
                        if(index < 0 || (unsigned)index >= arrayLen){
                            return failWithArgs("Array.removeAt index out of range");
                        }
                        auto removed = StarbytesArrayIndex(object,(unsigned)index);
                        if(removed){
                            StarbytesObjectReference(removed);
                        }
                        for(unsigned pos = (unsigned)index; pos + 1 < arrayLen; ++pos){
                            auto shifted = StarbytesArrayIndex(object,pos + 1);
                            StarbytesArraySet(object,pos,shifted);
                        }
                        StarbytesArrayPop(object);
                        releaseArgs(args);
                        StarbytesObjectRelease(object);
                        return removed;
                    }
                    if(methodName == "clear"){
                        if(argCount != 0){
                            return failWithArgs("Array.clear expects 0 arguments");
                        }
                        while(StarbytesArrayGetLength(object) > 0){
                            StarbytesArrayPop(object);
                        }
                        StarbytesObjectRelease(object);
                        return StarbytesBoolNew((StarbytesBoolVal)true);
                    }
                    if(methodName == "contains" || methodName == "indexOf"){
                        if(argCount != 1 || !collectArgs(args)){
                            return failWithArgs("Array method expects 1 argument");
                        }
                        int foundIndex = -1;
                        auto len = StarbytesArrayGetLength(object);
                        for(unsigned i = 0;i < len;++i){
                            auto value = StarbytesArrayIndex(object,i);
                            if(runtimeObjectEquals(value,args[0])){
                                foundIndex = (int)i;
                                break;
                            }
                        }
                        releaseArgs(args);
                        StarbytesObjectRelease(object);
                        if(methodName == "indexOf"){
                            return StarbytesNumNew(NumTypeInt,foundIndex);
                        }
                        return StarbytesBoolNew((StarbytesBoolVal)(foundIndex >= 0));
                    }
                    if(methodName == "slice"){
                        if(argCount != 2 || !collectArgs(args)){
                            return failWithArgs("Array.slice expects 2 arguments");
                        }
                        int start = 0;
                        int end = 0;
                        if(!expectIntArg(args[0],start) || !expectIntArg(args[1],end)){
                            return failWithArgs("Array.slice expects Int arguments");
                        }
                        int lenInt = (int)arrayLen;
                        start = clampSliceBound(start,lenInt);
                        end = clampSliceBound(end,lenInt);
                        if(end < start){
                            end = start;
                        }
                        auto out = StarbytesArrayNew();
                        StarbytesArrayReserve(out,(unsigned)(end - start));
                        for(int i = start;i < end;++i){
                            auto value = StarbytesArrayIndex(object,(unsigned)i);
                            StarbytesArrayPush(out,value);
                        }
                        releaseArgs(args);
                        StarbytesObjectRelease(object);
                        return out;
                    }
                    if(methodName == "join"){
                        if(argCount != 1 || !collectArgs(args)){
                            return failWithArgs("Array.join expects 1 argument");
                        }
                        std::string sep;
                        if(!expectStringArg(args[0],sep)){
                            return failWithArgs("Array.join expects String separator");
                        }
                        std::string out;
                        auto len = StarbytesArrayGetLength(object);
                        for(unsigned i = 0;i < len;++i){
                            if(i > 0){
                                out += sep;
                            }
                            out += objectToString(StarbytesArrayIndex(object,i));
                        }
                        releaseArgs(args);
                        StarbytesObjectRelease(object);
                        return StarbytesStrNewWithData(out.c_str());
                    }
                    if(methodName == "copy"){
                        if(argCount != 0){
                            return failWithArgs("Array.copy expects 0 arguments");
                        }
                        auto out = StarbytesArrayCopy(object);
                        StarbytesObjectRelease(object);
                        return out;
                    }
                    if(methodName == "reverse"){
                        if(argCount != 0){
                            return failWithArgs("Array.reverse expects 0 arguments");
                        }
                        auto out = StarbytesArrayNew();
                        auto len = StarbytesArrayGetLength(object);
                        StarbytesArrayReserve(out,len);
                        for(unsigned i = len;i > 0;--i){
                            StarbytesArrayPush(out,StarbytesArrayIndex(object,i - 1));
                        }
                        StarbytesObjectRelease(object);
                        return out;
                    }
                    discardExprArgs(in,argCount);
                    StarbytesObjectRelease(object);
                    return nullptr;
                }

                if(StarbytesObjectTypecheck(object,StarbytesDictType())){
                    auto dictLen = StarbytesObjectGetProperty(object,"length");
                    auto keys = StarbytesObjectGetProperty(object,"keys");
                    auto values = StarbytesObjectGetProperty(object,"values");
                    if(methodName == "isEmpty"){
                        if(argCount != 0){
                            return failWithArgs("Dict.isEmpty expects 0 arguments");
                        }
                        bool isEmpty = !dictLen || StarbytesNumGetIntValue(dictLen) == 0;
                        StarbytesObjectRelease(object);
                        return StarbytesBoolNew((StarbytesBoolVal)isEmpty);
                    }
                    if(methodName == "has" || methodName == "get" || methodName == "remove"){
                        if(argCount != 1 || !collectArgs(args)){
                            return failWithArgs("Dict method expects 1 argument");
                        }
                        if(!isDictKeyObject(args[0])){
                            return failWithArgs("Dict key must be String/Int/Long/Float/Double");
                        }
                        if(methodName == "has"){
                            auto value = StarbytesDictGet(object,args[0]);
                            releaseArgs(args);
                            StarbytesObjectRelease(object);
                            return StarbytesBoolNew((StarbytesBoolVal)(value != nullptr));
                        }
                        if(methodName == "get"){
                            auto value = StarbytesDictGet(object,args[0]);
                            if(value){
                                StarbytesObjectReference(value);
                            }
                            releaseArgs(args);
                            StarbytesObjectRelease(object);
                            return value;
                        }
                        unsigned len = keys ? StarbytesArrayGetLength(keys) : 0;
                        int found = -1;
                        for(unsigned i = 0;i < len;++i){
                            auto keyAt = StarbytesArrayIndex(keys,i);
                            if(runtimeObjectEquals(keyAt,args[0])){
                                found = (int)i;
                                break;
                            }
                        }
                        if(found < 0){
                            return failWithArgs("Dict key not found");
                        }
                        auto removed = StarbytesArrayIndex(values,(unsigned)found);
                        if(removed){
                            StarbytesObjectReference(removed);
                        }
                        for(unsigned pos = (unsigned)found; pos + 1 < len; ++pos){
                            StarbytesArraySet(keys,pos,StarbytesArrayIndex(keys,pos + 1));
                            StarbytesArraySet(values,pos,StarbytesArrayIndex(values,pos + 1));
                        }
                        StarbytesArrayPop(keys);
                        StarbytesArrayPop(values);
                        if(dictLen){
                            StarbytesNumAssign(dictLen,NumTypeInt,(int)(len - 1));
                        }
                        releaseArgs(args);
                        StarbytesObjectRelease(object);
                        return removed;
                    }
                    if(methodName == "set"){
                        if(argCount != 2 || !collectArgs(args)){
                            return failWithArgs("Dict.set expects 2 arguments");
                        }
                        if(!isDictKeyObject(args[0])){
                            return failWithArgs("Dict key must be String/Int/Long/Float/Double");
                        }
                        StarbytesDictSet(object,args[0],args[1]);
                        releaseArgs(args);
                        StarbytesObjectRelease(object);
                        return StarbytesBoolNew((StarbytesBoolVal)true);
                    }
                    if(methodName == "keys"){
                        if(argCount != 0){
                            return failWithArgs("Dict.keys expects 0 arguments");
                        }
                        auto out = keys ? StarbytesArrayCopy(keys) : StarbytesArrayNew();
                        StarbytesObjectRelease(object);
                        return out;
                    }
                    if(methodName == "values"){
                        if(argCount != 0){
                            return failWithArgs("Dict.values expects 0 arguments");
                        }
                        auto out = values ? StarbytesArrayCopy(values) : StarbytesArrayNew();
                        StarbytesObjectRelease(object);
                        return out;
                    }
                    if(methodName == "clear"){
                        if(argCount != 0){
                            return failWithArgs("Dict.clear expects 0 arguments");
                        }
                        while(keys && StarbytesArrayGetLength(keys) > 0){
                            StarbytesArrayPop(keys);
                        }
                        while(values && StarbytesArrayGetLength(values) > 0){
                            StarbytesArrayPop(values);
                        }
                        if(dictLen){
                            StarbytesNumAssign(dictLen,NumTypeInt,0);
                        }
                        StarbytesObjectRelease(object);
                        return StarbytesBoolNew((StarbytesBoolVal)true);
                    }
                    if(methodName == "copy"){
                        if(argCount != 0){
                            return failWithArgs("Dict.copy expects 0 arguments");
                        }
                        auto out = StarbytesDictCopy(object);
                        StarbytesObjectRelease(object);
                        return out;
                    }
                    discardExprArgs(in,argCount);
                    StarbytesObjectRelease(object);
                    return nullptr;
                }
            }

            StarbytesClassType classType = StarbytesClassObjectGetClass(object);
            auto *classMeta = findClassByType(classType);
            if(!classMeta){
                StarbytesObjectRelease(object);
                discardExprArgs(in,argCount);
                return nullptr;
            }

            auto *methodOwner = findMethodOwnerInHierarchy(classMeta,methodName);
            if(!methodOwner){
                StarbytesObjectRelease(object);
                discardExprArgs(in,argCount);
                return nullptr;
            }

            std::string className = rtidToString(methodOwner->name);
            std::string mangledName = mangleClassMethodName(className,methodName);
            auto *runtimeMethod = findFunctionByName(string_ref(mangledName));
            if(!runtimeMethod){
                StarbytesObjectRelease(object);
                discardExprArgs(in,argCount);
                return nullptr;
            }

            StarbytesObject result = nullptr;
            if(runtimeMethod->isLazy){
                result = scheduleLazyCall(in,runtimeMethod,argCount,object);
            }
            else {
                result = invokeFunc(in,runtimeMethod,argCount,object);
            }
            StarbytesObjectRelease(object);
            return result;
            break;
        }
        case CODE_RTREGEX_LITERAL: {
            RTID patternId;
            RTID flagsId;
            in >> &patternId;
            in >> &flagsId;

            std::string pattern = rtidToString(patternId);
            std::string flags = rtidToString(flagsId);
            uint32_t options = regexCompileOptionsFromFlags(flags);
            int errCode = 0;
            PCRE2_SIZE errOffset = 0;
            pcre2_code_8 *compiled = pcre2_compile_8(
                (PCRE2_SPTR8)pattern.c_str(),
                PCRE2_ZERO_TERMINATED,
                options,
                &errCode,
                &errOffset,
                nullptr
            );
            if(!compiled){
                lastRuntimeError = "regex compile error";
                unsigned char buffer[256] = {0};
                int msgRc = pcre2_get_error_message_8(errCode,buffer,sizeof(buffer));
                if(msgRc > 0){
                    lastRuntimeError = std::string((char *)buffer,(size_t)msgRc);
                }
                else if(msgRc == 0 && buffer[0] != '\0'){
                    lastRuntimeError = std::string((char *)buffer);
                }

                if(!lastRuntimeError.empty()){
                    lastRuntimeError += " (offset ";
                    lastRuntimeError += std::to_string((size_t)errOffset);
                    lastRuntimeError += ")";
                }
                return nullptr;
            }
            pcre2_code_free_8(compiled);

            StarbytesObject regexObj = StarbytesObjectNew(StarbytesRegexType());
            StarbytesObjectAddProperty(regexObj,(char *)"pattern",StarbytesStrNewWithData(pattern.c_str()));
            StarbytesObjectAddProperty(regexObj,(char *)"flags",StarbytesStrNewWithData(flags.c_str()));
            lastRuntimeError.clear();
            return regexObj;
        }
        default:
            break;
    }
    return nullptr;
}

void InterpImpl::skipExprFromCode(std::istream &in,RTCode code){
    switch (code) {
        case CODE_RTOBJCREATE:
        case CODE_RTINTOBJCREATE: {
            StarbytesObject object = nullptr;
            in >> &object;
            if(object){
                StarbytesObjectRelease(object);
            }
            break;
        }
        case CODE_RTVAR_REF:
        case CODE_RTFUNC_REF: {
            RTID id;
            in >> &id;
            break;
        }
        case CODE_RTLOCAL_REF: {
            uint32_t slot = 0;
            in.read((char *)&slot,sizeof(slot));
            break;
        }
        case CODE_RTTYPED_LOCAL_REF: {
            RTTypedNumericKind kind = RTTYPED_NUM_OBJECT;
            uint32_t slot = 0;
            in.read((char *)&kind,sizeof(kind));
            in.read((char *)&slot,sizeof(slot));
            break;
        }
        case CODE_RTIVKFUNC: {
            skipExpr(in);
            unsigned argCount = 0;
            in.read((char *)&argCount,sizeof(argCount));
            while(argCount > 0){
                skipExpr(in);
                --argCount;
            }
            break;
        }
        case CODE_UNARY_OPERATOR: {
            RTCode unaryCode = UNARY_OP_NOT;
            in.read((char *)&unaryCode,sizeof(unaryCode));
            (void)unaryCode;
            skipExpr(in);
            break;
        }
        case CODE_BINARY_OPERATOR: {
            RTCode binaryCode = BINARY_OP_PLUS;
            in.read((char *)&binaryCode,sizeof(binaryCode));
            (void)binaryCode;
            skipExpr(in);
            skipExpr(in);
            break;
        }
        case CODE_RTTYPED_BINARY: {
            RTTypedNumericKind kind = RTTYPED_NUM_OBJECT;
            RTTypedBinaryOp op = RTTYPED_BINARY_ADD;
            in.read((char *)&kind,sizeof(kind));
            in.read((char *)&op,sizeof(op));
            skipExpr(in);
            skipExpr(in);
            break;
        }
        case CODE_RTTYPED_NEGATE: {
            RTTypedNumericKind kind = RTTYPED_NUM_OBJECT;
            in.read((char *)&kind,sizeof(kind));
            skipExpr(in);
            break;
        }
        case CODE_RTTYPED_COMPARE: {
            RTTypedNumericKind kind = RTTYPED_NUM_OBJECT;
            RTTypedCompareOp op = RTTYPED_COMPARE_EQ;
            in.read((char *)&kind,sizeof(kind));
            in.read((char *)&op,sizeof(op));
            skipExpr(in);
            skipExpr(in);
            break;
        }
        case CODE_RTNEWOBJ: {
            RTID classId;
            in >> &classId;
            unsigned argCount = 0;
            in.read((char *)&argCount,sizeof(argCount));
            while(argCount > 0){
                skipExpr(in);
                --argCount;
            }
            break;
        }
        case CODE_RTMEMBER_GET: {
            skipExpr(in);
            RTID memberId;
            in >> &memberId;
            break;
        }
        case CODE_RTMEMBER_SET: {
            skipExpr(in);
            RTID memberId;
            in >> &memberId;
            skipExpr(in);
            break;
        }
        case CODE_RTMEMBER_IVK: {
            skipExpr(in);
            RTID memberId;
            in >> &memberId;
            unsigned argCount = 0;
            in.read((char *)&argCount,sizeof(argCount));
            while(argCount > 0){
                skipExpr(in);
                --argCount;
            }
            break;
        }
        case CODE_RTREGEX_LITERAL: {
            RTID patternId;
            RTID flagsId;
            in >> &patternId;
            in >> &flagsId;
            break;
        }
        case CODE_RTVAR_SET: {
            RTID id;
            in >> &id;
            skipExpr(in);
            break;
        }
        case CODE_RTLOCAL_SET: {
            uint32_t slot = 0;
            in.read((char *)&slot,sizeof(slot));
            skipExpr(in);
            break;
        }
        case CODE_RTLOCAL_DECL: {
            uint32_t slot = 0;
            bool hasInitValue = false;
            in.read((char *)&slot,sizeof(slot));
            in.read((char *)&hasInitValue,sizeof(hasInitValue));
            if(hasInitValue){
                skipExpr(in);
            }
            break;
        }
        case CODE_RTSECURE_LOCAL_DECL: {
            uint32_t targetSlot = 0;
            bool hasCatchSlot = false;
            bool hasCatchType = false;
            in.read((char *)&targetSlot,sizeof(targetSlot));
            in.read((char *)&hasCatchSlot,sizeof(hasCatchSlot));
            if(hasCatchSlot){
                uint32_t catchSlot = 0;
                in.read((char *)&catchSlot,sizeof(catchSlot));
            }
            in.read((char *)&hasCatchType,sizeof(hasCatchType));
            if(hasCatchType){
                RTID catchTypeId;
                in >> &catchTypeId;
            }
            skipExpr(in);
            RTCode blockBegin = CODE_MODULE_END;
            in.read((char *)&blockBegin,sizeof(blockBegin));
            if(blockBegin == CODE_RTBLOCK_BEGIN){
                skipRuntimeBlock(in,CODE_RTBLOCK_END);
            }
            break;
        }
        case CODE_RTINDEX_GET: {
            skipExpr(in);
            skipExpr(in);
            break;
        }
        case CODE_RTTYPED_INDEX_GET: {
            RTTypedNumericKind kind = RTTYPED_NUM_OBJECT;
            in.read((char *)&kind,sizeof(kind));
            skipExpr(in);
            skipExpr(in);
            break;
        }
        case CODE_RTINDEX_SET: {
            skipExpr(in);
            skipExpr(in);
            skipExpr(in);
            break;
        }
        case CODE_RTTYPED_INDEX_SET: {
            RTTypedNumericKind kind = RTTYPED_NUM_OBJECT;
            in.read((char *)&kind,sizeof(kind));
            skipExpr(in);
            skipExpr(in);
            skipExpr(in);
            break;
        }
        case CODE_RTARRAY_LITERAL: {
            unsigned elementCount = 0;
            in.read((char *)&elementCount,sizeof(elementCount));
            while(elementCount > 0){
                skipExpr(in);
                --elementCount;
            }
            break;
        }
        case CODE_RTDICT_LITERAL: {
            unsigned pairCount = 0;
            in.read((char *)&pairCount,sizeof(pairCount));
            while(pairCount > 0){
                skipExpr(in);
                skipExpr(in);
                --pairCount;
            }
            break;
        }
        case CODE_RTTYPECHECK: {
            skipExpr(in);
            RTID typeId;
            in >> &typeId;
            break;
        }
        case CODE_RTTERNARY: {
            skipExpr(in);
            skipExpr(in);
            skipExpr(in);
            break;
        }
        case CODE_RTCAST: {
            skipExpr(in);
            RTID typeId;
            in >> &typeId;
            break;
        }
        case CODE_RTTYPED_INTRINSIC: {
            RTTypedNumericKind kind = RTTYPED_NUM_OBJECT;
            RTTypedIntrinsicOp op = RTTYPED_INTRINSIC_SQRT;
            in.read((char *)&kind,sizeof(kind));
            in.read((char *)&op,sizeof(op));
            skipExpr(in);
            break;
        }
        default:
            break;
    }
}

void InterpImpl::skipExpr(std::istream &in){
    RTCode exprCode = CODE_MODULE_END;
    if(!in.read((char *)&exprCode,sizeof(RTCode))){
        return;
    }
    skipExprFromCode(in,exprCode);
}

void InterpImpl::skipRuntimeStmt(std::istream &in,RTCode code){
    if(code == CODE_RTVAR){
        RTVar var;
        in >> &var;
        if(var.hasInitValue){
            skipExpr(in);
        }
        return;
    }
    if(code == CODE_RTRETURN){
        bool hasValue = false;
        in.read((char *)&hasValue,sizeof(hasValue));
        if(hasValue){
            skipExpr(in);
        }
        return;
    }
    if(code == CODE_RTSECURE_DECL){
        RTID targetVarId;
        in >> &targetVarId;
        bool hasCatchBinding = false;
        in.read((char *)&hasCatchBinding,sizeof(hasCatchBinding));
        if(hasCatchBinding){
            RTID catchBindingId;
            in >> &catchBindingId;
        }
        bool hasCatchType = false;
        in.read((char *)&hasCatchType,sizeof(hasCatchType));
        if(hasCatchType){
            RTID catchTypeId;
            in >> &catchTypeId;
        }
        skipExpr(in);
        RTCode blockBegin = CODE_MODULE_END;
        in.read((char *)&blockBegin,sizeof(blockBegin));
        if(blockBegin == CODE_RTBLOCK_BEGIN){
            skipRuntimeBlock(in,CODE_RTBLOCK_END);
        }
        return;
    }
    if(code == CODE_RTLOCAL_DECL){
        uint32_t slot = 0;
        bool hasInitValue = false;
        in.read((char *)&slot,sizeof(slot));
        in.read((char *)&hasInitValue,sizeof(hasInitValue));
        if(hasInitValue){
            skipExpr(in);
        }
        return;
    }
    if(code == CODE_RTSECURE_LOCAL_DECL){
        uint32_t targetSlot = 0;
        bool hasCatchSlot = false;
        bool hasCatchType = false;
        in.read((char *)&targetSlot,sizeof(targetSlot));
        in.read((char *)&hasCatchSlot,sizeof(hasCatchSlot));
        if(hasCatchSlot){
            uint32_t catchSlot = 0;
            in.read((char *)&catchSlot,sizeof(catchSlot));
        }
        in.read((char *)&hasCatchType,sizeof(hasCatchType));
        if(hasCatchType){
            RTID catchTypeId;
            in >> &catchTypeId;
        }
        skipExpr(in);
        RTCode blockBegin = CODE_MODULE_END;
        in.read((char *)&blockBegin,sizeof(blockBegin));
        if(blockBegin == CODE_RTBLOCK_BEGIN){
            skipRuntimeBlock(in,CODE_RTBLOCK_END);
        }
        return;
    }
    if(code == CODE_CONDITIONAL){
        unsigned condSpecCount = 0;
        in.read((char *)&condSpecCount,sizeof(condSpecCount));
        while(condSpecCount > 0){
            RTCode specType = COND_TYPE_IF;
            in.read((char *)&specType,sizeof(specType));
            if(specType != COND_TYPE_ELSE){
                skipExpr(in);
            }
            RTCode blockBegin = CODE_MODULE_END;
            in.read((char *)&blockBegin,sizeof(blockBegin));
            if(blockBegin == CODE_RTBLOCK_BEGIN){
                skipRuntimeBlock(in,CODE_RTBLOCK_END);
            }
            else if(blockBegin == CODE_RTFUNCBLOCK_BEGIN){
                skipRuntimeBlock(in,CODE_RTFUNCBLOCK_END);
            }
            --condSpecCount;
        }
        RTCode endCode = CODE_MODULE_END;
        in.read((char *)&endCode,sizeof(endCode));
        return;
    }
    if(code == CODE_RTFUNC){
        RTFuncTemplate tempFunc;
        in >> &tempFunc;
        return;
    }
    if(code == CODE_RTCLASS_DEF){
        RTClass tempClass;
        in >> &tempClass;
        return;
    }
    skipExprFromCode(in,code);
}

void InterpImpl::skipRuntimeBlock(std::istream &in,RTCode endCode){
    RTCode currentCode = CODE_MODULE_END;
    if(!in.read((char *)&currentCode,sizeof(currentCode))){
        return;
    }
    while(in.good() && currentCode != endCode){
        skipRuntimeStmt(in,currentCode);
        if(!in.read((char *)&currentCode,sizeof(currentCode))){
            break;
        }
    }
}

void InterpImpl::executeRuntimeBlock(std::istream &in,RTCode endCode,bool *willReturn,StarbytesObject *return_val){
    RTCode currentCode = CODE_MODULE_END;
    if(!in.read((char *)&currentCode,sizeof(currentCode))){
        return;
    }
    while(in.good() && currentCode != endCode){
        if(!*willReturn){
            execNorm(currentCode,in,willReturn,return_val);
            processMicrotasks();
        }
        else {
            skipRuntimeStmt(in,currentCode);
        }
        if(!in.read((char *)&currentCode,sizeof(currentCode))){
            break;
        }
    }
}

void InterpImpl::execNorm(RTCode &code,std::istream &in,bool * willReturn,StarbytesObject *return_val){
    if(code == CODE_RTVAR){
        RTVar var;
        in >> &var;
        string_ref var_name (var.id.value,var.id.len);
//            std::cout << std::boolalpha << var.hasInitValue << std::endl;
//            std::cout << var_name.data() << std::endl;
        if(var.hasInitValue){
            auto val = evalExpr(in);
            allocator->allocVariable(var_name,val);
        }
        else {
            auto nativeValueName = resolveNativeValueName(&var);
            if(!nativeValueName.empty()){
                auto callback = findNativeValueCallback(string_ref(nativeValueName));
                if(!callback){
                    lastRuntimeError = "native global `" + nativeValueName + "` is not loaded";
                    return;
                }
                auto val = invokeNativeFuncWithValues(callback,{},nullptr);
                if(!val){
                    if(lastRuntimeError.empty()){
                        lastRuntimeError = "native global `" + nativeValueName + "` returned no value";
                    }
                    return;
                }
                allocator->allocVariable(var_name,val);
            }
        }
    }
    else if(code == CODE_RTLOCAL_DECL){
        uint32_t slot = 0;
        bool hasInitValue = false;
        in.read((char *)&slot,sizeof(slot));
        in.read((char *)&hasInitValue,sizeof(hasInitValue));
        if(hasInitValue){
            auto val = evalExpr(in);
            storeLocalSlotOwned(slot,val);
        }
    }
    else if(code == CODE_RTSECURE_DECL){
        RTID targetVarId;
        in >> &targetVarId;
        bool hasCatchBinding = false;
        in.read((char *)&hasCatchBinding,sizeof(hasCatchBinding));
        RTID catchBindingId;
        if(hasCatchBinding){
            in >> &catchBindingId;
        }
        bool hasCatchType = false;
        in.read((char *)&hasCatchType,sizeof(hasCatchType));
        if(hasCatchType){
            RTID catchTypeId;
            in >> &catchTypeId;
        }

        auto guardedValue = evalExpr(in);
        RTCode blockBegin = CODE_MODULE_END;
        in.read((char *)&blockBegin,sizeof(blockBegin));
        if(blockBegin != CODE_RTBLOCK_BEGIN){
            if(guardedValue){
                StarbytesObjectRelease(guardedValue);
            }
            return;
        }

        if(guardedValue){
            allocator->allocVariable(string_ref(targetVarId.value,targetVarId.len),guardedValue);
            skipRuntimeBlock(in,CODE_RTBLOCK_END);
            lastRuntimeError.clear();
        }
        else {
            if(hasCatchBinding){
                auto errorText = lastRuntimeError.empty()? std::string("error") : lastRuntimeError;
                auto errorObject = StarbytesStrNewWithData(errorText.c_str());
                allocator->allocVariable(string_ref(catchBindingId.value,catchBindingId.len),errorObject);
            }
            lastRuntimeError.clear();
            executeRuntimeBlock(in,CODE_RTBLOCK_END,willReturn,return_val);
        }
    }
    else if(code == CODE_RTSECURE_LOCAL_DECL){
        uint32_t targetSlot = 0;
        bool hasCatchSlot = false;
        bool hasCatchType = false;
        in.read((char *)&targetSlot,sizeof(targetSlot));
        in.read((char *)&hasCatchSlot,sizeof(hasCatchSlot));
        uint32_t catchSlot = 0;
        if(hasCatchSlot){
            in.read((char *)&catchSlot,sizeof(catchSlot));
        }
        in.read((char *)&hasCatchType,sizeof(hasCatchType));
        if(hasCatchType){
            RTID catchTypeId;
            in >> &catchTypeId;
        }

        auto guardedValue = evalExpr(in);
        RTCode blockBegin = CODE_MODULE_END;
        in.read((char *)&blockBegin,sizeof(blockBegin));
        if(blockBegin != CODE_RTBLOCK_BEGIN){
            if(guardedValue){
                StarbytesObjectRelease(guardedValue);
            }
            return;
        }

        if(guardedValue){
            storeLocalSlotOwned(targetSlot,guardedValue);
            skipRuntimeBlock(in,CODE_RTBLOCK_END);
            lastRuntimeError.clear();
        }
        else {
            if(hasCatchSlot){
                auto errorText = lastRuntimeError.empty()? std::string("error") : lastRuntimeError;
                auto errorObject = StarbytesStrNewWithData(errorText.c_str());
                storeLocalSlotOwned(catchSlot,errorObject);
            }
            lastRuntimeError.clear();
            executeRuntimeBlock(in,CODE_RTBLOCK_END,willReturn,return_val);
        }
    }
    else if(code == CODE_CONDITIONAL){
        unsigned condSpecCount = 0;
        in.read((char *)&condSpecCount,sizeof(condSpecCount));
        bool branchTaken = false;
        for(unsigned i = 0;i < condSpecCount;i++){
            RTCode specType = COND_TYPE_IF;
            in.read((char *)&specType,sizeof(specType));

            if(specType == COND_TYPE_IF){
                bool condition = false;
                if(!branchTaken && !*willReturn){
                    auto condObject = evalExpr(in);
                    assert(StarbytesObjectTypecheck(condObject,StarbytesBoolType()));
                    condition = (bool)StarbytesBoolValue(condObject);
                    StarbytesObjectRelease(condObject);
                }
                else {
                    skipExpr(in);
                }

                RTCode blockBegin = CODE_MODULE_END;
                in.read((char *)&blockBegin,sizeof(blockBegin));
                if(blockBegin != CODE_RTBLOCK_BEGIN){
                    continue;
                }

                if(condition && !branchTaken && !*willReturn){
                    executeRuntimeBlock(in,CODE_RTBLOCK_END,willReturn,return_val);
                    branchTaken = true;
                }
                else {
                    skipRuntimeBlock(in,CODE_RTBLOCK_END);
                }
                continue;
            }

            if(specType == COND_TYPE_ELSE){
                RTCode blockBegin = CODE_MODULE_END;
                in.read((char *)&blockBegin,sizeof(blockBegin));
                if(blockBegin != CODE_RTBLOCK_BEGIN){
                    continue;
                }

                if(!branchTaken && !*willReturn){
                    executeRuntimeBlock(in,CODE_RTBLOCK_END,willReturn,return_val);
                    branchTaken = true;
                }
                else {
                    skipRuntimeBlock(in,CODE_RTBLOCK_END);
                }
                continue;
            }

            if(specType == COND_TYPE_LOOPIF){
                auto conditionPos = in.tellg();
                auto loopExitPos = conditionPos;
                while(true){
                    in.clear();
                    in.seekg(conditionPos);
                    auto condObject = evalExpr(in);
                    assert(StarbytesObjectTypecheck(condObject,StarbytesBoolType()));
                    bool condition = (bool)StarbytesBoolValue(condObject);
                    StarbytesObjectRelease(condObject);

                    RTCode blockBegin = CODE_MODULE_END;
                    in.read((char *)&blockBegin,sizeof(blockBegin));
                    if(blockBegin != CODE_RTBLOCK_BEGIN){
                        loopExitPos = in.tellg();
                        break;
                    }

                    if(condition && !*willReturn){
                        executeRuntimeBlock(in,CODE_RTBLOCK_END,willReturn,return_val);
                        loopExitPos = in.tellg();
                        if(*willReturn){
                            break;
                        }
                    }
                    else {
                        skipRuntimeBlock(in,CODE_RTBLOCK_END);
                        loopExitPos = in.tellg();
                        break;
                    }
                }
                in.clear();
                in.seekg(loopExitPos);
                continue;
            }
        }

        RTCode conditionalEnd = CODE_MODULE_END;
        in.read((char *)&conditionalEnd,sizeof(conditionalEnd));
    }
    else if(code == CODE_RTRETURN){
        bool hasValue;
        in.read((char *)&hasValue,sizeof(hasValue));
        if(hasValue) {
            auto object = evalExpr(in);
            *return_val = object;
        }
        else {
            *return_val = nullptr;
        }
        *willReturn = true;
         
    }
    else if(code == CODE_RTFUNC){
        RTFuncTemplate funcTemp;
        in >> &funcTemp;
        functions.push_back(funcTemp);
    }
    else if(code == CODE_RTIVKFUNC
         || code == CODE_UNARY_OPERATOR
         || code == CODE_RTTYPED_NEGATE
         || code == CODE_BINARY_OPERATOR
         || code == CODE_RTTYPED_BINARY
         || code == CODE_RTTYPED_COMPARE
         || code == CODE_RTTYPECHECK
         || code == CODE_RTTERNARY
         || code == CODE_RTCAST
         || code == CODE_RTTYPED_INTRINSIC
         || code == CODE_RTMEMBER_SET
         || code == CODE_RTMEMBER_IVK
         || code == CODE_RTMEMBER_GET
         || code == CODE_RTNEWOBJ
         || code == CODE_RTREGEX_LITERAL
         || code == CODE_RTVAR_SET
         || code == CODE_RTLOCAL_REF
         || code == CODE_RTTYPED_LOCAL_REF
         || code == CODE_RTLOCAL_SET
         || code == CODE_RTARRAY_LITERAL
         || code == CODE_RTINDEX_GET
         || code == CODE_RTTYPED_INDEX_GET
         || code == CODE_RTINDEX_SET
         || code == CODE_RTTYPED_INDEX_SET
         || code == CODE_RTDICT_LITERAL){
        in.seekg(-static_cast<std::streamoff>(sizeof(RTCode)),std::ios_base::cur);
        auto result = evalExpr(in);
        if(result){
            StarbytesObjectRelease(result);
        }
    }
    else if(code == CODE_RTCLASS_DEF){
        RTClass _class;
        in >> &_class;
        auto className = rtidToString(_class.name);
        auto classType = StarbytesMakeClass(className.c_str());
        classTypeByName[className] = classType;
        runtimeClassRegistry[classType] = className;
        classIndexByType[classType] = classes.size();
        classes.emplace_back(std::move(_class));
    }
}

void InterpImpl::exec(std::istream & in){
    std::cout << "Interp Starting" << std::endl;
    RTCode code = CODE_MODULE_END;
    std::string g = "GLOBAL";
    allocator->setScope(g);
    activeInput = &in;
    lastRuntimeError.clear();
    if(!in.read((char *)&code,sizeof(RTCode))){
        activeInput = nullptr;
        return;
    }
    bool temp = false;
    StarbytesObject temp2 = nullptr;
    while(in.good() && code != CODE_MODULE_END){
        execNorm(code,in,&temp,&temp2);
        processMicrotasks();
        if(!lastRuntimeError.empty()){
            break;
        }
        if(!in.read((char *)&code,sizeof(RTCode))){
            break;
        }
    };
    processMicrotasks();
    allocator->clearScope();
    activeInput = nullptr;
}

InterpImpl::~InterpImpl(){
    while(!localFrames.empty()){
        popLocalFrame();
    }
    while(!microtaskQueue.empty()){
        auto call = std::move(microtaskQueue.front());
        microtaskQueue.pop_front();
        if(call.task){
            StarbytesObjectRelease(call.task);
        }
        if(call.boundSelf){
            StarbytesObjectRelease(call.boundSelf);
        }
        for(auto *arg : call.args){
            if(arg){
                StarbytesObjectRelease(arg);
            }
        }
    }
    for(auto *module : nativeModules){
        starbytes_native_mod_close(module);
    }
    nativeModules.clear();
}

bool InterpImpl::addExtension(const std::string &path) {
    if(path.empty()){
        return false;
    }
    auto *module = starbytes_native_mod_load(string_ref(path));
    if(!module){
        lastRuntimeError = "failed to load native module `" + path + "`";
        return false;
    }
    nativeModules.push_back(module);
    nativeCallbackCache.clear();
    nativeValueCallbackCache.clear();
    return true;
}

std::shared_ptr<Interp> Interp::Create(){
    return std::make_shared<InterpImpl>();
}

}
