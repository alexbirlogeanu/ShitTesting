#pragma once

#include "Renderer.h"

class CTexture;
class SkyRenderer : public Renderer
{
public:
	SkyRenderer();
	virtual ~SkyRenderer();

	void RenderSkyboxPass();
	void BlendSunPass();

	void SetupSkyBoxSubpass(VkRenderPass renderPass, uint32_t subpassId);
	void SetupBlendSunSubpass(VkRenderPass renderPass, uint32_t subpassId);

	void PreRender();
private:
	struct SSkyParams
	{
		glm::vec4   CameraDir;
		glm::vec4   CameraRight;
		glm::vec4   CameraUp;
		glm::vec4   Frustrum;
		glm::vec4   DirLightColor;
		//maybe proj
	};

	void InitInternal() override;
	void UpdateGraphicInterface() override;
	void CreateDescriptorSetLayouts() override;
	void AllocateDescriptorSets() override;

private:
	Mesh*					m_quadMesh;

	BufferHandle*			m_boxParamsBuffer;

	CTexture*				m_skyTexture;
	CGraphicPipeline        m_boxPipeline;
	CGraphicPipeline        m_sunPipeline;

	VkDescriptorSet			m_boxDescriptorSet;
	DescriptorSetLayout		m_boxDescriptorSetLayout;

	DescriptorSetLayout		m_sunDescriptorSetLayout;
	VkDescriptorSet         m_sunDescriptorSet;
	
	VkSampler				m_sampler;
};


