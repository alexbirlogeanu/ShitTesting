#pragma once

#include "Renderer.h"
#include "VulkanLoader.h"
#include "defines.h"
#include "Utils.h"
#include "Mesh.h"

enum ESunPass
{
    ESunPass_Sun = 0,
    ESunPass_BlurV,
    ESunPass_BlurH,
    ESunPass_BlurRadial,
    ESunPass_Count
};

enum ESunFB
{
    ESunFB_Sun,
    ESunFB_Blur1,
    ESunFB_Blur2,
    ESunFB_Final,
    ESunFB_Count
};
class CTexture;
class CUITextContainer;
class CUIManager;
class CSunRenderer : public CRenderer
{
public:
    CSunRenderer(VkRenderPass renderPass);
    virtual ~CSunRenderer();

    virtual void Init() override;
    virtual void Render() override;

    void SetSunTexture(CTexture* t) { m_sunTexture = t; UpdateSunDescriptors(); }
    bool RegisterPick(unsigned int key);

    void CreateEditInfo(CUIManager* manager);
protected:
    struct SSunParams
    {
        glm::vec4 LightDir;
        glm::vec4 LightColor;
        glm::mat4 ViewMatrix;
        glm::mat4 ProjMatrix;
        glm::vec4 CameraRight;
        glm::vec4 CameraUp;
        glm::vec4 Scale; //zw texture scale
    };

    struct SRadialBlurParams
    {
        glm::vec4 ProjSunPos;
        glm::vec4 LightDensity;
        glm::vec4 LightDecay;
        glm::vec4 SampleWeight;
        glm::vec4 LightExposure;
        glm::vec4 ShaftSamples;
    };

    virtual void UpdateGraphicInterface() override;
    virtual void CreateDescriptorSetLayout() override;
    virtual void PopulatePoolInfo(std::vector<VkDescriptorPoolSize>&, unsigned int& maxSets) override;
    virtual void UpdateResourceTable() override;

    void UpdateSunDescriptors();
    void UpdateShaderParams();
    void UpdateEditInfo();
protected:
    VkDescriptorSetLayout       m_blurSetLayout;
    VkDescriptorSetLayout       m_sunDescriptorSetLayout;
    VkDescriptorSetLayout       m_radialBlurSetLayout;

    VkBuffer                    m_sunParamsBuffer;
    VkDeviceMemory              m_sunParamsMemory;

    VkBuffer                    m_radialBlurParamsBuffer;
    VkDeviceMemory              m_radialBlurParamsMemory;

    VkDescriptorSet             m_blurVDescSet;
    VkDescriptorSet             m_blurHDescSet;
    VkDescriptorSet             m_blurRadialDescSet;
    VkDescriptorSet             m_sunDescriptorSet;
    
    CGraphicPipeline                   m_blurVPipeline;
    CGraphicPipeline                   m_blurHPipeline;
    CGraphicPipeline                   m_blurRadialPipeline;
    CGraphicPipeline                   m_sunPipeline;

    Mesh*                       m_quad;
    CTexture*                   m_sunTexture;
    VkSampler                   m_sampler;
    VkSampler                   m_neareastSampler;

    bool                        m_renderSun;
    
    float                       m_sunScale;
    //light shafts
    float                       m_lightShaftDensity;
    float                       m_lightShaftDecay;
    float                       m_lightShaftWeight;
    float                       m_lightShaftExposure;
    float                       m_lightShaftSamples;

    //EDit mode & debug
    bool                        m_isEditMode;
    CUITextContainer*           m_editInfo;
};
