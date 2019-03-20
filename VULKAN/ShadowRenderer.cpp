#include "ShadowRenderer.h"

#include "glm/glm.hpp"
#include "Texture.h"
#include "ResourceTable.h"
#include "Object.h"
#include "Batch.h"
#include "Input.h"
#include "UI.h"
#include "GraphicEngine.h"
#include "Framebuffer.h"

#include <random>

//////////////////////////////////////////////////////////////////////////
//CShadowResolveRenderer
//////////////////////////////////////////////////////////////////////////
struct ShadowResolveParameters
{
    glm::vec4   LightDirection;
    glm::vec4   CameraPosition; 
    glm::mat4   ShadowProjMatrix;
	glm::ivec4	NSplits;
	SplitsArrayType Splits;
};

CShadowResolveRenderer::CShadowResolveRenderer(VkRenderPass renderpass)
    : CRenderer(renderpass, "ShadowFactorRenderPass")
    , m_quad(nullptr)
    , m_depthSampler(VK_NULL_HANDLE)
    , m_descriptorLayout(VK_NULL_HANDLE)
    , m_descriptorSet(VK_NULL_HANDLE)
    , m_uniformBuffer(VK_NULL_HANDLE)
    , m_linearSampler(VK_NULL_HANDLE)
    , m_nearSampler(VK_NULL_HANDLE)
    , m_blockerDistrText(nullptr)
    , m_PCFDistrText(nullptr)
#ifdef USE_SHADOW_BLUR
    , m_blurSetLayout(VK_NULL_HANDLE)
    , m_vBlurDescSet(VK_NULL_HANDLE)
    , m_hBlurDescSet(VK_NULL_HANDLE)
#endif
{
}

CShadowResolveRenderer::~CShadowResolveRenderer()
{
    VkDevice dev = vk::g_vulkanContext.m_device;

    vk::DestroySampler(dev, m_depthSampler, nullptr);
    vk::DestroySampler(dev, m_linearSampler, nullptr);
    vk::DestroySampler(dev, m_nearSampler, nullptr);

    vk::DestroyDescriptorSetLayout(dev, m_descriptorLayout, nullptr);
#ifdef USE_SHADOW_BLUR
    vk::DestroyDescriptorSetLayout(dev, m_blurSetLayout, nullptr);
#endif
	MemoryManager::GetInstance()->FreeHandle(m_uniformBuffer);

    delete m_PCFDistrText;
    delete m_blockerDistrText;
}

