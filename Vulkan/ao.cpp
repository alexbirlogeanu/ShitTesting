#include "ao.h"

#include "Utils.h"
#include "Mesh.h"

#include <random>

struct SSAOConstParams
{
    glm::vec4 Samples[64];
    glm::vec4 Noise[16];
};

struct SSAOVarParams
{
    glm::mat4 ProjMatrix;
    glm::mat4 ViewMatrix;
};

struct SBlurParams
{
    glm::vec4 Horizontal;
};

enum Bindings
{
    Bindings_Normals = 0,
    Bindings_Positions,
    Bindings_Depth,
    Bindings_Uniform,
    Bindings_Count
};

CAORenderer::CAORenderer(VkRenderPass renderPass)
    : CRenderer(renderPass, "AmbientOcclussionRenderPass")
    , m_quad(nullptr)
    , m_constParamsBuffer(VK_NULL_HANDLE)
    , m_constParamsMemory(VK_NULL_HANDLE)
    , m_varParamsBuffer(VK_NULL_HANDLE)
    , m_varParamsMemory(VK_NULL_HANDLE)
    , m_sampler(VK_NULL_HANDLE)
    , m_constDescSetLayout(VK_NULL_HANDLE)
    , m_varDescSetLayout(VK_NULL_HANDLE)
    , m_blurDescSetLayout(VK_NULL_HANDLE)
{
}

CAORenderer::~CAORenderer()
{
    VkDevice dev = vk::g_vulkanContext.m_device;

    vk::FreeMemory(dev, m_constParamsMemory, nullptr);
    vk::DestroyBuffer(dev, m_constParamsBuffer, nullptr);

    vk::FreeMemory(dev, m_varParamsMemory, nullptr);
    vk::DestroyBuffer(dev, m_varParamsBuffer, nullptr);

    vk::DestroySampler(dev, m_sampler, nullptr);

    vk::DestroyDescriptorSetLayout(dev, m_constDescSetLayout, nullptr);
    vk::DestroyDescriptorSetLayout(dev, m_varDescSetLayout, nullptr);
    vk::DestroyDescriptorSetLayout(dev, m_blurDescSetLayout, nullptr);

    //delete m_quad;
}

