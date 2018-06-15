#include "Fog.h"

#include "defines.h"
#include "Utils.h"
#include "Mesh.h"
#include <vector>
#include "glm/glm.hpp"

struct SFogParams
{
    glm::mat4   ViewMatrix;
};

CFogRenderer::CFogRenderer(VkRenderPass renderpass)
    : CRenderer(renderpass, "FogRenderPass")
    , m_descriptorSet(VK_NULL_HANDLE)
    , m_descriptorLayout(VK_NULL_HANDLE)
    , m_sampler(VK_NULL_HANDLE)
    , m_quad(nullptr)
    , m_fogParamsBuffer(VK_NULL_HANDLE)
    , m_fogParamsMemory(VK_NULL_HANDLE)
{
}

CFogRenderer::~CFogRenderer()
{
    VkDevice dev = vk::g_vulkanContext.m_device;

    vk::DestroyDescriptorSetLayout(dev, m_descriptorLayout, nullptr);
    vk::DestroySampler(dev, m_sampler, nullptr);
}

void CFogRenderer::Init()
{
    CRenderer::Init();

    AllocDescriptorSets(m_descriptorPool, m_descriptorLayout, &m_descriptorSet);
    AllocBufferMemory(m_fogParamsBuffer, m_fogParamsMemory, sizeof(SFogParams), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

    m_pipline.SetVertexInputState(Mesh::GetVertexDesc());
    m_pipline.SetDepthTest(false);
    m_pipline.SetDepthWrite(false);
    m_pipline.SetVertexShaderFile("screenquad.vert");
    m_pipline.SetFragmentShaderFile("fog.frag");
    
    VkPipelineColorBlendAttachmentState blend = CGraphicPipeline::CreateDefaultBlendState();
    blend.blendEnable = VK_TRUE;
    blend.colorBlendOp = VK_BLEND_OP_ADD;
    blend.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT;

    m_pipline.AddBlendState(blend);
    m_pipline.AddBlendState(CGraphicPipeline::CreateDefaultBlendState());
    m_pipline.CreatePipelineLayout(m_descriptorLayout);
    m_pipline.Init(this, m_renderPass, 0);

    CreateNearestSampler(m_sampler);
    m_quad = CreateFullscreenQuad();
}

void CFogRenderer::Render()
{
    //update shader params
    {
        VkDevice dev = vk::g_vulkanContext.m_device;
        SFogParams* params;
        VULKAN_ASSERT(vk::MapMemory(dev, m_fogParamsMemory, 0, VK_WHOLE_SIZE, 0, (void**)&params));
        params->ViewMatrix = ms_camera.GetViewMatrix();
        vk::UnmapMemory(dev, m_fogParamsMemory);
    }

    StartRenderPass();
    VkCommandBuffer cmdBuff = vk::g_vulkanContext.m_mainCommandBuffer;
    vk::CmdBindPipeline(cmdBuff, m_pipline.GetBindPoint(), m_pipline.Get());
    vk::CmdBindDescriptorSets(cmdBuff, m_pipline.GetBindPoint(), m_pipline.GetLayout(), 0, 1, &m_descriptorSet, 0, nullptr);

    m_quad->Render();
    
    EndRenderPass();
}

void CFogRenderer::UpdateGraphicInterface()
{
    ImageHandle* positionImage = g_commonResources.GetAs<ImageHandle*>(EResourceType_PositionsImage);

    VkDescriptorImageInfo imgInfo;
    imgInfo.sampler = m_sampler;
	imgInfo.imageView = positionImage->GetView();
    imgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorBufferInfo bufInfo;
    bufInfo.buffer = m_fogParamsBuffer;
    bufInfo.offset = 0;
    bufInfo.range = VK_WHOLE_SIZE;

    std::vector<VkWriteDescriptorSet> wDesc;

    wDesc.push_back(InitUpdateDescriptor(m_descriptorSet, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &imgInfo));
    wDesc.push_back(InitUpdateDescriptor(m_descriptorSet, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &bufInfo));

    vk::UpdateDescriptorSets(vk::g_vulkanContext.m_device, (uint32_t)wDesc.size(), wDesc.data(), 0, nullptr);
}

void CFogRenderer::CreateDescriptorSetLayout()
{
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    bindings.push_back(CreateDescriptorBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT));
    bindings.push_back(CreateDescriptorBinding(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT));

    VkDescriptorSetLayoutCreateInfo crtInfo;
    cleanStructure(crtInfo);
    crtInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    crtInfo.bindingCount = (uint32_t)bindings.size();
    crtInfo.pBindings = bindings.data();

    VULKAN_ASSERT(vk::CreateDescriptorSetLayout(vk::g_vulkanContext.m_device, &crtInfo, nullptr, &m_descriptorLayout));
}

void CFogRenderer::PopulatePoolInfo(std::vector<VkDescriptorPoolSize>& poolSize, unsigned int& maxSets)
{
    maxSets = 1;
    AddDescriptorType(poolSize, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1);
    AddDescriptorType(poolSize, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1);
}