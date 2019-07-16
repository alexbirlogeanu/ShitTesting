#include "SunRenderer.h"
#include "Texture.h"
#include "UI.h"
#include "Input.h"
#include "GraphicEngine.h"
#include "Framebuffer.h"

SunRenderer::SunRenderer()
    : Renderer()
    , m_blurRadialDescSet(VK_NULL_HANDLE)
    , m_sunDescriptorSet(VK_NULL_HANDLE)
    , m_sunParamsBuffer(VK_NULL_HANDLE)
    , m_radialBlurParamsBuffer(VK_NULL_HANDLE)
    , m_blurHDescSet(VK_NULL_HANDLE)
    , m_blurVDescSet(VK_NULL_HANDLE)
    , m_quad(nullptr)
    , m_sampler(VK_NULL_HANDLE)
    , m_neareastSampler(VK_NULL_HANDLE)
    , m_sunTexture(nullptr)
    , m_isEditMode(false)

    //, m_editInfo(nullptr)
{
    m_sunScale = 2.0f;
    m_lightShaftDensity = 1.5f;
    m_lightShaftDecay = 0.85f;
    m_lightShaftWeight = 1.55f;
    m_lightShaftExposure = 0.04f;
    m_lightShaftSamples = 16.0f;

	std::vector<WPARAM> keys{ '1', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', 'F', 'G'};
	InputManager::GetInstance()->MapKeysPressed(keys, InputManager::KeyPressedCallback(this, &SunRenderer::OnKeyPressed));
}

SunRenderer::~SunRenderer()
{
    VkDevice dev = vk::g_vulkanContext.m_device;

    vk::DestroySampler(dev, m_sampler, nullptr);
	vk::DestroySampler(dev, m_neareastSampler, nullptr);

	MemoryManager::GetInstance()->FreeHandle( m_sunParamsBuffer->GetRootParent()); //i dont like this style. 

    //delete m_quad;
}

void SunRenderer::SetupPipeline(CGraphicPipeline& pipeline, VkRenderPass renderPass, unsigned int subpass, char* vertex, char* fragment, DescriptorSetLayout& layout)
{
	pipeline.SetCullMode(VK_CULL_MODE_NONE);
	//pipeline.SetDepthTest(VK_FALSE);
	pipeline.SetVertexInputState(Mesh::GetVertexDesc());
	pipeline.SetViewport(m_frameWidth, m_frameHeight);
	pipeline.SetScissor(m_frameWidth, m_frameHeight);
	pipeline.AddBlendState(CGraphicPipeline::CreateDefaultBlendState());
	pipeline.SetVertexShaderFile(vertex);
	pipeline.SetFragmentShaderFile(fragment);
	pipeline.CreatePipelineLayout(layout.Get());
	pipeline.Setup(renderPass, subpass);
}

void SunRenderer::Render(CGraphicPipeline& pipline, VkDescriptorSet& set)
{
	if (!m_renderSun)
		return;

	VkCommandBuffer cmdBuf = vk::g_vulkanContext.m_mainCommandBuffer;
	vk::CmdBindPipeline(cmdBuf, pipline.GetBindPoint(), pipline.Get());
	vk::CmdBindDescriptorSets(cmdBuf, pipline.GetBindPoint(), pipline.GetLayout(), 0, 1, &set, 0, nullptr);

	m_quad->Render();
}

void SunRenderer::SetupSunSubpass(VkRenderPass renderPass, uint32_t subpassId)
{
	SetupPipeline(m_sunPipeline, renderPass, subpassId, "sun.vert", "sun.frag", m_sunDescriptorSetLayout);
}

void SunRenderer::SetupBlurVSubpass(VkRenderPass renderPass, uint32_t subpassId)
{
	SetupPipeline(m_blurVPipeline, renderPass, subpassId, "screenquad.vert", "vblur.frag", m_blurSetLayout);
}

void SunRenderer::SetupBlurHSubpass(VkRenderPass renderPass, uint32_t subpassId)
{
	SetupPipeline(m_blurHPipeline, renderPass, subpassId, "screenquad.vert", "vblur.frag", m_blurSetLayout);
}

void SunRenderer::SetupBlurRadialSubpass(VkRenderPass renderPass, uint32_t subpassId)
{
	SetupPipeline(m_blurRadialPipeline, renderPass, subpassId, "screenquad.vert", "radialblur.frag", m_radialBlurSetLayout);
}

void SunRenderer::RenderSunSubpass()
{
	BeginMarkerSection("RenderSunSprite");
	Render(m_sunPipeline, m_sunDescriptorSet);
	EndMarkerSection();
}

void SunRenderer::RenderBlurVSubpass()
{
	BeginMarkerSection("BlurVertical");
	Render(m_blurVPipeline, m_blurVDescSet);
	EndMarkerSection();
}

void SunRenderer::RenderBlurHSubpass()
{
	BeginMarkerSection("BlurHorizontal");
	Render(m_blurHPipeline, m_blurHDescSet);
	EndMarkerSection();
}

void SunRenderer::RenderRadialBlurSubpass()
{
	BeginMarkerSection("RadialBlur");
	Render(m_blurRadialPipeline, m_blurRadialDescSet);
	EndMarkerSection();
}

void SunRenderer::InitInternal()
{
	VkDeviceSize totalSize = sizeof(SSunParams) + sizeof(SRadialBlurParams);
	BufferHandle* bigBuffer = MemoryManager::GetInstance()->CreateBuffer(EMemoryContextType::UniformBuffers, { sizeof(SSunParams), sizeof(SRadialBlurParams) }, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
	m_sunParamsBuffer = bigBuffer->CreateSubbuffer(sizeof(SSunParams));
	m_radialBlurParamsBuffer = bigBuffer->CreateSubbuffer(sizeof(SRadialBlurParams));

    CreateLinearSampler(m_sampler);
    CreateNearestSampler(m_neareastSampler);

    m_quad = CreateFullscreenQuad();

	m_sunTexture = new CTexture("sun2.png", false);
	ResourceLoader::GetInstance()->LoadTexture(&m_sunTexture);

	VkExtent3D fbDimensions = GraphicEngine::GetAttachment("SunFinal")->GetDimensions();
	m_frameWidth = fbDimensions.width;
	m_frameHeight = fbDimensions.height;
}

void SunRenderer::PreRender()
{
	UpdateShaderParams();
}

void SunRenderer::UpdateGraphicInterface()
{
    ImageHandle* depthBuffer = GraphicEngine::GetAttachment("Depth")->GetHandle();
	VkDescriptorBufferInfo wSunBuffer = m_sunParamsBuffer->GetDescriptor();
	VkDescriptorImageInfo wSunImg = m_sunTexture->GetTextureDescriptor();

    VkDescriptorImageInfo wSunOutImg = CreateDescriptorImageInfo(m_sampler, GraphicEngine::GetAttachment("SunSprite")->GetHandle()->GetView(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkDescriptorImageInfo wBlurVImg = CreateDescriptorImageInfo(m_sampler, GraphicEngine::GetAttachment("SunBlur1")->GetHandle()->GetView(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkDescriptorImageInfo wBlurHImg = CreateDescriptorImageInfo(m_sampler, GraphicEngine::GetAttachment("SunBlur2")->GetHandle()->GetView(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    VkDescriptorImageInfo depthImg;
    depthImg.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	depthImg.imageView = depthBuffer->GetView();
    depthImg.sampler = m_neareastSampler;

	VkDescriptorBufferInfo rbBuff = m_radialBlurParamsBuffer->GetDescriptor();

    std::vector<VkWriteDescriptorSet> wDesc;
    wDesc.push_back(InitUpdateDescriptor(m_blurVDescSet, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &wSunOutImg));
    wDesc.push_back(InitUpdateDescriptor(m_blurHDescSet, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &wBlurVImg));
    wDesc.push_back(InitUpdateDescriptor(m_blurRadialDescSet, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &wBlurHImg));
    wDesc.push_back(InitUpdateDescriptor(m_blurRadialDescSet, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &rbBuff));
	wDesc.push_back(InitUpdateDescriptor(m_sunDescriptorSet, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &wSunBuffer));
	wDesc.push_back(InitUpdateDescriptor(m_sunDescriptorSet, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &wSunImg));
    wDesc.push_back(InitUpdateDescriptor(m_sunDescriptorSet, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &depthImg));

    vk::UpdateDescriptorSets(vk::g_vulkanContext.m_device, (uint32_t)wDesc.size(), wDesc.data(), 0, nullptr);
    //UpdateSunDescriptors();
};

bool SunRenderer::OnKeyPressed(const KeyInput& keyInput)
{
	WPARAM key = keyInput.GetKeyPressed();
    //TRAP(m_editInfo);
    if(key == '1')
    {
		m_isEditMode = !m_isEditMode;
        //m_editInfo->SetVisible(m_isEditMode);
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

void SunRenderer::UpdateEditInfo()
{
   /* m_editInfo->SetTextItem(0, "(E/R)Weight: " + std::to_string(m_lightShaftWeight));
    m_editInfo->SetTextItem(1, "(T/Y)Decay: " + std::to_string(m_lightShaftDecay));
    m_editInfo->SetTextItem(2, "(U/I)Density: " + std::to_string(m_lightShaftDensity));
    m_editInfo->SetTextItem(3, "(O/P)Exposure: " + std::to_string(m_lightShaftExposure));
    m_editInfo->SetTextItem(4, "(F/G)Samples: " + std::to_string(m_lightShaftSamples));*/
}

void SunRenderer::CreateEditInfo()
{
    std::vector<std::string> texts;
    texts.resize(5);
    //m_editInfo = manager->CreateTextContainerItem(texts, glm::uvec2(20, 300), 5, 48); 
}

void SunRenderer::CreateDescriptorSetLayouts()
{
	m_blurSetLayout.AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
	m_blurSetLayout.Construct();
	RegisterDescriptorSetLayout(&m_blurSetLayout);

	m_sunDescriptorSetLayout.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
	m_sunDescriptorSetLayout.AddBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
	m_sunDescriptorSetLayout.AddBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
	m_sunDescriptorSetLayout.Construct();
	RegisterDescriptorSetLayout(&m_sunDescriptorSetLayout);

	m_radialBlurSetLayout.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
	m_radialBlurSetLayout.AddBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
	m_radialBlurSetLayout.Construct();
	RegisterDescriptorSetLayout(&m_radialBlurSetLayout);
}

void SunRenderer::AllocateDescriptorSets()
{
	m_sunDescriptorSet = m_descriptorPool.AllocateDescriptorSet(m_sunDescriptorSetLayout);
	m_blurVDescSet = m_descriptorPool.AllocateDescriptorSet(m_blurSetLayout);
	m_blurHDescSet = m_descriptorPool.AllocateDescriptorSet(m_blurSetLayout);
	m_blurRadialDescSet = m_descriptorPool.AllocateDescriptorSet(m_radialBlurSetLayout);
}

void SunRenderer::UpdateShaderParams()
{
    float fbWidth = float(m_frameWidth);
    float fbHeight = float(m_frameHeight);

    glm::mat4 proj;
    PerspectiveMatrix(proj);
    //proj = glm::ortho(-5.0f, 5.0f, -5.0f, 5.0f, 0.1f, 50.0f);
    ConvertToProjMatrix(proj);

	const CCamera& camera = GraphicEngine::GetActiveCamera();

    SSunParams* sunParams = m_sunParamsBuffer->GetPtr<SSunParams*>();
    sunParams->Scale = glm::vec4(m_sunScale, 0.0f, fbWidth, fbHeight);
    sunParams->LightDir = -directionalLight.GetDirection();
    sunParams->LightColor = directionalLight.GetLightIradiance();
    sunParams->LightColor.w = directionalLight.GetLightIntensity();
    sunParams->ViewMatrix = camera.GetViewMatrix();
    sunParams->ProjMatrix = proj;
    sunParams->CameraRight = glm::vec4(camera.GetRightVector(), 0.0f);
    sunParams->CameraUp = glm::vec4(camera.GetUpVector(), 0.0f);

    const float farDist = camera.GetFar();
    glm::vec4 sunPos = proj * camera.GetViewMatrix() * glm::vec4(glm::vec3(-directionalLight.GetDirection()) * farDist, 1.0f);
    sunPos = sunPos / sunPos.w;
	sunPos.z = 1.0f;

    SRadialBlurParams* rbParams = m_radialBlurParamsBuffer->GetPtr<SRadialBlurParams*>();
    rbParams->ProjSunPos = sunPos;
    rbParams->LightDensity = glm::vec4(m_lightShaftDensity);
    rbParams->LightDecay = glm::vec4(m_lightShaftDecay);
    rbParams->LightExposure = glm::vec4(m_lightShaftExposure);
    rbParams->SampleWeight = glm::vec4(m_lightShaftWeight);
    rbParams->ShaftSamples = glm::vec4(m_lightShaftSamples);

    m_renderSun = glm::all(glm::greaterThanEqual(glm::vec3(sunPos), glm::vec3(-1.0f))) && glm::all(glm::lessThanEqual(glm::vec3(sunPos), glm::vec3(1.0f)));
}
