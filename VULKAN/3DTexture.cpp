#include "3DTexture.h"
#include "Mesh.h"
#include "Texture.h"
#include "Input.h"

#include <random>
C3DTextureRenderer::C3DTextureRenderer (VkRenderPass renderPass)
    : CRenderer(renderPass)
    , m_generateDescLayout(VK_NULL_HANDLE)
    , m_generateDescSet(VK_NULL_HANDLE)
    , m_outTexture(nullptr)
    , m_width(512)
    , m_height(512)
    , m_depth(TEXTURE3DLAYERS)
    , m_uniformBuffer(nullptr)
    , m_needGenerateTexture(true)
	, m_isEnabled(false)
{
	InputManager::GetInstance()->MapKeyPressed(VK_F5, InputManager::KeyPressedCallback(this, &C3DTextureRenderer::OnKeyPressed));
}

C3DTextureRenderer::~C3DTextureRenderer()
{
    VkDevice dev = vk::g_vulkanContext.m_device;

    vk::DestroyDescriptorSetLayout(dev, m_generateDescLayout, nullptr);

	MemoryManager::GetInstance()->FreeHandle(m_outTexture);
	MemoryManager::GetInstance()->FreeHandle(m_uniformBuffer);

    delete m_patternTexture;
}

void C3DTextureRenderer::Init()
{
    CRenderer::Init();
    AllocateOuputTexture();

    AllocDescriptorSets(m_descriptorPool, m_generateDescLayout, &m_generateDescSet);
	m_uniformBuffer = MemoryManager::GetInstance()->CreateBuffer(EMemoryContextType::UniformBuffers, sizeof(FogParameters), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

    FillParams();

    SImageData patData;
    Read2DTextureData(patData, std::string(TEXTDIR) + "wisp.png", true);

    m_patternTexture = new CTexture(patData, true);

    //m_generatePipeline.SetComputeShaderFile("generateVolumeTexture.comp");
    m_generatePipeline.SetComputeShaderFile("splitVolumeTexture.comp");

    m_generatePipeline.CreatePipelineLayout(m_generateDescLayout);
    m_generatePipeline.Init(this, VK_NULL_HANDLE, -1); //compute pipelines dont need render pass .. need to change CPipeline

    VkDescriptorImageInfo imgInfo = CreateDescriptorImageInfo(VK_NULL_HANDLE, m_outTexture->GetView(), VK_IMAGE_LAYOUT_GENERAL);
	VkDescriptorBufferInfo bufInfo = m_uniformBuffer->GetDescriptor();
    std::vector<VkWriteDescriptorSet> wDescs;
    wDescs.push_back(InitUpdateDescriptor(m_generateDescSet, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &imgInfo));
    wDescs.push_back(InitUpdateDescriptor(m_generateDescSet, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &m_patternTexture->GetTextureDescriptor()));
    wDescs.push_back(InitUpdateDescriptor(m_generateDescSet, 2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &bufInfo));
    vk::UpdateDescriptorSets(vk::g_vulkanContext.m_device, (uint32_t)wDescs.size(), wDescs.data(), 0, nullptr);
}

void C3DTextureRenderer::PreRender()
{
	UpdateParams();
}

void C3DTextureRenderer::Render()
{
    if (m_needGenerateTexture && m_isEnabled)
    {
        BeginMarkerSection("GenerateShitty3DTexture");
        PrepareTexture();
        VkCommandBuffer cmdBuffer = vk::g_vulkanContext.m_mainCommandBuffer;
        vk::CmdBindPipeline(cmdBuffer, m_generatePipeline.GetBindPoint(), m_generatePipeline.Get());
        vk::CmdBindDescriptorSets(cmdBuffer, m_generatePipeline.GetBindPoint(), m_generatePipeline.GetLayout(),0, 1, &m_generateDescSet, 0, nullptr);
        TRAP(m_width % 32 == 0 && m_height % 32 == 0);
        //vk::CmdDispatch(cmdBuffer, m_width / 32, 1, m_depth); //??
        vk::CmdDispatch(cmdBuffer, 2048 / 32, 2048 / 32, 1);

        WaitComputeFinish();
        EndMarkerSection();
        m_needGenerateTexture = false;
    }
}

void C3DTextureRenderer::CreateDescriptorSetLayout()
{
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    bindings.push_back(CreateDescriptorBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT));
    bindings.push_back(CreateDescriptorBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT));
    bindings.push_back(CreateDescriptorBinding(2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT));

    VkDescriptorSetLayoutCreateInfo crtInfo;
    cleanStructure(crtInfo);
    crtInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    crtInfo.bindingCount = (uint32_t)bindings.size();
    crtInfo.pBindings = bindings.data();

    VULKAN_ASSERT(vk::CreateDescriptorSetLayout(vk::g_vulkanContext.m_device, &crtInfo, nullptr, &m_generateDescLayout));
}

