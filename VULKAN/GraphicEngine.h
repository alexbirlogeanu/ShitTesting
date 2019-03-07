#pragma once

#include "Singleton.h"
#include "VulkanLoader.h"

#include <string>
#include <unordered_map>

//#define GetGraphicEngine() GraphicEngine::GetInstance()

class ImageHandle;
class GraphicEngine : public Singleton<GraphicEngine>
{
	friend class Singleton<GraphicEngine>;
public:
	void Init();
	static ImageHandle* GetAttachment(const std::string& name);
private:
	GraphicEngine();
	virtual ~GraphicEngine();

	void						CreateFramebufferAttachments();
	VkImageCreateInfo			GetAttachmentCreateImageInfo(VkFormat format, VkExtent3D dimensions, uint32_t layers, VkImageUsageFlags additionalUsage = 0);
private:
	std::unordered_map<std::string, ImageHandle*>	m_framebufferAttachments; //a map with all the attachments used for rendering
};