void CAORenderer::Init()
{
    CRenderer::Init();

    AllocDescriptors();

    AllocBufferMemory(m_constParamsBuffer, m_constParamsMemory, sizeof(SSAOConstParams), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    AllocBufferMemory(m_varParamsBuffer, m_varParamsMemory, sizeof(SSAOVarParams), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

    InitSSAOParams();

    CreateNearestSampler(m_sampler);
    m_quad = CreateFullscreenQuad();

    std::vector<VkDescriptorSetLayout> mainPassLayouts;

    mainPassLayouts.push_back(m_constDescSetLayout);
    mainPassLayouts.push_back(m_varDescSetLayout);

    m_mainPipeline.SetVertexShaderFile("screenquad.vert");
    m_mainPipeline.SetFragmentShaderFile("ssao.frag");
    m_mainPipeline.SetDepthTest(false);
    m_mainPipeline.SetDepthWrite(false);
    m_mainPipeline.SetVertexInputState(Mesh::GetVertexDesc());
    m_mainPipeline.AddBlendState(CGraphicPipeline::CreateDefaultBlendState(), 2);
    m_mainPipeline.CreatePipelineLayout(mainPassLayouts);
    m_mainPipeline.Init(this, m_renderPass, ESSAOPass_Main);

    m_hblurPipeline.SetVertexShaderFile("screenquad.vert");
    m_hblurPipeline.SetFragmentShaderFile("hblur.frag");
    m_hblurPipeline.SetDepthTest(false);
    m_hblurPipeline.SetDepthWrite(false);
    m_hblurPipeline.SetVertexInputState(Mesh::GetVertexDesc());
    m_hblurPipeline.AddBlendState(CGraphicPipeline::CreateDefaultBlendState());
    m_hblurPipeline.CreatePipelineLayout(m_blurDescSetLayout);
    m_hblurPipeline.Init(this, m_renderPass, ESSAOPass_HBlur);


    m_vblurPipeline.SetVertexShaderFile("screenquad.vert");
    m_vblurPipeline.SetFragmentShaderFile("vblur.frag");
    m_vblurPipeline.SetDepthTest(false);
    m_vblurPipeline.SetDepthWrite(false);
    m_vblurPipeline.SetVertexInputState(Mesh::GetVertexDesc());
    m_vblurPipeline.AddBlendState(CGraphicPipeline::CreateDefaultBlendState());
    m_vblurPipeline.CreatePipelineLayout(m_blurDescSetLayout);
    m_vblurPipeline.Init(this, m_renderPass, ESSAOPass_VBlur);
}

void CAORenderer::Render()
{
    UpdateParams();

    StartRenderPass();
    VkCommandBuffer cmdBuff = vk::g_vulkanContext.m_mainCommandBuffer;

    vk::CmdBindPipeline(cmdBuff, m_mainPipeline.GetBindPoint(), m_mainPipeline.Get());
    vk::CmdBindDescriptorSets(cmdBuff, m_mainPipeline.GetBindPoint(), m_mainPipeline.GetLayout(), 0, (uint32_t)m_mainPassSets.size(), m_mainPassSets.data(), 0, nullptr);
    m_quad->Render();

    vk::CmdNextSubpass(cmdBuff, VK_SUBPASS_CONTENTS_INLINE);
    /*VkImageMemoryBarrier imgBar;
    VkImage img = m_framebuffer->GetColorImage(0);
    AddImageBarrier(imgBar, img, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 
    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
    vk::CmdPipelineBarrier(cmdBuff, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 0, nullptr, 1, &imgBar);  */

    vk::CmdBindPipeline(cmdBuff, m_hblurPipeline.GetBindPoint(), m_hblurPipeline.Get());
    vk::CmdBindDescriptorSets(cmdBuff, m_hblurPipeline.GetBindPoint(), m_hblurPipeline.GetLayout(), 0, 1, &m_blurPassSets[0], 0, nullptr);

    m_quad->Render();
    
    vk::CmdNextSubpass(cmdBuff, VK_SUBPASS_CONTENTS_INLINE);

    vk::CmdBindPipeline(cmdBuff, m_vblurPipeline.GetBindPoint(), m_vblurPipeline.Get());
    vk::CmdBindDescriptorSets(cmdBuff, m_vblurPipeline.GetBindPoint(), m_vblurPipeline.GetLayout(), 0, 1, &m_blurPassSets[1], 0, nullptr);

    m_quad->Render();

    EndRenderPass();
}

void CAORenderer::UpdateGraphicInterface(CFrameBuffer* gbuffer)
{
    std::vector<VkWriteDescriptorSet> wDescSets;
    VkDescriptorImageInfo normalsImgInfo;
    normalsImgInfo.sampler = m_sampler;
    normalsImgInfo.imageView = gbuffer->GetColorImageView(2); //GBuffer_Normals
    normalsImgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    
    VkDescriptorImageInfo positionsImgInfo;
    positionsImgInfo.sampler = m_sampler;
    positionsImgInfo.imageView = gbuffer->GetColorImageView(3); //GBuffer_Position
    positionsImgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo depthImgInfo;
    depthImgInfo.sampler = m_sampler;
    depthImgInfo.imageView = gbuffer->GetDepthImageView();
    depthImgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorBufferInfo constBuffInfo;
    constBuffInfo.buffer = m_constParamsBuffer;
    constBuffInfo.offset = 0;
    constBuffInfo.range = VK_WHOLE_SIZE;

    VkDescriptorBufferInfo varBuffInfo;
    varBuffInfo.buffer = m_varParamsBuffer;
    varBuffInfo.offset = 0;
    varBuffInfo.range = VK_WHOLE_SIZE;

    VkDescriptorImageInfo blurImgInfo;
    blurImgInfo.sampler = m_sampler;
    blurImgInfo.imageView =  m_framebuffer->GetColorImageView(0);
    blurImgInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkDescriptorImageInfo blurImgInfo2;
    blurImgInfo2.sampler = m_sampler;
    blurImgInfo2.imageView =  m_framebuffer->GetColorImageView(2);
    blurImgInfo2.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    wDescSets.push_back(InitUpdateDescriptor(m_mainPassSets[0], Bindings_Normals, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &normalsImgInfo));
    wDescSets.push_back(InitUpdateDescriptor(m_mainPassSets[0], Bindings_Positions, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &positionsImgInfo));
    wDescSets.push_back(InitUpdateDescriptor(m_mainPassSets[0], Bindings_Depth, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &depthImgInfo));
    wDescSets.push_back(InitUpdateDescriptor(m_mainPassSets[0], Bindings_Uniform, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &constBuffInfo));
    wDescSets.push_back(InitUpdateDescriptor(m_mainPassSets[1], 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &varBuffInfo));
    wDescSets.push_back(InitUpdateDescriptor(m_blurPassSets[0], 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &blurImgInfo));
    wDescSets.push_back(InitUpdateDescriptor(m_blurPassSets[1], 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &blurImgInfo2));

    vk::UpdateDescriptorSets(vk::g_vulkanContext.m_device, (uint32_t)wDescSets.size(), wDescSets.data(), 0, nullptr);
}

void CAORenderer::CreateDescriptorSetLayout()
{
    {
        std::vector<VkDescriptorSetLayoutBinding> bindings;
        bindings.resize(Bindings_Count);
        bindings[Bindings_Normals] = CreateDescriptorBinding(Bindings_Normals, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
        bindings[Bindings_Positions] = CreateDescriptorBinding(Bindings_Positions, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
        bindings[Bindings_Depth] = CreateDescriptorBinding(Bindings_Depth, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
        bindings[Bindings_Uniform] = CreateDescriptorBinding(Bindings_Uniform, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);

        VkDescriptorSetLayoutCreateInfo crtInfo;
        cleanStructure(crtInfo);
        crtInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        crtInfo.bindingCount = (uint32_t)bindings.size();
        crtInfo.pBindings = bindings.data();

        VULKAN_ASSERT(vk::CreateDescriptorSetLayout(vk::g_vulkanContext.m_device, &crtInfo, nullptr, &m_constDescSetLayout));
    }

    //this is reserved for varying params
    {
        VkDescriptorSetLayoutBinding binding = CreateDescriptorBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);

        VkDescriptorSetLayoutCreateInfo crtInfo;
        cleanStructure(crtInfo);
        crtInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        crtInfo.bindingCount = 1;
        crtInfo.pBindings = &binding;

        VULKAN_ASSERT(vk::CreateDescriptorSetLayout(vk::g_vulkanContext.m_device, &crtInfo, nullptr, &m_varDescSetLayout));
    }

    //blur subpass
    {
        std::vector<VkDescriptorSetLayoutBinding> bindings;
        bindings.resize(1);
        bindings[0] = CreateDescriptorBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);

        VkDescriptorSetLayoutCreateInfo crtInfo;
        cleanStructure(crtInfo);
        crtInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        crtInfo.bindingCount = (uint32_t)bindings.size();
        crtInfo.pBindings = bindings.data();

        VULKAN_ASSERT(vk::CreateDescriptorSetLayout(vk::g_vulkanContext.m_device, &crtInfo, nullptr, &m_blurDescSetLayout));
    }
}

void CAORenderer::PopulatePoolInfo(std::vector<VkDescriptorPoolSize>& poolSize, unsigned int& maxSets)
{
    AddDescriptorType(poolSize, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 5);
    AddDescriptorType(poolSize, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2);
    maxSets = 4;
}

void CAORenderer::AllocDescriptors()
{
    std::vector<VkDescriptorSetLayout> mainPassLayouts;
    mainPassLayouts.push_back(m_constDescSetLayout);
    mainPassLayouts.push_back(m_varDescSetLayout);

    m_mainPassSets.resize(2);
    AllocDescriptorSets(m_descriptorPool, mainPassLayouts, m_mainPassSets);
    m_blurPassSets.resize(2);
    AllocDescriptorSets(m_descriptorPool, m_blurDescSetLayout, &m_blurPassSets[0]);
    AllocDescriptorSets(m_descriptorPool, m_blurDescSetLayout, &m_blurPassSets[1]);
}

 void CAORenderer::InitSSAOParams()
 {
     TRAP(m_constParamsBuffer);

     m_samples.resize(64);
     m_noise.resize(16);

    unsigned int seed = 162039;
    std::mt19937 generator (seed);
    std::uniform_real_distribution<float> distribution(0.0f, 1.0f);

    SSAOConstParams* mem = nullptr;
    VULKAN_ASSERT(vk::MapMemory(vk::g_vulkanContext.m_device, m_constParamsMemory, 0, VK_WHOLE_SIZE, 0, (void**)&mem));

    for(unsigned int i = 0; i < 64; ++i)
    {
        //tangent space
        float x = distribution(generator) * 2.0f - 1.0f;
        float y = distribution(generator) * 2.0f - 1.0f;
        float z = distribution(generator);
        glm::vec3 sample = glm::normalize(glm::vec3(x, y, z));
        //see what happens if you multiply with random float
        float scale = float(i) / 64.0f;
        scale = glm::mix(0.1f, 1.0f, scale * scale); //nici asta nu inteleg de ce e la patrat
        m_samples[i] = glm::vec4(sample * scale, 0.0f);
    }

    memcpy(mem->Samples, m_samples.data(), sizeof(mem->Samples));

    for(unsigned int i = 0; i < 16; ++i)
    {
        //tangent space too
        //this is a random rotation. we dont rotate around the z axis
        float x = distribution(generator) * 2.0f - 1.0f;
        float y = distribution(generator) * 2.0f - 1.0f;

        glm::vec3 noise = glm::vec3(x, y, 0.0f);
        m_noise[i] = glm::vec4(noise, 0.0f);
    }

    memcpy(mem->Noise, m_noise.data(), sizeof(mem->Noise));

    vk::UnmapMemory(vk::g_vulkanContext.m_device, m_constParamsMemory);
 }

 void CAORenderer::UpdateParams()
 {
     static glm::mat4 projMat;
     PerspectiveMatrix(projMat);
     ConvertToProjMatrix(projMat);

     SSAOVarParams* params;
     VULKAN_ASSERT(vk::MapMemory(vk::g_vulkanContext.m_device, m_varParamsMemory, 0, VK_WHOLE_SIZE, 0, (void**)&params));
     params->ProjMatrix = projMat;
     params->ViewMatrix = ms_camera.GetViewMatrix();
     vk::UnmapMemory(vk::g_vulkanContext.m_device, m_varParamsMemory);
 }