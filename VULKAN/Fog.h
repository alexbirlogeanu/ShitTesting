#pragma once

#include "Renderer.h"
#include "VulkanLoader.h"

class Mesh;
class BufferHandle;
class CFogRenderer : public CRenderer
{
public:
    CFogRenderer(VkRenderPass renderpass);
    virtual ~CFogRenderer();

    virtual void Init() override;
    virtual void Render() override;
	virtual void PreRender() override;
protected:
    virtual void CreateDescriptorSetLayout() override;
    virtual void PopulatePoolInfo(std::vector<VkDescriptorPoolSize>&, unsigned int& maxSets) override;
    virtual void UpdateGraphicInterface() override;
private:
    VkDescriptorSetLayout       m_descriptorLayout;
    VkDescriptorSet             m_descriptorSet;
    VkSampler                   m_sampler;

    BufferHandle*               m_fogParamsBuffer;

    CGraphicPipeline			m_pipline;
    Mesh*                       m_quad;
};