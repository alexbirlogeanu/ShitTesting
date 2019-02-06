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

struct ShadowSplit
{
	glm::mat4 ProjViewMatrix;
	glm::vec4 NearFar;
};

class ShadowMapRenderer : public CRenderer //this one have to be refactored. Maybe will not need this anymore
{
public:
	typedef std::array<ShadowSplit, SHADOWSPLITS> SplitsArrayType;
public:
    ShadowMapRenderer(VkRenderPass renderPass);
    virtual ~ShadowMapRenderer();

    void Init() override;
    void Render() override;
	void PreRender() override;
protected:
    void UpdateResourceTable() override;
    void CreateDescriptorSetLayout() override;
    virtual void PopulatePoolInfo(std::vector<VkDescriptorPoolSize>& poolSize, unsigned int& maxSets) override;
	
	void ComputeCascadeSplitMatrices();
	void ComputeCascadeViewMatrix(const CFrustum& splitFrustrum, const glm::vec3& lightDir, const glm::vec3& lightUp, glm::mat4& outView);
	void ComputeCascadeProjMatrix(const CFrustum& splitFrustum, const glm::mat4& lightViewMatrix, const glm::mat4& lightProjMatrix, glm::mat4& outCroppedProjMatrix);

    void UpdateGraphicInterface() override;

	bool OnKeyPressed(const KeyInput& key);
private:
    glm::mat4                       m_shadowViewProj;

    CGraphicPipeline                m_pipeline;
    VkDescriptorSetLayout           m_descriptorSetLayout;

	BufferHandle*					m_splitsBuffer;
	VkDescriptorSet					m_splitsDescSet;
	DescriptorSetLayout				m_splitDescLayout;

	SplitsArrayType					m_splitProjMatrix;
	float							m_splitsAlphaFactor;

	//
	bool							m_isDebugMode;
	CUIText*						m_debugText;
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
