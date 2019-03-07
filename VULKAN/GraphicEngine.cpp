#include "GraphicEngine.h"

#include "Utils.h"
#include "MemoryManager.h"

GraphicEngine::GraphicEngine()
{

}

GraphicEngine::~GraphicEngine()
{

}

void GraphicEngine::Init()
{
	CreateFramebufferAttachments();
}

ImageHandle* GraphicEngine::GetAttachment(const std::string& name)
{
	static auto& attachments = GetInstance()->m_framebufferAttachments;
	auto it = attachments.find(name);
	TRAP(it != attachments.end() && "Attachement doesn't exists");
	return it->second;
}

void GraphicEngine::CreateFramebufferAttachments()
{
	VkExtent3D defaultDimensions { WIDTH, HEIGHT, 1 };
	VkExtent3D shadowDimensions{ SHADOWW, SHADOWH, 1 };

	auto createAttachment = [&] (const std::string& name, const VkImageCreateInfo& info)
	{
		ImageHandle* img = MemoryManager::GetInstance()->CreateImage(EMemoryContextType::Framebuffers, info, name + "Att");
		m_framebufferAttachments.emplace(name, img);

	};

	//GBuffer creation
	createAttachment("Albedo", GetAttachmentCreateImageInfo(VK_FORMAT_R8G8B8A8_UNORM, defaultDimensions, 1, VK_IMAGE_USAGE_SAMPLED_BIT));
	createAttachment("Specular", GetAttachmentCreateImageInfo(VK_FORMAT_R8G8B8A8_UNORM, defaultDimensions, 1, VK_IMAGE_USAGE_SAMPLED_BIT));
	createAttachment("Normals", GetAttachmentCreateImageInfo(VK_FORMAT_R16G16B16A16_SFLOAT, defaultDimensions, 1, VK_IMAGE_USAGE_SAMPLED_BIT));
	createAttachment("Positions", GetAttachmentCreateImageInfo(VK_FORMAT_R16G16B16A16_SFLOAT, defaultDimensions, 1, VK_IMAGE_USAGE_SAMPLED_BIT));
	createAttachment("DefferedDebug", GetAttachmentCreateImageInfo(VK_FORMAT_R8G8B8A8_UNORM, defaultDimensions, 1));
	createAttachment("Depth", GetAttachmentCreateImageInfo(VK_FORMAT_D24_UNORM_S8_UINT, defaultDimensions, VK_IMAGE_USAGE_SAMPLED_BIT));

	//shadow map / shadow resolve
	createAttachment("ShadowMap", GetAttachmentCreateImageInfo(VK_FORMAT_D24_UNORM_S8_UINT, shadowDimensions, SHADOWSPLITS, VK_IMAGE_USAGE_SAMPLED_BIT));
	createAttachment("ShadowResolveFinal", GetAttachmentCreateImageInfo(VK_FORMAT_R32_SFLOAT, defaultDimensions, 1, VK_IMAGE_USAGE_SAMPLED_BIT));
	createAttachment("ShadowResolveDebug", GetAttachmentCreateImageInfo(VK_FORMAT_R16G16B16A16_SFLOAT, defaultDimensions, 1, VK_IMAGE_USAGE_SAMPLED_BIT));
	createAttachment("ShadowResolveBlur", GetAttachmentCreateImageInfo(VK_FORMAT_R32_SFLOAT, defaultDimensions, 1, VK_IMAGE_USAGE_SAMPLED_BIT));

	//OUTPUT attachment

}

VkImageCreateInfo  GraphicEngine::GetAttachmentCreateImageInfo(VkFormat format, VkExtent3D dimensions, uint32_t layers, VkImageUsageFlags additionalUsage)
{
	VkImageUsageFlags usage = (IsColorFormat(format))? VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT : VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

	VkImageCreateInfo imgInfo;
	cleanStructure(imgInfo);
	imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imgInfo.pNext = nullptr;
	imgInfo.flags = 0;
	imgInfo.imageType = VK_IMAGE_TYPE_2D;
	imgInfo.format = format;
	imgInfo.extent.height = dimensions.height;
	imgInfo.extent.width = dimensions.width;
	imgInfo.extent.depth = dimensions.depth;
	imgInfo.arrayLayers = layers;
	imgInfo.mipLevels = 1;
	imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imgInfo.usage = usage | additionalUsage;
	imgInfo.queueFamilyIndexCount = 0;
	imgInfo.pQueueFamilyIndices = nullptr;
	imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;

	return imgInfo;
}
