#pragma once

#include "VulkanLoader.h"
#include "defines.h"
#include <string>

void StartDebugMarker(const std::string& markerName);
void EndDebugMarker(const std::string& markerName);

#define BeginMarkerSection(label) { \
    const std::string markerName = label; \
    StartDebugMarker(markerName);

#define EndMarkerSection() EndDebugMarker(markerName); \
    }

template<typename T>
VkDebugReportObjectTypeEXT GetDebugObjectType()
{
    TRAP(false && "Specialize this template for your type");
    return VK_DEBUG_REPORT_OBJECT_TYPE_UNKNOWN_EXT;
}

template<>
VkDebugReportObjectTypeEXT GetDebugObjectType<VkImage>();
template<>
VkDebugReportObjectTypeEXT GetDebugObjectType<VkImageView>();
template<>
VkDebugReportObjectTypeEXT GetDebugObjectType<VkSampler>();

template<typename T>
void SetObjectDebugName(T object, const std::string& name)
{
    VkDebugMarkerObjectNameInfoEXT objectMarker;
    cleanStructure(objectMarker);
    objectMarker.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT;
    objectMarker.objectType = GetDebugObjectType<T>();
    objectMarker.object = (uint64_t)object;
    objectMarker.pObjectName = name.data();

    VULKAN_ASSERT(vk::DebugMarkerSetObjectNameEXT(vk::g_vulkanContext.m_device, &objectMarker));
}