#include "DirectionalLightRenderer.h"

#include "Utils.h"
#include "GraphicEngine.h"
#include "Framebuffer.h"

struct LightShaderParams
{
	glm::vec4       dirLight;
	glm::vec4       cameraPos;
	glm::vec4       lightIradiance;
	glm::mat4       pointVPMatrix;
	glm::mat4       pointModelMatrix;
	glm::vec4       pointRadius;
	glm::vec4       pointColor;
};

LightRenderer::LightRenderer()
	: Renderer()
	, m_sampler(VK_NULL_HANDLE)
	, m_shaderUniformBuffer(nullptr)
	, m_depthSampler(VK_NULL_HANDLE)
	, m_descriptorSet(VK_NULL_HANDLE)
{

}
LightRenderer::~LightRenderer()
{
	VkDevice dev = vk::g_vulkanContext.m_device;
	vk::DestroySampler(dev, m_sampler, nullptr);
	MemoryManager::GetInstance()->FreeHandle(m_shaderUniformBuffer);
}

void LightRenderer::AllocateDescriptorSets()
{
	m_descriptorSet = m_descriptorPool.AllocateDescriptorSet(m_descriptorSetLayout);
}

void LightRenderer::Render()
{
	VkCommandBuffer cmdBuffer = vk::g_vulkanContext.m_mainCommandBuffer;

	StartDebugMarker("LightingPass");
	vk::CmdBindPipeline(cmdBuffer, m_pipeline.GetBindPoint(), m_pipeline.Get());
	vk::CmdBindDescriptorSets(cmdBuffer, m_pipeline.GetBindPoint(), m_pipeline.GetLayout(), 0, 1, &m_descriptorSet, 0, nullptr);
	vk::CmdDraw(cmdBuffer, 4, 1, 0, 0);
	EndDebugMarker("LightingPass");
}

void LightRenderer::PreRender()
{
	const FrameConstants& fc = GraphicEngine::GetFrameConstants();
	LightShaderParams* newParams = m_shaderUniformBuffer->GetPtr<LightShaderParams*>();

	newParams->dirLight = fc.DirectionalLightDirection;
	newParams->cameraPos = glm::vec4(ms_camera.GetPos(), 1);
	newParams->lightIradiance = fc.DirectionaLightIrradiance;
	newParams->lightIradiance[3] = fc.DirectionalLightIntensity; //HEHE
}

void LightRenderer::InitInternal()
{
	CreateNearestSampler(m_sampler);

	CreateNearestSampler(m_depthSampler);

	m_shaderUniformBuffer = MemoryManager::GetInstance()->CreateBuffer(EMemoryContextType::UniformBuffers, sizeof(LightShaderParams), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
}

void LightRenderer::Setup(VkRenderPass renderPass, uint32_t subpassId)
{
	m_pipeline.SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN);
	m_pipeline.AddBlendState(CGraphicPipeline::CreateDefaultBlendState(), 2);
	m_pipeline.SetVertexShaderFile("light.vert");
	m_pipeline.SetFragmentShaderFile("light.frag");
	m_pipeline.SetDepthTest(false);

	m_pipeline.CreatePipelineLayout(m_descriptorSetLayout.Get());
	m_pipeline.Setup(renderPass, subpassId);
}

void LightRenderer::UpdateGraphicInterface()
{

	std::vector<VkDescriptorImageInfo> imgInfo;
	auto attachmentToDescImageInfo = [&](const std::string& attName) 
	{
		ImageHandle* handle = GraphicEngine::GetAttachment(attName)->GetHandle();
		return CreateDescriptorImageInfo(m_sampler, handle->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	};
	VkDescriptorImageInfo albedo = attachmentToDescImageInfo("Albedo");
	VkDescriptorImageInfo specular = attachmentToDescImageInfo("Specular");
	VkDescriptorImageInfo normal = attachmentToDescImageInfo("Normals");
	VkDescriptorImageInfo position = attachmentToDescImageInfo("Positions");
	VkDescriptorImageInfo shadowMap = attachmentToDescImageInfo("ShadowResolveFinal");
	VkDescriptorImageInfo aoMap = attachmentToDescImageInfo("AOFinal");

	VkDescriptorBufferInfo buffInfo = m_shaderUniformBuffer->GetDescriptor();
	std::vector<VkWriteDescriptorSet> writeSets;
	writeSets.push_back(InitUpdateDescriptor(m_descriptorSet, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &buffInfo));
	writeSets.push_back(InitUpdateDescriptor(m_descriptorSet, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &albedo));
	writeSets.push_back(InitUpdateDescriptor(m_descriptorSet, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &specular));
	writeSets.push_back(InitUpdateDescriptor(m_descriptorSet, 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &normal));
	writeSets.push_back(InitUpdateDescriptor(m_descriptorSet, 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &position));
	writeSets.push_back(InitUpdateDescriptor(m_descriptorSet, 5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &shadowMap));
	writeSets.push_back(InitUpdateDescriptor(m_descriptorSet, 6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &aoMap));

	vk::UpdateDescriptorSets(vk::g_vulkanContext.m_device, (uint32_t)writeSets.size(), writeSets.data(), 0, nullptr);
}

void LightRenderer::CreateDescriptorSetLayouts()
{
	m_descriptorSetLayout.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
	m_descriptorSetLayout.AddBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
	m_descriptorSetLayout.AddBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
	m_descriptorSetLayout.AddBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
	m_descriptorSetLayout.AddBinding(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
	m_descriptorSetLayout.AddBinding(5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
	m_descriptorSetLayout.AddBinding(6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);

	m_descriptorSetLayout.Construct();

	RegisterDescriptorSetLayout(&m_descriptorSetLayout);
}