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
	void CreateBlurPipelines(CGraphicPipeline& pipeline, bool isVertical);

	void CreateImages();
	void ClearImages();
private:
	CComputePipeline		m_ssrPipeline;
	CGraphicPipeline		m_blurHPipeline;
	CGraphicPipeline		m_blurVPipeline;
	CGraphicPipeline		m_ssrResolvePipeline;

	//CGraphicPipeline		m_ssrPipeline; //try to use compute for this
	BufferHandle*			m_ssrConstantsBuffer;
	ImageHandle*			m_ssrOutputImage;
	ImageHandle*			m_ssrDebugImage;

	VkDescriptorSet			m_ssrDescSet;
	VkDescriptorSetLayout	m_ssrDescLayout;

	VkDescriptorSet			m_blurVDescSet;
	VkDescriptorSet			m_blurHDescSet;
	VkDescriptorSetLayout	m_blurDescLayout;

	VkDescriptorSet			m_resolveDescSet;
	VkDescriptorSetLayout	m_resolveLayout;

	VkSampler				m_ssrSampler;
	VkSampler				m_resolveSampler;
	VkSampler				m_linearSampler;

	Mesh*					m_quad;
	
	glm::mat4				m_projMatrix;
	glm::mat4				m_viewToScreenSpaceMatrix;

	uint32_t				m_resolutionX;
	uint32_t				m_resolutionY;
	const uint32_t			m_cellSize;
};