// This file is generated.
#include "VulkanLoader.h"
#include "defines.h"
#include <vector>
#include <algorithm>
#include <iostream>
#include <cstdlib>

namespace vk {
    SVUlkanContext      g_vulkanContext;

    PFN_vkCreateInstance CreateInstance;
    PFN_vkDestroyInstance DestroyInstance;
    PFN_vkEnumeratePhysicalDevices EnumeratePhysicalDevices;
    PFN_vkGetPhysicalDeviceFeatures GetPhysicalDeviceFeatures;
    PFN_vkGetPhysicalDeviceFormatProperties GetPhysicalDeviceFormatProperties;
    PFN_vkGetPhysicalDeviceImageFormatProperties GetPhysicalDeviceImageFormatProperties;
    PFN_vkGetPhysicalDeviceProperties GetPhysicalDeviceProperties;
    PFN_vkGetPhysicalDeviceQueueFamilyProperties GetPhysicalDeviceQueueFamilyProperties;
    PFN_vkGetPhysicalDeviceMemoryProperties GetPhysicalDeviceMemoryProperties;
    PFN_vkGetInstanceProcAddr GetInstanceProcAddr;
    PFN_vkGetDeviceProcAddr GetDeviceProcAddr;
    PFN_vkCreateDevice CreateDevice;
    PFN_vkDestroyDevice DestroyDevice;
    PFN_vkEnumerateInstanceExtensionProperties EnumerateInstanceExtensionProperties;
    PFN_vkEnumerateDeviceExtensionProperties EnumerateDeviceExtensionProperties;
    PFN_vkEnumerateDeviceLayerProperties EnumerateDeviceLayerProperties;
    PFN_vkEnumerateInstanceLayerProperties EnumerateInstanceLayerProperties;
    PFN_vkGetDeviceQueue GetDeviceQueue;
    PFN_vkQueueSubmit QueueSubmit;
    PFN_vkQueueWaitIdle QueueWaitIdle;
    PFN_vkDeviceWaitIdle DeviceWaitIdle;
    PFN_vkAllocateMemory AllocateMemory;
    PFN_vkFreeMemory FreeMemory;
    PFN_vkMapMemory MapMemory;
    PFN_vkUnmapMemory UnmapMemory;
    PFN_vkFlushMappedMemoryRanges FlushMappedMemoryRanges;
    PFN_vkInvalidateMappedMemoryRanges InvalidateMappedMemoryRanges;
    PFN_vkGetDeviceMemoryCommitment GetDeviceMemoryCommitment;
    PFN_vkBindBufferMemory BindBufferMemory;
    PFN_vkBindImageMemory BindImageMemory;
    PFN_vkGetBufferMemoryRequirements GetBufferMemoryRequirements;
    PFN_vkGetImageMemoryRequirements GetImageMemoryRequirements;
    PFN_vkGetImageSparseMemoryRequirements GetImageSparseMemoryRequirements;
    PFN_vkGetPhysicalDeviceSparseImageFormatProperties GetPhysicalDeviceSparseImageFormatProperties;
    PFN_vkQueueBindSparse QueueBindSparse;
    PFN_vkCreateFence CreateFence;
    PFN_vkDestroyFence DestroyFence;
    PFN_vkResetFences ResetFences;
    PFN_vkGetFenceStatus GetFenceStatus;
    PFN_vkWaitForFences WaitForFences;
    PFN_vkCreateSemaphore CreateSemaphore;
    PFN_vkDestroySemaphore DestroySemaphore;
    PFN_vkCreateEvent CreateEvent;
    PFN_vkDestroyEvent DestroyEvent;
    PFN_vkGetEventStatus GetEventStatus;
    PFN_vkSetEvent SetEvent;
    PFN_vkResetEvent ResetEvent;
    PFN_vkCreateQueryPool CreateQueryPool;
    PFN_vkDestroyQueryPool DestroyQueryPool;
    PFN_vkGetQueryPoolResults GetQueryPoolResults;
    PFN_vkCreateBuffer CreateBuffer;
    PFN_vkDestroyBuffer DestroyBuffer;
    PFN_vkCreateBufferView CreateBufferView;
    PFN_vkDestroyBufferView DestroyBufferView;
    PFN_vkCreateImage CreateImage;
    PFN_vkDestroyImage DestroyImage;
    PFN_vkGetImageSubresourceLayout GetImageSubresourceLayout;
    PFN_vkCreateImageView CreateImageView;
    PFN_vkDestroyImageView DestroyImageView;
    PFN_vkCreateShaderModule CreateShaderModule;
    PFN_vkDestroyShaderModule DestroyShaderModule;
    PFN_vkCreatePipelineCache CreatePipelineCache;
    PFN_vkDestroyPipelineCache DestroyPipelineCache;
    PFN_vkGetPipelineCacheData GetPipelineCacheData;
    PFN_vkMergePipelineCaches MergePipelineCaches;
    PFN_vkCreateGraphicsPipelines CreateGraphicsPipelines;
    PFN_vkCreateComputePipelines CreateComputePipelines;
    PFN_vkDestroyPipeline DestroyPipeline;
    PFN_vkCreatePipelineLayout CreatePipelineLayout;
    PFN_vkDestroyPipelineLayout DestroyPipelineLayout;
    PFN_vkCreateSampler CreateSampler;
    PFN_vkDestroySampler DestroySampler;
    PFN_vkCreateDescriptorSetLayout CreateDescriptorSetLayout;
    PFN_vkDestroyDescriptorSetLayout DestroyDescriptorSetLayout;
    PFN_vkCreateDescriptorPool CreateDescriptorPool;
    PFN_vkDestroyDescriptorPool DestroyDescriptorPool;
    PFN_vkResetDescriptorPool ResetDescriptorPool;
    PFN_vkAllocateDescriptorSets AllocateDescriptorSets;
    PFN_vkFreeDescriptorSets FreeDescriptorSets;
    PFN_vkUpdateDescriptorSets UpdateDescriptorSets;
    PFN_vkCreateFramebuffer CreateFramebuffer;
    PFN_vkDestroyFramebuffer DestroyFramebuffer;
    PFN_vkCreateRenderPass CreateRenderPass;
    PFN_vkDestroyRenderPass DestroyRenderPass;
    PFN_vkGetRenderAreaGranularity GetRenderAreaGranularity;
    PFN_vkCreateCommandPool CreateCommandPool;
    PFN_vkDestroyCommandPool DestroyCommandPool;
    PFN_vkResetCommandPool ResetCommandPool;
    PFN_vkAllocateCommandBuffers AllocateCommandBuffers;
    PFN_vkFreeCommandBuffers FreeCommandBuffers;
    PFN_vkBeginCommandBuffer BeginCommandBuffer;
    PFN_vkEndCommandBuffer EndCommandBuffer;
    PFN_vkResetCommandBuffer ResetCommandBuffer;
    PFN_vkCmdBindPipeline CmdBindPipeline;
    PFN_vkCmdSetViewport CmdSetViewport;
    PFN_vkCmdSetScissor CmdSetScissor;
    PFN_vkCmdSetLineWidth CmdSetLineWidth;
    PFN_vkCmdSetDepthBias CmdSetDepthBias;
    PFN_vkCmdSetBlendConstants CmdSetBlendConstants;
    PFN_vkCmdSetDepthBounds CmdSetDepthBounds;
    PFN_vkCmdSetStencilCompareMask CmdSetStencilCompareMask;
    PFN_vkCmdSetStencilWriteMask CmdSetStencilWriteMask;
    PFN_vkCmdSetStencilReference CmdSetStencilReference;
    PFN_vkCmdBindDescriptorSets CmdBindDescriptorSets;
    PFN_vkCmdBindIndexBuffer CmdBindIndexBuffer;
    PFN_vkCmdBindVertexBuffers CmdBindVertexBuffers;
    PFN_vkCmdDraw CmdDraw;
    PFN_vkCmdDrawIndexed CmdDrawIndexed;
    PFN_vkCmdDrawIndirect CmdDrawIndirect;
    PFN_vkCmdDrawIndexedIndirect CmdDrawIndexedIndirect;
    PFN_vkCmdDispatch CmdDispatch;
    PFN_vkCmdDispatchIndirect CmdDispatchIndirect;
    PFN_vkCmdCopyBuffer CmdCopyBuffer;
    PFN_vkCmdCopyImage CmdCopyImage;
    PFN_vkCmdBlitImage CmdBlitImage;
    PFN_vkCmdCopyBufferToImage CmdCopyBufferToImage;
    PFN_vkCmdCopyImageToBuffer CmdCopyImageToBuffer;
    PFN_vkCmdUpdateBuffer CmdUpdateBuffer;
    PFN_vkCmdFillBuffer CmdFillBuffer;
    PFN_vkCmdClearColorImage CmdClearColorImage;
    PFN_vkCmdClearDepthStencilImage CmdClearDepthStencilImage;
    PFN_vkCmdClearAttachments CmdClearAttachments;
    PFN_vkCmdResolveImage CmdResolveImage;
    PFN_vkCmdSetEvent CmdSetEvent;
    PFN_vkCmdResetEvent CmdResetEvent;
    PFN_vkCmdWaitEvents CmdWaitEvents;
    PFN_vkCmdPipelineBarrier CmdPipelineBarrier;
    PFN_vkCmdBeginQuery CmdBeginQuery;
    PFN_vkCmdEndQuery CmdEndQuery;
    PFN_vkCmdResetQueryPool CmdResetQueryPool;
    PFN_vkCmdWriteTimestamp CmdWriteTimestamp;
    PFN_vkCmdCopyQueryPoolResults CmdCopyQueryPoolResults;
    PFN_vkCmdPushConstants CmdPushConstants;
    PFN_vkCmdBeginRenderPass CmdBeginRenderPass;
    PFN_vkCmdNextSubpass CmdNextSubpass;
    PFN_vkCmdEndRenderPass CmdEndRenderPass;
    PFN_vkCmdExecuteCommands CmdExecuteCommands;
    PFN_vkDestroySurfaceKHR DestroySurfaceKHR;
    PFN_vkGetPhysicalDeviceSurfaceSupportKHR GetPhysicalDeviceSurfaceSupportKHR;
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR GetPhysicalDeviceSurfaceCapabilitiesKHR;
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR GetPhysicalDeviceSurfaceFormatsKHR;
    PFN_vkGetPhysicalDeviceSurfacePresentModesKHR GetPhysicalDeviceSurfacePresentModesKHR;
    PFN_vkCreateSwapchainKHR CreateSwapchainKHR;
    PFN_vkDestroySwapchainKHR DestroySwapchainKHR;
    PFN_vkGetSwapchainImagesKHR GetSwapchainImagesKHR;
    PFN_vkAcquireNextImageKHR AcquireNextImageKHR;
    PFN_vkQueuePresentKHR QueuePresentKHR;
    PFN_vkGetPhysicalDeviceDisplayPropertiesKHR GetPhysicalDeviceDisplayPropertiesKHR;
    PFN_vkGetPhysicalDeviceDisplayPlanePropertiesKHR GetPhysicalDeviceDisplayPlanePropertiesKHR;
    PFN_vkGetDisplayPlaneSupportedDisplaysKHR GetDisplayPlaneSupportedDisplaysKHR;
    PFN_vkGetDisplayModePropertiesKHR GetDisplayModePropertiesKHR;
    PFN_vkCreateDisplayModeKHR CreateDisplayModeKHR;
    PFN_vkGetDisplayPlaneCapabilitiesKHR GetDisplayPlaneCapabilitiesKHR;
    PFN_vkCreateDisplayPlaneSurfaceKHR CreateDisplayPlaneSurfaceKHR;
    PFN_vkCreateSharedSwapchainsKHR CreateSharedSwapchainsKHR;
#ifdef VK_USE_PLATFORM_XLIB_KHR
    PFN_vkCreateXlibSurfaceKHR CreateXlibSurfaceKHR;
    PFN_vkGetPhysicalDeviceXlibPresentationSupportKHR GetPhysicalDeviceXlibPresentationSupportKHR;
#endif
#ifdef VK_USE_PLATFORM_XCB_KHR
    PFN_vkCreateXcbSurfaceKHR CreateXcbSurfaceKHR;
    PFN_vkGetPhysicalDeviceXcbPresentationSupportKHR GetPhysicalDeviceXcbPresentationSupportKHR;
#endif
#ifdef VK_USE_PLATFORM_WAYLAND_KHR
    PFN_vkCreateWaylandSurfaceKHR CreateWaylandSurfaceKHR;
    PFN_vkGetPhysicalDeviceWaylandPresentationSupportKHR GetPhysicalDeviceWaylandPresentationSupportKHR;
#endif
#ifdef VK_USE_PLATFORM_MIR_KHR
    PFN_vkCreateMirSurfaceKHR CreateMirSurfaceKHR;
    PFN_vkGetPhysicalDeviceMirPresentationSupportKHR GetPhysicalDeviceMirPresentationSupportKHR;
#endif
#ifdef VK_USE_PLATFORM_ANDROID_KHR
    PFN_vkCreateAndroidSurfaceKHR CreateAndroidSurfaceKHR;
#endif
#ifdef VK_USE_PLATFORM_WIN32_KHR
    PFN_vkCreateWin32SurfaceKHR CreateWin32SurfaceKHR;
    PFN_vkGetPhysicalDeviceWin32PresentationSupportKHR GetPhysicalDeviceWin32PresentationSupportKHR;
#endif
    PFN_vkCreateDebugReportCallbackEXT CreateDebugReportCallbackEXT;
    PFN_vkDestroyDebugReportCallbackEXT DestroyDebugReportCallbackEXT;
    PFN_vkDebugReportMessageEXT DebugReportMessageEXT;
    
