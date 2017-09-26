#include "SkyRenderer.h"
#include "Texture.h"
#include "UI.h"

CSunRenderer::CSunRenderer(VkRenderPass renderPass)
    : CRenderer(renderPass)
    , m_blurSetLayout(VK_NULL_HANDLE)
    , m_blurRadialDescSet(VK_NULL_HANDLE)
    , m_radialBlurSetLayout(VK_NULL_HANDLE)
    , m_sunDescriptorSetLayout(VK_NULL_HANDLE)
    , m_sunDescriptorSet(VK_NULL_HANDLE)
    , m_sunParamsMemory(VK_NULL_HANDLE)
    , m_sunParamsBuffer(VK_NULL_HANDLE)
    , m_radialBlurParamsBuffer(VK_NULL_HANDLE)
    , m_blurHDescSet(VK_NULL_HANDLE)
    , m_blurVDescSet(VK_NULL_HANDLE)
    , m_quad(nullptr)
    , m_sampler(VK_NULL_HANDLE)
    , m_neareastSampler(VK_NULL_HANDLE)
    , m_sunTexture(nullptr)
    , m_isEditMode(false)
    , m_editInfo(nullptr)
{
    m_sunScale = 2.0f;
    m_lightShaftDensity = 1.5f;
    m_lightShaftDecay = 0.85f;
    m_lightShaftWeight = 1.55f;
    m_lightShaftExposure = 0.04f;
    m_lightShaftSamples = 16.0f;
}

CSunRenderer::~CSunRenderer()
{
    VkDevice dev = vk::g_vulkanContext.m_device;

    vk::DestroyDescriptorSetLayout(dev, m_blurSetLayout, nullptr);
    vk::DestroyDescriptorSetLayout(dev, m_sunDescriptorSetLayout, nullptr);
    vk::DestroySampler(dev, m_sampler, nullptr);

    vk::FreeMemory(dev, m_sunParamsMemory, nullptr);
    vk::DestroyBuffer(dev, m_sunParamsBuffer, nullptr);

    vk::FreeMemory(dev, m_radialBlurParamsMemory, nullptr);
    vk::DestroyBuffer(dev, m_radialBlurParamsBuffer, nullptr);

    //delete m_quad;
}

