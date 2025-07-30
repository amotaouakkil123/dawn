#include "dawn/platform/WorkerThread.h"

#include <condition_variable>
#include <functional>
#include <thread>

#include "dawn/common/Assert.h"

namespace {

class AsyncWaitableEventImpl {
public:

    AsyncWaitableEventImpl() : mIsComplete(false) {}

    void Wait() {
        std::unique_lock<std::mutex> lock(mMutex);
        mCondition.wait(lock, [this] { return mIsComplete; })
    }

    bool IsComplete() {
        std::lock_guard<std::mutex> lock(mMutex);
        return mIsComplete;
    }

    void MarkAsComplete() {
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mIsComplete = true;
        }
        mCondition.notify_all();
    }

private:

    std::mutex mMutex;
    std::condition_variable mCondition;
    bool mIsComplete;
};

class AsyncWaitableEvent final : public dawn::platform::WaitableEvent {
public:

    AsyncWaitableEvent() : mWaitableEventImpl(std::make_shared<AsyncWaitableEventImpl>()) {}

    void Wait() override { mWaitableEventImpl->Wait(); }

    bool IsComplete() override { return mWaitableEventImpl->IsComplete(); }

    std::shared_ptr<AsyncWaitableEventImpl> GetWaitableEventImpl() const {
        return mWaitableEventImpl;
    }

private:

    std::shared_ptr<AsyncWaitableEventImpl> mWaitableEventImpl;
};

} // anonymous namespace

namespace dawn::platform {

std::unique_ptr<dawn::platform::WaitableEvent> AsyncWorkerThreadPool::PostWorkerTask(
    dawn::platform::PostWorkerTaskCallback callback,
    void* userdata) {
    std::unique_ptr<AsyncWaitableEvent> waitableEvent = std::make_unique<AsyncWaitableEvent>();

    std::function<void()> doTask = [callback, userdata,
                                    waitableEventImpl = waitableEvent->GetWaitableEventImpl()]() {
        callback(userdata);
        waitableEventImpl->MarkAsComplete();
    };

    std::thread thread(doTask);
    thread.detach();

    return waitableEvent;
}

}   // namespace dawn::platform