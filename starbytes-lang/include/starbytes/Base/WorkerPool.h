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

class WorkerPool {
    using PackagedTask = std::packaged_task<void()>;
    using TaskT = std::function<void()>;
    std::queue<PackagedTask> tasks;
    std::vector<Worker> workers;
    unsigned t_num;
public:
    WorkerPool(unsigned __t_num):t_num(__t_num){
        workers.reserve(t_num);
        while(workers.size() != t_num){
            workers.emplace_back([this](){
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