    PFN_vkCmdDebugMarkerBeginEXT CmdDebugMarkerBeginEXT;
    PFN_vkCmdDebugMarkerEndEXT CmdDebugMarkerEndEXT;
    PFN_vkCmdDebugMarkerInsertEXT CmdDebugMarkerInsertEXT;
    PFN_vkDebugMarkerSetObjectNameEXT DebugMarkerSetObjectNameEXT;
    PFN_vkDebugMarkerSetObjectTagEXT DebugMarkerSetObjectTagEXT;

    void init_dispatch_table_top()
    {
        CreateInstance = reinterpret_cast<PFN_vkCreateInstance>(GetInstanceProcAddr(VK_NULL_HANDLE, "vkCreateInstance"));
        EnumerateInstanceExtensionProperties = reinterpret_cast<PFN_vkEnumerateInstanceExtensionProperties>(GetInstanceProcAddr(VK_NULL_HANDLE, "vkEnumerateInstanceExtensionProperties"));
        EnumerateInstanceLayerProperties = reinterpret_cast<PFN_vkEnumerateInstanceLayerProperties>(GetInstanceProcAddr(VK_NULL_HANDLE, "vkEnumerateInstanceLayerProperties"));
    }

    void init_dispatch_table_middle(VkInstance instance)
    {
        GetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(GetInstanceProcAddr(instance, "vkGetInstanceProcAddr"));

        DestroyInstance = reinterpret_cast<PFN_vkDestroyInstance>(GetInstanceProcAddr(instance, "vkDestroyInstance"));
        EnumeratePhysicalDevices = reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(GetInstanceProcAddr(instance, "vkEnumeratePhysicalDevices"));
        GetPhysicalDeviceFeatures = reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures>(GetInstanceProcAddr(instance, "vkGetPhysicalDeviceFeatures"));
        GetPhysicalDeviceFormatProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceFormatProperties>(GetInstanceProcAddr(instance, "vkGetPhysicalDeviceFormatProperties"));
        GetPhysicalDeviceImageFormatProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceImageFormatProperties>(GetInstanceProcAddr(instance, "vkGetPhysicalDeviceImageFormatProperties"));
        GetPhysicalDeviceProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(GetInstanceProcAddr(instance, "vkGetPhysicalDeviceProperties"));
        GetPhysicalDeviceQueueFamilyProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(GetInstanceProcAddr(instance, "vkGetPhysicalDeviceQueueFamilyProperties"));
        GetPhysicalDeviceMemoryProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(GetInstanceProcAddr(instance, "vkGetPhysicalDeviceMemoryProperties"));
        CreateDevice = reinterpret_cast<PFN_vkCreateDevice>(GetInstanceProcAddr(instance, "vkCreateDevice"));
        EnumerateDeviceExtensionProperties = reinterpret_cast<PFN_vkEnumerateDeviceExtensionProperties>(GetInstanceProcAddr(instance, "vkEnumerateDeviceExtensionProperties"));
        EnumerateDeviceLayerProperties = reinterpret_cast<PFN_vkEnumerateDeviceLayerProperties>(GetInstanceProcAddr(instance, "vkEnumerateDeviceLayerProperties"));
        GetPhysicalDeviceSparseImageFormatProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceSparseImageFormatProperties>(GetInstanceProcAddr(instance, "vkGetPhysicalDeviceSparseImageFormatProperties"));
        DestroySurfaceKHR = reinterpret_cast<PFN_vkDestroySurfaceKHR>(GetInstanceProcAddr(instance, "vkDestroySurfaceKHR"));
        GetPhysicalDeviceSurfaceSupportKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceSupportKHR>(GetInstanceProcAddr(instance, "vkGetPhysicalDeviceSurfaceSupportKHR"));
        GetPhysicalDeviceSurfaceCapabilitiesKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR>(GetInstanceProcAddr(instance, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR"));
        GetPhysicalDeviceSurfaceFormatsKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceFormatsKHR>(GetInstanceProcAddr(instance, "vkGetPhysicalDeviceSurfaceFormatsKHR"));
        GetPhysicalDeviceSurfacePresentModesKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfacePresentModesKHR>(GetInstanceProcAddr(instance, "vkGetPhysicalDeviceSurfacePresentModesKHR"));
        GetPhysicalDeviceDisplayPropertiesKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceDisplayPropertiesKHR>(GetInstanceProcAddr(instance, "vkGetPhysicalDeviceDisplayPropertiesKHR"));
        GetPhysicalDeviceDisplayPlanePropertiesKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceDisplayPlanePropertiesKHR>(GetInstanceProcAddr(instance, "vkGetPhysicalDeviceDisplayPlanePropertiesKHR"));
        GetDisplayPlaneSupportedDisplaysKHR = reinterpret_cast<PFN_vkGetDisplayPlaneSupportedDisplaysKHR>(GetInstanceProcAddr(instance, "vkGetDisplayPlaneSupportedDisplaysKHR"));
        GetDisplayModePropertiesKHR = reinterpret_cast<PFN_vkGetDisplayModePropertiesKHR>(GetInstanceProcAddr(instance, "vkGetDisplayModePropertiesKHR"));
        CreateDisplayModeKHR = reinterpret_cast<PFN_vkCreateDisplayModeKHR>(GetInstanceProcAddr(instance, "vkCreateDisplayModeKHR"));
        GetDisplayPlaneCapabilitiesKHR = reinterpret_cast<PFN_vkGetDisplayPlaneCapabilitiesKHR>(GetInstanceProcAddr(instance, "vkGetDisplayPlaneCapabilitiesKHR"));
        CreateDisplayPlaneSurfaceKHR = reinterpret_cast<PFN_vkCreateDisplayPlaneSurfaceKHR>(GetInstanceProcAddr(instance, "vkCreateDisplayPlaneSurfaceKHR"));
#ifdef VK_USE_PLATFORM_XLIB_KHR
        CreateXlibSurfaceKHR = reinterpret_cast<PFN_vkCreateXlibSurfaceKHR>(GetInstanceProcAddr(instance, "vkCreateXlibSurfaceKHR"));
#endif
#ifdef VK_USE_PLATFORM_XLIB_KHR
        GetPhysicalDeviceXlibPresentationSupportKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceXlibPresentationSupportKHR>(GetInstanceProcAddr(instance, "vkGetPhysicalDeviceXlibPresentationSupportKHR"));
#endif
#ifdef VK_USE_PLATFORM_XCB_KHR
        CreateXcbSurfaceKHR = reinterpret_cast<PFN_vkCreateXcbSurfaceKHR>(GetInstanceProcAddr(instance, "vkCreateXcbSurfaceKHR"));
#endif
#ifdef VK_USE_PLATFORM_XCB_KHR
        GetPhysicalDeviceXcbPresentationSupportKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceXcbPresentationSupportKHR>(GetInstanceProcAddr(instance, "vkGetPhysicalDeviceXcbPresentationSupportKHR"));
#endif
#ifdef VK_USE_PLATFORM_WAYLAND_KHR
        CreateWaylandSurfaceKHR = reinterpret_cast<PFN_vkCreateWaylandSurfaceKHR>(GetInstanceProcAddr(instance, "vkCreateWaylandSurfaceKHR"));
#endif
#ifdef VK_USE_PLATFORM_WAYLAND_KHR
        GetPhysicalDeviceWaylandPresentationSupportKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceWaylandPresentationSupportKHR>(GetInstanceProcAddr(instance, "vkGetPhysicalDeviceWaylandPresentationSupportKHR"));
#endif
#ifdef VK_USE_PLATFORM_MIR_KHR
        CreateMirSurfaceKHR = reinterpret_cast<PFN_vkCreateMirSurfaceKHR>(GetInstanceProcAddr(instance, "vkCreateMirSurfaceKHR"));
#endif
#ifdef VK_USE_PLATFORM_MIR_KHR
        GetPhysicalDeviceMirPresentationSupportKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceMirPresentationSupportKHR>(GetInstanceProcAddr(instance, "vkGetPhysicalDeviceMirPresentationSupportKHR"));
#endif
#ifdef VK_USE_PLATFORM_ANDROID_KHR
        CreateAndroidSurfaceKHR = reinterpret_cast<PFN_vkCreateAndroidSurfaceKHR>(GetInstanceProcAddr(instance, "vkCreateAndroidSurfaceKHR"));
#endif
#ifdef VK_USE_PLATFORM_WIN32_KHR
        CreateWin32SurfaceKHR = reinterpret_cast<PFN_vkCreateWin32SurfaceKHR>(GetInstanceProcAddr(instance, "vkCreateWin32SurfaceKHR"));
#endif
#ifdef VK_USE_PLATFORM_WIN32_KHR
        GetPhysicalDeviceWin32PresentationSupportKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceWin32PresentationSupportKHR>(GetInstanceProcAddr(instance, "vkGetPhysicalDeviceWin32PresentationSupportKHR"));
#endif
        CreateDebugReportCallbackEXT = reinterpret_cast<PFN_vkCreateDebugReportCallbackEXT>(GetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT"));
        DestroyDebugReportCallbackEXT = reinterpret_cast<PFN_vkDestroyDebugReportCallbackEXT>(GetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT"));
        DebugReportMessageEXT = reinterpret_cast<PFN_vkDebugReportMessageEXT>(GetInstanceProcAddr(instance, "vkDebugReportMessageEXT"));
#ifdef VK_EXT_debug_marker
        CmdDebugMarkerBeginEXT = reinterpret_cast<PFN_vkCmdDebugMarkerBeginEXT>(GetInstanceProcAddr(instance, "vkCmdDebugMarkerBeginEXT"));
        CmdDebugMarkerEndEXT = reinterpret_cast<PFN_vkCmdDebugMarkerEndEXT>(GetInstanceProcAddr(instance, "vkCmdDebugMarkerEndEXT"));
        CmdDebugMarkerInsertEXT = reinterpret_cast<PFN_vkCmdDebugMarkerInsertEXT>(GetInstanceProcAddr(instance, "vkCmdDebugMarkerInsertEXT"));
        DebugMarkerSetObjectTagEXT = reinterpret_cast<PFN_vkDebugMarkerSetObjectTagEXT>(GetInstanceProcAddr(instance, "vkDebugMarkerSetObjectTagEXT"));
        DebugMarkerSetObjectNameEXT = reinterpret_cast<PFN_vkDebugMarkerSetObjectNameEXT>(GetInstanceProcAddr(instance, "vkDebugMarkerSetObjectNameEXT"));
#endif
    }

    void init_dispatch_table_bottom(VkInstance instance, VkDevice dev)
    {
        GetDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(GetInstanceProcAddr(instance, "vkGetDeviceProcAddr"));
        GetDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(GetDeviceProcAddr(dev, "vkGetDeviceProcAddr"));

        DestroyDevice = reinterpret_cast<PFN_vkDestroyDevice>(GetDeviceProcAddr(dev, "vkDestroyDevice"));
        GetDeviceQueue = reinterpret_cast<PFN_vkGetDeviceQueue>(GetDeviceProcAddr(dev, "vkGetDeviceQueue"));
        QueueSubmit = reinterpret_cast<PFN_vkQueueSubmit>(GetDeviceProcAddr(dev, "vkQueueSubmit"));
        QueueWaitIdle = reinterpret_cast<PFN_vkQueueWaitIdle>(GetDeviceProcAddr(dev, "vkQueueWaitIdle"));
        DeviceWaitIdle = reinterpret_cast<PFN_vkDeviceWaitIdle>(GetDeviceProcAddr(dev, "vkDeviceWaitIdle"));
        AllocateMemory = reinterpret_cast<PFN_vkAllocateMemory>(GetDeviceProcAddr(dev, "vkAllocateMemory"));
        FreeMemory = reinterpret_cast<PFN_vkFreeMemory>(GetDeviceProcAddr(dev, "vkFreeMemory"));
        MapMemory = reinterpret_cast<PFN_vkMapMemory>(GetDeviceProcAddr(dev, "vkMapMemory"));
        UnmapMemory = reinterpret_cast<PFN_vkUnmapMemory>(GetDeviceProcAddr(dev, "vkUnmapMemory"));
        FlushMappedMemoryRanges = reinterpret_cast<PFN_vkFlushMappedMemoryRanges>(GetDeviceProcAddr(dev, "vkFlushMappedMemoryRanges"));
        InvalidateMappedMemoryRanges = reinterpret_cast<PFN_vkInvalidateMappedMemoryRanges>(GetDeviceProcAddr(dev, "vkInvalidateMappedMemoryRanges"));
        GetDeviceMemoryCommitment = reinterpret_cast<PFN_vkGetDeviceMemoryCommitment>(GetDeviceProcAddr(dev, "vkGetDeviceMemoryCommitment"));
        BindBufferMemory = reinterpret_cast<PFN_vkBindBufferMemory>(GetDeviceProcAddr(dev, "vkBindBufferMemory"));
        BindImageMemory = reinterpret_cast<PFN_vkBindImageMemory>(GetDeviceProcAddr(dev, "vkBindImageMemory"));
        GetBufferMemoryRequirements = reinterpret_cast<PFN_vkGetBufferMemoryRequirements>(GetDeviceProcAddr(dev, "vkGetBufferMemoryRequirements"));
        GetImageMemoryRequirements = reinterpret_cast<PFN_vkGetImageMemoryRequirements>(GetDeviceProcAddr(dev, "vkGetImageMemoryRequirements"));
        GetImageSparseMemoryRequirements = reinterpret_cast<PFN_vkGetImageSparseMemoryRequirements>(GetDeviceProcAddr(dev, "vkGetImageSparseMemoryRequirements"));
        QueueBindSparse = reinterpret_cast<PFN_vkQueueBindSparse>(GetDeviceProcAddr(dev, "vkQueueBindSparse"));
        CreateFence = reinterpret_cast<PFN_vkCreateFence>(GetDeviceProcAddr(dev, "vkCreateFence"));
        DestroyFence = reinterpret_cast<PFN_vkDestroyFence>(GetDeviceProcAddr(dev, "vkDestroyFence"));
        ResetFences = reinterpret_cast<PFN_vkResetFences>(GetDeviceProcAddr(dev, "vkResetFences"));
        GetFenceStatus = reinterpret_cast<PFN_vkGetFenceStatus>(GetDeviceProcAddr(dev, "vkGetFenceStatus"));
        WaitForFences = reinterpret_cast<PFN_vkWaitForFences>(GetDeviceProcAddr(dev, "vkWaitForFences"));
        CreateSemaphore = reinterpret_cast<PFN_vkCreateSemaphore>(GetDeviceProcAddr(dev, "vkCreateSemaphore"));
        DestroySemaphore = reinterpret_cast<PFN_vkDestroySemaphore>(GetDeviceProcAddr(dev, "vkDestroySemaphore"));
        CreateEvent = reinterpret_cast<PFN_vkCreateEvent>(GetDeviceProcAddr(dev, "vkCreateEvent"));
        DestroyEvent = reinterpret_cast<PFN_vkDestroyEvent>(GetDeviceProcAddr(dev, "vkDestroyEvent"));
        GetEventStatus = reinterpret_cast<PFN_vkGetEventStatus>(GetDeviceProcAddr(dev, "vkGetEventStatus"));
        SetEvent = reinterpret_cast<PFN_vkSetEvent>(GetDeviceProcAddr(dev, "vkSetEvent"));
        ResetEvent = reinterpret_cast<PFN_vkResetEvent>(GetDeviceProcAddr(dev, "vkResetEvent"));
        CreateQueryPool = reinterpret_cast<PFN_vkCreateQueryPool>(GetDeviceProcAddr(dev, "vkCreateQueryPool"));
        DestroyQueryPool = reinterpret_cast<PFN_vkDestroyQueryPool>(GetDeviceProcAddr(dev, "vkDestroyQueryPool"));
        GetQueryPoolResults = reinterpret_cast<PFN_vkGetQueryPoolResults>(GetDeviceProcAddr(dev, "vkGetQueryPoolResults"));
        CreateBuffer = reinterpret_cast<PFN_vkCreateBuffer>(GetDeviceProcAddr(dev, "vkCreateBuffer"));
        DestroyBuffer = reinterpret_cast<PFN_vkDestroyBuffer>(GetDeviceProcAddr(dev, "vkDestroyBuffer"));
        CreateBufferView = reinterpret_cast<PFN_vkCreateBufferView>(GetDeviceProcAddr(dev, "vkCreateBufferView"));
        DestroyBufferView = reinterpret_cast<PFN_vkDestroyBufferView>(GetDeviceProcAddr(dev, "vkDestroyBufferView"));
        CreateImage = reinterpret_cast<PFN_vkCreateImage>(GetDeviceProcAddr(dev, "vkCreateImage"));
        DestroyImage = reinterpret_cast<PFN_vkDestroyImage>(GetDeviceProcAddr(dev, "vkDestroyImage"));
        GetImageSubresourceLayout = reinterpret_cast<PFN_vkGetImageSubresourceLayout>(GetDeviceProcAddr(dev, "vkGetImageSubresourceLayout"));
        CreateImageView = reinterpret_cast<PFN_vkCreateImageView>(GetDeviceProcAddr(dev, "vkCreateImageView"));
        DestroyImageView = reinterpret_cast<PFN_vkDestroyImageView>(GetDeviceProcAddr(dev, "vkDestroyImageView"));
        CreateShaderModule = reinterpret_cast<PFN_vkCreateShaderModule>(GetDeviceProcAddr(dev, "vkCreateShaderModule"));
        DestroyShaderModule = reinterpret_cast<PFN_vkDestroyShaderModule>(GetDeviceProcAddr(dev, "vkDestroyShaderModule"));
        CreatePipelineCache = reinterpret_cast<PFN_vkCreatePipelineCache>(GetDeviceProcAddr(dev, "vkCreatePipelineCache"));
        DestroyPipelineCache = reinterpret_cast<PFN_vkDestroyPipelineCache>(GetDeviceProcAddr(dev, "vkDestroyPipelineCache"));
        GetPipelineCacheData = reinterpret_cast<PFN_vkGetPipelineCacheData>(GetDeviceProcAddr(dev, "vkGetPipelineCacheData"));
        MergePipelineCaches = reinterpret_cast<PFN_vkMergePipelineCaches>(GetDeviceProcAddr(dev, "vkMergePipelineCaches"));
        CreateGraphicsPipelines = reinterpret_cast<PFN_vkCreateGraphicsPipelines>(GetDeviceProcAddr(dev, "vkCreateGraphicsPipelines"));
        CreateComputePipelines = reinterpret_cast<PFN_vkCreateComputePipelines>(GetDeviceProcAddr(dev, "vkCreateComputePipelines"));
        DestroyPipeline = reinterpret_cast<PFN_vkDestroyPipeline>(GetDeviceProcAddr(dev, "vkDestroyPipeline"));
        CreatePipelineLayout = reinterpret_cast<PFN_vkCreatePipelineLayout>(GetDeviceProcAddr(dev, "vkCreatePipelineLayout"));
        DestroyPipelineLayout = reinterpret_cast<PFN_vkDestroyPipelineLayout>(GetDeviceProcAddr(dev, "vkDestroyPipelineLayout"));
        CreateSampler = reinterpret_cast<PFN_vkCreateSampler>(GetDeviceProcAddr(dev, "vkCreateSampler"));
        DestroySampler = reinterpret_cast<PFN_vkDestroySampler>(GetDeviceProcAddr(dev, "vkDestroySampler"));
        CreateDescriptorSetLayout = reinterpret_cast<PFN_vkCreateDescriptorSetLayout>(GetDeviceProcAddr(dev, "vkCreateDescriptorSetLayout"));
        DestroyDescriptorSetLayout = reinterpret_cast<PFN_vkDestroyDescriptorSetLayout>(GetDeviceProcAddr(dev, "vkDestroyDescriptorSetLayout"));
        CreateDescriptorPool = reinterpret_cast<PFN_vkCreateDescriptorPool>(GetDeviceProcAddr(dev, "vkCreateDescriptorPool"));
        DestroyDescriptorPool = reinterpret_cast<PFN_vkDestroyDescriptorPool>(GetDeviceProcAddr(dev, "vkDestroyDescriptorPool"));
        ResetDescriptorPool = reinterpret_cast<PFN_vkResetDescriptorPool>(GetDeviceProcAddr(dev, "vkResetDescriptorPool"));
        AllocateDescriptorSets = reinterpret_cast<PFN_vkAllocateDescriptorSets>(GetDeviceProcAddr(dev, "vkAllocateDescriptorSets"));
        FreeDescriptorSets = reinterpret_cast<PFN_vkFreeDescriptorSets>(GetDeviceProcAddr(dev, "vkFreeDescriptorSets"));
        UpdateDescriptorSets = reinterpret_cast<PFN_vkUpdateDescriptorSets>(GetDeviceProcAddr(dev, "vkUpdateDescriptorSets"));
        CreateFramebuffer = reinterpret_cast<PFN_vkCreateFramebuffer>(GetDeviceProcAddr(dev, "vkCreateFramebuffer"));
        DestroyFramebuffer = reinterpret_cast<PFN_vkDestroyFramebuffer>(GetDeviceProcAddr(dev, "vkDestroyFramebuffer"));
        CreateRenderPass = reinterpret_cast<PFN_vkCreateRenderPass>(GetDeviceProcAddr(dev, "vkCreateRenderPass"));
        DestroyRenderPass = reinterpret_cast<PFN_vkDestroyRenderPass>(GetDeviceProcAddr(dev, "vkDestroyRenderPass"));
        GetRenderAreaGranularity = reinterpret_cast<PFN_vkGetRenderAreaGranularity>(GetDeviceProcAddr(dev, "vkGetRenderAreaGranularity"));
        CreateCommandPool = reinterpret_cast<PFN_vkCreateCommandPool>(GetDeviceProcAddr(dev, "vkCreateCommandPool"));
        DestroyCommandPool = reinterpret_cast<PFN_vkDestroyCommandPool>(GetDeviceProcAddr(dev, "vkDestroyCommandPool"));
        ResetCommandPool = reinterpret_cast<PFN_vkResetCommandPool>(GetDeviceProcAddr(dev, "vkResetCommandPool"));
        AllocateCommandBuffers = reinterpret_cast<PFN_vkAllocateCommandBuffers>(GetDeviceProcAddr(dev, "vkAllocateCommandBuffers"));
        FreeCommandBuffers = reinterpret_cast<PFN_vkFreeCommandBuffers>(GetDeviceProcAddr(dev, "vkFreeCommandBuffers"));
        BeginCommandBuffer = reinterpret_cast<PFN_vkBeginCommandBuffer>(GetDeviceProcAddr(dev, "vkBeginCommandBuffer"));
        EndCommandBuffer = reinterpret_cast<PFN_vkEndCommandBuffer>(GetDeviceProcAddr(dev, "vkEndCommandBuffer"));
        ResetCommandBuffer = reinterpret_cast<PFN_vkResetCommandBuffer>(GetDeviceProcAddr(dev, "vkResetCommandBuffer"));
        CmdBindPipeline = reinterpret_cast<PFN_vkCmdBindPipeline>(GetDeviceProcAddr(dev, "vkCmdBindPipeline"));
        CmdSetViewport = reinterpret_cast<PFN_vkCmdSetViewport>(GetDeviceProcAddr(dev, "vkCmdSetViewport"));
        CmdSetScissor = reinterpret_cast<PFN_vkCmdSetScissor>(GetDeviceProcAddr(dev, "vkCmdSetScissor"));
        CmdSetLineWidth = reinterpret_cast<PFN_vkCmdSetLineWidth>(GetDeviceProcAddr(dev, "vkCmdSetLineWidth"));
        CmdSetDepthBias = reinterpret_cast<PFN_vkCmdSetDepthBias>(GetDeviceProcAddr(dev, "vkCmdSetDepthBias"));
        CmdSetBlendConstants = reinterpret_cast<PFN_vkCmdSetBlendConstants>(GetDeviceProcAddr(dev, "vkCmdSetBlendConstants"));
        CmdSetDepthBounds = reinterpret_cast<PFN_vkCmdSetDepthBounds>(GetDeviceProcAddr(dev, "vkCmdSetDepthBounds"));
        CmdSetStencilCompareMask = reinterpret_cast<PFN_vkCmdSetStencilCompareMask>(GetDeviceProcAddr(dev, "vkCmdSetStencilCompareMask"));
        CmdSetStencilWriteMask = reinterpret_cast<PFN_vkCmdSetStencilWriteMask>(GetDeviceProcAddr(dev, "vkCmdSetStencilWriteMask"));
        CmdSetStencilReference = reinterpret_cast<PFN_vkCmdSetStencilReference>(GetDeviceProcAddr(dev, "vkCmdSetStencilReference"));
        CmdBindDescriptorSets = reinterpret_cast<PFN_vkCmdBindDescriptorSets>(GetDeviceProcAddr(dev, "vkCmdBindDescriptorSets"));
        CmdBindIndexBuffer = reinterpret_cast<PFN_vkCmdBindIndexBuffer>(GetDeviceProcAddr(dev, "vkCmdBindIndexBuffer"));
        CmdBindVertexBuffers = reinterpret_cast<PFN_vkCmdBindVertexBuffers>(GetDeviceProcAddr(dev, "vkCmdBindVertexBuffers"));
        CmdDraw = reinterpret_cast<PFN_vkCmdDraw>(GetDeviceProcAddr(dev, "vkCmdDraw"));
        CmdDrawIndexed = reinterpret_cast<PFN_vkCmdDrawIndexed>(GetDeviceProcAddr(dev, "vkCmdDrawIndexed"));
        CmdDrawIndirect = reinterpret_cast<PFN_vkCmdDrawIndirect>(GetDeviceProcAddr(dev, "vkCmdDrawIndirect"));
        CmdDrawIndexedIndirect = reinterpret_cast<PFN_vkCmdDrawIndexedIndirect>(GetDeviceProcAddr(dev, "vkCmdDrawIndexedIndirect"));
        CmdDispatch = reinterpret_cast<PFN_vkCmdDispatch>(GetDeviceProcAddr(dev, "vkCmdDispatch"));
        CmdDispatchIndirect = reinterpret_cast<PFN_vkCmdDispatchIndirect>(GetDeviceProcAddr(dev, "vkCmdDispatchIndirect"));
        CmdCopyBuffer = reinterpret_cast<PFN_vkCmdCopyBuffer>(GetDeviceProcAddr(dev, "vkCmdCopyBuffer"));
        CmdCopyImage = reinterpret_cast<PFN_vkCmdCopyImage>(GetDeviceProcAddr(dev, "vkCmdCopyImage"));
        CmdBlitImage = reinterpret_cast<PFN_vkCmdBlitImage>(GetDeviceProcAddr(dev, "vkCmdBlitImage"));
        CmdCopyBufferToImage = reinterpret_cast<PFN_vkCmdCopyBufferToImage>(GetDeviceProcAddr(dev, "vkCmdCopyBufferToImage"));
        CmdCopyImageToBuffer = reinterpret_cast<PFN_vkCmdCopyImageToBuffer>(GetDeviceProcAddr(dev, "vkCmdCopyImageToBuffer"));
        CmdUpdateBuffer = reinterpret_cast<PFN_vkCmdUpdateBuffer>(GetDeviceProcAddr(dev, "vkCmdUpdateBuffer"));
        CmdFillBuffer = reinterpret_cast<PFN_vkCmdFillBuffer>(GetDeviceProcAddr(dev, "vkCmdFillBuffer"));
        CmdClearColorImage = reinterpret_cast<PFN_vkCmdClearColorImage>(GetDeviceProcAddr(dev, "vkCmdClearColorImage"));
        CmdClearDepthStencilImage = reinterpret_cast<PFN_vkCmdClearDepthStencilImage>(GetDeviceProcAddr(dev, "vkCmdClearDepthStencilImage"));
        CmdClearAttachments = reinterpret_cast<PFN_vkCmdClearAttachments>(GetDeviceProcAddr(dev, "vkCmdClearAttachments"));
        CmdResolveImage = reinterpret_cast<PFN_vkCmdResolveImage>(GetDeviceProcAddr(dev, "vkCmdResolveImage"));
        CmdSetEvent = reinterpret_cast<PFN_vkCmdSetEvent>(GetDeviceProcAddr(dev, "vkCmdSetEvent"));
        CmdResetEvent = reinterpret_cast<PFN_vkCmdResetEvent>(GetDeviceProcAddr(dev, "vkCmdResetEvent"));
        CmdWaitEvents = reinterpret_cast<PFN_vkCmdWaitEvents>(GetDeviceProcAddr(dev, "vkCmdWaitEvents"));
        CmdPipelineBarrier = reinterpret_cast<PFN_vkCmdPipelineBarrier>(GetDeviceProcAddr(dev, "vkCmdPipelineBarrier"));
        CmdBeginQuery = reinterpret_cast<PFN_vkCmdBeginQuery>(GetDeviceProcAddr(dev, "vkCmdBeginQuery"));
        CmdEndQuery = reinterpret_cast<PFN_vkCmdEndQuery>(GetDeviceProcAddr(dev, "vkCmdEndQuery"));
        CmdResetQueryPool = reinterpret_cast<PFN_vkCmdResetQueryPool>(GetDeviceProcAddr(dev, "vkCmdResetQueryPool"));
        CmdWriteTimestamp = reinterpret_cast<PFN_vkCmdWriteTimestamp>(GetDeviceProcAddr(dev, "vkCmdWriteTimestamp"));
        CmdCopyQueryPoolResults = reinterpret_cast<PFN_vkCmdCopyQueryPoolResults>(GetDeviceProcAddr(dev, "vkCmdCopyQueryPoolResults"));
        CmdPushConstants = reinterpret_cast<PFN_vkCmdPushConstants>(GetDeviceProcAddr(dev, "vkCmdPushConstants"));
        CmdBeginRenderPass = reinterpret_cast<PFN_vkCmdBeginRenderPass>(GetDeviceProcAddr(dev, "vkCmdBeginRenderPass"));
        CmdNextSubpass = reinterpret_cast<PFN_vkCmdNextSubpass>(GetDeviceProcAddr(dev, "vkCmdNextSubpass"));
        CmdEndRenderPass = reinterpret_cast<PFN_vkCmdEndRenderPass>(GetDeviceProcAddr(dev, "vkCmdEndRenderPass"));
        CmdExecuteCommands = reinterpret_cast<PFN_vkCmdExecuteCommands>(GetDeviceProcAddr(dev, "vkCmdExecuteCommands"));
        CreateSwapchainKHR = reinterpret_cast<PFN_vkCreateSwapchainKHR>(GetDeviceProcAddr(dev, "vkCreateSwapchainKHR"));
        DestroySwapchainKHR = reinterpret_cast<PFN_vkDestroySwapchainKHR>(GetDeviceProcAddr(dev, "vkDestroySwapchainKHR"));
        GetSwapchainImagesKHR = reinterpret_cast<PFN_vkGetSwapchainImagesKHR>(GetDeviceProcAddr(dev, "vkGetSwapchainImagesKHR"));
        AcquireNextImageKHR = reinterpret_cast<PFN_vkAcquireNextImageKHR>(GetDeviceProcAddr(dev, "vkAcquireNextImageKHR"));
        QueuePresentKHR = reinterpret_cast<PFN_vkQueuePresentKHR>(GetDeviceProcAddr(dev, "vkQueuePresentKHR"));
        CreateSharedSwapchainsKHR = reinterpret_cast<PFN_vkCreateSharedSwapchainsKHR>(GetDeviceProcAddr(dev, "vkCreateSharedSwapchainsKHR"));
    }

    unsigned int SVUlkanContext::GetMemTypeIndex(uint32_t bitsType, VkFlags reqMask )
    {
        VkPhysicalDeviceMemoryProperties memProps = g_vulkanContext.m_memProperties;

        for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i)
        {
            if ((bitsType & (1 << i)) &&
                ((memProps.memoryTypes[i].propertyFlags & reqMask) == reqMask))
                return i;
        }

        TRAP(false);
        return -1;
    }

    bool CheckInstanceForNeededExtension(std::vector<const char*>& mandatoryExtensions)
    {
        bool result = true;
        //check if instance has extension installed
            unsigned int extCnt;
        EnumerateInstanceExtensionProperties(nullptr, &extCnt, nullptr);
        std::vector<VkExtensionProperties> extensions;
        extensions.resize(extCnt);
        EnumerateInstanceExtensionProperties(nullptr, &extCnt, extensions.data());

        mandatoryExtensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
        mandatoryExtensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
        mandatoryExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);

        for(unsigned int i = 0; i < mandatoryExtensions.size(); ++i)
        {
            auto found = std::find_if(extensions.begin(), extensions.end(), [&](VkExtensionProperties ext)
            {
                return (strcmp(ext.extensionName, mandatoryExtensions[i]) == 0);
            });

            if(found == extensions.end())
            {
                TRAP(false);
                return false;
            }
        }

        unsigned layersCnt;
        std::vector<VkLayerProperties> layers;
        EnumerateInstanceLayerProperties(&layersCnt, nullptr);
        layers.resize(layersCnt);
        EnumerateInstanceLayerProperties(&layersCnt, layers.data());

        return true;
    }
    unsigned int ms_inst = 0;
    void InitInstance()
    {
        std::vector<const char*> mandatoryExtensions;
        CheckInstanceForNeededExtension(mandatoryExtensions);

        std::vector<char*> layersName(1, "VK_LAYER_LUNARG_standard_validation");
        layersName.push_back("VK_LAYER_RENDERDOC_Capture");

        VkInstanceCreateInfo instCrtInfo;
        cleanStructure(instCrtInfo);
        instCrtInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instCrtInfo.pNext = nullptr;
        instCrtInfo.flags = 0;
        instCrtInfo.pApplicationInfo = nullptr;
        instCrtInfo.enabledLayerCount = (uint32_t)layersName.size();
        instCrtInfo.ppEnabledLayerNames = layersName.data();
        instCrtInfo.enabledExtensionCount = (unsigned int)mandatoryExtensions.size();
        instCrtInfo.ppEnabledExtensionNames = mandatoryExtensions.data();

        VkResult result = CreateInstance(&instCrtInfo, nullptr, &g_vulkanContext.m_instance);
        TRAP(result >= VK_SUCCESS);
    }

    unsigned int GetQueueFamilyIndex()
    {
        VkPhysicalDevice& physicalDevice = g_vulkanContext.m_physicalDevice;
        std::vector<VkQueueFamilyProperties> queueProperties;
        unsigned int queuePropCnt;
        GetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queuePropCnt, nullptr);
        queueProperties.resize(queuePropCnt);
        GetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queuePropCnt, queueProperties.data());

        int supportedQueueFlags = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT;

        unsigned int queueIndex = ~0;
        for(unsigned int i = 0; i < queueProperties.size(); ++i)
        {
            if((queueProperties[i].queueFlags & supportedQueueFlags) == supportedQueueFlags)
            {
                queueIndex = i;
                break;
            }
        }
        return queueIndex;
    }

