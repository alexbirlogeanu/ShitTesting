#pragma once

#include "Renderer.h"
#include "VulkanLoader.h"
#include "defines.h"
#include "Utils.h"

#include <vector>

///////////////////////
//Test class
//////////////////////
#define TEXTURE3DLAYERS (unsigned int)32
//#define TEST3DTEXT

class Mesh;
class C3DTextureRenderer : public CRenderer
{
public:
    C3DTextureRenderer (VkRenderPass renderPass);
    virtual ~C3DTextureRenderer();

    virtual void Init() override;
    virtual void Render() override;

    VkImageView GetOutTexture() const { return m_outTextureView; }
protected:
    virtual void CreateDescriptorSetLayout() override;
    virtual void PopulatePoolInfo(std::vector<VkDescriptorPoolSize>&, unsigned int& maxSets) override;

    void AllocateOuputTexture();

    void CopyTexture();
    void AllocateAuxiliarMemory();

private:
    CPipeline                   m_pipeline;
    VkDescriptorSetLayout       m_descSetLayout;
    VkDescriptorSet             m_descSet;

    VkBuffer                    m_copyBuffer;
    VkDeviceMemory              m_copyMemory;

    //prototype 3D Texture
    VkImage                     m_outTexture;
    VkImageView                 m_outTextureView;
    VkDeviceMemory              m_outTextureMemory;
#ifdef TEST3DTEXT
    CPipeline                   m_readPipeline;
#endif
    VkSampler                   m_sampler;

    Mesh*                       m_quad;

};

class CVolumetricRenderer : public CRenderer
{
public:
    CVolumetricRenderer(VkRenderPass renderPass);
    virtual ~CVolumetricRenderer();

    virtual void Init() override;
    virtual void Render() override;
    void UpdateGraphicInterface(VkImageView texture3DView, VkImageView depthView);
protected:
    virtual void CreateDescriptorSetLayout() override;
    virtual void PopulatePoolInfo(std::vector<VkDescriptorPoolSize>& poolSize, unsigned int& maxSets) override;

    void CreateCullPipeline(CPipeline& pipeline, VkCullModeFlagBits cullmode);
    void UpdateShaderParams();
private:
    CPipeline               m_frontCullPipeline;
    CPipeline               m_backCullPipeline;
    CPipeline               m_volumetricPipeline;

    VkDescriptorSetLayout   m_volumeDescLayout;
    VkDescriptorSet         m_volumeDescSet;

    VkDescriptorSetLayout   m_volumetricDescLayout;
    VkDescriptorSet         m_volumetricDescSet;

    VkBuffer                m_uniformBuffer;
    VkDeviceMemory          m_uniformMemory;

    VkSampler               m_sampler;
    Mesh*                   m_cube;
};