void CSunRenderer::Init()
{
    CRenderer::Init();

    TRAP(m_framebuffer);

    AllocDescriptorSets(m_descriptorPool, m_sunDescriptorSetLayout, &m_sunDescriptorSet);
    AllocDescriptorSets(m_descriptorPool, m_blurSetLayout, &m_blurVDescSet);
    AllocDescriptorSets(m_descriptorPool, m_blurSetLayout, &m_blurHDescSet);
    AllocDescriptorSets(m_descriptorPool, m_radialBlurSetLayout, &m_blurRadialDescSet);

    AllocBufferMemory(m_sunParamsBuffer, m_sunParamsMemory, sizeof(SSunParams), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    AllocBufferMemory(m_radialBlurParamsBuffer, m_radialBlurParamsMemory, sizeof(SRadialBlurParams), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

    unsigned int width = m_framebuffer->GetWidth();
    unsigned int height = m_framebuffer->GetHeight();

    //common.AddBlendState(CPipeline::CreateDefaultBlendState());
    VkPipelineColorBlendAttachmentState defaultState = CPipeline::CreateDefaultBlendState();

    auto initPipeline = [&](CPipeline& pipeline, unsigned int subpass, char* vertex, char* fragment, VkDescriptorSetLayout& layout){
        pipeline.SetCullMode(VK_CULL_MODE_NONE);
        pipeline.SetDepthTest(VK_FALSE);
        pipeline.SetVertexInputState(Mesh::GetVertexDesc());
        pipeline.SetViewport(width, height);
        pipeline.SetScissor(width, height);
        pipeline.AddBlendState(defaultState);
        pipeline.SetVertexShaderFile(vertex);
        pipeline.SetFragmentShaderFile(fragment);
        pipeline.CreatePipelineLayout(layout);
        pipeline.Init(this, m_renderPass, subpass);
    };

   
    initPipeline(m_blurVPipeline, ESunPass_BlurV, "screenquad.vert", "vblur.frag", m_blurSetLayout);
    initPipeline(m_blurHPipeline, ESunPass_BlurH, "screenquad.vert", "hblur.frag", m_blurSetLayout);
    initPipeline(m_blurRadialPipeline, ESunPass_BlurRadial, "screenquad.vert", "radialblur.frag", m_radialBlurSetLayout);

    //warning
    m_sunPipeline.SetDepthTest(VK_TRUE);
    m_sunPipeline.SetDepthOp(VK_COMPARE_OP_LESS_OR_EQUAL);
    m_sunPipeline.SetDepthWrite(false);
    initPipeline(m_sunPipeline, ESunPass_Sun, "sun.vert", "sun.frag", m_sunDescriptorSetLayout);

    CreateLinearSampler(m_sampler);
    CreateNearestSampler(m_neareastSampler);

    m_quad = CreateFullscreenQuad();
}

void CSunRenderer::Render()
{
    UpdateShaderParams();

    StartRenderPass();
    VkCommandBuffer cmdBuf = vk::g_vulkanContext.m_mainCommandBuffer;
    auto renderPipeline = [&](CPipeline& pipline, VkDescriptorSet& set) {
        if(!m_renderSun)
            return;
        vk::CmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipline.Get());
        vk::CmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipline.GetLayout(), 0, 1, &set, 0, nullptr);

        m_quad->Render();
    };

    renderPipeline(m_sunPipeline, m_sunDescriptorSet);

    vk::CmdNextSubpass(cmdBuf, VK_SUBPASS_CONTENTS_INLINE);
    renderPipeline(m_blurVPipeline, m_blurVDescSet);

    vk::CmdNextSubpass(cmdBuf, VK_SUBPASS_CONTENTS_INLINE);
    renderPipeline(m_blurHPipeline, m_blurHDescSet);

    vk::CmdNextSubpass(cmdBuf, VK_SUBPASS_CONTENTS_INLINE);
    renderPipeline(m_blurRadialPipeline, m_blurRadialDescSet);

    EndRenderPass();
}

void CSunRenderer::UpdateGraphicInterface(VkImageView depthBuffer)
{
    VkDescriptorImageInfo wSuntImg;
    wSuntImg.imageView = m_framebuffer->GetColorImageView(ESunFB_Sun);
    wSuntImg.sampler = m_sampler;
    wSuntImg.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo wBlurVImg;
    wBlurVImg.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    wBlurVImg.imageView = m_framebuffer->GetColorImageView(ESunFB_Blur1);
    wBlurVImg.sampler = m_sampler;

    VkDescriptorImageInfo wBlurHImg;
    wBlurHImg.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    wBlurHImg.imageView = m_framebuffer->GetColorImageView(ESunFB_Blur2);
    wBlurHImg.sampler = m_sampler;

    VkDescriptorImageInfo depthImg;
    depthImg.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    depthImg.imageView = depthBuffer;
    depthImg.sampler = m_neareastSampler;

    VkDescriptorBufferInfo rbBuff;
    rbBuff.buffer = m_radialBlurParamsBuffer;
    rbBuff.offset = 0;
    rbBuff.range = VK_WHOLE_SIZE;

    std::vector<VkWriteDescriptorSet> wDesc;
    
    wDesc.push_back(InitUpdateDescriptor(m_blurVDescSet, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &wSuntImg));
    wDesc.push_back(InitUpdateDescriptor(m_blurHDescSet, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &wBlurVImg));
    wDesc.push_back(InitUpdateDescriptor(m_blurRadialDescSet, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &wBlurHImg));
    wDesc.push_back(InitUpdateDescriptor(m_blurRadialDescSet, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &rbBuff));
    wDesc.push_back(InitUpdateDescriptor(m_sunDescriptorSet, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &depthImg));

    vk::UpdateDescriptorSets(vk::g_vulkanContext.m_device, (uint32_t)wDesc.size(), wDesc.data(), 0, nullptr);
    //UpdateSunDescriptors();
};

