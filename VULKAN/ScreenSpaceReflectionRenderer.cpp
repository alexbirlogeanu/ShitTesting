#include "ScreenSpaceReflectionRenderer.h"

#include "Utils.h"
#include "MemoryManager.h"
#include "ResourceTable.h"

struct SSRConstants
{
	glm::vec4 ScreenInfo;
	glm::mat4 ViewMatrix;
	glm::mat4 ProjMatrix;
	glm::mat4 InvProjMatrix;
	glm::mat4 ViewSpaceToScreenSpace;
};

enum
{
	SSRBinding_InConstants,
	SSRBinding_InNormal,
	SSRBinding_InPosition,
	SSRBinding_InDepth,
	SSRBinding_OutImage,
	SSRBinding_OutDebug
};

enum
{
	Subpass_BlurH,
	Subpass_BlurV
};

ScreenSpaceReflectionsRenderer::ScreenSpaceReflectionsRenderer(VkRenderPass renderPass)
	: CRenderer(renderPass, "SSR")
	, m_ssrConstantsBuffer(nullptr)
	, m_ssrSampler(VK_NULL_HANDLE)
	, m_resolveSampler(VK_NULL_HANDLE)
	, m_linearSampler(VK_NULL_HANDLE)
	, m_ssrDescSet(VK_NULL_HANDLE)
	, m_ssrDescLayout(VK_NULL_HANDLE)
	, m_blurDescLayout(VK_NULL_HANDLE)
	, m_blurVDescSet(VK_NULL_HANDLE)
	, m_blurHDescSet(VK_NULL_HANDLE)
	, m_resolveDescSet(VK_NULL_HANDLE)
	, m_resolveLayout(VK_NULL_HANDLE)
	, m_ssrOutputImage(nullptr)
	, m_ssrDebugImage(nullptr)
	, m_cellSize(32)
{
	unsigned int divider = 2;
	m_resolutionX = WIDTH / divider;
	m_resolutionY = HEIGHT / divider;
}

ScreenSpaceReflectionsRenderer::~ScreenSpaceReflectionsRenderer()
{
	VkDevice dev = vk::g_vulkanContext.m_device;

	vk::DestroySampler(dev, m_ssrSampler, nullptr);
	vk::DestroySampler(dev, m_resolveSampler, nullptr);
	vk::DestroySampler(dev, m_linearSampler, nullptr);

	MemoryManager::GetInstance()->FreeHandle(EMemoryContextType::Framebuffers, m_ssrOutputImage);
	MemoryManager::GetInstance()->FreeHandle(EMemoryContextType::Framebuffers, m_ssrDebugImage);

	vk::DestroyDescriptorSetLayout(dev, m_ssrDescLayout, nullptr);
	vk::DestroyDescriptorSetLayout(dev, m_blurDescLayout, nullptr);
	vk::DestroyDescriptorSetLayout(dev, m_resolveLayout, nullptr);
}