void C3DTextureRenderer::PopulatePoolInfo(std::vector<VkDescriptorPoolSize>& poolSize, unsigned int& maxSets)
{
    maxSets = 1;
    AddDescriptorType(poolSize, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1);
    AddDescriptorType(poolSize, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1);
    AddDescriptorType(poolSize, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1);
}

void C3DTextureRenderer::AllocateOuputTexture()
{
    unsigned int width = 2048 / 12; //m_width;
    unsigned int height = 2048 / 12; //m_height;
    unsigned int depth = m_depth;

    VkImageCreateInfo imgCrtInfo;
    cleanStructure(imgCrtInfo);
    imgCrtInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imgCrtInfo.pNext = nullptr;
    imgCrtInfo.flags = 0;
    imgCrtInfo.imageType = VK_IMAGE_TYPE_3D;
	imgCrtInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imgCrtInfo.extent.width = width;
    imgCrtInfo.extent.height = height;
    imgCrtInfo.extent.depth = 144;
    imgCrtInfo.mipLevels = 1;
    imgCrtInfo.arrayLayers = 1;
    imgCrtInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imgCrtInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgCrtInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    imgCrtInfo.queueFamilyIndexCount = 0;
    imgCrtInfo.pQueueFamilyIndices = NULL;
    imgCrtInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	m_outTexture = MemoryManager::GetInstance()->CreateImage(EMemoryContextType::Textures, imgCrtInfo, "3DTexture2");
}

void C3DTextureRenderer::PrepareTexture()
{
    static VkImageLayout currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    static VkAccessFlags currentAccess = 0;
    VkCommandBuffer cmdBuffer = vk::g_vulkanContext.m_mainCommandBuffer;

    VkImageMemoryBarrier layoutChangeBarrier = m_outTexture->CreateMemoryBarrier(currentLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, currentAccess, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT);

    vk::CmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 0, nullptr, 1, &layoutChangeBarrier);

    VkClearColorValue clrValue;
    cleanStructure(clrValue);

    VkImageSubresourceRange range;
    cleanStructure(range);
    range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    range.baseMipLevel = 0;
    range.baseArrayLayer = 0;
    range.levelCount = 1;
    range.layerCount = VK_REMAINING_ARRAY_LAYERS;

    vk::CmdClearColorImage(cmdBuffer, m_outTexture->Get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clrValue, 1, &range);

    VkImageMemoryBarrier clearImageBarrier = m_outTexture->CreateMemoryBarrier(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
    vk::CmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 0, nullptr, 1, &clearImageBarrier);

    currentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    currentAccess = VK_ACCESS_SHADER_READ_BIT;
}

void C3DTextureRenderer::WaitComputeFinish()
{
    VkImageMemoryBarrier waitBarrier = m_outTexture->CreateMemoryBarrier(VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT);

    vk::CmdPipelineBarrier(vk::g_vulkanContext.m_mainCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 0, nullptr, 1, &waitBarrier);
}

void C3DTextureRenderer::UpdateResourceTable()
{
    g_commonResources.SetAs<ImageHandle*>(&m_outTexture, EResourceType_VolumetricImage);
}

