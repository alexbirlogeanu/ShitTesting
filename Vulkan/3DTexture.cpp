#include "3DTexture.h"
#include "Mesh.h"

C3DTextureRenderer::C3DTextureRenderer (VkRenderPass renderPass)
    : CRenderer(renderPass)
    , m_descSetLayout(VK_NULL_HANDLE)
    , m_descSet(VK_NULL_HANDLE)
    , m_quad(nullptr)
    , m_sampler(VK_NULL_HANDLE)
    , m_copyBuffer(VK_NULL_HANDLE)
    , m_copyMemory(VK_NULL_HANDLE)
    , m_outTextureMemory(VK_NULL_HANDLE)
    , m_outTexture(VK_NULL_HANDLE)
    , m_outTextureView(VK_NULL_HANDLE)
{
}

C3DTextureRenderer::~C3DTextureRenderer()
{
    VkDevice dev = vk::g_vulkanContext.m_device;
    vk::DestroySampler(dev, m_sampler, nullptr);
    vk::DestroyDescriptorSetLayout(dev, m_descSetLayout, nullptr);

    //vk::FreeMemory(dev, m_copyMemory, nullptr);

    //vk::DestroyBuffer(dev, m_copyBuffer, nullptr);
    vk::DestroyImage(dev, m_outTexture, nullptr);
    vk::DestroyImageView(dev, m_outTextureView, nullptr);
}

void C3DTextureRenderer::Init()
{
    CRenderer::Init();
    unsigned int width = m_framebuffer->GetWidth();
    unsigned int height = m_framebuffer->GetHeight();

    AllocateOuputTexture();

    m_pipeline.SetDepthTest(VK_FALSE);
    m_pipeline.SetDepthWrite(VK_FALSE);
    m_pipeline.SetVertexInputState(Mesh::GetVertexDesc());
    m_pipeline.SetViewport(width, height);
    m_pipeline.SetScissor(width, height);
    m_pipeline.SetVertexShaderFile("screenquad.vert");
    m_pipeline.SetFragmentShaderFile("render3Dtexture.frag");
    m_pipeline.SetGeometryShaderFile("volume.geom");
    m_pipeline.SetCullMode(VK_CULL_MODE_NONE);
    m_pipeline.AddBlendState(CGraphicPipeline::CreateDefaultBlendState());
    m_pipeline.CreatePipelineLayout(m_descSetLayout);
    m_pipeline.Init(this, m_renderPass, 0);

#ifdef TEST3DTEXT
    m_readPipeline.SetDepthTest(VK_FALSE);
    m_readPipeline.SetDepthWrite(VK_FALSE);
    m_readPipeline.SetVertexInputState(Mesh::GetVertexDesc());
    m_readPipeline.SetViewport(width, height);
    m_readPipeline.SetScissor(width, height);
    m_readPipeline.SetVertexShaderFile("screenquad.vert");
    m_readPipeline.SetFragmentShaderFile("sample3Dtexture.frag");
    m_readPipeline.SetCullMode(VK_CULL_MODE_NONE);
    m_readPipeline.AddBlendState(CGraphicPipeline::CreateDefaultBlendState());
    m_readPipeline.CreatePipelineLayout(m_descSetLayout);
    m_readPipeline.Init(this, m_renderPass, 1);
#endif
    m_quad = CreateFullscreenQuad();

    AllocDescriptorSets(m_descriptorPool, m_descSetLayout, &m_descSet);

    CreateLinearSampler(m_sampler);
    VkDescriptorImageInfo imgInfo = CreateDescriptorImageInfo(m_sampler, m_framebuffer->GetColorImageView(0), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        
    VkWriteDescriptorSet wDesc = InitUpdateDescriptor(m_descSet, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &imgInfo);

    vk::UpdateDescriptorSets(vk::g_vulkanContext.m_device, 1, &wDesc, 0, nullptr);
}

void C3DTextureRenderer::Render()
{
    StartRenderPass();
    VkCommandBuffer cmdBuffer = vk::g_vulkanContext.m_mainCommandBuffer;
    vk::CmdBindPipeline(cmdBuffer, m_pipeline.GetBindPoint(), m_pipeline.Get());

    m_quad->Render(-1, TEXTURE3DLAYERS);

#ifdef TEST3DTEXT
    vk::CmdNextSubpass(cmdBuffer, VK_SUBPASS_CONTENTS_INLINE);
    vk::CmdBindPipeline(cmdBuffer, m_readPipeline.GetBindPoint(), m_readPipeline.Get());
    vk::CmdBindDescriptorSets(cmdBuffer, m_readPipeline.GetBindPoint(), m_readPipeline.GetLayout(), 0, 1, &m_descSet, 0, nullptr);

    m_quad->Render();
#endif
    EndRenderPass();

    CopyTexture();
}

void C3DTextureRenderer::CreateDescriptorSetLayout()
{
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    bindings.push_back(CreateDescriptorBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT));

    VkDescriptorSetLayoutCreateInfo crtInfo;
    cleanStructure(crtInfo);
    crtInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    crtInfo.bindingCount = (uint32_t)bindings.size();
    crtInfo.pBindings = bindings.data();

    VULKAN_ASSERT(vk::CreateDescriptorSetLayout(vk::g_vulkanContext.m_device, &crtInfo, nullptr, &m_descSetLayout));
}