void ScreenSpaceReflectionsRenderer::Init()
{
	CRenderer::Init();

	AllocDescriptorSets(m_descriptorPool, m_ssrDescLayout, &m_ssrDescSet);
	//i think this method is slow, but we only allocate just once
	AllocDescriptorSets(m_descriptorPool, m_blurDescLayout, &m_blurVDescSet);
	AllocDescriptorSets(m_descriptorPool, m_blurDescLayout, &m_blurHDescSet);
	AllocDescriptorSets(m_descriptorPool, m_resolveLayout, &m_resolveDescSet);

	if (m_resolutionX == WIDTH && m_resolutionY == HEIGHT) //full resolution
		CreateNearestSampler(m_ssrSampler);
	else
		CreateLinearSampler(m_ssrSampler); //for half resolution

	CreateNearestSampler(m_resolveSampler);
	CreateLinearSampler(m_linearSampler);

	CreatePipelines();
	CreateImages();

	m_ssrConstantsBuffer = MemoryManager::GetInstance()->CreateBuffer(EMemoryContextType::UniformBuffers, sizeof(SSRConstants), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

	PerspectiveMatrix(m_projMatrix);
	ConvertToProjMatrix(m_projMatrix);

	m_viewToScreenSpaceMatrix = glm::mat4(glm::vec4(WIDTH * 0.5f, 0.0f, 0.0f, 0.0f),
								glm::vec4(0.0f, HEIGHT * 0.5f, 0.0f, 0.0f),
								glm::vec4(0.0f, 0.0f, 1.0f, 0.0f),
								glm::vec4(WIDTH * 0.5, HEIGHT * 0.5f, 0.0f, 1.0f));

	m_viewToScreenSpaceMatrix = m_viewToScreenSpaceMatrix * m_projMatrix;

	m_quad = CreateFullscreenQuad();
}

void ScreenSpaceReflectionsRenderer::PreRender()
{
	SSRConstants* consts = m_ssrConstantsBuffer->GetPtr<SSRConstants*>();
	consts->ProjMatrix = m_projMatrix;
	consts->ViewMatrix = ms_camera.GetViewMatrix();
	consts->InvProjMatrix = glm::inverse(m_projMatrix);
	consts->ScreenInfo = glm::vec4(float(WIDTH), float(HEIGHT), 1.0f / float(WIDTH), 1.0f / float(HEIGHT));
	consts->ViewSpaceToScreenSpace = m_viewToScreenSpaceMatrix;
}

void ScreenSpaceReflectionsRenderer::Render()
{
	VkCommandBuffer cmdBuff = vk::g_vulkanContext.m_mainCommandBuffer;

	StartDebugMarker("SSRCompute");
	ClearImages();
	vk::CmdBindPipeline(cmdBuff, m_ssrPipeline.GetBindPoint(), m_ssrPipeline.Get());
	vk::CmdBindDescriptorSets(cmdBuff, m_ssrPipeline.GetBindPoint(), m_ssrPipeline.GetLayout(), 0, 1, &m_ssrDescSet, 0, nullptr);
	vk::CmdDispatch(cmdBuff, m_resolutionX / m_cellSize + ((m_resolutionX % m_cellSize == 0) ? 0 : 1), m_resolutionY / m_cellSize + ((m_resolutionY / m_cellSize == 0) ? 0 : 1), 1);
	EndDebugMarker("SSRCompute");

	StartRenderPass();

	vk::CmdBindPipeline(cmdBuff, m_blurHPipeline.GetBindPoint(), m_blurHPipeline.Get());
	vk::CmdBindDescriptorSets(cmdBuff, m_blurHPipeline.GetBindPoint(), m_blurHPipeline.GetLayout(), 0, 1, &m_blurHDescSet, 0, nullptr);

	m_quad->Render();

	vk::CmdNextSubpass(cmdBuff, VK_SUBPASS_CONTENTS_INLINE);
	vk::CmdBindPipeline(cmdBuff, m_blurVPipeline.GetBindPoint(), m_blurVPipeline.Get());
	vk::CmdBindDescriptorSets(cmdBuff, m_blurVPipeline.GetBindPoint(), m_blurVPipeline.GetLayout(), 0, 1, &m_blurVDescSet, 0, nullptr);

	m_quad->Render();

	vk::CmdNextSubpass(cmdBuff, VK_SUBPASS_CONTENTS_INLINE);
	vk::CmdBindPipeline(cmdBuff, m_ssrResolvePipeline.GetBindPoint(), m_ssrResolvePipeline.Get());
	vk::CmdBindDescriptorSets(cmdBuff, m_ssrResolvePipeline.GetBindPoint(), m_ssrResolvePipeline.GetLayout(), 0, 1, &m_resolveDescSet, 0, nullptr);
	vk::CmdPushConstants(cmdBuff, m_ssrResolvePipeline.GetLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(glm::mat4), &ms_camera.GetViewMatrix());

	m_quad->Render();

	EndRenderPass();
}

void ScreenSpaceReflectionsRenderer::CreateDescriptorSetLayout()
{
	{
		std::vector<VkDescriptorSetLayoutBinding> bindings;

		bindings.push_back(CreateDescriptorBinding(SSRBinding_InConstants, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT));
		bindings.push_back(CreateDescriptorBinding(SSRBinding_InPosition, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT));
		bindings.push_back(CreateDescriptorBinding(SSRBinding_InNormal, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT));
		bindings.push_back(CreateDescriptorBinding(SSRBinding_InDepth, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT));
		bindings.push_back(CreateDescriptorBinding(SSRBinding_OutImage, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT));
		bindings.push_back(CreateDescriptorBinding(SSRBinding_OutDebug, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT));

		NewDescriptorSetLayout(bindings, &m_ssrDescLayout);
	}

	{
		std::vector<VkDescriptorSetLayoutBinding> bindings;
		bindings.push_back(CreateDescriptorBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT));

		NewDescriptorSetLayout(bindings, &m_blurDescLayout);
	}

	{
		std::vector<VkDescriptorSetLayoutBinding> bindings;
		bindings.push_back(CreateDescriptorBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT));
		bindings.push_back(CreateDescriptorBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT));
		bindings.push_back(CreateDescriptorBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT));
		bindings.push_back(CreateDescriptorBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT));
		bindings.push_back(CreateDescriptorBinding(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT));

		NewDescriptorSetLayout(bindings, &m_resolveLayout);
	}
}

