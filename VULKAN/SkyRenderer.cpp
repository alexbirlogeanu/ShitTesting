#include "SkyRenderer.h"

#include "GraphicEngine.h"
#include "Utils.h"
#include "Framebuffer.h"
#include "Texture.h"
#include "ResourceLoader.h"
#include "GraphicEngine.h"

SkyRenderer::SkyRenderer()
	: Renderer()
	, m_quadMesh(nullptr)
	, m_skyTexture(nullptr)
	, m_boxParamsBuffer(VK_NULL_HANDLE)
	, m_boxDescriptorSet(VK_NULL_HANDLE)
	, m_sampler(VK_NULL_HANDLE)
	, m_sunDescriptorSet(VK_NULL_HANDLE)
{
}

SkyRenderer::~SkyRenderer()
{
	VkDevice dev = vk::g_vulkanContext.m_device;

	MemoryManager::GetInstance()->FreeHandle(m_boxParamsBuffer);
}

void SkyRenderer::RenderSkyboxPass()
{
	VkCommandBuffer cmdBuffer = vk::g_vulkanContext.m_mainCommandBuffer;
	BeginMarkerSection("SkyBox");
	vk::CmdBindPipeline(cmdBuffer, m_boxPipeline.GetBindPoint(), m_boxPipeline.Get());
	vk::CmdBindDescriptorSets(cmdBuffer, m_boxPipeline.GetBindPoint(), m_boxPipeline.GetLayout(), 0, 1, &m_boxDescriptorSet, 0, nullptr);
	m_quadMesh->Render();
	EndMarkerSection();
}

void SkyRenderer::BlendSunPass()
{
	BeginMarkerSection("BlendSun");
	VkCommandBuffer cmdBuffer = vk::g_vulkanContext.m_mainCommandBuffer;
	vk::CmdBindPipeline(cmdBuffer, m_sunPipeline.GetBindPoint(), m_sunPipeline.Get());
	vk::CmdBindDescriptorSets(cmdBuffer, m_sunPipeline.GetBindPoint(), m_sunPipeline.GetLayout(), 0, 1, &m_sunDescriptorSet, 0, nullptr);
	m_quadMesh->Render();
	EndMarkerSection();
}

void SkyRenderer::SetupSkyBoxSubpass(VkRenderPass renderPass, uint32_t subpassId)
{
	m_boxPipeline.SetVertexShaderFile("skybox.vert");
	m_boxPipeline.SetFragmentShaderFile("skybox.frag");
	m_boxPipeline.SetVertexInputState(Mesh::GetVertexDesc());
	m_boxPipeline.AddBlendState(CGraphicPipeline::CreateDefaultBlendState());
	m_boxPipeline.SetDepthTest(true);
	m_boxPipeline.SetDepthWrite(false);
	m_boxPipeline.CreatePipelineLayout(m_boxDescriptorSetLayout.Get());
	m_boxPipeline.Setup(renderPass, subpassId);
	RegisterPipeline(&m_boxPipeline);
}

void SkyRenderer::SetupBlendSunSubpass(VkRenderPass renderPass, uint32_t subpassId)
{
	VkPipelineColorBlendAttachmentState addState;
	cleanStructure(addState);
	addState.blendEnable = VK_TRUE;
	addState.colorBlendOp = VK_BLEND_OP_ADD;
	addState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
	addState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	addState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT;

	m_sunPipeline.SetVertexShaderFile("screenquad.vert");
	m_sunPipeline.SetFragmentShaderFile("passtrough.frag");
	m_sunPipeline.SetVertexInputState(Mesh::GetVertexDesc());
	m_sunPipeline.AddBlendState(addState);
	m_sunPipeline.SetDepthTest(false);
	m_sunPipeline.CreatePipelineLayout(m_sunDescriptorSetLayout.Get());
	m_sunPipeline.Setup(renderPass, subpassId);
	RegisterPipeline(&m_sunPipeline);
}

void SkyRenderer::InitInternal()
{
	m_boxParamsBuffer = MemoryManager::GetInstance()->CreateBuffer(EMemoryContextType::UniformBuffers, sizeof(SSkyParams), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
	m_quadMesh = CreateFullscreenQuad();
	
	m_skyTexture = new CTexture("side.png", false);
	ResourceLoader::GetInstance()->LoadTexture(&m_skyTexture);

	CreateLinearSampler(m_sampler);
}

void SkyRenderer::CreateDescriptorSetLayouts()
{
	m_boxDescriptorSetLayout.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
	m_boxDescriptorSetLayout.AddBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
	m_boxDescriptorSetLayout.Construct();
	RegisterDescriptorSetLayout(&m_boxDescriptorSetLayout);

	m_sunDescriptorSetLayout.AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
	m_sunDescriptorSetLayout.Construct();
	RegisterDescriptorSetLayout(&m_sunDescriptorSetLayout);
}

void SkyRenderer::AllocateDescriptorSets()
{
	m_boxDescriptorSet = m_descriptorPool.AllocateDescriptorSet(m_boxDescriptorSetLayout);
	m_sunDescriptorSet = m_descriptorPool.AllocateDescriptorSet(m_sunDescriptorSetLayout);
}

void SkyRenderer::PreRender()
{
	const auto& camera = GraphicEngine::GetInstance()->GetActiveCamera();

	SSkyParams* newParams = m_boxParamsBuffer->GetPtr<SSkyParams*>();
	newParams->CameraDir = glm::vec4(camera.GetFrontVector(), 0.0f);
	newParams->CameraUp = glm::vec4(camera.GetUpVector(), 0.0f);
	newParams->CameraRight = glm::vec4(camera.GetRightVector(), 0.0f);
	newParams->Frustrum = glm::vec4(glm::radians(75.0f), 16.0f / 9.0f, 100.0f, 0.0f);
	newParams->DirLightColor = directionalLight.GetLightIradiance();
}

void SkyRenderer::UpdateGraphicInterface()
{
	VkDescriptorBufferInfo wBuffer = m_boxParamsBuffer->GetDescriptor();
	VkDescriptorImageInfo wImage = m_skyTexture->GetTextureDescriptor();
	VkDescriptorImageInfo sunImage = CreateDescriptorImageInfo(m_sampler, GraphicEngine::GetAttachment("SunFinal")->GetHandle()->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	std::vector<VkWriteDescriptorSet> writeDesc;
	writeDesc.resize(3);
	writeDesc[0] = InitUpdateDescriptor(m_boxDescriptorSet, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &wBuffer);
	writeDesc[1] = InitUpdateDescriptor(m_boxDescriptorSet, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &wImage);
	writeDesc[2] = InitUpdateDescriptor(m_sunDescriptorSet, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &sunImage);

	vk::UpdateDescriptorSets(vk::g_vulkanContext.m_device, (uint32_t)writeDesc.size(), writeDesc.data(), 0, nullptr);
}
