#include "RTStdlib.h"

#include "starbytes/interop.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <limits>

namespace starbytes {
    namespace Runtime::stdlib {
        namespace {

        bool objectToNumber(StarbytesObject object,long double &value,StarbytesNumT &numType){
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

        int numericTypeRank(StarbytesNumT numType){
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

        StarbytesNumT promoteNumericType(StarbytesNumT lhs,StarbytesNumT rhs){
            return numericTypeRank(lhs) >= numericTypeRank(rhs) ? lhs : rhs;
        }

        StarbytesObject makeNumber(long double value,StarbytesNumT numType){
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

        }

        inline void _print_timestamp(){
//            auto t = time(NULL);
                
//            std::cout << "[" << std::asctime(gmtime(&t)) << "]" << std::flush;
        }
    
        void _print_rt_internal_obj(StarbytesObject object,map<StarbytesClassType,std::string> &reg);
    
        void _print_rt_obj(StarbytesObject object,map<StarbytesClassType,std::string> &reg){
            if(!object){
                std::cout << "null" << std::flush;
                return;
            }
            if(StarbytesObjectIs(object)){
                _print_rt_internal_obj(object,reg);
            }
            else {
                StarbytesClassType t = StarbytesClassObjectGetClass(object);
                
                std::cout << reg[t] << "(" << std::flush;
                unsigned prop_n = StarbytesObjectGetPropertyCount(object);
                for(unsigned i = 0;i < prop_n;i++){
                    if(i > 0){
                        std::cout << ", " << std::flush;
                    }
                    auto prop = StarbytesObjectIndexProperty(object,i);
                    std::cout << prop->name << "=" << std::flush;
                    _print_rt_obj(prop->data,reg);
                }
                std::cout << ")" << std::flush;
            };
        }
    
        void _print_rt_internal_obj(StarbytesObject object,map<StarbytesClassType,std::string> &reg){
            if(StarbytesObjectTypecheck(object,StarbytesNumType())){
                    std::cout << "\x1b[33m" << std::flush;
                    auto numType = StarbytesNumGetType(object);
                    if(numType == NumTypeInt){
                        std::cout << StarbytesNumGetIntValue(object);
                    }
                    else if(numType == NumTypeLong){
                        std::cout << StarbytesNumGetLongValue(object);
                    }
                    else if(numType == NumTypeFloat){
                        float value = StarbytesNumGetFloatValue(object);
                        double intTemp = 0.0;
                        if(std::modf((double)value,&intTemp) == 0.0){
                            std::cout << (long long)intTemp;
                        }
                        else {
                            std::cout << std::dec << value;
                        }
                    }
                    else {
                        double value = StarbytesNumGetDoubleValue(object);
                        double intTemp = 0.0;
                        if(std::modf(value,&intTemp) == 0.0){
                            std::cout << (long long)intTemp;
                        }
                        else {
                            std::cout << std::dec << value;
                        }
                    }
                    std::cout << "\x1b[0m" << std::flush;
                }
            else if(StarbytesObjectTypecheck(object,StarbytesBoolType())){
                    auto val = (bool)StarbytesBoolValue(object);
                    std::cout << "\x1b[35m" << std::boolalpha << val << std::noboolalpha << "\x1b[0m" << std::flush;
                }
            else if(StarbytesObjectTypecheck(object,StarbytesStrType())){
                    auto data = StarbytesStrGetBuffer(object);
                    std::cout << "\x1b[32m" << "\"" << data << "\"" << "\x1b[0m" << std::flush;
                }
            else if(StarbytesObjectTypecheck(object,StarbytesRegexType())){
                    auto patternObj = StarbytesObjectGetProperty(object,"pattern");
                    auto flagsObj = StarbytesObjectGetProperty(object,"flags");
                    auto pattern = patternObj ? StarbytesStrGetBuffer(patternObj) : (char *)"";
                    auto flags = flagsObj ? StarbytesStrGetBuffer(flagsObj) : (char *)"";
                    std::cout << "\x1b[36m" << "/" << pattern << "/" << flags << "\x1b[0m" << std::flush;
                }
            else if(StarbytesObjectTypecheck(object,StarbytesTaskType())){
                    auto state = StarbytesTaskGetState(object);
                    if(state == StarbytesTaskPending){
                        std::cout << "<Task pending>" << std::flush;
                    }
                    else if(state == StarbytesTaskResolved){
                        auto value = StarbytesTaskGetValue(object);
                        std::cout << "<Task resolved:" << std::flush;
                        if(value){
                            _print_rt_obj(value,reg);
                            StarbytesObjectRelease(value);
                        }
                        else {
                            std::cout << "null" << std::flush;
                        }
                        std::cout << ">" << std::flush;
                    }
                    else {
                        auto err = StarbytesTaskGetError(object);
                        std::cout << "<Task rejected:" << (err ? err : "error") << ">" << std::flush;
                    }
                }
            else if(StarbytesObjectTypecheck(object,StarbytesArrayType())){
                    
                    std::cout << "[" << std::flush;
                    auto length = StarbytesArrayGetLength(object);
                    for(unsigned i = 0;i < length;i++){
                        
                        if(i > 0){
                            std::cout << "," << std::flush;
                        };
                        
                        _print_rt_obj(StarbytesArrayIndex(object,i),reg);
                    };
                    std::cout << "]" << std::flush;
                }
            /// Print StarbytesDict
            else {
                    std::cout << "{" << std::flush;
                    StarbytesArray keys = StarbytesObjectGetProperty(object,"keys"),vals = StarbytesObjectGetProperty(object,"values");
                    
                    auto len = StarbytesArrayGetLength(keys);
                    for(unsigned i = 0;i < len;i++){
                        _print_rt_obj(StarbytesArrayIndex(keys,i),reg);
                        std::cout << ":" << std::flush;
                        _print_rt_obj(StarbytesArrayIndex(vals,i),reg);
                    }
                    std::cout << "}" << std::flush;
                }
            }
    
        /// Print Function
        void print(StarbytesObject object,map<StarbytesClassType,std::string> &reg){
            // map<StarbytesClassType,std::string> &reg
//            _print_timestamp();
            _print_rt_obj(object,reg);
            std::cout << std::endl;
        }

        void addMathBuiltinTemplates(std::vector<RTFuncTemplate> &functions){
            auto addBuiltinTemplate = [&](const char *name,std::initializer_list<const char *> params){
                RTFuncTemplate funcTemplate;
                funcTemplate.name = {strlen(name),name};
                for(auto *param : params){
                    funcTemplate.argsTemplate.push_back({strlen(param),param});
                }
                functions.push_back(std::move(funcTemplate));
            };
            addBuiltinTemplate("sqrt",{"value"});
            addBuiltinTemplate("abs",{"value"});
            addBuiltinTemplate("min",{"lhs","rhs"});
            addBuiltinTemplate("max",{"lhs","rhs"});
        }

        bool isMathBuiltinFunction(string_ref funcName){
            return funcName == "sqrt"
                || funcName == "abs"
                || funcName == "min"
                || funcName == "max";
        }

        StarbytesObject invokeMathBuiltinFunction(string_ref funcName,
                                                  ArrayRef<StarbytesObject> args,
                                                  std::string &lastRuntimeError){
            auto fail = [&](const std::string &message) -> StarbytesObject {
                lastRuntimeError = message;
                return nullptr;
            };

            if(funcName == "sqrt"){
                if(args.size() != 1){
                    return fail("sqrt expects exactly one argument");
                }
                long double value = 0.0;
                StarbytesNumT valueType = NumTypeInt;
                if(!objectToNumber(args[0],value,valueType)){
                    return fail("sqrt requires a numeric argument");
                }
                if(value < 0.0){
                    return fail("sqrt requires non-negative numeric input");
                }
                return StarbytesNumNew(NumTypeDouble,std::sqrt((double)value));
            }

            if(funcName == "abs"){
                if(args.size() != 1){
                    return fail("abs expects exactly one argument");
                }
                long double value = 0.0;
                StarbytesNumT valueType = NumTypeInt;
                if(!objectToNumber(args[0],value,valueType)){
                    return fail("abs requires a numeric argument");
                }
                if(valueType == NumTypeInt){
                    int original = StarbytesNumGetIntValue(args[0]);
                    if(original == std::numeric_limits<int>::min()){
                        return fail("abs overflowed Int minimum value");
                    }
                    return StarbytesNumNew(NumTypeInt,original < 0 ? -original : original);
                }
                if(valueType == NumTypeLong){
                    int64_t original = StarbytesNumGetLongValue(args[0]);
                    if(original == std::numeric_limits<int64_t>::min()){
                        return fail("abs overflowed Long minimum value");
                    }
                    return StarbytesNumNew(NumTypeLong,original < 0 ? -original : original);
                }
                if(valueType == NumTypeFloat){
                    return StarbytesNumNew(NumTypeFloat,(float)std::fabs((double)StarbytesNumGetFloatValue(args[0])));
                }
                return StarbytesNumNew(NumTypeDouble,std::fabs(StarbytesNumGetDoubleValue(args[0])));
            }

            if(funcName == "min" || funcName == "max"){
                if(args.size() != 2){
                    return fail((funcName == "min")? "min expects exactly two arguments" : "max expects exactly two arguments");
                }
                long double lhsValue = 0.0;
                long double rhsValue = 0.0;
                StarbytesNumT lhsType = NumTypeInt;
                StarbytesNumT rhsType = NumTypeInt;
                if(!objectToNumber(args[0],lhsValue,lhsType) || !objectToNumber(args[1],rhsValue,rhsType)){
                    return fail((funcName == "min")? "min requires numeric arguments" : "max requires numeric arguments");
                }
                auto resultType = promoteNumericType(lhsType,rhsType);
                long double value = (funcName == "min")? std::min(lhsValue,rhsValue) : std::max(lhsValue,rhsValue);
                return makeNumber(value,resultType);
            }

            return fail("unknown builtin math function");
        }
    
    }

    
}
