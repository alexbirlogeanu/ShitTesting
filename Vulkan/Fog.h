#pragma once

#include "Renderer.h"
#include "VulkanLoader.h"

class Mesh;
class CFogRenderer : public CRenderer
{
public:
    CFogRenderer(VkRenderPass renderpass);
    virtual ~CFogRenderer();

    virtual void Init() override;
    virtual void Render() override;

    void UpdateGraphicInterface(VkImageView imgView);
protected:
    virtual void CreateDescriptorSetLayout() override;
    virtual void PopulatePoolInfo(std::vector<VkDescriptorPoolSize>&, unsigned int& maxSets) override;
private:
    VkDescriptorSetLayout       m_descriptorLayout;
    VkDescriptorSet             m_descriptorSet;
    VkSampler                   m_sampler;

    VkBuffer                    m_fogParamsBuffer;
    VkDeviceMemory              m_fogParamsMemory;

    CPipeline                   m_pipline;
    Mesh*                       m_quad;
};