void CShadowResolveRenderer::Init()
{
    CRenderer::Init();

    AllocDescriptorSets(m_descriptorPool, m_descriptorLayout, &m_descriptorSet);
#ifdef USE_SHADOW_BLUR
    AllocDescriptorSets(m_descriptorPool, m_blurSetLayout, &m_vBlurDescSet);
    AllocDescriptorSets(m_descriptorPool, m_blurSetLayout, &m_hBlurDescSet);
#endif
    CreateLinearSampler(m_linearSampler);
    CreateNearestSampler(m_nearSampler);

    VkSamplerCreateInfo samplerDepthCreateInfo;
    cleanStructure(samplerDepthCreateInfo);
    samplerDepthCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerDepthCreateInfo.magFilter = VK_FILTER_NEAREST;
    samplerDepthCreateInfo.minFilter = VK_FILTER_NEAREST;
    samplerDepthCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerDepthCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerDepthCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerDepthCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerDepthCreateInfo.mipLodBias = 0.0;
    samplerDepthCreateInfo.anisotropyEnable = VK_FALSE;
    samplerDepthCreateInfo.maxAnisotropy = 0;
    samplerDepthCreateInfo.compareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    samplerDepthCreateInfo.minLod = 0.0;
    samplerDepthCreateInfo.maxLod = 0.0;
    samplerDepthCreateInfo.compareEnable = VK_TRUE;
    samplerDepthCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

    VULKAN_ASSERT(vk::CreateSampler(vk::g_vulkanContext.m_device, &samplerDepthCreateInfo, nullptr, &m_depthSampler));

	m_uniformBuffer = MemoryManager::GetInstance()->CreateBuffer(EMemoryContextType::UniformBuffers, sizeof(ShadowResolveParameters), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    m_quad = CreateFullscreenQuad();

    unsigned int width = m_framebuffer->GetWidth();
    unsigned int height = m_framebuffer->GetHeight();

    m_pipeline.SetVertexShaderFile("screenquad.vert");
    m_pipeline.SetFragmentShaderFile("shadowresult.frag");
    m_pipeline.SetDepthTest(false);
    m_pipeline.SetVertexInputState(Mesh::GetVertexDesc());
    m_pipeline.AddBlendState(CGraphicPipeline::CreateDefaultBlendState(), 2);
    m_pipeline.SetViewport(width, height);
    m_pipeline.SetScissor(width, height);
    m_pipeline.CreatePipelineLayout(m_descriptorLayout);
    m_pipeline.Init(this, m_renderPass, 0);

    CreateDistributionTextures();

#ifdef USE_SHADOW_BLUR
    SetupBlurPipeline(m_vBlurPipeline, true);
    SetupBlurPipeline(m_hBlurPipeline, false);
    m_vBlurPipeline.Init(this, m_renderPass, 1);
    m_hBlurPipeline.Init(this, m_renderPass, 2);
#endif
}

void CShadowResolveRenderer::PreRender()
{
	UpdateShaderParams();
}

void CShadowResolveRenderer::Render()
{
    StartRenderPass();
    VkCommandBuffer cmdBuff = vk::g_vulkanContext.m_mainCommandBuffer;
    vk::CmdBindPipeline(cmdBuff, m_pipeline.GetBindPoint(), m_pipeline.Get());
    vk::CmdBindDescriptorSets(cmdBuff, m_pipeline.GetBindPoint(), m_pipeline.GetLayout(), 0, 1, &m_descriptorSet, 0, nullptr);

    m_quad->Render();
#ifdef USE_SHADOW_BLUR
    vk::CmdNextSubpass(cmdBuff, VK_SUBPASS_CONTENTS_INLINE);
    vk::CmdBindPipeline(cmdBuff, m_vBlurPipeline.GetBindPoint(), m_vBlurPipeline.Get());
    vk::CmdBindDescriptorSets(cmdBuff, m_vBlurPipeline.GetBindPoint(), m_vBlurPipeline.GetLayout(), 0, 1, &m_vBlurDescSet, 0, nullptr);

    m_quad->Render();

    vk::CmdNextSubpass(cmdBuff, VK_SUBPASS_CONTENTS_INLINE);
    vk::CmdBindPipeline(cmdBuff, m_hBlurPipeline.GetBindPoint(), m_hBlurPipeline.Get());
    vk::CmdBindDescriptorSets(cmdBuff, m_hBlurPipeline.GetBindPoint(), m_hBlurPipeline.GetLayout(), 0, 1, &m_hBlurDescSet, 0, nullptr);

    m_quad->Render();
#endif
    EndRenderPass();
}

void CShadowResolveRenderer::UpdateShaderParams()
{
    glm::mat4 shadowProj = g_commonResources.GetAs<glm::mat4>(EResourceType_ShadowProjViewMat);

    ShadowResolveParameters* params = m_uniformBuffer->GetPtr<ShadowResolveParameters*>();
    params->ShadowProjMatrix = shadowProj;
    params->LightDirection = directionalLight.GetDirection();
    params->CameraPosition = glm::vec4(ms_camera.GetPos(), 1.0f);
	params->Splits = g_commonResources.GetAs<SplitsArrayType>(EResourceType_ShadowMapSplits);
	params->NSplits = glm::ivec4(SHADOWSPLITS);
}

void CShadowResolveRenderer::UpdateGraphicInterface()
{
	ImageHandle* normalImage = g_commonResources.GetAs<ImageHandle*>(EResourceType_NormalsImage);
	ImageHandle* positionImage = g_commonResources.GetAs<ImageHandle*>(EResourceType_PositionsImage);
	ImageHandle* shadowMapImage = g_commonResources.GetAs<ImageHandle*>(EResourceType_ShadowMapImage);
	ImageHandle* depthImage = g_commonResources.GetAs<ImageHandle*>(EResourceType_DepthBufferImage);

    VkDescriptorImageInfo normalInfo = CreateDescriptorImageInfo(m_nearSampler, normalImage->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    VkDescriptorImageInfo posInfo = CreateDescriptorImageInfo(m_nearSampler, positionImage->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    VkDescriptorImageInfo shadowhInfo = CreateDescriptorImageInfo(m_depthSampler, shadowMapImage->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	//VkDescriptorImageInfo shadowhInfo = CreateDescriptorImageInfo(m_depthSampler, shadowMapImage->GetLayerView(0), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	VkDescriptorBufferInfo constInfo = m_uniformBuffer->GetDescriptor();
    VkDescriptorImageInfo shadowText = CreateDescriptorImageInfo(m_nearSampler, shadowMapImage->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    VkDescriptorImageInfo blockerDistText = CreateDescriptorImageInfo(m_nearSampler, m_blockerDistrText->GetImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    VkDescriptorImageInfo pcfDistText = CreateDescriptorImageInfo(m_nearSampler, m_PCFDistrText->GetImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	VkDescriptorImageInfo depthText = CreateDescriptorImageInfo(m_nearSampler, depthImage->GetView(), VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

    std::vector<VkWriteDescriptorSet> wDesc;
    wDesc.push_back(InitUpdateDescriptor(m_descriptorSet, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &constInfo));
    wDesc.push_back(InitUpdateDescriptor(m_descriptorSet, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &shadowhInfo));
    wDesc.push_back(InitUpdateDescriptor(m_descriptorSet, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &posInfo));
    wDesc.push_back(InitUpdateDescriptor(m_descriptorSet, 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &normalInfo));
    wDesc.push_back(InitUpdateDescriptor(m_descriptorSet, 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &shadowText));
    wDesc.push_back(InitUpdateDescriptor(m_descriptorSet, 5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &blockerDistText));
    wDesc.push_back(InitUpdateDescriptor(m_descriptorSet, 6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &pcfDistText));
	wDesc.push_back(InitUpdateDescriptor(m_descriptorSet, 7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &depthText));

#ifdef USE_SHADOW_BLUR
    VkSampler blurSampler = m_nearSampler;
    VkDescriptorImageInfo vBlur = CreateDescriptorImageInfo(blurSampler, m_framebuffer->GetColorImageView(0), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkDescriptorImageInfo hBlur = CreateDescriptorImageInfo(blurSampler, m_framebuffer->GetColorImageView(2), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    wDesc.push_back(InitUpdateDescriptor(m_vBlurDescSet, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &vBlur));
    wDesc.push_back(InitUpdateDescriptor(m_hBlurDescSet, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &hBlur));
#endif
    vk::UpdateDescriptorSets(vk::g_vulkanContext.m_device, (uint32_t)wDesc.size(), wDesc.data(), 0, nullptr);
}

void CShadowResolveRenderer::CreateDescriptorSetLayout()
{
    {
        std::vector<VkDescriptorSetLayoutBinding> bindings;
        bindings.push_back(CreateDescriptorBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT));
        bindings.push_back(CreateDescriptorBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT));
        bindings.push_back(CreateDescriptorBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT));
        bindings.push_back(CreateDescriptorBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT));
        bindings.push_back(CreateDescriptorBinding(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT));
        bindings.push_back(CreateDescriptorBinding(5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT));
        bindings.push_back(CreateDescriptorBinding(6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT));
		bindings.push_back(CreateDescriptorBinding(7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT));

        VkDescriptorSetLayoutCreateInfo crtInfo;
        cleanStructure(crtInfo);
        crtInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        crtInfo.bindingCount = (uint32_t)bindings.size();
        crtInfo.pBindings = bindings.data();

        VULKAN_ASSERT(vk::CreateDescriptorSetLayout(vk::g_vulkanContext.m_device, &crtInfo, nullptr, &m_descriptorLayout));
    }

#ifdef USE_SHADOW_BLUR
    {
        std::vector<VkDescriptorSetLayoutBinding> bindings;
        bindings.push_back(CreateDescriptorBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT));

        VkDescriptorSetLayoutCreateInfo crtInfo;
        cleanStructure(crtInfo);
        crtInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        crtInfo.bindingCount = (uint32_t)bindings.size();
        crtInfo.pBindings = bindings.data();

        VULKAN_ASSERT(vk::CreateDescriptorSetLayout(vk::g_vulkanContext.m_device, &crtInfo, nullptr, &m_blurSetLayout));
    }
#endif
}

void CShadowResolveRenderer::PopulatePoolInfo(std::vector<VkDescriptorPoolSize>& poolSize, unsigned int& maxSets)
{
    maxSets = 3;
    AddDescriptorType(poolSize, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1);
    AddDescriptorType(poolSize, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 9);
}

void CShadowResolveRenderer::UpdateResourceTable()
{
    UpdateResourceTableForColor(0, EResourceType_ResolvedShadowImage);
}

#ifdef USE_SHADOW_BLUR
void CShadowResolveRenderer::SetupBlurPipeline(CGraphicPipeline& pipeline, bool isVertical)
{
    unsigned int width = m_framebuffer->GetWidth();
    unsigned int height = m_framebuffer->GetHeight();

    pipeline.SetVertexShaderFile("screenquad.vert");
    pipeline.SetFragmentShaderFile((isVertical)? "vblur.frag" : "hblur.frag");
    //pipeline.SetFragmentShaderFile("passtrough.frag");
    pipeline.SetDepthTest(false);
    pipeline.SetVertexInputState(Mesh::GetVertexDesc());
    pipeline.AddBlendState(CGraphicPipeline::CreateDefaultBlendState());
    pipeline.SetViewport(width, height);
    pipeline.SetScissor(width, height);
    pipeline.CreatePipelineLayout(m_blurSetLayout);
}
#endif
CTexture* CreateDistTexture(glm::vec2* data, unsigned int samples)
{
    SImageData textData(samples, 1, 1, VK_FORMAT_R32G32_SFLOAT, (unsigned char*)data);
    return new CTexture(textData, false);
}

void CShadowResolveRenderer::CreateDistributionTextures() //this textures are not used
{
    const unsigned int blockSamples = 32;
    const unsigned int PCFSamples = 64;

    glm::vec2 blockerDistData[blockSamples];
    glm::vec2 PCFDistData[PCFSamples];

    unsigned int seed = 6914692;
    std::mt19937 generator (seed);
    std::uniform_real_distribution<float> distribution(-1.0f, 1.0f);

    for(unsigned int i = 0; i < blockSamples; ++i)
        blockerDistData[i] = glm::vec2(distribution(generator), distribution(generator));

    for(unsigned int i = 0; i < PCFSamples; ++i)
        PCFDistData[i] = glm::vec2(distribution(generator), distribution(generator));

    m_blockerDistrText = CreateDistTexture(blockerDistData, blockSamples);
    m_PCFDistrText = CreateDistTexture(PCFDistData, PCFSamples);
}

//////////////////////////////////////////////////////////////////
//ShadowResolveRenderer
//////////////////////////////////////////////////////////////////

struct ShadowResolveParameters2
{
	glm::vec4   LightDirection;
	glm::vec4   CameraPosition;
	glm::mat4   ShadowProjMatrix;
	glm::ivec4	NSplits;
	SplitsArrayType Splits;
};


ShadowResolveRenderer::ShadowResolveRenderer()
	: Renderer()
	, m_quad(nullptr)
	, m_depthSampler(VK_NULL_HANDLE)
	, m_descriptorSet(VK_NULL_HANDLE)
	, m_uniformBuffer(nullptr)
	, m_linearSampler(VK_NULL_HANDLE)
	, m_nearSampler(VK_NULL_HANDLE)
	, m_blockerDistrText(nullptr)
	, m_PCFDistrText(nullptr)
{

}

ShadowResolveRenderer::~ShadowResolveRenderer()
{
	VkDevice dev = vk::g_vulkanContext.m_device;

	delete m_PCFDistrText;
	delete m_blockerDistrText;

	MemoryManager::GetInstance()->FreeHandle(m_uniformBuffer);

	vk::DestroySampler(dev, m_depthSampler, nullptr);
	vk::DestroySampler(dev, m_linearSampler, nullptr);
	vk::DestroySampler(dev, m_nearSampler, nullptr);
}

void ShadowResolveRenderer::Setup(VkRenderPass renderPass, uint32_t subpassId)
{
	unsigned int width = WIDTH;  //this is not ok
	unsigned int height = HEIGHT;

	m_pipeline.SetVertexShaderFile("screenquad.vert");
	m_pipeline.SetFragmentShaderFile("shadowresult.frag");
	m_pipeline.SetDepthTest(false);
	m_pipeline.SetVertexInputState(Mesh::GetVertexDesc());
	m_pipeline.AddBlendState(CGraphicPipeline::CreateDefaultBlendState(), 2);
	m_pipeline.SetViewport(width, height);
	m_pipeline.SetScissor(width, height);
	m_pipeline.CreatePipelineLayout(m_descriptorLayout.Get());
	m_pipeline.Setup(renderPass, subpassId);
	RegisterPipeline(&m_pipeline);
}

void ShadowResolveRenderer::Render()
{
	StartDebugMarker("ShadowResolve");
	VkCommandBuffer cmdBuff = vk::g_vulkanContext.m_mainCommandBuffer;
	vk::CmdBindPipeline(cmdBuff, m_pipeline.GetBindPoint(), m_pipeline.Get());
	vk::CmdBindDescriptorSets(cmdBuff, m_pipeline.GetBindPoint(), m_pipeline.GetLayout(), 0, 1, &m_descriptorSet, 0, nullptr);

	m_quad->Render();
	EndDebugMarker("ShadowResolve");
}

void ShadowResolveRenderer::PreRender()
{
	UpdateShaderParams();
}

void ShadowResolveRenderer::CreateDescriptorSetLayouts()
{
	std::vector<VkDescriptorSetLayoutBinding> bindings;

	m_descriptorLayout.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
	m_descriptorLayout.AddBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
	m_descriptorLayout.AddBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
	m_descriptorLayout.AddBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
	m_descriptorLayout.AddBinding(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
	m_descriptorLayout.AddBinding(5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
	m_descriptorLayout.AddBinding(6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
	m_descriptorLayout.AddBinding(7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);

	m_descriptorLayout.Construct();

	RegisterDescriptorSetLayout(&m_descriptorLayout);
}

void ShadowResolveRenderer::UpdateGraphicInterface()
{
	ImageHandle* normalImage = GraphicEngine::GetAttachment("Normals")->GetHandle();
	ImageHandle* positionImage = GraphicEngine::GetAttachment("Positions")->GetHandle();
	ImageHandle* shadowMapImage = GraphicEngine::GetAttachment("ShadowMap")->GetHandle();
	ImageHandle* depthImage = GraphicEngine::GetAttachment("Depth")->GetHandle();

	VkDescriptorImageInfo normalInfo = CreateDescriptorImageInfo(m_nearSampler, normalImage->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	VkDescriptorImageInfo posInfo = CreateDescriptorImageInfo(m_nearSampler, positionImage->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	VkDescriptorImageInfo shadowhInfo = CreateDescriptorImageInfo(m_depthSampler, shadowMapImage->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	//VkDescriptorImageInfo shadowhInfo = CreateDescriptorImageInfo(m_depthSampler, shadowMapImage->GetLayerView(0), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	VkDescriptorBufferInfo constInfo = m_uniformBuffer->GetDescriptor();
	VkDescriptorImageInfo shadowText = CreateDescriptorImageInfo(m_nearSampler, shadowMapImage->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	VkDescriptorImageInfo blockerDistText = CreateDescriptorImageInfo(m_nearSampler, m_blockerDistrText->GetImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	VkDescriptorImageInfo pcfDistText = CreateDescriptorImageInfo(m_nearSampler, m_PCFDistrText->GetImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	VkDescriptorImageInfo depthText = CreateDescriptorImageInfo(m_nearSampler, depthImage->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	std::vector<VkWriteDescriptorSet> wDesc;
	wDesc.push_back(InitUpdateDescriptor(m_descriptorSet, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &constInfo));
	wDesc.push_back(InitUpdateDescriptor(m_descriptorSet, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &shadowhInfo));
	wDesc.push_back(InitUpdateDescriptor(m_descriptorSet, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &posInfo));
	wDesc.push_back(InitUpdateDescriptor(m_descriptorSet, 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &normalInfo));
	wDesc.push_back(InitUpdateDescriptor(m_descriptorSet, 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &shadowText));
	wDesc.push_back(InitUpdateDescriptor(m_descriptorSet, 5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &blockerDistText));
	wDesc.push_back(InitUpdateDescriptor(m_descriptorSet, 6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &pcfDistText));
	wDesc.push_back(InitUpdateDescriptor(m_descriptorSet, 7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &depthText));

	vk::UpdateDescriptorSets(vk::g_vulkanContext.m_device, (uint32_t)wDesc.size(), wDesc.data(), 0, nullptr);
}

void ShadowResolveRenderer::AllocateDescriptorSets()
{
	m_descriptorSet = m_descriptorPool.AllocateDescriptorSet(m_descriptorLayout);
}

void ShadowResolveRenderer::InitInternal()
{
	CreateLinearSampler(m_linearSampler);
	CreateNearestSampler(m_nearSampler);

	VkSamplerCreateInfo samplerDepthCreateInfo;
	cleanStructure(samplerDepthCreateInfo);
	samplerDepthCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerDepthCreateInfo.magFilter = VK_FILTER_NEAREST;
	samplerDepthCreateInfo.minFilter = VK_FILTER_NEAREST;
	samplerDepthCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	samplerDepthCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	samplerDepthCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	samplerDepthCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	samplerDepthCreateInfo.mipLodBias = 0.0;
	samplerDepthCreateInfo.anisotropyEnable = VK_FALSE;
	samplerDepthCreateInfo.maxAnisotropy = 0;
	samplerDepthCreateInfo.compareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	samplerDepthCreateInfo.minLod = 0.0;
	samplerDepthCreateInfo.maxLod = 0.0;
	samplerDepthCreateInfo.compareEnable = VK_TRUE;
	samplerDepthCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

	VULKAN_ASSERT(vk::CreateSampler(vk::g_vulkanContext.m_device, &samplerDepthCreateInfo, nullptr, &m_depthSampler));

	m_uniformBuffer = MemoryManager::GetInstance()->CreateBuffer(EMemoryContextType::UniformBuffers, sizeof(ShadowResolveParameters), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
	m_quad = CreateFullscreenQuad();

	CreateDistributionTextures();
}

void ShadowResolveRenderer::CreateDistributionTextures()
{
	const unsigned int blockSamples = 32;
	const unsigned int PCFSamples = 64;

	glm::vec2 blockerDistData[blockSamples];
	glm::vec2 PCFDistData[PCFSamples];

	unsigned int seed = 6914692;
	std::mt19937 generator(seed);
	std::uniform_real_distribution<float> distribution(-1.0f, 1.0f);

	for (unsigned int i = 0; i < blockSamples; ++i)
		blockerDistData[i] = glm::vec2(distribution(generator), distribution(generator));

	for (unsigned int i = 0; i < PCFSamples; ++i)
		PCFDistData[i] = glm::vec2(distribution(generator), distribution(generator));

	m_blockerDistrText = CreateDistTexture(blockerDistData, blockSamples);
	m_PCFDistrText = CreateDistTexture(PCFDistData, PCFSamples);
}

void ShadowResolveRenderer::UpdateShaderParams()
{
	const FrameConstants& fc = GraphicEngine::GetFrameConstants();

	ShadowResolveParameters2* params = m_uniformBuffer->GetPtr<ShadowResolveParameters2*>();
	params->ShadowProjMatrix = glm::mat4(1.0f); //unused. I think
	params->LightDirection = fc.DirectionalLightDirection; //unused
	params->CameraPosition = glm::vec4(ms_camera.GetPos(), 1.0f);
	params->Splits = fc.ShadowSplits;
	params->NSplits = glm::ivec4(fc.NumberOfShadowSplits);
}
