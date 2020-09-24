#include "Runtime/RuntimeEngine.h"
#include <string>
#include <vector>
#include <initializer_list>
#include <iostream>
#include <ctime>

namespace Starbytes::Runtime::Engine {

    enum class PtrType:int {
        CreateFunc,InvokeFunc
    };
    struct FuncPtr {
        PtrType type;
        std::vector<std::string> args;
    };

    struct StoredFunction {
        std::string name;
        std::initializer_list<FuncPtr *> body;
    };

    std::vector<StoredFunction *> storage;

    void starbytes_log(std::string & message){
        tm buf;
        time_t t = time(NULL);
        char RESULT [50];
        gmtime_s(&buf,&t);
        asctime_s(RESULT,sizeof (RESULT),&buf);
        std::string R (RESULT);
        size_t found = R.find("\n");
        if(found != std::string::npos){
            R.erase(R.begin()+found);
        }
        std::cout << "[" << R << "] - " << message;
    }

    //Dynamic Allocates `FuncPtr` object

    FuncPtr * create_func_ptr(PtrType _type,std::initializer_list<std::string> args){
        FuncPtr *rc = new FuncPtr();
        rc->args = std::vector(args);
        rc->type = _type;
        return rc;
    }

    void create_function(std::string name,std::initializer_list<FuncPtr *> body){
        StoredFunction *func = new StoredFunction();
        func->name = name;
        func->body = body;
        storage.push_back(func);
    }

    // void invoke_internal_func_ptr(FuncPtr *fn_ptr,std::vector<std::string> & args){
    //     if(fn_ptr->type == PtrType::CreateFunc){
    //         create_function(args[0],{});
    //     }
    //     else if(fn_ptr->type == PtrType::InvokeFunc){
            
    //     }
    // }


    void invoke_function(std::string name,std::vector<std::string> & args){
        if(name == "Log"){
            starbytes_log(args[0]);
        }
        else{
            for(auto stfnc : storage){
                if(stfnc->name == name){
                    for(auto fnc : stfnc->body){
                        if(fnc->type == PtrType::CreateFunc){
                            create_function(fnc->args[0],{});
                        }
                        else if(fnc->type == PtrType::InvokeFunc){
                            std::vector<std::string> ARGS (fnc->args);
                            ARGS.erase(ARGS.begin());
                            invoke_function(fnc->args[0],ARGS);
                        }
                    }
                }
            }
        }
    }

    void Program(){
        std::vector<std::string> args {"Hello From Starbytes!"};
        // invoke_function("Log",args);
        
        create_function("TestFunc", {create_func_ptr(PtrType::InvokeFunc,{"Log","Starbytes Test 2"})});
        invoke_function("TestFunc",args);
    }
    

};