    bool CheckDeviceExtentions(std::vector<const char*>& deviceMandatoryExt)
    {
        deviceMandatoryExt.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
        deviceMandatoryExt.push_back(VK_EXT_DEBUG_MARKER_EXTENSION_NAME);
		deviceMandatoryExt.push_back("VK_KHR_shader_draw_parameters");
		
        std::vector<VkExtensionProperties> deviceExtProperties;
        unsigned int devExtCnt;
        
        EnumerateDeviceExtensionProperties(g_vulkanContext.m_physicalDevice, nullptr, &devExtCnt, nullptr);
        deviceExtProperties.resize(devExtCnt);
        EnumerateDeviceExtensionProperties(g_vulkanContext.m_physicalDevice, nullptr, &devExtCnt, deviceExtProperties.data());

        for(unsigned int i = 0; i < deviceMandatoryExt.size(); ++i)
        {
            auto found = std::find_if(deviceExtProperties.begin(), deviceExtProperties.end(), [&](VkExtensionProperties ext)
            {
                return (strcmp(ext.extensionName, deviceMandatoryExt[i]) == 0);
            });

            if(found == deviceExtProperties.end())
            {
                TRAP(false);
                return false;
            }
        }

        unsigned int layersCnt;
        EnumerateDeviceLayerProperties(g_vulkanContext.m_physicalDevice, &layersCnt, nullptr);
        std::vector<VkLayerProperties> layers;
        layers.resize(layersCnt);
        EnumerateDeviceLayerProperties(g_vulkanContext.m_physicalDevice, &layersCnt, layers.data());
        return true;
    }

