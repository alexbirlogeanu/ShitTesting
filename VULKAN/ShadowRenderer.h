#pragma once

#include "Renderer.h"
#include "Utils.h"
#include "defines.h"
#include "VulkanLoader.h"

#include <vector>

//#define USE_SHADOW_BLUR

class Mesh;
class CTexture;
class Object;

class ShadowMapRenderer : public CRenderer
{
public:
    ShadowMapRenderer(VkRenderPass renderPass, const std::vector<Object*>& shadowCasters);
    virtual ~ShadowMapRenderer();

    void Init() override;
    void Render() override;
	void PreRender() override;
protected:
    void UpdateResourceTable() override;
    void CreateDescriptorSetLayout() override;
    virtual void PopulatePoolInfo(std::vector<VkDescriptorPoolSize>& poolSize, unsigned int& maxSets) override;

    void ComputeProjMatrix(glm::mat4& proj, const glm::mat4& view);
    //these 2 methods are duplicate code. See ObjectRenderer
    void InitNodesDescriptorSet();
    void InitNodesMemory();
    void UpdateShaderParams();
    void UpdateGraphicInterface() override;
private:
    //this paradigm is similar with the one in the objectRenderer. Maybe we can write it better
    struct Node
    {
        Object*             obj;
        VkDescriptorSet     descSet;
        BufferHandle*		buffer;

        Node()
            : obj(nullptr)
            , descSet(VK_NULL_HANDLE)
            , buffer(nullptr)
        {}

        Node (Object* o)
            : obj(o)
            , descSet(VK_NULL_HANDLE)
            , buffer(nullptr)
        {}
    };

    glm::mat4                       m_shadowViewProj;

    CGraphicPipeline                m_pipeline;
    VkDescriptorSetLayout           m_descriptorSetLayout;

    BufferHandle*                   m_instanceBuffer;
    std::vector<Node>               m_nodes;
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