bool CSunRenderer::RegisterPick(unsigned int key)
{
    TRAP(m_editInfo);
    if(key == '1')
    {
        m_isEditMode ^= true;
        m_editInfo->SetVisible(m_isEditMode);
        UpdateEditInfo();
    }

    if(!m_isEditMode)
        return false;

    switch(key)
    {
    case 'E':
        m_lightShaftWeight -= 0.05f;
        break;
    case 'R':
        m_lightShaftWeight += 0.05f;
        break;
    case 'T':
        m_lightShaftDecay -= 0.05f;
        break;
    case 'Y':
        m_lightShaftDecay += 0.05f;
        break;
    case 'U':
        m_lightShaftDensity -= 0.1f;
        break;
    case 'I':
        m_lightShaftDensity += 0.1f;
        break;
    case 'O':
        m_lightShaftExposure -= 0.01f;
        break;
    case 'P':
        m_lightShaftExposure += 0.01f;
        break;
    case 'F':
        m_lightShaftSamples -= 2.0f;
        break;
    case 'G':
        m_lightShaftSamples += 2.0f;
        break;
    default:
        return false;
    };

    m_lightShaftDecay = glm::max(m_lightShaftDecay, 0.0f);
    m_lightShaftDensity = glm::max(m_lightShaftDensity, 0.0f);
    m_lightShaftExposure = glm::max(m_lightShaftExposure, 0.0f);
    m_lightShaftWeight = glm::max(m_lightShaftWeight, 0.0f);
    m_lightShaftSamples = glm::clamp(m_lightShaftSamples, 1.0f, 64.0f);

    UpdateEditInfo();

    return true;
}

void CSunRenderer::UpdateEditInfo()
{
    m_editInfo->SetTextItem(0, "(E/R)Weight: " + std::to_string(m_lightShaftWeight));
    m_editInfo->SetTextItem(1, "(T/Y)Decay: " + std::to_string(m_lightShaftDecay));
    m_editInfo->SetTextItem(2, "(U/I)Density: " + std::to_string(m_lightShaftDensity));
    m_editInfo->SetTextItem(3, "(O/P)Exposure: " + std::to_string(m_lightShaftExposure));
    m_editInfo->SetTextItem(4, "(F/G)Samples: " + std::to_string(m_lightShaftSamples));
}

void CSunRenderer::CreateEditInfo(CUIManager* manager)
{
    std::vector<std::string> texts;
    texts.resize(5);
    m_editInfo = manager->CreateTextContainerItem(texts, glm::uvec2(20, 300), 5, 48); 
}

void CSunRenderer::CreateDescriptorSetLayout()
{
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
    
    {
        std::vector<VkDescriptorSetLayoutBinding> bindings;
        bindings.push_back(CreateDescriptorBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT));
        bindings.push_back(CreateDescriptorBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT));
        bindings.push_back(CreateDescriptorBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT));

        VkDescriptorSetLayoutCreateInfo crtInfo;
        cleanStructure(crtInfo);
        crtInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        crtInfo.bindingCount = (uint32_t)bindings.size();
        crtInfo.pBindings = bindings.data();

        VULKAN_ASSERT(vk::CreateDescriptorSetLayout(vk::g_vulkanContext.m_device, &crtInfo, nullptr, &m_sunDescriptorSetLayout));
    }

    {
        std::vector<VkDescriptorSetLayoutBinding> bindings;
        bindings.push_back(CreateDescriptorBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT));
        bindings.push_back(CreateDescriptorBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT));

        VkDescriptorSetLayoutCreateInfo crtInfo;
        cleanStructure(crtInfo);
        crtInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        crtInfo.bindingCount = (uint32_t)bindings.size();
        crtInfo.pBindings = bindings.data();

        VULKAN_ASSERT(vk::CreateDescriptorSetLayout(vk::g_vulkanContext.m_device, &crtInfo, nullptr, &m_radialBlurSetLayout));
    }
}