    void InitDevice()
    {
        VkInstance& instance = g_vulkanContext.m_instance;
        unsigned int physDevCnt;
        EnumeratePhysicalDevices(instance, &physDevCnt, nullptr);

        TRAP(physDevCnt == 1); //just a fast working version. if the machine has multiple devices we have get the right one
        EnumeratePhysicalDevices(instance, &physDevCnt, &g_vulkanContext.m_physicalDevice);
        
        VkPhysicalDevice& physicalDevice = g_vulkanContext.m_physicalDevice;
        VkPhysicalDeviceProperties physDevProps;
        GetPhysicalDeviceProperties(physicalDevice, &physDevProps);
        g_vulkanContext.m_limits = physDevProps.limits;

        GetPhysicalDeviceMemoryProperties(physicalDevice, &g_vulkanContext.m_memProperties);

        VkPhysicalDeviceFeatures devFeatures;
        GetPhysicalDeviceFeatures(physicalDevice, &devFeatures);

        std::vector<const char*> deviceMandatoryExt;
        //deviceMandatoryExt.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
        g_vulkanContext.m_queueFamilyIndex = GetQueueFamilyIndex();
        TRAP(g_vulkanContext.m_queueFamilyIndex != ~0);
        CheckDeviceExtentions(deviceMandatoryExt);
        TRAP(GetPhysicalDeviceWin32PresentationSupportKHR(physicalDevice, g_vulkanContext.m_queueFamilyIndex) == VK_TRUE); //supports win32 surface?  

        float queuePriority = 0.0f;
        VkDeviceQueueCreateInfo devQueueCrtInfo;
        cleanStructure(devQueueCrtInfo);
        devQueueCrtInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        devQueueCrtInfo.pNext = nullptr;
        devQueueCrtInfo.flags = 0;
        devQueueCrtInfo.queueFamilyIndex = g_vulkanContext.m_queueFamilyIndex;
        devQueueCrtInfo.queueCount = 1;
        devQueueCrtInfo.pQueuePriorities = &queuePriority;

        VkDeviceCreateInfo devCrtInfo;
        cleanStructure(devCrtInfo);
        devCrtInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        devCrtInfo.pNext = nullptr;
        devCrtInfo.flags = 0;
        devCrtInfo.queueCreateInfoCount = 1;
        devCrtInfo.pQueueCreateInfos = &devQueueCrtInfo;
        devCrtInfo.enabledLayerCount = 0;
        devCrtInfo.ppEnabledLayerNames = nullptr;
        devCrtInfo.enabledExtensionCount = (unsigned int)deviceMandatoryExt.size();
        devCrtInfo.ppEnabledExtensionNames = deviceMandatoryExt.data();
        devCrtInfo.pEnabledFeatures = &devFeatures;
        VkResult res = CreateDevice(physicalDevice, &devCrtInfo, nullptr, &g_vulkanContext.m_device);
        TRAP(res >= VK_SUCCESS);
        
        //TRAP(false);

    }

