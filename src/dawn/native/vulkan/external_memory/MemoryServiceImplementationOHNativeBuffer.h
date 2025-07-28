#ifndef SRC_DAWN_NATIVE_VULKAN_EXTERNAL_MEMORY_SERVICEIMPLEMENTATIONOHNATIVEBUFFER_H_
#define SRC_DAWN_NATIVE_VULKAN_EXTERNAL_MEMORY_SERVICEIMPLEMENTATIONOHNATIVEBUFFER_H_

#include <memory>

namespace dawn::native::vulkan {
class Device;
struct VulkanDeviceInfo;
}   // namespace dawn::native::vulkan

namespace dawn::native::vulkan::external_memory {
class ServiceImplementation;

bool CheckOHNativeBufferSupport(const VulkanDeviceInfo& deviceInfo);
std::unique_ptr<ServiceImplementation> CreateOHNativeBufferService(Device* device);

}   // namespace dawn::native::vulkan::external_memory

#endif  // SRC_DAWN_NATIVE_VULKAN_EXTERNAL_MEMORY_SERVICEIMPLMENTATIONNOHNATIVEBUFFER_H_