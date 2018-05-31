#include "ShadowRenderer.h"

#include "glm/glm.hpp"
#include "Texture.h"
#include "ResourceTable.h"
#include "Object.h"

#include <random>

//////////////////////////////////////////////////////////////////////////
//ShadowMapRenderer
//////////////////////////////////////////////////////////////////////////

struct ShadowMapParams
{
    glm::mat4 MVP;
};

ShadowMapRenderer::ShadowMapRenderer(VkRenderPass renderPass, const std::vector<Object*>& shadowCasters)
    : CRenderer(renderPass, "ShadowmapRenderPass")
    , m_instanceBuffer(VK_NULL_HANDLE)
    , m_instaceMemory(VK_NULL_HANDLE)
{
    m_nodes.reserve(shadowCasters.size());
    for (auto caster : shadowCasters)
    {
        m_nodes.push_back(Node(caster));
    }
}

ShadowMapRenderer::~ShadowMapRenderer()
{
    VkDevice dev = vk::g_vulkanContext.m_device;

    vk::FreeMemory(dev, m_instaceMemory, nullptr);
    vk::DestroyBuffer(dev, m_instanceBuffer, nullptr);
}

void ShadowMapRenderer::Init()
{
	if (m_nodes.empty())
		return;

    CRenderer::Init();

    m_pipeline.SetVertexInputState(Mesh::GetVertexDesc());
    m_pipeline.SetViewport(SHADOWW, SHADOWH);
    m_pipeline.SetScissor(SHADOWW, SHADOWH);
    m_pipeline.SetCullMode(VK_CULL_MODE_BACK_BIT);
    m_pipeline.SetVertexShaderFile("shadow.vert");
    //m_pipeline.SetFragmentShaderFile("shadowlineardepth.frag");
    m_pipeline.SetStencilTest(true);
    m_pipeline.SetStencilOp(VK_COMPARE_OP_ALWAYS);
    m_pipeline.SetStencilOperations(VK_STENCIL_OP_REPLACE, VK_STENCIL_OP_REPLACE, VK_STENCIL_OP_REPLACE);
    m_pipeline.SetStencilValues(0x01, 0x01, 1);

    m_pipeline.CreatePipelineLayout(m_descriptorSetLayout);
    m_pipeline.Init(this, m_renderPass, 0);

    InitNodesDescriptorSet();
    InitNodesMemory();
}

void ShadowMapRenderer::ComputeProjMatrix(glm::mat4& proj, const glm::mat4& view)
{
    //const CFrustrum& frustrum = ms_camera.GetFrustrum();
    CFrustrum frustrum (ms_camera.GetNear(), 5.0f);
    frustrum.Update(ms_camera.GetPos(), ms_camera.GetFrontVector(), ms_camera.GetUpVector(), ms_camera.GetRightVector(), ms_camera.GetFOV());

    glm::vec4 maxLimits = glm::vec4(std::numeric_limits<float>::min());
    glm::vec4 minLimits = glm::vec4(std::numeric_limits<float>::max());
    for(unsigned int i = 0; i < CFrustrum::FPCount; ++i)
    {
        glm::vec4 lightPos = view * glm::vec4(frustrum.GetPoint(i), 1.0f);
        maxLimits = glm::max(maxLimits, lightPos);
        minLimits = glm::min(minLimits, lightPos);
    }
    BoundingBox sceneBoundingbox = CScene::GetBoundingBox();

    //float near1;
    //float far1;
    //{
    //    BoundingBox bb = sceneBoundingbox;
    //    //transform it in light space
    //    bb.Max = glm::vec3(view * glm::vec4(bb.Max, 1.0f)); //LUL
    //    bb.Min = glm::vec3(view * glm::vec4(bb.Min, 1.0f));
    //    near1 = glm::min(bb.Max.z, bb.Min.z);
    //    far1 = glm::max(bb.Max.z, bb.Min.z);
    //}

    float near2 = std::numeric_limits<float>::max();
    float far2 = std::numeric_limits<float>::min();
    //{
    BoundingBox bb = sceneBoundingbox;
    std::vector<glm::vec3> bbPoints;
    bb.Transform(view, bbPoints);
    for(unsigned int i = 0; i < bbPoints.size(); ++i)
    {
        near2 = glm::min(bbPoints[i].z, near2);
        far2 = glm::max(bbPoints[i].z, far2);
    }
    //}
    proj = glm::ortho(minLimits.x, maxLimits.x , minLimits.y, maxLimits.y, -far2, -near2);
    ConvertToProjMatrix(proj);
}