    VkBool32 DebugReport(VkDebugReportFlagsEXT flags,
        VkDebugReportObjectTypeEXT obj_type,
        uint64_t object,
        size_t location,
        int32_t msg_code,
        const char *layer_prefix,
        const char *msg,
        void *user_data)
    {
        bool isError = (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) != 0;
        std::string typeStr = (isError)? "Error " : "Warning ";
        typeStr += msg;
        typeStr += "\n";
        OutputDebugString(typeStr.data());
        TRAP(!isError);
        return false;
    }

    void RegisterDebugInfo()
    {
        std::vector<VkLayerProperties> layer_props;
        uint32_t count = 0;
        vk::EnumerateInstanceLayerProperties(&count, nullptr);

        layer_props.resize(count);
        EnumerateInstanceLayerProperties(&count, layer_props.data());

        bool ok = false;
        for(auto it = layer_props.begin(); it != layer_props.end(); ++it)
        {
            if(strcmp(it->layerName, "VK_LAYER_LUNARG_standard_validation") == 0)
            {
                ok = true;
                break;
            }
        }

        TRAP(ok);

        VkDebugReportCallbackCreateInfoEXT debug_report_info; 
        cleanStructure(debug_report_info);
        debug_report_info.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;

        debug_report_info.flags = VK_DEBUG_REPORT_WARNING_BIT_EXT |
            VK_DEBUG_REPORT_ERROR_BIT_EXT;
        debug_report_info.pUserData = nullptr;
        debug_report_info.pfnCallback = (PFN_vkDebugReportCallbackEXT)DebugReport;

        VULKAN_ASSERT(vk::CreateDebugReportCallbackEXT(g_vulkanContext.m_instance,
            &debug_report_info, nullptr, &g_vulkanContext.m_debugReport));
    }

    void Load()
    {
        const char library[] = "vulkan-1.dll";
        const char renderDoc[] = "renderdoc.dll";

        HMODULE vulkanDll = LoadLibrary(library);
        TRAP(vulkanDll);
       
        
        GetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(GetProcAddress(vulkanDll, "vkGetInstanceProcAddr"));
        TRAP(GetInstanceProcAddr)
        //init instance

        init_dispatch_table_top();
        InitInstance();
        //HMODULE renderDocDll = LoadLibrary(renderDoc);
        //TRAP(renderDocDll);
        init_dispatch_table_middle(g_vulkanContext.m_instance);
        RegisterDebugInfo();
        
        InitDevice();
        init_dispatch_table_bottom(g_vulkanContext.m_instance, g_vulkanContext.m_device);
    }

    void Unload()
    {
        DestroyDebugReportCallbackEXT(g_vulkanContext.m_instance, g_vulkanContext.m_debugReport, nullptr);
        DestroyDevice(g_vulkanContext.m_device, nullptr);
        DestroyInstance(g_vulkanContext.m_instance, nullptr);
    }
} ;// namespace vk
