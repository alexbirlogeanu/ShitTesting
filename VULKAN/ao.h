#pragma once

#include "Renderer.h"
#include "defines.h"
#include "VulkanLoader.h"
#include "glm/glm.hpp"

#include <vector>

class Mesh;
class BufferHandle;

///////////////////////////////////////////////////////////////////
//AORenderer
///////////////////////////////////////////////////////////////////

class AORenderer : public Renderer
{
public:
	AORenderer();
	virtual ~AORenderer();


	void PreRender();
	//VkImageView GetOuput() { return m_framebuffer->GetColorImageView(0); }


	void SetupMainPass(VkRenderPass renderPass, uint32_t subpassId);
	void RenderMain();

	void SetupVBlurPass(VkRenderPass renderPass, uint32_t subpassId);
	void RenderVBlur();

	void SetupHBlurPass(VkRenderPass renderPass, uint32_t subpassId);
	void RenderHBlur();

protected:
	void InitInternal() override;
	void CreateDescriptorSetLayouts() override;
	void UpdateGraphicInterface() override;
	void AllocateDescriptorSets() override;
	
	void InitSSAOParams();

	void UpdateParams();
private:
	//buffers
	BufferHandle*					m_constParamsBuffer;
	BufferHandle*					m_varParamsBuffer;

	VkSampler						m_sampler;

	DescriptorSetLayout				m_constDescSetLayout;
	DescriptorSetLayout				m_varDescSetLayout;
	DescriptorSetLayout				m_blurDescSetLayout;

	CGraphicPipeline				m_mainPipeline;
	CGraphicPipeline				m_hblurPipeline;
	CGraphicPipeline				m_vblurPipeline;
	Mesh*							m_quad;

	std::vector<VkDescriptorSet>	m_mainPassSets;
	std::vector<VkDescriptorSet>	m_blurPassSets;

	//containers for const params for ssao (debug purpose mostly)
	std::vector<glm::vec4>			m_samples;
	std::vector<glm::vec4>			m_noise;
};