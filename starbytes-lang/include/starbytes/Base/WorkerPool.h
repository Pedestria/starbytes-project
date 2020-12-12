#include <thread>
#include <future>
#include <queue>
#include <mutex>
#include <functional>
#include "Macros.h"

#ifndef BASE_WORKER_H
#define BASE_WORKER_H

STARBYTES_FOUNDATION_NAMESPACE

using Worker = std::thread;

using UniqueLock = std::unique_lock<std::mutex>;

class WorkerPool {
    using PackagedTask = std::packaged_task<void()>;
    using TaskT = std::function<void()>;
    std::queue<PackagedTask> tasks;
    std::vector<Worker> workers;
    UniqueLock queue_lock;
    std::condition_variable queue_cond;
    unsigned t_num;
public:
    WorkerPool(unsigned __t_num):t_num(__t_num){
        workers.reserve(t_num);
        for(unsigned t_id = 0;t_id < t_num;++t_id){
            workers.emplace_back([this,t_id](){

                if(tasks.empty()){
                    return;
                }

                auto task = std::move(tasks.front());
                tasks.pop();
                task();
            });
        }

    };
    ~WorkerPool(){
        for(auto & __worker : workers){
            __worker.join();
        };
    };
};

NAMESPACE_END

#endif