void C3DTextureRenderer::PopulatePoolInfo(std::vector<VkDescriptorPoolSize>& poolSize, unsigned int& maxSets)
{
    maxSets = 1;
    AddDescriptorType(poolSize, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1);
}

void C3DTextureRenderer::AllocateOuputTexture()
{
    unsigned int width = m_framebuffer->GetWidth();
    unsigned int height = m_framebuffer->GetHeight();
    unsigned int depth = m_framebuffer->GetLayers(); //??

    VkImageCreateInfo imgCrtInfo;
    cleanStructure(imgCrtInfo);
    imgCrtInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imgCrtInfo.pNext = nullptr;
    imgCrtInfo.flags = 0;
    imgCrtInfo.imageType = VK_IMAGE_TYPE_3D;
    imgCrtInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    imgCrtInfo.extent.width = width;
    imgCrtInfo.extent.height = height;
    imgCrtInfo.extent.depth = depth;
    imgCrtInfo.mipLevels = 1;
    imgCrtInfo.arrayLayers = 1;
    imgCrtInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imgCrtInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgCrtInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imgCrtInfo.queueFamilyIndexCount = 0;
    imgCrtInfo.pQueueFamilyIndices = NULL;
    imgCrtInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    AllocImageMemory(imgCrtInfo, m_outTexture, m_outTextureMemory);

    VkImageViewCreateInfo viewCrtInfo;
    cleanStructure(viewCrtInfo);
    viewCrtInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewCrtInfo.pNext = nullptr;
    viewCrtInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    viewCrtInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;
    viewCrtInfo.image = m_outTexture;
    viewCrtInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewCrtInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewCrtInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewCrtInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewCrtInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewCrtInfo.subresourceRange.baseMipLevel = 0;
    viewCrtInfo.subresourceRange.levelCount = 1;
    viewCrtInfo.subresourceRange.baseArrayLayer = 0;
    viewCrtInfo.subresourceRange.layerCount = 1;
    
    VULKAN_ASSERT(vk::CreateImageView(vk::g_vulkanContext.m_device, &viewCrtInfo, nullptr, &m_outTextureView));

    unsigned int size = width * depth * height * GetBytesFromFormat(VK_FORMAT_R16G16B16A16_SFLOAT);
    AllocBufferMemory(m_copyBuffer, m_copyMemory, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
}

void C3DTextureRenderer::CopyTexture()
{
    unsigned int layers = m_framebuffer->GetLayers();
    unsigned int width = m_framebuffer->GetWidth();
    unsigned int height = m_framebuffer->GetHeight();

    VkCommandBuffer cmdBuffer = vk::g_vulkanContext.m_mainCommandBuffer;

    VkImage outImg = m_framebuffer->GetColorImage(0);
    VkImageMemoryBarrier imageBarriers[2];
    
    VkImageLayout outLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkAccessFlags outAccess = 0;

    AddImageBarrier(imageBarriers[0], outImg, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,  VK_IMAGE_ASPECT_COLOR_BIT);
    
    vk::CmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 0, nullptr, 1, imageBarriers);

    VkImageSubresourceLayers subresource;
    subresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresource.baseArrayLayer = 0;
    subresource.layerCount = layers;
    subresource.mipLevel = 0;

    VkBufferImageCopy region;
    cleanStructure(region);
    region.imageExtent.width = width;
    region.imageExtent.height = height;
    region.imageExtent.depth = 1;
    region.imageOffset.x = 0;
    region.imageOffset.y = 0;
    region.imageOffset.z = 0;
    region.imageSubresource = subresource;

    vk::CmdCopyImageToBuffer(cmdBuffer, outImg, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_copyBuffer,1, &region);

    VkBufferMemoryBarrier buffBarrier;
    AddBufferBarier(buffBarrier, m_copyBuffer, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
    AddImageBarrier(imageBarriers[0], outImg, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,  VK_IMAGE_ASPECT_COLOR_BIT);
    AddImageBarrier(imageBarriers[1], m_outTexture, outLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, outAccess, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
    outAccess = VK_ACCESS_SHADER_READ_BIT;
    outLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    
    vk::CmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 0, nullptr, 2, imageBarriers);
    
    cleanStructure(region);
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageSubresource.mipLevel = 0;
    region.imageExtent.width = width;
    region.imageExtent.height = height;
    region.imageExtent.depth = layers;

    vk::CmdCopyBufferToImage(cmdBuffer, m_copyBuffer, m_outTexture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    AddImageBarrier(imageBarriers[0], m_outTexture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT);

    vk::CmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 0, nullptr, 1, imageBarriers);

}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
//CVolumetricRendering
///////////////////////////////////////////////////////////////////////////////////////////////////////////

struct SVolumeParams
{
    glm::mat4 ModelMatrix;
    glm::mat4 ViewMatrix;
    glm::mat4 ProjMatrix;
};

CVolumetricRenderer::CVolumetricRenderer(VkRenderPass renderPass)
    : CRenderer(renderPass)
    , m_volumeDescLayout(VK_NULL_HANDLE)
    , m_volumetricDescLayout(VK_NULL_HANDLE)
    , m_volumeDescSet(VK_NULL_HANDLE)
    , m_volumetricDescSet(VK_NULL_HANDLE)
    , m_sampler(VK_NULL_HANDLE)
    , m_uniformBuffer(VK_NULL_HANDLE)
    , m_uniformMemory(VK_NULL_HANDLE)
    , m_cube(nullptr)
{
}

CVolumetricRenderer::~CVolumetricRenderer()
{
    VkDevice dev = vk::g_vulkanContext.m_device;
    vk::DestroyDescriptorSetLayout(dev, m_volumetricDescLayout, nullptr);
    vk::DestroyDescriptorSetLayout(dev, m_volumeDescLayout, nullptr);
    vk::DestroySampler(dev, m_sampler, nullptr);

    vk::FreeMemory(dev, m_uniformMemory, nullptr);
    vk::DestroyBuffer(dev, m_uniformBuffer, nullptr);
}

void CVolumetricRenderer::Init()
{
    CRenderer::Init();
    
    AllocDescriptorSets(m_descriptorPool, m_volumeDescLayout, &m_volumeDescSet);
    AllocDescriptorSets(m_descriptorPool, m_volumetricDescLayout, &m_volumetricDescSet);
    
    m_cube = CreateUnitCube();
    AllocBufferMemory(m_uniformBuffer, m_uniformMemory, sizeof(SVolumeParams), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

    CreateCullPipeline(m_frontCullPipeline, VK_CULL_MODE_FRONT_BIT);
    CreateCullPipeline(m_backCullPipeline, VK_CULL_MODE_BACK_BIT);
    CreateLinearSampler(m_sampler);

    m_volumetricPipeline.SetVertexShaderFile("transform.vert");
    m_volumetricPipeline.SetFragmentShaderFile("volumetric.frag");
    m_volumetricPipeline.SetDepthTest(false);
    m_volumetricPipeline.SetCullMode(VK_CULL_MODE_BACK_BIT);
    m_volumetricPipeline.CreatePipelineLayout(m_volumetricDescLayout);
    m_volumetricPipeline.AddBlendState(CGraphicPipeline::CreateDefaultBlendState(), 2); //change blend state
    m_volumetricPipeline.SetVertexInputState(Mesh::GetVertexDesc());
    m_volumetricPipeline.Init(this, m_renderPass, 2);
}

void CVolumetricRenderer::Render()
{
    UpdateShaderParams();
    VkCommandBuffer cmdBuffer = vk::g_vulkanContext.m_mainCommandBuffer;
    StartRenderPass();
    vk::CmdBindPipeline(cmdBuffer, m_frontCullPipeline.GetBindPoint(), m_frontCullPipeline.Get());
    vk::CmdBindDescriptorSets(cmdBuffer, m_frontCullPipeline.GetBindPoint(), m_frontCullPipeline.GetLayout(), 0, 1, &m_volumeDescSet, 0, nullptr);

    m_cube->Render();

    vk::CmdNextSubpass(cmdBuffer, VK_SUBPASS_CONTENTS_INLINE);
    vk::CmdBindPipeline(cmdBuffer, m_backCullPipeline.GetBindPoint(), m_backCullPipeline.Get());
    vk::CmdBindDescriptorSets(cmdBuffer, m_backCullPipeline.GetBindPoint(), m_backCullPipeline.GetLayout(), 0, 1, &m_volumeDescSet, 0, nullptr);

    m_cube->Render();

    vk::CmdNextSubpass(cmdBuffer, VK_SUBPASS_CONTENTS_INLINE);
    vk::CmdBindPipeline(cmdBuffer, m_volumetricPipeline.GetBindPoint(), m_volumetricPipeline.Get());
    vk::CmdBindDescriptorSets(cmdBuffer, m_volumetricPipeline.GetBindPoint(), m_volumetricPipeline.GetLayout(), 0, 1, &m_volumetricDescSet, 0, nullptr);

    m_cube->Render();

    EndRenderPass();
}

void CVolumetricRenderer::UpdateGraphicInterface(VkImageView texture3DView, VkImageView depthView)
{
    VkDescriptorBufferInfo uniformInfo = CreateDescriptorBufferInfo(m_uniformBuffer);
    VkDescriptorImageInfo text3DInfo = CreateDescriptorImageInfo(m_sampler, texture3DView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    VkDescriptorImageInfo depthInfo = CreateDescriptorImageInfo(m_sampler, depthView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    VkDescriptorImageInfo frontInfo = CreateDescriptorImageInfo(m_sampler, m_framebuffer->GetColorImageView(0), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    VkDescriptorImageInfo backtInfo = CreateDescriptorImageInfo(m_sampler, m_framebuffer->GetColorImageView(1), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);


    std::vector<VkWriteDescriptorSet> wDesc;
    wDesc.push_back(InitUpdateDescriptor(m_volumeDescSet, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &uniformInfo));
    wDesc.push_back(InitUpdateDescriptor(m_volumetricDescSet, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &uniformInfo));
    wDesc.push_back(InitUpdateDescriptor(m_volumetricDescSet, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &text3DInfo));
    wDesc.push_back(InitUpdateDescriptor(m_volumetricDescSet, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &frontInfo));
    wDesc.push_back(InitUpdateDescriptor(m_volumetricDescSet, 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &backtInfo));

    vk::UpdateDescriptorSets(vk::g_vulkanContext.m_device, (uint32_t)wDesc.size(), wDesc.data(), 0, nullptr);
}

void CVolumetricRenderer::CreateDescriptorSetLayout()
{
    {
        std::vector<VkDescriptorSetLayoutBinding> bindings;
        bindings.push_back(CreateDescriptorBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT));

        NewDescriptorSetLayout(bindings, &m_volumeDescLayout);
    }

    {
        std::vector<VkDescriptorSetLayoutBinding> bindings;
        bindings.push_back(CreateDescriptorBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT));
        bindings.push_back(CreateDescriptorBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT));
        bindings.push_back(CreateDescriptorBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT));
        bindings.push_back(CreateDescriptorBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT));

        NewDescriptorSetLayout(bindings, &m_volumetricDescLayout);
    }
}

