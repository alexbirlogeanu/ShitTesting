#include "PointLightRenderer2.h"

#include <random>
#include "MemoryManager.h"

struct PointLightParams
{
	glm::vec4 ViewSpacePosition;
	glm::vec4 Iradiance; //xyz iradiance, w - radius
	glm::vec4 Properties; //xyz - attenuation constants, w - attenuation denominator
};

struct GlobalParams
{
	glm::mat4 InvProjectionMatrix;
	glm::mat4 ViewMatrix;
	glm::vec4 CameraPosition;
	glm::ivec4 LightCount;
};

//for random generator things
static std::mt19937 Generator(1991);
static std::uniform_real_distribution<float> Distribution(0.f, 1.f);

PointLight::PointLight(glm::vec3 position)
	: m_intensity(5.f)
	, m_radius(2.5f)
	, m_attenuationConsts(1.f, 5.f, 2.f)
	, m_position(position)
{
	m_radiance = glm::vec3(Distribution(Generator), Distribution(Generator), Distribution(Generator));
}

void PointLight::Fill(PointLightParams& params, const glm::mat4& view) const //this is not necesary to fill all the params. will see
{
	float att = m_attenuationConsts.x + m_attenuationConsts.y * m_radius + m_attenuationConsts.z * glm::pow(m_radius, 2.f);
	params.ViewSpacePosition = view * glm::vec4(m_position, 1.f);
	params.Iradiance = glm::vec4(m_radiance * m_intensity, m_radius);
	params.Properties = glm::vec4(m_attenuationConsts, att);
}
////////////////////////////////////////////////////////////////////////////////////////////////////////

PointLightRenderer2::PointLightRenderer2(VkRenderPass renderPass)
	: CRenderer(renderPass, "DeferredTileShadingPass")
	, m_lightsBuffer(nullptr)
	, m_tileShadingDescSet(VK_NULL_HANDLE)
	, m_tileShadingDescLayout(VK_NULL_HANDLE)
	, m_resolveDescLayout(VK_NULL_HANDLE)
	, m_resolveDescSet(VK_NULL_HANDLE)
	, m_lightImage(nullptr)
	, m_debugImage(nullptr)
	, m_allocatedLights(0)
{
	PerspectiveMatrix(m_proj);
	ConvertToProjMatrix(m_proj);
}

PointLightRenderer2::~PointLightRenderer2()
{
	VkDevice dev = vk::g_vulkanContext.m_device;

	MemoryManager::GetInstance()->FreeHandle(m_lightsBuffer);

	vk::DestroyDescriptorSetLayout(dev, m_tileShadingDescLayout, nullptr);
	vk::DestroyDescriptorSetLayout(dev, m_resolveDescLayout, nullptr);

	MemoryManager::GetInstance()->FreeHandle(m_lightImage);
	MemoryManager::GetInstance()->FreeHandle( m_debugImage);
}

void PointLightRenderer2::Init()
{
	CRenderer::Init();
	CreateNearestSampler(Sampler);

	AllocDescriptorSets(m_descriptorPool, m_tileShadingDescLayout, &m_tileShadingDescSet);
	AllocDescriptorSets(m_descriptorPool, m_resolveDescLayout, &m_resolveDescSet);

	m_lightImage = CreateLightImage();
	m_debugImage = CreateLightImage();

	m_tileShadingPipeline.SetComputeShaderFile("tileshading.comp");
	m_tileShadingPipeline.CreatePipelineLayout(m_tileShadingDescLayout);
	m_tileShadingPipeline.Init(this, VK_NULL_HANDLE, -1);

	m_fullscreenQuad = CreateFullscreenQuad();

	m_resolvePipeline.SetVertexInputState(Mesh::GetVertexDesc());
	m_resolvePipeline.SetDepthWrite(VK_FALSE);
	m_resolvePipeline.SetDepthTest(VK_FALSE);
	m_resolvePipeline.SetVertexShaderFile("screenquad.vert");
	m_resolvePipeline.SetFragmentShaderFile("passtrough.frag");
	
	VkPipelineColorBlendAttachmentState blendState;
	cleanStructure(blendState);
	blendState.blendEnable = VK_TRUE;
	blendState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	blendState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
	blendState.colorBlendOp = VK_BLEND_OP_ADD;
	blendState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
		VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT;

	m_resolvePipeline.AddBlendState(blendState);

	m_resolvePipeline.CreatePipelineLayout(m_resolveDescLayout);
	m_resolvePipeline.Init(this, m_renderPass, 0);
	
}

void PointLightRenderer2::PreRender()
{
	UpdateLightsBuffer();
}

