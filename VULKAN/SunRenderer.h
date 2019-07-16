#pragma once

#include "Renderer.h"
#include "VulkanLoader.h"
#include "defines.h"
#include "Utils.h"
#include "Mesh.h"

class CTexture;
class CUIManager;
class KeyInput;

class SunRenderer : public Renderer
{
public:
	SunRenderer();
    virtual ~SunRenderer();

	void SetupSunSubpass(VkRenderPass renderPass, uint32_t subpassId);
	void SetupBlurVSubpass(VkRenderPass renderPass, uint32_t subpassId);
	void SetupBlurHSubpass(VkRenderPass renderPass, uint32_t subpassId);
	void SetupBlurRadialSubpass(VkRenderPass renderPass, uint32_t subpassId);

	void RenderSunSubpass();
	void RenderBlurVSubpass();
	void RenderBlurHSubpass();
	void RenderRadialBlurSubpass();

	void PreRender();
private:
	virtual void InitInternal() override;
	virtual void UpdateGraphicInterface() override;
	virtual void CreateDescriptorSetLayouts() override;
	virtual void AllocateDescriptorSets() override;

	void SetupPipeline(CGraphicPipeline& pipeline, VkRenderPass renderpass, unsigned int subpass, char* vertex, char* fragment, DescriptorSetLayout& layout);
	void Render(CGraphicPipeline& pipline, VkDescriptorSet& set);

	bool OnKeyPressed(const KeyInput& key);

    void CreateEditInfo();
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

    void UpdateShaderParams();
    void UpdateEditInfo();
protected:
	DescriptorSetLayout     m_blurSetLayout;
	DescriptorSetLayout     m_sunDescriptorSetLayout;
	DescriptorSetLayout     m_radialBlurSetLayout;

    BufferHandle*           m_sunParamsBuffer;
	BufferHandle*           m_radialBlurParamsBuffer;

    VkDescriptorSet         m_blurVDescSet;
    VkDescriptorSet         m_blurHDescSet;
    VkDescriptorSet         m_blurRadialDescSet;
    VkDescriptorSet         m_sunDescriptorSet;
    
    CGraphicPipeline        m_blurVPipeline;
    CGraphicPipeline        m_blurHPipeline;
    CGraphicPipeline        m_blurRadialPipeline;
    CGraphicPipeline        m_sunPipeline;

    Mesh*                   m_quad;
    CTexture*               m_sunTexture;
    VkSampler               m_sampler;
    VkSampler               m_neareastSampler;

    bool                    m_renderSun;
	uint32_t				m_frameWidth;
	uint32_t				m_frameHeight;

    float                   m_sunScale;
    //light shafts
    float                   m_lightShaftDensity;
    float                   m_lightShaftDecay;
    float                   m_lightShaftWeight;
    float                   m_lightShaftExposure;
    float                   m_lightShaftSamples;

    //Edit mode & debug
    bool                    m_isEditMode;
    //CUITextContainer*           m_editInfo;
};

