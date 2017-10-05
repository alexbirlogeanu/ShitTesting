#include "DebugMarkers.h"
#include <vector>

static std::vector<std::string> m_markersStack;

void StartDebugMarker(const std::string& markerName)
{
    VkDebugMarkerMarkerInfoEXT marker;
    cleanStructure(marker);
    marker.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT;
    marker.pNext = nullptr;
    marker.pMarkerName = markerName.data();
    //marker.color[0] = marker.color[2] = marker.color[3] = 1.0f;
    vk::CmdDebugMarkerBeginEXT(vk::g_vulkanContext.m_mainCommandBuffer, &marker);

    m_markersStack.push_back(markerName);
}

void EndDebugMarker(const std::string& markerName)
{
    TRAP( m_markersStack.back() == markerName); //need to close the current open marker
    vk::CmdDebugMarkerEndEXT(vk::g_vulkanContext.m_mainCommandBuffer);
    m_markersStack.pop_back();
}

template<>
VkDebugReportObjectTypeEXT GetDebugObjectType<VkImage>()
{
    return VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT;
}

template<>
VkDebugReportObjectTypeEXT GetDebugObjectType<VkSampler>()
{
    return VK_DEBUG_REPORT_OBJECT_TYPE_SAMPLER_EXT;
}

template<>
VkDebugReportObjectTypeEXT GetDebugObjectType<VkImageView>()
{
    return VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT;
}