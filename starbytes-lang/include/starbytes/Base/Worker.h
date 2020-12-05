#include <thread>
#include <future>
#include "starbytes/Base/Base.h"

#ifndef BASE_WORKER_H
#define BASE_WORKER_H

STARBYTES_FOUNDATION_NAMESPACE

template<class _Ty>
using Promise = std::promise<_Ty>;

template<class _Promise_Ty,typename ..._Args>
using ThreadFunction = void (*)(Promise<_Promise_Ty>,_Args...);

template<class _Prom_Ty>
class Worker {
    Promise<_Prom_Ty> promise;
    std::future<_Prom_Ty> fut;
    std::thread t;
    bool isClosed;
public:
    template<class Func_Ty,typename ..._Args>
    Worker(Func_Ty f,_Args ...args):t(f,args...,promise),isClosed(false){
        fut = promise.get_future();
    };
    _Prom_Ty & awaitVal(){
        return fut.get();
    };
    void close(){
        t.join();
        isClosed = true;
    };

    bool & hasClosed(){
        return isClosed;
    };
};

using SimpleThread = Worker<void>;

// template<class _Prom_Ty, typename _args>
// struct ThreadTask {
//     ThreadFunction<_Prom_Ty,_args> func_ptr;
//     ThreadTask(ThreadFunction<_Prom_Ty,_args> & _func_ptr):func_ptr(_func_ptr){};
// };
// template<class _Prom_Ty,typename _args>
// inline ThreadTask<_Prom_Ty,_args> create_thread_task(ThreadFunction<_Prom_Ty,_args> func_ptr){
//     return ThreadTask(func_ptr);
// };

// template<class _Prom_Ty,size_t s,typename _task_args>
// class WorkerPool {
// private:
//     using _Worker_Ty = Worker<_Prom_Ty>;
//     size_t _size = s;
//     std::array<_Worker_Ty,s> workers;
//     std::vector<ThreadTask<_Prom_Ty,_task_args>> tasks_to_complete;
//     template<typename ..._args>
//     void give_latest_task_to_worker(Worker<_Prom_Ty> & worker,_args ...args){
//         ThreadTask<_Prom_Ty,_task_args> task = tasks_to_complete.back();
//         worker = Worker(task.func_ptr,args...);
//     };
// public:
//     WorkerPool(){};
//     void init(){
        
//     };

//     size_t size(){
//         return _size;
//     };
// };

NAMESPACE_END

#endif