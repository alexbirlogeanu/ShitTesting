#include "Framebuffer.h"

#include "MemoryManager.h"

AttachmentInfo::AttachmentInfo(const std::string& st, ImageHandle* img, VkClearValue clrValue)
	: m_identifier(st)
	, m_image(img)
	, m_firstProducerGroup(nullptr)
	, m_clearValue(clrValue)
{

}

AttachmentInfo::~AttachmentInfo()
{
	MemoryManager::GetInstance()->FreeHandle(m_image);
}

const VkFormat AttachmentInfo::GetFormat() const
{
	return m_image->GetFormat();
}

const VkExtent3D& AttachmentInfo::GetDimensions() const
{
	return m_image->GetDimensions();
}

const uint32_t AttachmentInfo::GetLayers() const
{
	return m_image->GetLayersNumber();
}

const VkImage AttachmentInfo::GetVkImage() const
{
	return m_image->Get();
}

/////////////////////////////////////////////////////////
//Framebuffer V2
/////////////////////////////////////////////////////////
Framebuffer::Framebuffer(const VkRenderPass& renderPass, const std::unordered_set<AttachmentInfo*>& attachments)
	: m_renderPass(VK_NULL_HANDLE)
	, m_framebufferHandle(VK_NULL_HANDLE)
{
	//validate + get dimensions
	const auto& firstAtt = attachments.begin();
	uint32_t layers = (*firstAtt)->GetLayers();
	VkExtent3D dimensions = (*firstAtt)->GetDimensions();

	//validate
	for (auto it = firstAtt; it != attachments.end(); ++it)
	{
		TRAP((*it)->GetLayers() == layers && "All the images in the framebuffer should have the same number of layers");
		VkExtent3D currDimensions = (*it)->GetDimensions();
		TRAP(currDimensions.width == dimensions.width
			&& currDimensions.height == dimensions.height
			&& currDimensions.depth == dimensions.depth
			&& "All images from the framebuffer should have the same  dimensions");
	}

	m_renderArea.offset.x = 0;
	m_renderArea.offset.y = 0;
	m_renderArea.extent.width = dimensions.width;
	m_renderArea.extent.height = dimensions.height;

	std::vector<VkImageView> fbViews;
	fbViews.reserve(attachments.size());
	for (const auto& att : attachments)
	{
		fbViews.push_back(att->GetHandle()->GetView());
		m_clearColors.push_back(att->GetClearValue());
	}

	VkFramebufferCreateInfo fbCrtInfo;
	cleanStructure(fbCrtInfo);
	fbCrtInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	fbCrtInfo.pNext = nullptr;
	fbCrtInfo.flags = 0;
	fbCrtInfo.renderPass = m_renderPass;
	fbCrtInfo.attachmentCount = (uint32_t)fbViews.size();
	fbCrtInfo.pAttachments = fbViews.data();
	fbCrtInfo.width = dimensions.width;
	fbCrtInfo.height = dimensions.height;
	fbCrtInfo.layers = layers;

	VULKAN_ASSERT(vk::CreateFramebuffer(vk::g_vulkanContext.m_device, &fbCrtInfo, nullptr, &m_framebufferHandle));

	//save attachments for future use
	for (auto& att : attachments)
		m_attachmentMap.emplace(att->GetIdentifier(), att);
}

Framebuffer::~Framebuffer()
{
	vk::DestroyFramebuffer(vk::g_vulkanContext.m_device, m_framebufferHandle, nullptr);
}