void ShadowMapRenderer::InitNodesDescriptorSet()
{
    std::vector<VkDescriptorSetLayout> layouts (m_nodes.size(), m_descriptorSetLayout);
    std::vector<VkDescriptorSet> sets(m_nodes.size());

    AllocDescriptorSets(m_descriptorPool, layouts, sets);

    for (unsigned int i = 0; i < sets.size(); ++i)
    {
        m_nodes[i].descSet = sets[i];
    }

}

void ShadowMapRenderer::InitNodesMemory()
{
    VkDeviceSize memOffset = vk::g_vulkanContext.m_limits.minUniformBufferOffsetAlignment; 
    TRAP(memOffset > sizeof(ShadowMapParams) && "Change mem offset");
    AllocBufferMemory(m_instanceBuffer, m_instaceMemory,uint32_t( m_nodes.size() * memOffset), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

    for (unsigned int i = 0; i < m_nodes.size(); ++i)
    {
        m_nodes[i].offset = i * memOffset;
    }
}

void ShadowMapRenderer::UpdateShaderParams()
{
    void* memPtr = nullptr;
    VULKAN_ASSERT(vk::MapMemory(vk::g_vulkanContext.m_device, m_instaceMemory, 0, VK_WHOLE_SIZE, 0, &memPtr));

    for (auto& node : m_nodes)
    {
        ShadowMapParams* params = (ShadowMapParams*)((char*)memPtr + node.offset);
        params->MVP = m_shadowViewProj * node.obj->GetModelMatrix();
    }

    vk::UnmapMemory(vk::g_vulkanContext.m_device, m_instaceMemory);
}

void ShadowMapRenderer::UpdateGraphicInterface()
{
    //this section is to declare all the variables used for creating the update info in no particular order
    std::vector<VkWriteDescriptorSet> wDesc;
    //these have to be cached otherwise vk::UpdateDescriptorSets will crash, pointers will become invalid
    std::vector<VkDescriptorBufferInfo> buffInfos;
    buffInfos.reserve(m_nodes.size());

    for (auto& node : m_nodes)
    {
        buffInfos.push_back(CreateDescriptorBufferInfo(m_instanceBuffer, node.offset, sizeof(ShadowMapParams)));
        wDesc.push_back(InitUpdateDescriptor(node.descSet, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &buffInfos.back()));
    }

    vk::UpdateDescriptorSets(vk::g_vulkanContext.m_device, (uint32_t)wDesc.size(), wDesc.data(), 0, nullptr);
}

void ShadowMapRenderer::Render()
{
	if (m_nodes.empty())
		return;

    VkCommandBuffer cmd = vk::g_vulkanContext.m_mainCommandBuffer;

    vk::CmdBindPipeline(cmd, m_pipeline.GetBindPoint(), m_pipeline.Get());

    glm::vec3 lightDir (directionalLight.GetDirection());
    glm::vec3 eye = ms_camera.GetPos() - 1.0f * lightDir;

    glm::vec3 right = glm::cross(lightDir, glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 view = glm::lookAt(eye, ms_camera.GetPos(), glm::cross(lightDir, right));
    glm::mat4 proj;
    ComputeProjMatrix(proj, view);

    //= glm::ortho(-5.0f, 5.0f, -5.0f, 5.0f, 0.1f, 25.0f);
    m_shadowViewProj = proj * view;
    
    UpdateShaderParams();

    for (auto& node : m_nodes)
    {
        vk::CmdBindDescriptorSets(cmd, m_pipeline.GetBindPoint(), m_pipeline.GetLayout(), 0, 1, &node.descSet, 0, nullptr); //try to bind just once
        node.obj->Render();
    }

}

void ShadowMapRenderer::PopulatePoolInfo(std::vector<VkDescriptorPoolSize>& poolSize, unsigned int& maxSets)
{
    AddDescriptorType(poolSize, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, (uint32_t)m_nodes.size());
    maxSets = (uint32_t)m_nodes.size();
}

void ShadowMapRenderer::UpdateResourceTable()
{
    UpdateResourceTableForDepth(EResourceType_ShadowMapImage);
    g_commonResources.SetAs<glm::mat4>(&m_shadowViewProj, EResourceType_ShadowProjViewMat);
}

void ShadowMapRenderer::CreateDescriptorSetLayout()
{
    std::vector<VkDescriptorSetLayoutBinding> descCnt;
    descCnt.resize(1);
    descCnt[0] = CreateDescriptorBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);

    NewDescriptorSetLayout(descCnt, &m_descriptorSetLayout);
}

//////////////////////////////////////////////////////////////////////////
//CShadowResolveRenderer
//////////////////////////////////////////////////////////////////////////
struct ShadowResolveParameters
{
    glm::vec4   LightDirection;
    glm::vec4   CameraPosition; 
    glm::mat4   ShadowProjMatrix;
};

CShadowResolveRenderer::CShadowResolveRenderer(VkRenderPass renderpass)
    : CRenderer(renderpass, "ShadowFactorRenderPass")
    , m_quad(nullptr)
    , m_depthSampler(VK_NULL_HANDLE)
    , m_descriptorLayout(VK_NULL_HANDLE)
    , m_descriptorSet(VK_NULL_HANDLE)
    , m_uniformMemory(VK_NULL_HANDLE)
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
    vk::FreeMemory(dev, m_uniformMemory, nullptr);
    vk::DestroyBuffer(dev, m_uniformBuffer, nullptr);

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
    samplerDepthCreateInfo.magFilter = VK_FILTER_LINEAR;
    samplerDepthCreateInfo.minFilter = VK_FILTER_LINEAR;
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

    AllocBufferMemory(m_uniformBuffer, m_uniformMemory, sizeof(ShadowResolveParameters), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

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

void CShadowResolveRenderer::Render()
{
    UpdateShaderParams();
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

    ShadowResolveParameters* params = nullptr;
    VULKAN_ASSERT(vk::MapMemory(vk::g_vulkanContext.m_device, m_uniformMemory, 0, VK_WHOLE_SIZE,0, (void**)&params));
    params->ShadowProjMatrix = shadowProj;
    params->LightDirection = directionalLight.GetDirection();
    params->CameraPosition = glm::vec4(ms_camera.GetPos(), 1.0f);
    vk::UnmapMemory(vk::g_vulkanContext.m_device, m_uniformMemory);
}

void CShadowResolveRenderer::UpdateGraphicInterface()
{
    VkImageView normalView = g_commonResources.GetAs<VkImageView>(EResourceType_NormalsImageView);
    VkImageView posView = g_commonResources.GetAs<VkImageView>(EResourceType_PositionsImageView);
    VkImageView shadowMapView = g_commonResources.GetAs<VkImageView>(EResourceType_ShadowMapImageView);

    VkDescriptorImageInfo normalInfo = CreateDescriptorImageInfo(m_nearSampler, normalView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    VkDescriptorImageInfo posInfo = CreateDescriptorImageInfo(m_nearSampler, posView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    VkDescriptorImageInfo shadowhInfo = CreateDescriptorImageInfo(m_depthSampler, shadowMapView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    VkDescriptorBufferInfo constInfo = CreateDescriptorBufferInfo(m_uniformBuffer);
    VkDescriptorImageInfo shadowText = CreateDescriptorImageInfo(m_nearSampler, shadowMapView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    VkDescriptorImageInfo blockerDistText = CreateDescriptorImageInfo(m_nearSampler, m_blockerDistrText->GetImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    VkDescriptorImageInfo pcfDistText = CreateDescriptorImageInfo(m_nearSampler, m_PCFDistrText->GetImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    std::vector<VkWriteDescriptorSet> wDesc;
    wDesc.push_back(InitUpdateDescriptor(m_descriptorSet, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &constInfo));
    wDesc.push_back(InitUpdateDescriptor(m_descriptorSet, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &shadowhInfo));
    wDesc.push_back(InitUpdateDescriptor(m_descriptorSet, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &posInfo));
    wDesc.push_back(InitUpdateDescriptor(m_descriptorSet, 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &normalInfo));
    wDesc.push_back(InitUpdateDescriptor(m_descriptorSet, 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &shadowText));
    wDesc.push_back(InitUpdateDescriptor(m_descriptorSet, 5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &blockerDistText));
    wDesc.push_back(InitUpdateDescriptor(m_descriptorSet, 6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &pcfDistText));

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
    AddDescriptorType(poolSize, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 8);
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
