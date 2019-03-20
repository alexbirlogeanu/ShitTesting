#pragma once

#include "Renderer.h"
#include "Utils.h"
#include "defines.h"
#include "VulkanLoader.h"
#include "DescriptorsUtils.h"

#include <vector>

//#define USE_SHADOW_BLUR

class Mesh;
class CTexture;
class Object;
class CFrustum;
class CUIText;
class keyInput;

struct CShadowSplit
{
	glm::mat4 ProjViewMatrix;
	glm::vec4 NearFar;
};

class CShadowResolveRenderer : public CRenderer
{
public:
    CShadowResolveRenderer(VkRenderPass renderpass);
    virtual ~CShadowResolveRenderer();

    virtual void Init() override;
    virtual void Render() override;
	virtual void PreRender() override;

private:
    virtual void CreateDescriptorSetLayout() override;
    virtual void PopulatePoolInfo(std::vector<VkDescriptorPoolSize>& poolSize, unsigned int& maxSets) override;
    virtual void UpdateResourceTable() override;
    virtual void UpdateGraphicInterface() override;

#ifdef USE_SHADOW_BLUR
    void SetupBlurPipeline(CGraphicPipeline& pipeline, bool isVertical);
#endif
    void CreateDistributionTextures();
    void UpdateShaderParams();
private:
    Mesh*                   m_quad;

    CGraphicPipeline               m_pipeline;

    VkSampler               m_depthSampler;
    VkSampler               m_linearSampler;
    VkSampler               m_nearSampler;

    VkDescriptorSetLayout   m_descriptorLayout;
    VkDescriptorSet         m_descriptorSet;

    BufferHandle*           m_uniformBuffer;

    CTexture*               m_blockerDistrText;
    CTexture*               m_PCFDistrText;

#ifdef USE_SHADOW_BLUR
    CGraphicPipeline               m_vBlurPipeline;
    CGraphicPipeline               m_hBlurPipeline;
    VkDescriptorSetLayout   m_blurSetLayout;
    VkDescriptorSet         m_vBlurDescSet;
    VkDescriptorSet         m_hBlurDescSet;
#endif
};

//////////////////////////////////////////////////////////////////
//ShadowResolveRenderer
//////////////////////////////////////////////////////////////////

class ShadowResolveRenderer : public Renderer
{
public:
	ShadowResolveRenderer();
	virtual ~ShadowResolveRenderer();

	void Setup(VkRenderPass renderPass, uint32_t subpassId);
	void Render();
	void PreRender();

private:
	void CreateDescriptorSetLayouts() override;
	void UpdateGraphicInterface() override;
	void AllocateDescriptorSets() override;
	void InitInternal() override;

	void CreateDistributionTextures();
	void UpdateShaderParams();
private:
	Mesh*                   m_quad;

	CGraphicPipeline        m_pipeline;

	VkSampler               m_depthSampler;
	VkSampler               m_linearSampler;
	VkSampler               m_nearSampler;

	DescriptorSetLayout		m_descriptorLayout;
	VkDescriptorSet         m_descriptorSet;

	BufferHandle*           m_uniformBuffer;

	CTexture*               m_blockerDistrText;
	CTexture*               m_PCFDistrText;
};