void C3DTextureRenderer::FillParams()
{
    unsigned int seed = 162039;
    std::mt19937 generator (seed);
    std::uniform_real_distribution<float> distribution(-1.0f, 1.0f);

    glm::vec2 dir  = glm::normalize(glm::vec2(0.3f, 0.76f));
    auto directionNoise = [&]()
    {
        float limit = 0.15f;
        float noise = limit * distribution(generator);
        float alpha = glm::atan(dir.x, dir.y);
        alpha += noise;
        return glm::normalize(glm::vec2(glm::cos(alpha), glm::sin(alpha)));
    };

    unsigned int waves = 4;
    m_parameters.Globals = glm::vec4(dir, 0.0f, 1.0f);
    m_parameters.NumberOfWaves = glm::vec4((float)waves);
    //                                A,     Wavelength,  Speed * Dir
    m_parameters.Waves[0] = glm::vec4(0.4f, 0.5f, 0.055f * directionNoise());
    m_parameters.Waves[1] = glm::vec4(0.75f, 0.8f, 0.04f * directionNoise());
    m_parameters.Waves[2] = glm::vec4(0.5f, 0.4f, 0.05f * directionNoise());
    m_parameters.Waves[3] = glm::vec4(0.3f, 0.85f, 0.1f * directionNoise());
}

void C3DTextureRenderer::UpdateParams()
{
    static DWORD startTime = GetTickCount();
    DWORD now = GetTickCount();
    float timeSec = float(now - startTime) / 1000;
    m_parameters.Globals.z = timeSec;

	FogParameters* params = m_uniformBuffer->GetPtr<FogParameters*>();
    memcpy(params, &m_parameters, sizeof(FogParameters));
}

void C3DTextureRenderer::CopyTexture()
{
}

