#pragma once

#include "Renderer.h"
#include "defines.h"
#include "VulkanLoader.h"
#include "glm/glm.hpp"

class BufferHandle;
class ScreenSpaceReflectionsRenderer : public CRenderer
{
public:
	ScreenSpaceReflectionsRenderer(VkRenderPass renderPass);
	virtual ~ScreenSpaceReflectionsRenderer();

	virtual void Init() override;
	virtual void PreRender() override;
	virtual void Render() override;

private:
	virtual void CreateDescriptorSetLayout() override;
	virtual void UpdateResourceTable() override;
	virtual void UpdateGraphicInterface() override;
	virtual void PopulatePoolInfo(std::vector<VkDescriptorPoolSize>& poolSize, unsigned int& maxSets) override;

	void CreatePipelines();
private:
	CGraphicPipeline		m_ssrPipeline; //try to use compute for this
	BufferHandle*			m_ssrConstantsBuffer;

	VkDescriptorSet			m_ssrDescSet;
	VkDescriptorSetLayout	m_ssrDescLayout;
	VkSampler				m_sampler;

	Mesh*					m_quad;
	
	glm::mat4				m_projMatrix;
};