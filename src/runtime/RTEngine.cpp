#include "starbytes/runtime/RTEngine.h"
#include "starbytes/compiler/RTCode.h"
#include "RTStdlib.h"
#include "starbytes/base/ADT.h"

#include <iostream>
#include <iomanip>
#include <cstring>
#include <cassert>
#include <cmath>
#include <sstream>
#include <set>

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

#include "starbytes/interop.h"

namespace starbytes::Runtime {

struct RTScope {
    std::string name;
    RTScope *parent = nullptr;
};

RTScope *RTSCOPE_GLOBAL = new RTScope({"__GLOBAL__"});

static std::string rtidToString(const RTID &id){
    return std::string(id.value,id.len);
}

static std::string mangleClassMethodName(const std::string &className,const std::string &methodName){
    return "__" + className + "__" + methodName;
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

static bool objectToNumber(StarbytesObject object,double &value,bool &isFloat){
    if(!object || !StarbytesObjectTypecheck(object,StarbytesNumType())){
        return false;
    }
    auto numType = StarbytesNumGetType(object);
    isFloat = (numType == NumTypeFloat);
    if(numType == NumTypeFloat){
        value = StarbytesNumGetFloatValue(object);
    }
    else {
        value = StarbytesNumGetIntValue(object);
    }
    return true;
}

static StarbytesObject makeNumber(double value,bool isFloat){
    if(isFloat){
        return StarbytesNumNew(NumTypeFloat,value);
    }
    return StarbytesNumNew(NumTypeInt,(int)value);
}

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
void starbytes_native_mod_close(StarbytesNativeModule * mod);

class InterpImpl final : public Interp {

   void addExtension(string_ref path);

    std::unique_ptr<RTAllocator> allocator;
    
    std::vector<RTClass> classes;
    
    std::vector<RTFuncTemplate> functions;

    map<StarbytesClassType,std::string> runtimeClassRegistry;
    string_map<StarbytesClassType> classTypeByName;
    map<StarbytesClassType,size_t> classIndexByType;
    std::string lastRuntimeError;

    void execNorm(RTCode &code,std::istream &in,bool * willReturn,StarbytesObject *return_val);
    
    StarbytesObject invokeFunc(std::istream &in,RTFuncTemplate *func_temp,unsigned argCount,StarbytesObject boundSelf = nullptr);
    RTClass *findClassByName(string_ref className);
    RTClass *findClassByType(StarbytesClassType classType);
    bool buildClassHierarchy(RTClass *classMeta,std::vector<RTClass *> &hierarchy);
    RTClass *findMethodOwnerInHierarchy(RTClass *classMeta,const std::string &methodName);
    RTFuncTemplate *findFunctionByName(string_ref functionName);
    void discardExprArgs(std::istream &in,unsigned argCount);
    void skipExpr(std::istream &in);
    void skipExprFromCode(std::istream &in,RTCode code);
    void skipRuntimeStmt(std::istream &in,RTCode code);
    void skipRuntimeBlock(std::istream &in,RTCode endCode);
    void executeRuntimeBlock(std::istream &in,RTCode endCode,bool *willReturn,StarbytesObject *return_val);
    
    StarbytesObject evalExpr(std::istream &in);
    
public:
    InterpImpl():allocator(std::make_unique<RTAllocator>()){
        RTFuncTemplate printTemplate;
        printTemplate.name = {strlen("print"),"print"};
        printTemplate.argsTemplate.push_back({strlen("object"),"object"});
        functions.push_back(std::move(printTemplate));
    };
    ~InterpImpl() override = default;
    void exec(std::istream &in) override;
};

RTClass *InterpImpl::findClassByName(string_ref className){
    auto foundType = classTypeByName.find(std::string(className));
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
    std::set<std::string> visited;
    std::vector<RTClass *> reverseChain;
    for(auto *cursor = classMeta; cursor != nullptr;){
        auto cursorName = rtidToString(cursor->name);
        if(visited.find(cursorName) != visited.end()){
            return false;
        }
        visited.insert(cursorName);
        reverseChain.push_back(cursor);
        if(!cursor->hasSuperClass){
            break;
        }
        auto superName = rtidToString(cursor->superClassName);
        cursor = findClassByName(string_ref(superName));
    }
    hierarchy.assign(reverseChain.rbegin(),reverseChain.rend());
    return true;
}

RTClass *InterpImpl::findMethodOwnerInHierarchy(RTClass *classMeta,const std::string &methodName){
    std::set<std::string> visited;
    for(auto *cursor = classMeta; cursor != nullptr;){
        auto cursorName = rtidToString(cursor->name);
        if(visited.find(cursorName) != visited.end()){
            return nullptr;
        }
        visited.insert(cursorName);
        for(auto &method : cursor->methods){
            if(rtidToString(method.name) == methodName){
                return cursor;
            }
        }
        if(!cursor->hasSuperClass){
            break;
        }
        auto superName = rtidToString(cursor->superClassName);
        cursor = findClassByName(string_ref(superName));
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

void InterpImpl::discardExprArgs(std::istream &in,unsigned argCount){
    while(argCount > 0){
        auto arg = evalExpr(in);
        if(arg){
            StarbytesObjectRelease(arg);
        }
        --argCount;
    }
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
            stdlib::print(object_to_print,runtimeClassRegistry);
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
    else {
    
        std::string funcScope = std::string(func_name) + std::to_string(func_temp->invocations);

        if(boundSelf){
            StarbytesObjectReference(boundSelf);
            allocator->allocVariable(string_ref("self"),boundSelf,funcScope);
        }
                
        auto callSitePos = in.tellg();
        auto func_param_it = func_temp->argsTemplate.begin();
        while(argCount > 0){
            StarbytesObject obj = evalExpr(in);
            if(func_param_it != func_temp->argsTemplate.end()){
                allocator->allocVariable(string_ref(func_param_it->value,func_param_it->len),obj,funcScope);
                ++func_param_it;
            }
            else if(obj){
                StarbytesObjectRelease(obj);
            }
            --argCount;
        };
        auto resumePos = in.tellg();
        allocator->setScope(funcScope);
        auto current_pos = func_temp->block_start_pos;
        /// Invoke
        in.seekg(current_pos);
        
        func_temp->invocations += 1;
                
        RTCode code = CODE_MODULE_END;
        if(!in.read((char *)&code,sizeof(RTCode))){
            in.clear();
            in.seekg(callSitePos);
            allocator->clearScope();
            allocator->setScope(parentScope);
            return nullptr;
        }
        bool willReturn = false;
        StarbytesObject return_val = nullptr;
        while(in.good() && code != CODE_RTFUNCBLOCK_END){
            if(!willReturn){
                execNorm(code,in,&willReturn,&return_val);
            }
            if(!in.read((char *)&code,sizeof(RTCode))){
                break;
            }
        };
        in.seekg(resumePos);
        
        allocator->clearScope();
        allocator->setScope(parentScope);
        
        return return_val;
        
    }
    
}

StarbytesObject InterpImpl::evalExpr(std::istream & in){
    RTCode code;
    in.read((char *)&code,sizeof(RTCode));
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
            auto funcRefObject = (StarbytesFuncRef)evalExpr(in);
            unsigned argCount;
            in.read((char *)&argCount,sizeof(argCount));
            if(!funcRefObject){
                discardExprArgs(in,argCount);
                return nullptr;
            }
            auto returnValue = invokeFunc(in,StarbytesFuncRefGetPtr(funcRefObject),argCount);
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
            StarbytesObjectRelease(operand);
            return nullptr;
        }
        case CODE_BINARY_OPERATOR: {
            RTCode binaryCode = BINARY_OP_PLUS;
            in.read((char *)&binaryCode,sizeof(binaryCode));
            auto lhs = evalExpr(in);
            auto rhs = evalExpr(in);
            if(!lhs || !rhs){
                if(lhs){
                    StarbytesObjectRelease(lhs);
                }
                if(rhs){
                    StarbytesObjectRelease(rhs);
                }
                return nullptr;
            }

            auto finish = [&](StarbytesObject result){
                StarbytesObjectRelease(lhs);
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
                bool lhsFloat = false;
                bool rhsFloat = false;
                double lhsVal = 0.0;
                double rhsVal = 0.0;
                if(!objectToNumber(lhs,lhsVal,lhsFloat) || !objectToNumber(rhs,rhsVal,rhsFloat)){
                    return finish(nullptr);
                }
                if((binaryCode == BINARY_OP_DIV || binaryCode == BINARY_OP_MOD) && rhsVal == 0.0){
                    return finish(nullptr);
                }
                if(binaryCode == BINARY_OP_MUL){
                    bool useFloat = lhsFloat || rhsFloat;
                    return finish(makeNumber(lhsVal * rhsVal,useFloat));
                }
                if(binaryCode == BINARY_OP_DIV){
                    bool useFloat = lhsFloat || rhsFloat;
                    if(useFloat){
                        return finish(makeNumber(lhsVal / rhsVal,true));
                    }
                    return finish(makeNumber((double)((int)lhsVal / (int)rhsVal),false));
                }
                bool useFloat = lhsFloat || rhsFloat;
                if(useFloat){
                    return finish(makeNumber(std::fmod(lhsVal,rhsVal),true));
                }
                return finish(makeNumber((double)((int)lhsVal % (int)rhsVal),false));
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
            if(binaryCode == BINARY_LOGIC_AND || binaryCode == BINARY_LOGIC_OR){
                if(!(StarbytesObjectTypecheck(lhs,StarbytesBoolType()) && StarbytesObjectTypecheck(rhs,StarbytesBoolType()))){
                    return finish(nullptr);
                }
                bool l = (bool)StarbytesBoolValue(lhs);
                bool r = (bool)StarbytesBoolValue(rhs);
                bool result = (binaryCode == BINARY_LOGIC_AND)? (l && r) : (l || r);
                return finish(StarbytesBoolNew((StarbytesBoolVal)result));
            }
            return finish(nullptr);
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
               StarbytesObjectTypecheck(index,StarbytesNumType()) &&
               StarbytesNumGetType(index) == NumTypeInt){
                int idx = StarbytesNumGetIntValue(index);
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
               StarbytesObjectTypecheck(index,StarbytesNumType()) &&
               StarbytesNumGetType(index) == NumTypeInt){
                int idx = StarbytesNumGetIntValue(index);
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

            StarbytesClassType classType = StarbytesClassObjectGetClass(object);
            auto *classMeta = findClassByType(classType);
            if(!classMeta){
                StarbytesObjectRelease(object);
                discardExprArgs(in,argCount);
                return nullptr;
            }

            std::string methodName = rtidToString(memberId);
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

            auto result = invokeFunc(in,runtimeMethod,argCount,object);
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
        case CODE_RTINDEX_GET: {
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
         || code == CODE_BINARY_OPERATOR
         || code == CODE_RTMEMBER_SET
         || code == CODE_RTMEMBER_IVK
         || code == CODE_RTMEMBER_GET
         || code == CODE_RTNEWOBJ
         || code == CODE_RTREGEX_LITERAL
         || code == CODE_RTVAR_SET
         || code == CODE_RTINDEX_GET
         || code == CODE_RTINDEX_SET
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
    if(!in.read((char *)&code,sizeof(RTCode))){
        return;
    }
    bool temp = false;
    StarbytesObject temp2 = nullptr;
    while(in.good() && code != CODE_MODULE_END){
        execNorm(code,in,&temp,&temp2);
        if(!in.read((char *)&code,sizeof(RTCode))){
            break;
        }
    };
    allocator->clearScope();
}


void InterpImpl::addExtension(string_ref path) {

}

std::shared_ptr<Interp> Interp::Create(){
    return std::make_shared<InterpImpl>();
}

}
