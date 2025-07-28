#include "dawn/native/OHOSFunctions.h"

namespace dawn::native {

OHOSFunctions::OHOSFunctions() {
    if (!mNativeBufferLib.Open("libnative_buffer.so") ||
        !mNativeBufferLib.GetProc(&Reference, "OH_NativeBuffer_Reference") ||
        !mNativeBufferLib.GetProc(&Unreference, "OH_NativeBuffer_Unreference") ||
        !mNativeBufferLib.GetProc(&GetConfig, "OH_NativeBuffer_GetConfig")) {
            mNativeBufferLib.Close();
        }
}

OHOSFunctions::~OHOSFunctions() = default;

bool OHOSFunctions::IsValid() const {
    return mNativeBufferLib.Valid();
}

}   // namespace dawn::native