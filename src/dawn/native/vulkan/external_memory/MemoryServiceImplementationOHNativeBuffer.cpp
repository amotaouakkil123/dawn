#include "dawn/native/vulkan/external_memory/MemoryServiceImplementationOHNativeBuffer.h"
#include "dawn/common/Assert.h"
#include "dawn/native/vulkan/BackendVk.h"
#include "dawn/native/vulkan/DeviceVk.h"
#include "dawn/native/vulkan/PhysicalDeviceVk.h"
#include "dawn/native/vulkan/TextureVk.h"
#include "dawn/native/vulkan/UtilsVulkan.h"
#include "dawn/native/vulkan/VulkanError.h"
#include "dawn/native/vulkan/external_memory/MemoryServiceImplementation.h"

namespace dawn::native::vulkan::external_memory {

class ServiceImplementationOHNativeBuffer : public ServiceImplementation {
public:

    explicit ServiceImplementationOHNativeBuffer(Device* device)
        : ServiceImplementation(device), mSupported(CheckSupport(device->GetDeviceInfo())) {}
    ~ServiceImplementationOHNativeBuffer() override = default;

    static bool CheckSupport(const VulkanDeviceInfo& deviceInfo) {
        return deviceInfo.HasExt(DeviceExt::ExternalMemoryOHNativeBuffer);
    }

    bool SupportsImportMemory(VkFormat format,
                              VkImageType type,
                              VkImageTiling tiling,
                              VkImageUsageFlags usage,
                              VkImageCreateFlags flags) override {
        // Early out before we try using extension functions
        if (!mSupported) {
            return false;
        }

        VkPhysicalDeviceImageFormatInfo2 formatInfo = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2_KHR,
            .pNext = nullptr,
            .format = format,
            .type = type,
            .tiling = tiling,
            .usage = usage,
            .flags = flags,
        };

        VkPhysicalDeviceExternalImageFormatInfo externalFormatInfo = {
            .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OHOS_NATIVE_BUFFER_BIT_OHOS,
        };

        PNextChainBuilder formatInfoChain(&formatInfo);
        formatInfoChain.Add(&externalFormatInfo,
                            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO_KHR);
        
        VkImageFormatProperties2 formatProperties = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2_KHR,
            .pNext = nullptr,
        };

        VkExternalImageFormatProperties externalFormatProperties;
        PNextChainBuilder formatPropertiesChain(&formatProperties);
        formatPropertiesChain.Add(&externalFormatProperties,
                                  VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES_KHR);
                                
        VkResult result = VkResult::WrapUnsafe(mDevice->fn.GetPhysicalDeviceImageFormatProperties2(
            ToBackend(mDevice->GetPhysicalDevice())->GetVkPhysicalDevice(), &formatInfo,
            &formatProperties));
        
        // If handle not supported, result == VK_ERROR_FORMAT_NOT_SUPPORTED
        if (result != VK_SUCCESS) {
            return false;
        }

