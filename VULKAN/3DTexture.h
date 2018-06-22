#pragma once

#include "Renderer.h"
#include "VulkanLoader.h"
#include "defines.h"
#include "Utils.h"

#include <vector>

///////////////////////
//Test class
//////////////////////
#define TEXTURE3DLAYERS (unsigned int)1
//#define TEST3DTEXT

class Mesh;
class CTexture;
class ImageHandle;
class BufferHandle;
class C3DTextureRenderer : public CRenderer //give a proper name
{
public:
    C3DTextureRenderer (VkRenderPass renderPass);
    virtual ~C3DTextureRenderer();

    virtual void Init() override;
    virtual void Render() override;
	virtual void PreRender() override;

    VkImageView GetOutTexture() const { return m_outTexture->GetView(); } //TODO rename function
protected:
    virtual void CreateDescriptorSetLayout() override;
    virtual void PopulatePoolInfo(std::vector<VkDescriptorPoolSize>&, unsigned int& maxSets) override;

    void AllocateOuputTexture();

    void PrepareTexture();
    void WaitComputeFinish();

    void CopyTexture();
    void AllocateAuxiliarMemory();

    virtual void UpdateResourceTable() override;
    void FillParams();
    void UpdateParams();

    struct FogParameters
    {
        glm::vec4 Globals; //xy - Dir, Z - time
        glm::vec4 NumberOfWaves;
        glm::vec4 Waves[8]; // x - A, y - freq, zw - Velocity
    };

private:
   
    unsigned int                m_width;
    unsigned int                m_height;
    unsigned int                m_depth;

    FogParameters               m_parameters;				

	ImageHandle*                m_outTexture;

    CTexture*                   m_patternTexture;
    
    BufferHandle*               m_uniformBuffer;
    //VkDeviceMemory              m_uniformMemory;

    CComputePipeline            m_generatePipeline;
    VkDescriptorSetLayout       m_generateDescLayout;
    VkDescriptorSet             m_generateDescSet;

    bool                        m_needGenerateTexture;
};

class CVolumetricRenderer : public CRenderer
{
public:
    CVolumetricRenderer(VkRenderPass renderPass);
    virtual ~CVolumetricRenderer();

    virtual void Init() override;
    virtual void Render() override;
	virtual void PreRender() override;
protected:
    virtual void CreateDescriptorSetLayout() override;
    virtual void PopulatePoolInfo(std::vector<VkDescriptorPoolSize>& poolSize, unsigned int& maxSets) override;
    virtual void UpdateGraphicInterface() override;

    void CreateCullPipeline(CGraphicPipeline& pipeline, VkCullModeFlagBits cullmode);
    void UpdateShaderParams();
private:
    CGraphicPipeline               m_frontCullPipeline;
    CGraphicPipeline               m_backCullPipeline;
    CGraphicPipeline               m_volumetricPipeline;

    VkDescriptorSetLayout   m_volumeDescLayout;
    VkDescriptorSet         m_volumeDescSet;

    VkDescriptorSetLayout   m_volumetricDescLayout;
    VkDescriptorSet         m_volumetricDescSet;

    BufferHandle*           m_uniformBuffer;

    VkSampler               m_sampler;
    Mesh*                   m_cube;
};