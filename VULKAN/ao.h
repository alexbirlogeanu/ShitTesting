#pragma once

#include "Renderer.h"
#include "defines.h"
#include "VulkanLoader.h"
#include "glm/glm.hpp"

#include <vector>

enum ESSAOPass
{
    ESSAOPass_Main = 0,
    ESSAOPass_HBlur,
    ESSAOPass_VBlur,
    ESSAOPass_Count
};

class Mesh;
class BufferHandle;
class CAORenderer : public CRenderer
{
public:
    CAORenderer(VkRenderPass renderPass);
    virtual ~CAORenderer();

    virtual void Init() override;
    virtual void Render() override;
	virtual void PreRender() override;
    //VkImageView GetOuput() { return m_framebuffer->GetColorImageView(0); }

protected:
    virtual void CreateDescriptorSetLayout() override;
    void AllocDescriptors();
    void InitSSAOParams();
    virtual void UpdateGraphicInterface() override;

    void UpdateParams();
    virtual void PopulatePoolInfo(std::vector<VkDescriptorPoolSize>& poolSize, unsigned int& maxSets);
    virtual void UpdateResourceTable() override;
private:
    //buffers
    BufferHandle*           m_constParamsBuffer;
	BufferHandle*           m_varParamsBuffer;

    VkSampler               m_sampler;

    VkDescriptorSetLayout   m_constDescSetLayout;
    VkDescriptorSetLayout   m_varDescSetLayout;
    VkDescriptorSetLayout   m_blurDescSetLayout;
    std::vector<VkDescriptorSet> m_mainPassSets;
    std::vector<VkDescriptorSet> m_blurPassSets;

    //containers for const params for ssao (debug purpose mostly)
    std::vector<glm::vec4>  m_samples;
    std::vector<glm::vec4>  m_noise;

    CGraphicPipeline               m_mainPipeline;
    CGraphicPipeline               m_hblurPipeline;
    CGraphicPipeline               m_vblurPipeline;
    Mesh*                   m_quad;
};