void ScreenSpaceReflectionsRenderer::CreatePipelines()
{
	m_ssrPipeline.SetComputeShaderFile("ssr.comp");
	m_ssrPipeline.CreatePipelineLayout(m_ssrDescLayout);
	m_ssrPipeline.Init(this, VK_NULL_HANDLE, -1);

	//
	CreateBlurPipelines(m_blurHPipeline, false);
	CreateBlurPipelines(m_blurVPipeline, true);

	//
	m_ssrResolvePipeline.SetVertexShaderFile("screenquad.vert");
	m_ssrResolvePipeline.SetFragmentShaderFile("ssrresolve.frag");
	m_ssrResolvePipeline.AddBlendState(CGraphicPipeline::CreateDefaultBlendState(), 2); //change blend state
	m_ssrResolvePipeline.SetDepthTest(false);
	m_ssrResolvePipeline.SetVertexInputState(Mesh::GetVertexDesc());
	m_ssrResolvePipeline.SetViewport(m_framebuffer->GetWidth(), m_framebuffer->GetHeight());
	m_ssrResolvePipeline.AddPushConstant({ VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(glm::mat4) });
	m_ssrResolvePipeline.CreatePipelineLayout(m_resolveLayout);
	m_ssrResolvePipeline.Init(this, m_renderPass, 2);
}

void ScreenSpaceReflectionsRenderer::CreateBlurPipelines(CGraphicPipeline& pipeline, bool isVertical)
{
	uint32_t subpassID = isVertical ? Subpass_BlurV : Subpass_BlurH;

	pipeline.SetVertexShaderFile("screenquad.vert");
	pipeline.SetFragmentShaderFile(isVertical ? "vblur.frag" : "hblur.frag");
	pipeline.AddBlendState(CGraphicPipeline::CreateDefaultBlendState());
	pipeline.SetDepthTest(false);
	pipeline.SetDepthWrite(false);
	pipeline.SetVertexInputState(Mesh::GetVertexDesc());
	pipeline.SetViewport(m_framebuffer->GetWidth(), m_framebuffer->GetHeight());
	pipeline.CreatePipelineLayout(m_blurDescLayout);
	pipeline.Init(this, m_renderPass, subpassID);
}

void ScreenSpaceReflectionsRenderer::CreateImages()
{
	VkImageCreateInfo crtInfo;
	cleanStructure(crtInfo);
	crtInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	crtInfo.imageType = VK_IMAGE_TYPE_2D;
	crtInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	crtInfo.extent = {m_resolutionX, m_resolutionY, 1};
	crtInfo.mipLevels = 1;
	crtInfo.arrayLayers = 1;
	crtInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	crtInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	crtInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	crtInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	crtInfo.queueFamilyIndexCount = 0;
	crtInfo.pQueueFamilyIndices = NULL;

	m_ssrOutputImage = MemoryManager::GetInstance()->CreateImage(EMemoryContextType::Framebuffers, crtInfo, "SSROutputImage");
	m_ssrDebugImage = MemoryManager::GetInstance()->CreateImage(EMemoryContextType::Framebuffers, crtInfo, "SSRDebugImage");
}