        // TODO(http://crbug.com/dawn/206): Investigate dedicated only images
        VkFlags memoryFlags =
            externalFormatProperties.externalMemoryProperties.externalMemoryFeatures;
        return (memoryFlags & VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT) != 0;
    }

    bool SupportsCreateImage(const ExternalImageDescriptor* descriptor,
                             VkFormat format,
                             VkImageUsageFlags usage,
                             bool* supportsDisjoint) override {
        *supportsDisjoint = false;
        return mSupported;
    }

    ResultOrError<MemoryImportParams> GetMemoryImportParams(
        const ExternalImageDescriptor* descriptor,
        VkImage image) override {
        DAWN_INVALID_IF(descriptor->GetType() != ExternalImageType::OHNativeBuffer,
                        "ExternalImageDescriptor is not an OHNativeBuffer descriptor.");
        const ExternalImageDescriptorOHNativeBuffer* ohNativeBufferDescriptor = 
            static_cast<const ExternalImageDescriptorOHNativeBuffer*>(descriptor);

        VkNativeBufferPropertiesOHOS bufferProperties = {
            .sType = VK_STRUCTURE_TYPE_NATIVE_BUFFER_PROPERTIES_OHOS,
            .pNext = nullptr,
        };

        PNextChainBuilder bufferPropertiesChain(&bufferProperties);
        bufferPropertiesChain.Add(
            &bufferProperties,
            VK_STRUCTURE_TYPE_NATIVE_BUFFER_FORMAT_PROPERTIES_OHOS);
        
        DAWN_TRY(CheckVkSuccess(
            mDevice->fn.GetNativeBufferPropertiesOHOS(
                mDevice->GetVkDevice(), ohNativeBufferDescriptor->handle, &bufferProperties),
            "vkGetNativeBufferPropertiesOHOS"));

        MemoryImportParams params;
        params.allocationSize = bufferProperties.allocationSize;
        params.memoryTypeIndex = bufferProperties.memoryTypeBits;
        params.dedicatedAllocation = RequiresDedicatedAllocation(ohNativeBufferDescriptor, image);
        return params;
    }

    uint32_t GetQueueFamilyIndex() override { return VK_QUEUE_FAMILY_FOREIGN_EXT; }

    ResultOrError<VkDeviceMemory> ImportMemory(ExternalMemoryHandle handle,
                                               const MemoryImportParams& importParams,
                                               VkImage image) override {
        DAWN_INVALID_IF(handle == nullptr, "Importing memory with an invalid handle.");

        VkMemoryRequirements requirements;
        mDevice->fn.GetImageMemoryRequirements(mDevice->GetVkDevice(), image, &requirements);
        DAWN_INVALID_IF(requirements.size > importParams.allocationSize,
                        "Requested allocation size (%u) is smaller than the image requires (%u).",
                        importParams.allocationSize, requirements.size);
        
        VkMemoryAllocateInfo allocateInfo = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext = nullptr,
            .allocationSize = importParams.allocationSize,
            .memoryTypeIndex = importParams.memoryTypeIndex,
        };

        PNextChainBuilder allocateInfoChain(&allocateInfo);

        VkImportNativeBufferInfoOHOS importMemoryOHNBInfo = {
            .buffer = handle,
        };
        allocateInfoChain.Add(&importMemoryOHNBInfo,
                              VK_STRUCTURE_TYPE_IMPORT_NATIVE_BUFFER_INFO_OHOS);
        VkMemoryDedicatedAllocateInfo dedicatedAllocateInfo;
        if (importParams.dedicatedAllocation) {
            dedicatedAllocateInfo.image = image;
            dedicatedAllocateInfo.buffer = VkBuffer{};
            allocateInfoChain.Add(&dedicatedAllocateInfo,
                                  VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO);
        }

        VkDeviceMemory allocateMemory = VK_NULL_HANDLE;
        DAWN_TRY(CheckVkSuccess(mDevice->fn.AllocateMemory(mDevice->GetVkDevice(), &allocateInfo,
                                                            nullptr, &*allocateMemory),
                                 "vkAllocateMemory"));
        return allocateMemory;
    }

    ResultOrError<VkImage> CreateImage(const ExternalImageDescriptor* descriptor,
                                       const VkImageCreateInfo& baseCreateInfo) override {
        VkImageCreateInfo createInfo = baseCreateInfo;
        createInfo.flags |= VK_IMAGE_CREATE_ALIAS_BIT_KHR;
        createInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        PNextChainBuilder createInfoChain(&createInfo);

        VkExternalMemoryImageCreateInfo externalMemoryImageCreateInfo = {
            .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OHOS_NATIVE_BUFFER_BIT_OHOS,
        };
        createInfoChain.Add(&externalMemoryImageCreateInfo,
                            VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO);
        
        DAWN_ASSERT(IsSampleCountSupported(mDevice, createInfo));
        VkImage image;
        DAWN_TRY(CheckVkSuccess(
            mDevice->fn.CreateImage(mDevice->GetVkDevice(), &createInfo, nullptr, &*image),
            "CreateImage"));
        return image;
    }

    bool Supported() const override { return mSupported; }

private:
    bool mSupported = false;
};

bool CheckOHNativeBufferSupport(const VulkanDeviceInfo& deviceInfo) {
    return ServiceImplementationOHNativeBuffer::CheckSupport(deviceInfo);
}

std::unique_ptr<ServiceImplementation> CreateOHNativeBufferService(Device* device) {
    return std::make_unique<ServiceImplementationOHNativeBuffer>(device);
}

}