void PointLightRenderer2::Render()
{
	BeginMarkerSection("DeferredTileShading");
	PrepareLightImage();
	VkCommandBuffer cmdBuffer = vk::g_vulkanContext.m_mainCommandBuffer;
	const int gridCellSizeX = 16;
	const int gridCellSizeY = 16;
	const int gridCellsX = WIDTH / gridCellSizeX + ((WIDTH % gridCellSizeX != 0)? 1 : 0);
	const int gridCellsY = HEIGHT / gridCellSizeY + ((HEIGHT % gridCellSizeY != 0) ? 1 : 0);
	vk::CmdBindPipeline(cmdBuffer, m_tileShadingPipeline.GetBindPoint(), m_tileShadingPipeline.Get());
	vk::CmdBindDescriptorSets(cmdBuffer, m_tileShadingPipeline.GetBindPoint(), m_tileShadingPipeline.GetLayout(), 0, 1, &m_tileShadingDescSet, 0, nullptr);

	vk::CmdDispatch(cmdBuffer, gridCellsX, gridCellsY, 1);

	//here maybe we need a barrier to wait for compute to finish if the subpass dependecy doesnt work

	StartRenderPass();

	vk::CmdBindPipeline(cmdBuffer, m_resolvePipeline.GetBindPoint(), m_resolvePipeline.Get());
	vk::CmdBindDescriptorSets(cmdBuffer, m_resolvePipeline.GetBindPoint(), m_resolvePipeline.GetLayout(), 0, 1, &m_resolveDescSet, 0, nullptr);

	m_fullscreenQuad->Render();

	EndRenderPass();

	EndMarkerSection();
}

void PointLightRenderer2::InitializeLightGrid() //TODO
{
	int lightsPerRow = 8;
	int lightsPerCol = 8;
	for (int i = 0; i < lightsPerRow; ++i)
		for (int j = 0; j < lightsPerCol; ++j)
			m_lights.push_back(glm::vec3(float( 2 * (i -  lightsPerRow / 2)), 0.5f, -2.f + float(2 * (j - lightsPerCol / 2))));
}

void PointLightRenderer2::UpdateLightsBuffer()
{
	if (m_allocatedLights < m_lights.capacity())
		ReallocLightsBuffer();

	//update view matrix and all the lights
	void* ptr = m_lightsBuffer->GetPtr<void*>();
	//first must to update the global params
	GlobalParams* gParams = (GlobalParams*)ptr;
	gParams->InvProjectionMatrix = glm::inverse(m_proj);
	gParams->LightCount = glm::ivec4((uint32_t)m_lights.size());
	gParams->CameraPosition = glm::vec4(ms_camera.GetPos(), 1.0f);
	gParams->ViewMatrix = ms_camera.GetViewMatrix();

	//after the lights count a vector with variable lights should be filled
	PointLightParams* lightsStart = (PointLightParams*)((uint8_t*)gParams + sizeof(GlobalParams));
	glm::mat4 view = ms_camera.GetViewMatrix();

	for (unsigned int i = 0; i < m_lights.size(); ++i)
	{
		m_lights[i].Fill(lightsStart[i], view);
	}
}

void PointLightRenderer2::ReallocLightsBuffer()
{
	if (m_lightsBuffer != nullptr)
	{
		VkDevice dev = vk::g_vulkanContext.m_device;
		MemoryManager::GetInstance()->FreeHandle(m_lightsBuffer);

	}

	std::vector<VkDeviceSize> sizes((uint32_t)m_lights.capacity(), sizeof(PointLightParams));
	sizes.push_back(sizeof(GlobalParams));
	m_lightsBuffer = MemoryManager::GetInstance()->CreateBuffer(EMemoryContextType::UniformBuffers, sizes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	VkDescriptorBufferInfo lightsBufferInfo = m_lightsBuffer->GetDescriptor();
	VkWriteDescriptorSet wDesc = InitUpdateDescriptor(m_tileShadingDescSet, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &lightsBufferInfo);

	vk::UpdateDescriptorSets(vk::g_vulkanContext.m_device, 1, &wDesc, 0, nullptr);
}

ImageHandle* PointLightRenderer2::CreateLightImage()
{
	VkImageCreateInfo imgCrtInfo;
	cleanStructure(imgCrtInfo);
	imgCrtInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imgCrtInfo.pNext = nullptr;
	imgCrtInfo.flags = 0;
	imgCrtInfo.imageType = VK_IMAGE_TYPE_2D;
	imgCrtInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	imgCrtInfo.extent.width = WIDTH;
	imgCrtInfo.extent.height = HEIGHT;
	imgCrtInfo.extent.depth = 1;
	imgCrtInfo.mipLevels = 1;
	imgCrtInfo.arrayLayers = 1;
	imgCrtInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imgCrtInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imgCrtInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	imgCrtInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imgCrtInfo.queueFamilyIndexCount = 0;
	imgCrtInfo.pQueueFamilyIndices = NULL;
	
	return MemoryManager::GetInstance()->CreateImage(EMemoryContextType::Framebuffers, imgCrtInfo, "TileShadingOutImage");
}

void PointLightRenderer2::PrepareLightImage()
{
	static VkImageLayout currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	static VkAccessFlags currentAcces = 0;

	VkCommandBuffer cmdBuffer = vk::g_vulkanContext.m_mainCommandBuffer;

	//layout transition
	VkImageMemoryBarrier preClearBarrier[2];
	preClearBarrier[0] = m_lightImage->CreateMemoryBarrier(currentLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, currentAcces, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
	preClearBarrier[1] = m_debugImage->CreateMemoryBarrier(currentLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, currentAcces, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
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

	vk::CmdClearColorImage(cmdBuffer, m_lightImage->Get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clrValue, 1, &range); //clear color maybe ??
	vk::CmdClearColorImage(cmdBuffer, m_debugImage->Get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clrValue, 1, &range);

	VkImageMemoryBarrier preWriteBarrier[2];
	preWriteBarrier[0] = m_lightImage->CreateMemoryBarrier(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
		VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
	preWriteBarrier[1] = m_debugImage->CreateMemoryBarrier(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
		VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT);

	vk::CmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 0, nullptr, 2, preWriteBarrier);

	currentLayout = VK_IMAGE_LAYOUT_GENERAL;
	currentAcces = VK_ACCESS_SHADER_WRITE_BIT;
}

void PointLightRenderer2::CreateDescriptorSetLayout()
{
	{
		std::vector<VkDescriptorSetLayoutBinding> bindings;
		bindings.push_back(CreateDescriptorBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT));
		bindings.push_back(CreateDescriptorBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT));
		bindings.push_back(CreateDescriptorBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT));
		bindings.push_back(CreateDescriptorBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT));
		bindings.push_back(CreateDescriptorBinding(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT));
		bindings.push_back(CreateDescriptorBinding(5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT));
		bindings.push_back(CreateDescriptorBinding(6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT));
		bindings.push_back(CreateDescriptorBinding(7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT));

		NewDescriptorSetLayout(bindings, &m_tileShadingDescLayout);
	}
	{
		std::vector<VkDescriptorSetLayoutBinding> bindings;
		bindings.push_back(CreateDescriptorBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT));

		NewDescriptorSetLayout(bindings, &m_resolveDescLayout);
	}

}