void ScreenSpaceReflectionsRenderer::ClearImages()
{
	static VkImageLayout currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	static VkAccessFlags currentAcces = 0;

	VkCommandBuffer cmdBuffer = vk::g_vulkanContext.m_mainCommandBuffer;

	//layout transition
	VkImageMemoryBarrier preClearBarrier[2];
	preClearBarrier[0] = m_ssrOutputImage->CreateMemoryBarrier(currentLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, currentAcces, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
	preClearBarrier[1] = m_ssrDebugImage->CreateMemoryBarrier(currentLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, currentAcces, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
	vk::CmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 0, nullptr, 2, preClearBarrier);

	VkClearColorValue clrValue;
	cleanStructure(clrValue);

	VkImageSubresourceRange range;
	cleanStructure(range);
	range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	range.baseMipLevel = 0;
	range.baseArrayLayer = 0;
	range.levelCount = 1;
	range.layerCount = VK_REMAINING_ARRAY_LAYERS;

	vk::CmdClearColorImage(cmdBuffer, m_ssrOutputImage->Get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clrValue, 1, &range); //clear color maybe ??
	vk::CmdClearColorImage(cmdBuffer, m_ssrDebugImage->Get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clrValue, 1, &range);

	VkImageMemoryBarrier preWriteBarrier[2];
	preWriteBarrier[0] = m_ssrOutputImage->CreateMemoryBarrier(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
		VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
	preWriteBarrier[1] = m_ssrDebugImage->CreateMemoryBarrier(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
		VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT);

	vk::CmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 0, nullptr, 2, preWriteBarrier);

	currentLayout = VK_IMAGE_LAYOUT_GENERAL;
	currentAcces = VK_ACCESS_SHADER_WRITE_BIT;
}

void ScreenSpaceReflectionsRenderer::UpdateResourceTable()
{
}

void ScreenSpaceReflectionsRenderer::UpdateGraphicInterface()
{
	std::vector<VkWriteDescriptorSet> wDesc;
	VkDescriptorImageInfo normals = CreateDescriptorImageInfo(m_ssrSampler, g_commonResources.GetAs<ImageHandle*>(EResourceType_NormalsImage)->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	VkDescriptorImageInfo positions = CreateDescriptorImageInfo(m_ssrSampler, g_commonResources.GetAs<ImageHandle*>(EResourceType_PositionsImage)->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	VkDescriptorImageInfo depth = CreateDescriptorImageInfo(m_ssrSampler, g_commonResources.GetAs<ImageHandle*>(EResourceType_DepthBufferImage)->GetView(), VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

	VkDescriptorImageInfo color = CreateDescriptorImageInfo(m_ssrSampler, g_commonResources.GetAs<ImageHandle*>(EResourceType_FinalImage)->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	VkDescriptorImageInfo specular = CreateDescriptorImageInfo(m_ssrSampler, g_commonResources.GetAs<ImageHandle*>(EResourceType_SpecularImage)->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	VkDescriptorImageInfo ssrOut = CreateDescriptorImageInfo(m_ssrSampler, m_ssrOutputImage->GetView(), VK_IMAGE_LAYOUT_GENERAL);
	VkDescriptorImageInfo ssrDebug = CreateDescriptorImageInfo(m_ssrSampler, m_ssrDebugImage->GetView(), VK_IMAGE_LAYOUT_GENERAL);
	VkDescriptorImageInfo blurH = CreateDescriptorImageInfo(m_ssrSampler, m_framebuffer->GetColorImageView(2), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	VkDescriptorImageInfo blurFinal = CreateDescriptorImageInfo(m_linearSampler, m_framebuffer->GetColorImageView(3), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	VkDescriptorImageInfo ssrRayTrace = CreateDescriptorImageInfo(m_resolveSampler, m_ssrOutputImage->GetView(), VK_IMAGE_LAYOUT_GENERAL);

	VkDescriptorBufferInfo constants = m_ssrConstantsBuffer->GetDescriptor();

	wDesc.push_back(InitUpdateDescriptor(m_ssrDescSet, SSRBinding_InConstants, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &constants));
	wDesc.push_back(InitUpdateDescriptor(m_ssrDescSet, SSRBinding_InNormal, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &normals));
	wDesc.push_back(InitUpdateDescriptor(m_ssrDescSet, SSRBinding_InPosition, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &positions));
	wDesc.push_back(InitUpdateDescriptor(m_ssrDescSet, SSRBinding_InDepth, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &depth));
	wDesc.push_back(InitUpdateDescriptor(m_ssrDescSet, SSRBinding_OutImage, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &ssrOut));
	wDesc.push_back(InitUpdateDescriptor(m_ssrDescSet, SSRBinding_OutDebug, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &ssrDebug));

	wDesc.push_back(InitUpdateDescriptor(m_blurHDescSet, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &color));
	wDesc.push_back(InitUpdateDescriptor(m_blurVDescSet, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &blurH));

	wDesc.push_back(InitUpdateDescriptor(m_resolveDescSet, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &ssrRayTrace));
	wDesc.push_back(InitUpdateDescriptor(m_resolveDescSet, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &blurFinal));
	wDesc.push_back(InitUpdateDescriptor(m_resolveDescSet, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &normals));
	wDesc.push_back(InitUpdateDescriptor(m_resolveDescSet, 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &specular));
	wDesc.push_back(InitUpdateDescriptor(m_resolveDescSet, 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &positions));

	vk::UpdateDescriptorSets(vk::g_vulkanContext.m_device, (uint32_t)wDesc.size(), wDesc.data(), 0, nullptr);
}

void ScreenSpaceReflectionsRenderer::PopulatePoolInfo(std::vector<VkDescriptorPoolSize>& poolSize, unsigned int& maxSets)
{
	AddDescriptorType(poolSize, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1);
	AddDescriptorType(poolSize, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 10);
	AddDescriptorType(poolSize, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2);

	maxSets = 4;
}