#ifndef SRC_DAWN_NATIVE_OHOSFUNCTIONS_H_
#define SRC_DAWN_NATIVE_OHOSFUNCTIONS_H_

#include <native_buffer/native_buffer.h>
#include "dawn/common/DynamicLib.h"

namespace dawn::native {

// A helper class that dynamically loads the native window library on HarmonyOS
class OHOSFunctions {
public:

    OHOSFunctions();
    ~OHOSFunctions();

    bool IsValid() const;

    void (*Reference)(::OH_NativeBuffer* buffer) = nullptr;
    void (*Unreference)(::OH_NativeBuffer* buffer) = nullptr;
    void (*GetConfig)(::OH_NativeBuffer* buffer, ::OH_NativeBuffer_Config* config) = nullptr;

private:
    DynamicLib mNativeBufferLib;
};

}   // namespace dawn::native

#endif  // SRC_DAWN_NATIVE_OHOSFUNCTIONS_H_