void CVolumetricRenderer::PopulatePoolInfo(std::vector<VkDescriptorPoolSize>& poolSize, unsigned int& maxSets)
{
    maxSets = 2;
    AddDescriptorType(poolSize, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2);
    AddDescriptorType(poolSize, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3);
}

void CVolumetricRenderer::CreateCullPipeline(CGraphicPipeline& pipeline, VkCullModeFlagBits cullmode)
{
    pipeline.SetVertexShaderFile("transform.vert");
    pipeline.SetFragmentShaderFile("volume.frag");
    pipeline.SetVertexInputState(Mesh::GetVertexDesc());
    pipeline.SetDepthTest(false);
    pipeline.AddBlendState(CGraphicPipeline::CreateDefaultBlendState());
    pipeline.SetCullMode(cullmode);
    pipeline.CreatePipelineLayout(m_volumeDescLayout);

    unsigned int subPass = -1;
    if ((cullmode & VK_CULL_MODE_BACK_BIT) == cullmode )
    {
        subPass = 1;
    }
    else if ((cullmode & VK_CULL_MODE_FRONT_BIT) == cullmode )
    {
        subPass = 0;
    }

    TRAP(subPass != -1);
    pipeline.Init(this, m_renderPass, subPass);
}

void CVolumetricRenderer::UpdateShaderParams()
{
    glm::mat4 proj;
    PerspectiveMatrix(proj);
    ConvertToProjMatrix(proj);

    SVolumeParams* params = nullptr;
    VULKAN_ASSERT(vk::MapMemory(vk::g_vulkanContext.m_device, m_uniformMemory, 0, VK_WHOLE_SIZE, 0, (void**)&params));
    params->ModelMatrix = glm::mat4(1.0f);
    params->ProjMatrix = proj;
    params->ViewMatrix = ms_camera.GetViewMatrix();
    vk::UnmapMemory(vk::g_vulkanContext.m_device, m_uniformMemory);
}