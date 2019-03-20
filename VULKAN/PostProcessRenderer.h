#pragma once

#include "Renderer.h"
#include "DescriptorsUtils.h"

class BufferHandle;
class CTexture;
class PostProcessRenderer : public Renderer
{
public:
	PostProcessRenderer();
	virtual ~PostProcessRenderer();

	void Render();
	void UpdateShaderParams();
	void Setup(VkRenderPass renderPass, uint32_t subpassId);
protected:
	void InitInternal() override;
	void CreateDescriptorSetLayouts() override;
	void UpdateGraphicInterface() override;
	void AllocateDescriptorSets() override;

private:
	VkSampler				m_sampler;
	VkSampler				m_linearSampler;

	BufferHandle*			m_uniformBuffer;

	Mesh*                   m_quadMesh;
	CGraphicPipeline        m_pipeline;

	VkDescriptorSet			m_descriptorSet;
	DescriptorSetLayout		m_descriptorSetLayout;

	CTexture*				m_lut;
};
