#pragma once

#include "Singleton.h"
#include "VulkanLoader.h"

#include <string>
#include <unordered_map>
#include <utility>

//#define GetGraphicEngine() GraphicEngine::GetInstance()

class ImageHandle;

class AttachmentInfo
{
	friend class GraphicEngine;
	HEAP_ONLY(AttachmentInfo);
public:
	//implement
	const std::string&		GetIdentifier() const { return m_identifier; }
	ImageHandle*			GetImage() const { return m_image; }
	VkFormat				GetFormat() const;
private:
	AttachmentInfo(const std::string& st, ImageHandle* img);

private:
	std::string				m_identifier;
	ImageHandle*			m_image;
};

class GraphicEngine : public Singleton<GraphicEngine>
{
	friend class Singleton<GraphicEngine>;
public:
	void					Init();
	static AttachmentInfo*	GetAttachment(const std::string& name);
private:
	GraphicEngine();
	virtual ~GraphicEngine();

	void						CreateFramebufferAttachments();
	VkImageCreateInfo			GetAttachmentCreateImageInfo(VkFormat format, VkExtent3D dimensions, uint32_t layers, VkImageUsageFlags additionalUsage = 0);
private:
	std::unordered_map<std::string, AttachmentInfo*>	m_framebufferAttachments;
};
