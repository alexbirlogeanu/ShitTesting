#pragma once

#include "defines.h"
#include "VulkanLoader.h"

#include <string>
#include <unordered_set>
#include <unordered_map>

class ImageHandle;
class GPUTaskGroup;

class AttachmentInfo
{
	friend class GraphicEngine;
	friend class RenderGraph;
	HEAP_ONLY(AttachmentInfo);
public:

	const std::string&		GetIdentifier() const { return m_identifier; }
	ImageHandle*			GetHandle() const { return m_image; }
	const VkClearValue&		GetClearValue() const { return m_clearValue; }
	const VkFormat			GetFormat() const;
	const VkExtent3D&		GetDimensions() const;
	const uint32_t			GetLayers() const;
	const VkImage			GetVkImage() const;

	bool					IsFirstOccurence(GPUTaskGroup* group) const { return group == m_firstProducerGroup; }
private:
	AttachmentInfo(const std::string& st, ImageHandle* img, VkClearValue clrValue);

	void SetFirstGroup(GPUTaskGroup* group) { m_firstProducerGroup = group; }
private:
	std::string					m_identifier;
	ImageHandle*				m_image;
	VkClearValue				m_clearValue;

	//this member will be filled by the render graph after the topological sort. Will keep the group which uses for the first time this attachment to write into it
	GPUTaskGroup*				m_firstProducerGroup;
};


/////////////////////////////////////////////////////////
//Framebuffer V2
/////////////////////////////////////////////////////////

class Framebuffer
{
	HEAP_ONLY(Framebuffer);
public:
	Framebuffer(const VkRenderPass& renderPass, const std::vector<const AttachmentInfo*>& attachments);

	//getters
	const std::vector<VkClearValue>&	GetClearColors() const { return m_clearColors; }
	const VkFramebuffer					GetFramebufferHandle() const { return m_framebufferHandle; }
	const VkRect2D						GetRenderArea() const { return m_renderArea; }
private:
	VkFramebuffer										m_framebufferHandle;
	VkRenderPass										m_renderPass;
	VkRect2D											m_renderArea;

	std::unordered_map<std::string, const AttachmentInfo*>	m_attachmentMap;
	std::vector<VkClearValue>							m_clearColors;
};