bool C3DTextureRenderer::OnKeyPressed(const KeyInput& input)
{
	if (input.IsKeyPressed(VK_F5))
	{
		m_isEnabled = !m_isEnabled;
		return true;
	}
	return false;
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
    : CRenderer(renderPass, "VolumeticRenderPass")
    , m_volumeDescLayout(VK_NULL_HANDLE)
    , m_volumetricDescLayout(VK_NULL_HANDLE)
    , m_volumeDescSet(VK_NULL_HANDLE)
    , m_volumetricDescSet(VK_NULL_HANDLE)
    , m_sampler(VK_NULL_HANDLE)
    , m_uniformBuffer(nullptr)
    , m_cube(nullptr)
	, m_isEnabled(false)
{
	InputManager::GetInstance()->MapKeyPressed(VK_F5, InputManager::KeyPressedCallback(this, &CVolumetricRenderer::OnKeyPressed));
}

CVolumetricRenderer::~CVolumetricRenderer()
{
    VkDevice dev = vk::g_vulkanContext.m_device;
    vk::DestroyDescriptorSetLayout(dev, m_volumetricDescLayout, nullptr);
    vk::DestroyDescriptorSetLayout(dev, m_volumeDescLayout, nullptr);
    vk::DestroySampler(dev, m_sampler, nullptr);

	MemoryManager::GetInstance()->FreeHandle(m_uniformBuffer);
}

void CVolumetricRenderer::Init()
{
    CRenderer::Init();
    
    AllocDescriptorSets(m_descriptorPool, m_volumeDescLayout, &m_volumeDescSet);
    AllocDescriptorSets(m_descriptorPool, m_volumetricDescLayout, &m_volumetricDescSet);
    
    m_cube = CreateUnitCube();
	m_uniformBuffer = MemoryManager::GetInstance()->CreateBuffer(EMemoryContextType::UniformBuffers, sizeof(SVolumeParams), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

    CreateCullPipeline(m_frontCullPipeline, VK_CULL_MODE_FRONT_BIT);
    CreateCullPipeline(m_backCullPipeline, VK_CULL_MODE_BACK_BIT);
    CreateLinearSampler(m_sampler, false);

    VkPipelineColorBlendAttachmentState blend;
    blend.blendEnable = VK_TRUE;
    blend.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blend.colorBlendOp = VK_BLEND_OP_ADD;
    blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blend.alphaBlendOp = VK_BLEND_OP_ADD;
    blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT;

    m_volumetricPipeline.SetVertexShaderFile("transform.vert");
    m_volumetricPipeline.SetFragmentShaderFile("volumetric.frag");
    m_volumetricPipeline.SetDepthTest(true);
    m_volumetricPipeline.SetDepthWrite(false);
    m_volumetricPipeline.SetCullMode(VK_CULL_MODE_BACK_BIT);
    m_volumetricPipeline.CreatePipelineLayout(m_volumetricDescLayout);
    m_volumetricPipeline.AddBlendState(blend);
    m_volumetricPipeline.AddBlendState(CGraphicPipeline::CreateDefaultBlendState()); //change blend state
    m_volumetricPipeline.SetVertexInputState(Mesh::GetVertexDesc());
    m_volumetricPipeline.Init(this, m_renderPass, 2);
}

void CVolumetricRenderer::PreRender()
{
	if (!m_isEnabled)
		return;

	UpdateShaderParams();
}

void CVolumetricRenderer::Render()
{
	if (!m_isEnabled)
		return;

    VkCommandBuffer cmdBuffer = vk::g_vulkanContext.m_mainCommandBuffer;
    StartRenderPass();
    
    BeginMarkerSection("FrontCullVolume");
    vk::CmdBindPipeline(cmdBuffer, m_frontCullPipeline.GetBindPoint(), m_frontCullPipeline.Get());
    vk::CmdBindDescriptorSets(cmdBuffer, m_frontCullPipeline.GetBindPoint(), m_frontCullPipeline.GetLayout(), 0, 1, &m_volumeDescSet, 0, nullptr);

    m_cube->Render();

    EndMarkerSection();

    BeginMarkerSection("BackCullVolume");
    vk::CmdNextSubpass(cmdBuffer, VK_SUBPASS_CONTENTS_INLINE);
    vk::CmdBindPipeline(cmdBuffer, m_backCullPipeline.GetBindPoint(), m_backCullPipeline.Get());
    vk::CmdBindDescriptorSets(cmdBuffer, m_backCullPipeline.GetBindPoint(), m_backCullPipeline.GetLayout(), 0, 1, &m_volumeDescSet, 0, nullptr);
    
    m_cube->Render();
    EndMarkerSection();

    BeginMarkerSection("VolumetricRender");
    vk::CmdNextSubpass(cmdBuffer, VK_SUBPASS_CONTENTS_INLINE);
    vk::CmdBindPipeline(cmdBuffer, m_volumetricPipeline.GetBindPoint(), m_volumetricPipeline.Get());
    vk::CmdBindDescriptorSets(cmdBuffer, m_volumetricPipeline.GetBindPoint(), m_volumetricPipeline.GetLayout(), 0, 1, &m_volumetricDescSet, 0, nullptr);

    m_cube->Render();
    EndMarkerSection();

    EndRenderPass();
}

void CVolumetricRenderer::UpdateGraphicInterface()
{
	ImageHandle* texture3D = g_commonResources.GetAs<ImageHandle*>(EResourceType_VolumetricImage);
	ImageHandle* deptht = g_commonResources.GetAs<ImageHandle*>(EResourceType_DepthBufferImage);

    VkDescriptorBufferInfo uniformInfo = m_uniformBuffer->GetDescriptor();
	VkDescriptorImageInfo text3DInfo = CreateDescriptorImageInfo(m_sampler, texture3D->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	VkDescriptorImageInfo depthInfo = CreateDescriptorImageInfo(m_sampler, deptht->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    VkDescriptorImageInfo frontInfo = CreateDescriptorImageInfo(m_sampler, m_framebuffer->GetColorImageView(0), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkDescriptorImageInfo backtInfo = CreateDescriptorImageInfo(m_sampler, m_framebuffer->GetColorImageView(1), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

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
    float s = 2.0f;

    glm::mat4 modelMatrix (1.0f);
    //modelMatrix = glm::translate(modelMatrix, glm::vec3(0.0f, -0.75f, -3.0f));
    //modelMatrix = glm::scale(modelMatrix, glm::vec3(s, 1.0f, s));
    modelMatrix = glm::translate(modelMatrix, glm::vec3(0.0f, 0.0f, -3.0f));
    modelMatrix = glm::scale(modelMatrix, glm::vec3(s, s, s));

	SVolumeParams* params = m_uniformBuffer->GetPtr<SVolumeParams*>();
    params->ModelMatrix = modelMatrix;
    params->ProjMatrix = proj;
    params->ViewMatrix = ms_camera.GetViewMatrix();
}

bool CVolumetricRenderer::OnKeyPressed(const KeyInput& input)
{
	if (input.IsKeyPressed(VK_F5))
	{
		m_isEnabled = !m_isEnabled;
		return true;
	}

	return false;
}