void PointLightRenderer2::PopulatePoolInfo(std::vector<VkDescriptorPoolSize>& poolSize, unsigned int& maxSets)
{
	maxSets = 2;
	AddDescriptorType(poolSize, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2);
	AddDescriptorType(poolSize, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1);
	AddDescriptorType(poolSize, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 6);
}

void PointLightRenderer2::UpdateResourceTable()
{
}

void PointLightRenderer2::UpdateGraphicInterface()
{
	VkDescriptorImageInfo lightImageInfo = CreateDescriptorImageInfo(Sampler, m_lightImage->GetView(), VK_IMAGE_LAYOUT_GENERAL);
	VkDescriptorImageInfo debugImageInfo = CreateDescriptorImageInfo(Sampler, m_debugImage->GetView(), VK_IMAGE_LAYOUT_GENERAL);
	VkDescriptorImageInfo albedoImageInfo = CreateDescriptorImageInfo(Sampler, g_commonResources.GetAs<ImageHandle*>(EResourceType_AlbedoImage)->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	VkDescriptorImageInfo specularImageInfo = CreateDescriptorImageInfo(Sampler, g_commonResources.GetAs<ImageHandle*>(EResourceType_SpecularImage)->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	VkDescriptorImageInfo normalImageInfo = CreateDescriptorImageInfo(Sampler, g_commonResources.GetAs<ImageHandle*>(EResourceType_NormalsImage)->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	VkDescriptorImageInfo positionImageInfo = CreateDescriptorImageInfo(Sampler, g_commonResources.GetAs<ImageHandle*>(EResourceType_PositionsImage)->GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	VkDescriptorImageInfo depthImageInfo = CreateDescriptorImageInfo(Sampler, g_commonResources.GetAs<ImageHandle*>(EResourceType_DepthBufferImage)->GetView(), VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
	VkDescriptorImageInfo resolveLightImgInfo = CreateDescriptorImageInfo(Sampler, m_lightImage->GetView(), VK_IMAGE_LAYOUT_GENERAL);

	std::vector<VkWriteDescriptorSet> wDesc;
	wDesc.push_back(InitUpdateDescriptor(m_tileShadingDescSet, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &lightImageInfo));
	wDesc.push_back(InitUpdateDescriptor(m_tileShadingDescSet, 2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &debugImageInfo));
	wDesc.push_back(InitUpdateDescriptor(m_tileShadingDescSet, 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &albedoImageInfo));
	wDesc.push_back(InitUpdateDescriptor(m_tileShadingDescSet, 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &specularImageInfo));
	wDesc.push_back(InitUpdateDescriptor(m_tileShadingDescSet, 5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &normalImageInfo));
	wDesc.push_back(InitUpdateDescriptor(m_tileShadingDescSet, 6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &positionImageInfo));
	wDesc.push_back(InitUpdateDescriptor(m_tileShadingDescSet, 7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &depthImageInfo));
	wDesc.push_back(InitUpdateDescriptor(m_resolveDescSet, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &resolveLightImgInfo));

	vk::UpdateDescriptorSets(vk::g_vulkanContext.m_device, (uint32_t)wDesc.size(), wDesc.data(), 0, nullptr);
}
