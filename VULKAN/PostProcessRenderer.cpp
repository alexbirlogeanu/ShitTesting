#include "PostProcessRenderer.h"

#include "Framebuffer.h"
#include "glm/glm.hpp"
#include "GraphicEngine.h"
#include "MemoryManager.h"
#include "Texture.h"
#include "Utils.h"

struct PostProcessParam
{
	glm::vec4       screenCoords; //x = width, y = height, z = 1/width, w = 1/height
};

PostProcessRenderer::PostProcessRenderer()
	: Renderer()
	, m_sampler(VK_NULL_HANDLE)
	, m_uniformBuffer(nullptr)
	, m_quadMesh(nullptr)
	, m_descriptorSet(VK_NULL_HANDLE)
	, m_linearSampler(VK_NULL_HANDLE)
{

}

PostProcessRenderer::~PostProcessRenderer()
{
	VkDevice device = vk::g_vulkanContext.m_device;
	vk::DestroySampler(device, m_sampler, nullptr);
	vk::DestroySampler(device, m_linearSampler, nullptr);

	MemoryManager::GetInstance()->FreeHandle(m_uniformBuffer);
}

void PostProcessRenderer::Render()
{
	StartDebugMarker("HDRPass");
	VkCommandBuffer cmdBuffer = vk::g_vulkanContext.m_mainCommandBuffer;
	vk::CmdBindPipeline(cmdBuffer, m_pipeline.GetBindPoint(), m_pipeline.Get());
	vk::CmdBindDescriptorSets(cmdBuffer, m_pipeline.GetBindPoint(), m_pipeline.GetLayout(), 0, 1, &m_descriptorSet, 0, nullptr);

	m_quadMesh->Render();
	EndDebugMarker("HDRPass");
}

void PostProcessRenderer::Setup(VkRenderPass renderPass, uint32_t subpassId)
{
	m_pipeline.SetVertexInputState(Mesh::GetVertexDesc());
	m_pipeline.SetViewport(WIDTH, HEIGHT);
	m_pipeline.SetScissor(WIDTH, HEIGHT);
	m_pipeline.SetVertexShaderFile("screenquad.vert");
	m_pipeline.SetFragmentShaderFile("hdrgamma.frag");
	m_pipeline.SetDepthTest(false);
	m_pipeline.SetDepthWrite(false);
	VkPipelineColorBlendAttachmentState blendAtt = CGraphicPipeline::CreateDefaultBlendState();
	m_pipeline.AddBlendState(blendAtt);

	m_pipeline.CreatePipelineLayout(m_descriptorSetLayout.Get());
	m_pipeline.Setup(renderPass, subpassId);

	RegisterPipeline(&m_pipeline);
}

void PostProcessRenderer::UpdateShaderParams()
{
	PostProcessParam* params = m_uniformBuffer->GetPtr<PostProcessParam*>();
	params->screenCoords.x = WIDTH;
	params->screenCoords.y = HEIGHT;
	params->screenCoords.z = 1.0f / WIDTH;
	params->screenCoords.w = 1.0f / HEIGHT;
}

void PostProcessRenderer::InitInternal()
{
	CreateNearestSampler(m_sampler);
	CreateLinearSampler(m_linearSampler);

	m_uniformBuffer = MemoryManager::GetInstance()->CreateBuffer(EMemoryContextType::UniformBuffers, sizeof(PostProcessParam), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

	m_quadMesh = CreateFullscreenQuad();

	SImageData lutData;
	ReadLUTTextureData(lutData, std::string(TEXTDIR) + "LUT_Warm.png", true);

	m_lut = new CTexture(lutData, true);

}

void PostProcessRenderer::CreateDescriptorSetLayouts()
{
	m_descriptorSetLayout.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
	m_descriptorSetLayout.AddBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
	m_descriptorSetLayout.AddBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
	m_descriptorSetLayout.Construct();

	RegisterDescriptorSetLayout(&m_descriptorSetLayout);
}

void PostProcessRenderer::UpdateGraphicInterface()
{
	ImageHandle* defferedOutput = GraphicEngine::GetAttachment("DirectionalLightingFinal")->GetHandle();
	VkDescriptorImageInfo imgInfo = CreateDescriptorImageInfo(m_sampler, defferedOutput->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	VkDescriptorBufferInfo buffInfo = m_uniformBuffer->GetDescriptor();
	VkDescriptorImageInfo lutInfo = m_lut->GetTextureDescriptor();

	std::vector<VkWriteDescriptorSet> wDescSet;
	wDescSet.push_back(InitUpdateDescriptor(m_descriptorSet, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &buffInfo));
	wDescSet.push_back(InitUpdateDescriptor(m_descriptorSet, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &imgInfo));
	wDescSet.push_back(InitUpdateDescriptor(m_descriptorSet, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &lutInfo));

	vk::UpdateDescriptorSets(vk::g_vulkanContext.m_device, (uint32_t)wDescSet.size(), wDescSet.data(), 0, nullptr);
}

void PostProcessRenderer::AllocateDescriptorSets()
{
	m_descriptorSet = m_descriptorPool.AllocateDescriptorSet(m_descriptorSetLayout);
}
