#pragma once

#include "Renderer.h"
#include "VulkanLoader.h"
#include "DescriptorsUtils.h"

class LightRenderer : public Renderer
{
public:
	LightRenderer();
	virtual ~LightRenderer();

	void Setup(VkRenderPass renderPass, uint32_t subpassId);
	void PreRender();
	void Render();

protected:
	void InitInternal() override;
	void UpdateGraphicInterface() override;
	void CreateDescriptorSetLayouts()override;
	void AllocateDescriptorSets() override;
protected:
	VkSampler                       m_sampler;
	VkSampler                       m_depthSampler;

	BufferHandle*                   m_shaderUniformBuffer;

	DescriptorSetLayout				m_descriptorSetLayout;
	VkDescriptorSet                 m_descriptorSet;

	CGraphicPipeline                m_pipeline;
};
