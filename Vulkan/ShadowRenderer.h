#pragma once

#include "Renderer.h"
#include "Utils.h"
#include "defines.h"
#include "VulkanLoader.h"

//#define USE_SHADOW_BLUR

class Mesh;
class CTexture;
class CShadowResolveRenderer : public CRenderer
{
public:
    CShadowResolveRenderer(VkRenderPass renderpass);
    virtual ~CShadowResolveRenderer();

    virtual void Init() override;
    virtual void Render() override;

    void UpdateGraphicInterface(VkImageView normalView, VkImageView posView, VkImageView shadowMapView);
    void UpdateShaderParams(glm::mat4 shadowProj);

private:
    virtual void CreateDescriptorSetLayout() override;
    virtual void PopulatePoolInfo(std::vector<VkDescriptorPoolSize>& poolSize, unsigned int& maxSets) override;
#ifdef USE_SHADOW_BLUR
    void SetupBlurPipeline(CGraphicPipeline& pipeline, bool isVertical);
#endif
    void CreateDistributionTextures();

private:
    Mesh*                   m_quad;

    CGraphicPipeline               m_pipeline;

    VkSampler               m_depthSampler;
    VkSampler               m_linearSampler;
    VkSampler               m_nearSampler;

    VkDescriptorSetLayout   m_descriptorLayout;
    VkDescriptorSet         m_descriptorSet;

    VkDeviceMemory          m_uniformMemory;
    VkBuffer                m_uniformBuffer;

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
