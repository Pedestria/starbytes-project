#include <starbytes/interop.h>

#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

namespace {

struct NativeArgsLayout {
    unsigned argc = 0;
    unsigned index = 0;
    StarbytesObject *argv = nullptr;
};

struct MutexState {
    std::mutex native;
    bool locked = false;
    std::thread::id owner;
};

struct ConditionState {
    std::condition_variable_any native;
};

struct SemaphoreState {
    std::mutex mutex;
    std::condition_variable cv;
    int count = 0;
    int maxCount = 1;
};

struct EventState {
    std::mutex mutex;
    std::condition_variable cv;
    bool signaled = false;
    bool manualReset = true;
};

std::unordered_map<StarbytesObject,std::unique_ptr<MutexState>> g_mutexRegistry;
std::unordered_map<StarbytesObject,std::unique_ptr<ConditionState>> g_conditionRegistry;
std::unordered_map<StarbytesObject,std::unique_ptr<SemaphoreState>> g_semaphoreRegistry;
std::unordered_map<StarbytesObject,std::unique_ptr<EventState>> g_eventRegistry;

StarbytesObject makeBool(bool value) {
    return StarbytesBoolNew(value ? StarbytesBoolTrue : StarbytesBoolFalse);
}

StarbytesObject makeInt(int value) {
    return StarbytesNumNew(NumTypeInt,value);
}

StarbytesObject makeString(const std::string &value) {
    return StarbytesStrNewWithData(value.c_str());
}

bool readIntArg(StarbytesFuncArgs args,int &outValue) {
    auto arg = StarbytesFuncArgsGetArg(args);
    if(!arg || !StarbytesObjectTypecheck(arg,StarbytesNumType()) || StarbytesNumGetType(arg) != NumTypeInt) {
        return false;
    }
    outValue = StarbytesNumGetIntValue(arg);
    return true;
}

bool readBoolArg(StarbytesFuncArgs args,bool &outValue) {
    auto arg = StarbytesFuncArgsGetArg(args);
    if(!arg || !StarbytesObjectTypecheck(arg,StarbytesBoolType())) {
        return false;
    }
    outValue = (StarbytesBoolValue(arg) == StarbytesBoolTrue);
    return true;
}

void skipOptionalModuleReceiver(StarbytesFuncArgs args,unsigned expectedUserArgs) {
    auto *raw = reinterpret_cast<NativeArgsLayout *>(args);
    if(!raw || raw->argc < raw->index) {
        return;
    }
    auto remaining = raw->argc - raw->index;
    if(remaining == expectedUserArgs + 1) {
        (void)StarbytesFuncArgsGetArg(args);
    }
}

MutexState *requireMutexSelf(StarbytesFuncArgs args) {
    auto self = StarbytesFuncArgsGetArg(args);
    if(!self) {
        return nullptr;
    }
    auto it = g_mutexRegistry.find(self);
    if(it == g_mutexRegistry.end()) {
        return nullptr;
    }
    return it->second.get();
}

ConditionState *requireConditionSelf(StarbytesFuncArgs args) {
    auto self = StarbytesFuncArgsGetArg(args);
    if(!self) {
        return nullptr;
    }
    auto it = g_conditionRegistry.find(self);
    if(it == g_conditionRegistry.end()) {
        return nullptr;
    }
    return it->second.get();
}

SemaphoreState *requireSemaphoreSelf(StarbytesFuncArgs args) {
    auto self = StarbytesFuncArgsGetArg(args);
    if(!self) {
        return nullptr;
    }
    auto it = g_semaphoreRegistry.find(self);
    if(it == g_semaphoreRegistry.end()) {
        return nullptr;
    }
    return it->second.get();
}

EventState *requireEventSelf(StarbytesFuncArgs args) {
    auto self = StarbytesFuncArgsGetArg(args);
    if(!self) {
        return nullptr;
    }
    auto it = g_eventRegistry.find(self);
    if(it == g_eventRegistry.end()) {
        return nullptr;
    }
    return it->second.get();
}

MutexState *findMutexByObject(StarbytesObject object) {
    auto it = g_mutexRegistry.find(object);
    if(it == g_mutexRegistry.end()) {
        return nullptr;
    }
    return it->second.get();
}

STARBYTES_FUNC(Threading_Mutex_lock) {
    auto *state = requireMutexSelf(args);
    if(!state) {
        return nullptr;
    }

    auto current = std::this_thread::get_id();
    if(state->locked && state->owner == current) {
        return makeBool(true);
    }

    state->native.lock();
    state->locked = true;
    state->owner = current;
    return makeBool(true);
}

STARBYTES_FUNC(Threading_Mutex_tryLock) {
    auto *state = requireMutexSelf(args);
    if(!state) {
        return makeBool(false);
    }

    auto current = std::this_thread::get_id();
    if(state->locked && state->owner == current) {
        return makeBool(true);
    }

    if(state->native.try_lock()) {
        state->locked = true;
        state->owner = current;
        return makeBool(true);
    }
    return makeBool(false);
}

STARBYTES_FUNC(Threading_Mutex_unlock) {
    auto *state = requireMutexSelf(args);
    if(!state) {
        return nullptr;
    }

    auto current = std::this_thread::get_id();
    if(!state->locked || state->owner != current) {
        return nullptr;
    }

    state->locked = false;
    state->owner = std::thread::id();
    state->native.unlock();
    return makeBool(true);
}

STARBYTES_FUNC(Threading_Condition_wait) {
    auto *condState = requireConditionSelf(args);
    if(!condState) {
        return nullptr;
    }

    auto mutexObj = StarbytesFuncArgsGetArg(args);
    int timeoutMillis = -1;
    if(!mutexObj || !readIntArg(args,timeoutMillis)) {
        return nullptr;
    }

    auto *mutexState = findMutexByObject(mutexObj);
    if(!mutexState) {
        return nullptr;
    }

    auto current = std::this_thread::get_id();
    if(!mutexState->locked || mutexState->owner != current) {
        return nullptr;
    }

    std::unique_lock<std::mutex> lock(mutexState->native,std::adopt_lock);
    mutexState->locked = false;
    mutexState->owner = std::thread::id();

    bool signaled = true;
    if(timeoutMillis < 0) {
        condState->native.wait(lock);
    }
    else {
        auto status = condState->native.wait_for(lock,std::chrono::milliseconds(timeoutMillis));
        signaled = (status != std::cv_status::timeout);
    }

    mutexState->locked = true;
    mutexState->owner = current;
    lock.release();

    return makeBool(signaled);
}

STARBYTES_FUNC(Threading_Condition_notifyOne) {
    auto *condState = requireConditionSelf(args);
    if(!condState) {
        return nullptr;
    }
    condState->native.notify_one();
    return makeBool(true);
}

STARBYTES_FUNC(Threading_Condition_notifyAll) {
    auto *condState = requireConditionSelf(args);
    if(!condState) {
        return nullptr;
    }
    condState->native.notify_all();
    return makeBool(true);
}

STARBYTES_FUNC(Threading_Semaphore_acquire) {
    auto *state = requireSemaphoreSelf(args);
    if(!state) {
        return nullptr;
    }

    int timeoutMillis = -1;
    if(!readIntArg(args,timeoutMillis)) {
        return nullptr;
    }

    std::unique_lock<std::mutex> lock(state->mutex);
    if(timeoutMillis < 0) {
        state->cv.wait(lock,[&]() { return state->count > 0; });
    }
    else {
        if(!state->cv.wait_for(lock,std::chrono::milliseconds(timeoutMillis),[&]() { return state->count > 0; })) {
            return makeBool(false);
        }
    }
    state->count -= 1;
    return makeBool(true);
}

STARBYTES_FUNC(Threading_Semaphore_release) {
    auto *state = requireSemaphoreSelf(args);
    if(!state) {
        return nullptr;
    }

    int amount = 0;
    if(!readIntArg(args,amount) || amount <= 0) {
        return nullptr;
    }

    {
        std::unique_lock<std::mutex> lock(state->mutex);
        if(state->count > state->maxCount - amount) {
            return nullptr;
        }
        state->count += amount;
    }
    state->cv.notify_all();
    return makeBool(true);
}

STARBYTES_FUNC(Threading_Semaphore_currentCount) {
    auto *state = requireSemaphoreSelf(args);
    if(!state) {
        return makeInt(0);
    }

    std::unique_lock<std::mutex> lock(state->mutex);
    return makeInt(state->count);
}

STARBYTES_FUNC(Threading_Event_wait) {
    auto *state = requireEventSelf(args);
    if(!state) {
        return nullptr;
    }

    int timeoutMillis = -1;
    if(!readIntArg(args,timeoutMillis)) {
        return nullptr;
    }

    std::unique_lock<std::mutex> lock(state->mutex);
    if(timeoutMillis < 0) {
        state->cv.wait(lock,[&]() { return state->signaled; });
    }
    else {
        if(!state->cv.wait_for(lock,std::chrono::milliseconds(timeoutMillis),[&]() { return state->signaled; })) {
            return makeBool(false);
        }
    }

    if(!state->manualReset) {
        state->signaled = false;
    }
    return makeBool(true);
}

STARBYTES_FUNC(Threading_Event_set) {
    auto *state = requireEventSelf(args);
    if(!state) {
        return nullptr;
    }

    {
        std::unique_lock<std::mutex> lock(state->mutex);
        state->signaled = true;
    }

    if(state->manualReset) {
        state->cv.notify_all();
    }
    else {
        state->cv.notify_one();
    }
    return makeBool(true);
}

STARBYTES_FUNC(Threading_Event_reset) {
    auto *state = requireEventSelf(args);
    if(!state) {
        return nullptr;
    }

    std::unique_lock<std::mutex> lock(state->mutex);
    state->signaled = false;
    return makeBool(true);
}

STARBYTES_FUNC(Threading_Event_isSet) {
    auto *state = requireEventSelf(args);
    if(!state) {
        return makeBool(false);
    }

    std::unique_lock<std::mutex> lock(state->mutex);
    return makeBool(state->signaled);
}

STARBYTES_FUNC(Threading_mutexCreate) {
    skipOptionalModuleReceiver(args,0);

    auto object = StarbytesObjectNew(StarbytesMakeClass("Mutex"));
    g_mutexRegistry[object] = std::make_unique<MutexState>();
    return object;
}

STARBYTES_FUNC(Threading_conditionCreate) {
    skipOptionalModuleReceiver(args,0);

    auto object = StarbytesObjectNew(StarbytesMakeClass("Condition"));
    g_conditionRegistry[object] = std::make_unique<ConditionState>();
    return object;
}

STARBYTES_FUNC(Threading_semaphoreCreate) {
    skipOptionalModuleReceiver(args,2);

    int initial = 0;
    int maxCount = 0;
    if(!readIntArg(args,initial) || !readIntArg(args,maxCount)) {
        return nullptr;
    }
    if(maxCount <= 0 || initial < 0 || initial > maxCount) {
        return nullptr;
    }

    auto object = StarbytesObjectNew(StarbytesMakeClass("Semaphore"));
    auto state = std::make_unique<SemaphoreState>();
    state->count = initial;
    state->maxCount = maxCount;
    g_semaphoreRegistry[object] = std::move(state);
    return object;
}

STARBYTES_FUNC(Threading_eventCreate) {
    skipOptionalModuleReceiver(args,2);

    bool initial = false;
    bool manualReset = true;
    if(!readBoolArg(args,initial) || !readBoolArg(args,manualReset)) {
        return nullptr;
    }

    auto object = StarbytesObjectNew(StarbytesMakeClass("Event"));
    auto state = std::make_unique<EventState>();
    state->signaled = initial;
    state->manualReset = manualReset;
    g_eventRegistry[object] = std::move(state);
    return object;
}

STARBYTES_FUNC(Threading_currentThreadId) {
    skipOptionalModuleReceiver(args,0);

    auto id = std::this_thread::get_id();
    auto hashed = std::hash<std::thread::id>{}(id);
    return makeString(std::to_string(hashed));
}

STARBYTES_FUNC(Threading_hardwareConcurrency) {
    skipOptionalModuleReceiver(args,0);

    auto value = std::thread::hardware_concurrency();
    if(value == 0) {
        value = 1;
    }
    return makeInt((int)value);
}

STARBYTES_FUNC(Threading_yieldNow) {
    skipOptionalModuleReceiver(args,0);

    std::this_thread::yield();
    return makeBool(true);
}

STARBYTES_FUNC(Threading_sleepMillis) {
    skipOptionalModuleReceiver(args,1);

    int ms = 0;
    if(!readIntArg(args,ms) || ms < 0) {
        return nullptr;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    return makeBool(true);
}

void addFunc(StarbytesNativeModule *module,const char *name,unsigned argCount,StarbytesFuncCallback callback) {
    StarbytesFuncDesc desc;
    desc.name = CStringMake(name);
    desc.argCount = argCount;
    desc.callback = callback;
    StarbytesNativeModuleAddDesc(module,&desc);
}

}

STARBYTES_NATIVE_MOD_MAIN() {
    auto module = StarbytesNativeModuleCreate();

    addFunc(module,"Threading_Mutex_lock",1,Threading_Mutex_lock);
    addFunc(module,"Threading_Mutex_tryLock",1,Threading_Mutex_tryLock);
    addFunc(module,"Threading_Mutex_unlock",1,Threading_Mutex_unlock);

    addFunc(module,"Threading_Condition_wait",3,Threading_Condition_wait);
    addFunc(module,"Threading_Condition_notifyOne",1,Threading_Condition_notifyOne);
    addFunc(module,"Threading_Condition_notifyAll",1,Threading_Condition_notifyAll);

    addFunc(module,"Threading_Semaphore_acquire",2,Threading_Semaphore_acquire);
    addFunc(module,"Threading_Semaphore_release",2,Threading_Semaphore_release);
    addFunc(module,"Threading_Semaphore_currentCount",1,Threading_Semaphore_currentCount);

    addFunc(module,"Threading_Event_wait",2,Threading_Event_wait);
    addFunc(module,"Threading_Event_set",1,Threading_Event_set);
    addFunc(module,"Threading_Event_reset",1,Threading_Event_reset);
    addFunc(module,"Threading_Event_isSet",1,Threading_Event_isSet);

    addFunc(module,"Threading_mutexCreate",0,Threading_mutexCreate);
    addFunc(module,"Threading_conditionCreate",0,Threading_conditionCreate);
    addFunc(module,"Threading_semaphoreCreate",2,Threading_semaphoreCreate);
    addFunc(module,"Threading_eventCreate",2,Threading_eventCreate);

    addFunc(module,"Threading_currentThreadId",0,Threading_currentThreadId);
    addFunc(module,"Threading_hardwareConcurrency",0,Threading_hardwareConcurrency);
    addFunc(module,"Threading_yieldNow",0,Threading_yieldNow);
    addFunc(module,"Threading_sleepMillis",1,Threading_sleepMillis);

    return module;
}