void CSunRenderer::UpdateSunDescriptors()
{
    VkDescriptorBufferInfo wBuffer;
    cleanStructure(wBuffer);
    wBuffer.buffer = m_sunParamsBuffer;
    wBuffer.offset = 0;
    wBuffer.range = VK_WHOLE_SIZE;

    VkDescriptorImageInfo wImg = m_sunTexture->GetTextureDescriptor();

    std::vector<VkWriteDescriptorSet> writeDesc;
    writeDesc.push_back(InitUpdateDescriptor(m_sunDescriptorSet, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &wBuffer));
    writeDesc.push_back(InitUpdateDescriptor(m_sunDescriptorSet, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &wImg));

    vk::UpdateDescriptorSets(vk::g_vulkanContext.m_device, (uint32_t)writeDesc.size(), writeDesc.data(), 0, nullptr);
}

void CSunRenderer::UpdateShaderParams()
{
    float fbWidth = float(m_framebuffer->GetWidth());
    float fbHeight = float(m_framebuffer->GetHeight());

    glm::mat4 proj;
    PerspectiveMatrix(proj);
    //proj = glm::ortho(-5.0f, 5.0f, -5.0f, 5.0f, 0.1f, 50.0f);
    ConvertToProjMatrix(proj);

    SSunParams* sunParams = nullptr;

    VULKAN_ASSERT(vk::MapMemory(vk::g_vulkanContext.m_device, m_sunParamsMemory, 0, VK_WHOLE_SIZE, 0, (void**)&sunParams));
    sunParams->Scale = glm::vec4(m_sunScale, 0.0f, fbWidth, fbHeight);
    sunParams->LightDir = -directionalLight.GetDirection();
    sunParams->LightColor = directionalLight.GetLightIradiance();
    sunParams->LightColor.w = directionalLight.GetLightIntensity();
    sunParams->ViewMatrix =  ms_camera.GetViewMatrix();
    sunParams->ProjMatrix = proj;
    sunParams->CameraRight = glm::vec4(ms_camera.GetRightVector(), 0.0f);
    sunParams->CameraUp = glm::vec4(ms_camera.GetUpVector(), 0.0f);
    vk::UnmapMemory(vk::g_vulkanContext.m_device, m_sunParamsMemory);

    static const float farDist = 20.0f;
    glm::vec4 sunPos = proj * ms_camera.GetViewMatrix() * glm::vec4(glm::vec3(-directionalLight.GetDirection()) * farDist, 1.0f);
    sunPos = sunPos / sunPos.w;

    SRadialBlurParams* rbParams = nullptr;
    VULKAN_ASSERT(vk::MapMemory(vk::g_vulkanContext.m_device, m_radialBlurParamsMemory, 0, VK_WHOLE_SIZE, 0, (void**)&rbParams));
    rbParams->ProjSunPos = sunPos;
    rbParams->LightDensity = glm::vec4(m_lightShaftDensity);
    rbParams->LightDecay = glm::vec4(m_lightShaftDecay);
    rbParams->LightExposure = glm::vec4(m_lightShaftExposure);
    rbParams->SampleWeight = glm::vec4(m_lightShaftWeight);
    rbParams->ShaftSamples = glm::vec4(m_lightShaftSamples);
    vk::UnmapMemory(vk::g_vulkanContext.m_device, m_radialBlurParamsMemory);

    m_renderSun = glm::all(glm::greaterThanEqual(glm::vec3(sunPos), glm::vec3(-1.5f))) && glm::all(glm::lessThanEqual(glm::vec3(sunPos), glm::vec3(1.5f)));
}

void CSunRenderer::PopulatePoolInfo(std::vector<VkDescriptorPoolSize>& poolSize, unsigned int& maxSets)
{
    maxSets = 4;
    AddDescriptorType(poolSize, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 5);
    AddDescriptorType(poolSize, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2);
}