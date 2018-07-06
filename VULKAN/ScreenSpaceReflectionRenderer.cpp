#include "ScreenSpaceReflectionRenderer.h"

#include "Utils.h"
#include "MemoryManager.h"
#include "ResourceTable.h"

enum
{
	Binding_Uniform,
	Binding_Normal,
	Binding_Position,
	Binding_Color,
	Binding_Depth,
	Binding_Count
};

struct SSRConstants
{
	glm::vec4 ViewDirection;
	glm::mat4 ViewMatrix;
	glm::mat4 ProjMatrix;
};

ScreenSpaceReflectionsRenderer::ScreenSpaceReflectionsRenderer(VkRenderPass renderPass)
	: CRenderer(renderPass, "SSR")
	, m_ssrConstantsBuffer(nullptr)
	, m_sampler(VK_NULL_HANDLE)
	, m_ssrDescSet(VK_NULL_HANDLE)
	, m_ssrDescLayout(VK_NULL_HANDLE)
{

}

ScreenSpaceReflectionsRenderer::~ScreenSpaceReflectionsRenderer()
{
	VkDevice dev = vk::g_vulkanContext.m_device;

	vk::DestroySampler(dev, m_sampler, nullptr);
	vk::DestroyDescriptorSetLayout(dev, m_ssrDescLayout, nullptr);
}

void ScreenSpaceReflectionsRenderer::Init()
{
	CRenderer::Init();
	AllocDescriptorSets(m_descriptorPool, m_ssrDescLayout, &m_ssrDescSet);
	CreateNearestSampler(m_sampler);

	m_ssrConstantsBuffer = MemoryManager::GetInstance()->CreateBuffer(EMemoryContextType::UniformBuffers, sizeof(SSRConstants), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

	CreatePipelines();

	PerspectiveMatrix(m_projMatrix);
	ConvertToProjMatrix(m_projMatrix);

	m_quad = CreateFullscreenQuad();
}

void ScreenSpaceReflectionsRenderer::PreRender()
{
	SSRConstants* consts = m_ssrConstantsBuffer->GetPtr<SSRConstants*>();
	consts->ViewDirection = glm::vec4(ms_camera.GetFrontVector(), 1.0f);
	consts->ProjMatrix = m_projMatrix;
	consts->ViewMatrix = ms_camera.GetViewMatrix();
}

void ScreenSpaceReflectionsRenderer::Render()
{
	StartRenderPass();

	VkCommandBuffer cmdBuff = vk::g_vulkanContext.m_mainCommandBuffer;
	vk::CmdBindPipeline(cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_ssrPipeline.Get());
	vk::CmdBindDescriptorSets(cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_ssrPipeline.GetLayout(), 0, 1, &m_ssrDescSet, 0, nullptr);

	m_quad->Render();

	EndRenderPass();
}

void ScreenSpaceReflectionsRenderer::CreateDescriptorSetLayout()
{
	{
		std::vector<VkDescriptorSetLayoutBinding> bindings;

		bindings.push_back(CreateDescriptorBinding(Binding_Uniform, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT));
		bindings.push_back(CreateDescriptorBinding(Binding_Color, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT));
		bindings.push_back(CreateDescriptorBinding(Binding_Normal, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT));
		bindings.push_back(CreateDescriptorBinding(Binding_Position, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT));
		bindings.push_back(CreateDescriptorBinding(Binding_Depth, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT));

		VkDescriptorSetLayoutCreateInfo crtInfo;
		cleanStructure(crtInfo);
		crtInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		crtInfo.bindingCount = (uint32_t)bindings.size();
		crtInfo.pBindings = bindings.data();

		VULKAN_ASSERT(vk::CreateDescriptorSetLayout(vk::g_vulkanContext.m_device, &crtInfo, nullptr, &m_ssrDescLayout));
	}
}

void ScreenSpaceReflectionsRenderer::CreatePipelines()
{
	m_ssrPipeline.SetVertexShaderFile("screenquad.vert");
	m_ssrPipeline.SetFragmentShaderFile("ssr.frag");
	m_ssrPipeline.AddBlendState(CGraphicPipeline::CreateDefaultBlendState(), 2);
	m_ssrPipeline.SetDepthTest(false);
	m_ssrPipeline.SetDepthWrite(false);
	m_ssrPipeline.SetVertexInputState(Mesh::GetVertexDesc());
	m_ssrPipeline.CreatePipelineLayout(m_ssrDescLayout);
	m_ssrPipeline.SetViewport(m_framebuffer->GetWidth(), m_framebuffer->GetHeight());
	m_ssrPipeline.Init(this, m_renderPass, 0);
}

void ScreenSpaceReflectionsRenderer::UpdateResourceTable()
{
}

void ScreenSpaceReflectionsRenderer::UpdateGraphicInterface()
{
	std::vector<VkWriteDescriptorSet> wDesc;
	VkDescriptorImageInfo normals = CreateDescriptorImageInfo(m_sampler, g_commonResources.GetAs<ImageHandle*>(EResourceType_NormalsImage)->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	VkDescriptorImageInfo positions = CreateDescriptorImageInfo(m_sampler, g_commonResources.GetAs<ImageHandle*>(EResourceType_PositionsImage)->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	VkDescriptorImageInfo depth = CreateDescriptorImageInfo(m_sampler, g_commonResources.GetAs<ImageHandle*>(EResourceType_DepthBufferImage)->GetView(), VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

	VkDescriptorImageInfo color = CreateDescriptorImageInfo(m_sampler, g_commonResources.GetAs<ImageHandle*>(EResourceType_FinalImage)->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	
	VkDescriptorBufferInfo constants = m_ssrConstantsBuffer->GetDescriptor();

	wDesc.push_back(InitUpdateDescriptor(m_ssrDescSet, Binding_Uniform, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &constants));
	wDesc.push_back(InitUpdateDescriptor(m_ssrDescSet, Binding_Normal, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &normals));
	wDesc.push_back(InitUpdateDescriptor(m_ssrDescSet, Binding_Color, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &color));
	wDesc.push_back(InitUpdateDescriptor(m_ssrDescSet, Binding_Depth, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &depth));
	wDesc.push_back(InitUpdateDescriptor(m_ssrDescSet, Binding_Position, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &positions));

	vk::UpdateDescriptorSets(vk::g_vulkanContext.m_device, (uint32_t)wDesc.size(), wDesc.data(), 0, nullptr);
}

void ScreenSpaceReflectionsRenderer::PopulatePoolInfo(std::vector<VkDescriptorPoolSize>& poolSize, unsigned int& maxSets)
{
	AddDescriptorType(poolSize, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1);
	AddDescriptorType(poolSize, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4);

	maxSets = 1;
}