#ifndef SRC_DAWN_PLATFORM_WORKERTHREAD_H_
#define SRC_DAWN_PLATFORM_WORKERTHREAD_H_

#include <memory>

#include "dawn/common/NonCopyable.h"
#include "dawn/platform/DawnPlatform.h"

namespace dawn::platform {

class AsyncWorkerThreadPool : public dawn::platform::WorkerTaskPool, public NonCopyable {
public:

    std::unique_ptr<dawn::platform::WaitableEvent> PostWorkerTask(
        dawn::platform::PostWorkerTaskCallback callback,
        void* userdata) override;
};

}   // namespace dawn::platform

#endif    // SRC_DAWN_PLATFORM_WORKERTHREAD_H_