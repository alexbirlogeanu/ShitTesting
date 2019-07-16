#include "GraphicEngine.h"

#include "Utils.h"
#include "MemoryManager.h"
#include "Framebuffer.h"
#include "RenderTaskGraph.h"
#include "Geometry.h"
#include "Input.h"
#include "Material.h"

#include "ao.h"
#include "TerrainRenderer.h"
#include "VegetationRenderer.h"
#include "ShadowRenderer.h"
#include "DirectionalLightRenderer.h"
#include "PostProcessRenderer.h"
#include "SunRenderer.h"
#include "SkyRenderer.h"

//include managers
#include "Texture.h"
#include "Batch.h"


void GraphicEngine::UpdateTaskGraph::Prepare()
{
	TopologicalSort();
}

//////////////////////////////////////////////////////////////////////////////////////////////
//GraphicEngine
//////////////////////////////////////////////////////////////////////////////////////////////
GraphicEngine::GraphicEngine()
	: m_currentBuffer(0)
	, m_queue(VK_NULL_HANDLE)
	, m_commandPool(VK_NULL_HANDLE)
	, m_mainCommandBuffer(VK_NULL_HANDLE)
	, m_aquireImageFence(VK_NULL_HANDLE)
	, m_renderFence(VK_NULL_HANDLE)
	, m_renderSemaphore(VK_NULL_HANDLE)
	, m_surface(VK_NULL_HANDLE)
	, m_swapChain(VK_NULL_HANDLE)
	, m_renderGraph(nullptr)
	, m_preRenderTaskGraph(nullptr)
	, m_updateTaskGraph(nullptr)
	, m_splitsAlphaFactor(0.15f)
{

}

GraphicEngine::~GraphicEngine()
{
	MaterialLibrary::DestroyInstance();
	BatchRenderer::DestroyInstance();
	ResourceLoader::DestroyInstance();
	CTextureManager::DestroyInstance();
	MeshManager::DestroyInstance();
	MemoryManager::DestroyInstance();

	VkDevice dev = vk::g_vulkanContext.m_device;
	delete m_renderGraph;

	vk::DestroySwapchainKHR(dev, m_swapChain, nullptr);
	vk::DestroyCommandPool(dev, m_commandPool, nullptr);
	vk::DestroyFence(dev, m_aquireImageFence, nullptr);
	vk::DestroyFence(dev, m_renderFence, nullptr);
	vk::DestroySemaphore(dev, m_renderSemaphore, nullptr);

	for (auto& att : m_framebufferAttachments)
		att.second->Destroy();
}

void GraphicEngine::Init(HWND windowHandle, HINSTANCE appInstance)
{
	MemoryManager::CreateInstance();
	MeshManager::CreateInstance();
	CTextureManager::CreateInstance();
	ResourceLoader::CreateInstance();
	MaterialLibrary::CreateInstance();
	BatchRenderer::CreateInstance();

	MaterialLibrary::GetInstance()->Initialize();

	CreateFramebufferAttachments();
	CreateSurface(windowHandle, appInstance);
	CreateSwapChains();
	GetQueue();
	CreateCommandBuffer();
	CreateSynchronizationHelpers();
	InitRenderers();
	InitRenderGraph();
	InitPreRenderGraph();
	InitUpdateGraph();
	InitInputMap();

}

void GraphicEngine::Update(float dt)
{
	m_activeCamera.Update();
}

void GraphicEngine::PreRender()
{
	MemoryManager::GetInstance()->MapMemoryContext(EMemoryContextType::UniformBuffers);
	m_preRenderTaskGraph->Execute();
	MemoryManager::GetInstance()->UnmapMemoryContext(EMemoryContextType::UniformBuffers);
}

void GraphicEngine::Render()
{
	StartFrame();

	m_updateTaskGraph->Execute(); //this can be removed and create a new task group that can be added to the renderGraph ?? its easier for now
	m_renderGraph->Execute();
	
	EndFrame();
}


AttachmentInfo* GraphicEngine::GetAttachment(const std::string& name)
{
	static auto& attachments = GetInstance()->m_framebufferAttachments;
	auto it = attachments.find(name);
	TRAP(it != attachments.end() && "Attachement doesn't exists");
	return it->second;
}

const FrameConstants&	GraphicEngine::GetFrameConstants()
{
	return GetInstance()->m_frameConstants;
}

CCamera& GraphicEngine::GetActiveCamera()
{
	return GetInstance()->m_activeCamera;
}

void GraphicEngine::CreateFramebufferAttachments()
{
	VkExtent3D defaultDimensions { WIDTH, HEIGHT, 1 };
	VkExtent3D shadowDimensions{ SHADOWW, SHADOWH, 1 };
	VkExtent3D halfDimensions{ WIDTH / 2, HEIGHT / 2, 1 };

	VkClearValue fZeroColor;
	cleanStructure(fZeroColor);
	fZeroColor.color.float32[0] = fZeroColor.color.float32[1] = fZeroColor.color.float32[2] = fZeroColor.color.float32[3] = .0f;

	VkClearValue fOneColor;
	cleanStructure(fOneColor);
	fOneColor.color.float32[0] = fOneColor.color.float32[1] = fOneColor.color.float32[2] = fZeroColor.color.float32[3] = 1.0f;

	VkClearValue depthClearValue;
	cleanStructure(depthClearValue);
	depthClearValue.depthStencil.depth = 1.0f;

	auto createAttachment = [&] (const std::string& name, const VkImageCreateInfo& info, VkClearValue clrValue)
	{
		ImageHandle* img = MemoryManager::GetInstance()->CreateImage(EMemoryContextType::Framebuffers, info, name + "Att");
		m_framebufferAttachments.emplace(name, new AttachmentInfo(name, img, clrValue));
	};

	//GBuffer creation
	createAttachment("Albedo", GetAttachmentCreateImageInfo(VK_FORMAT_R8G8B8A8_UNORM, defaultDimensions, 1, VK_IMAGE_USAGE_SAMPLED_BIT), fZeroColor);
	createAttachment("Specular", GetAttachmentCreateImageInfo(VK_FORMAT_R8G8B8A8_UNORM, defaultDimensions, 1, VK_IMAGE_USAGE_SAMPLED_BIT), fZeroColor);
	createAttachment("Normals", GetAttachmentCreateImageInfo(VK_FORMAT_R16G16B16A16_SFLOAT, defaultDimensions, 1, VK_IMAGE_USAGE_SAMPLED_BIT), fZeroColor);
	createAttachment("Positions", GetAttachmentCreateImageInfo(VK_FORMAT_R16G16B16A16_SFLOAT, defaultDimensions, 1, VK_IMAGE_USAGE_SAMPLED_BIT), fZeroColor);
	createAttachment("DefferedDebug", GetAttachmentCreateImageInfo(VK_FORMAT_R8G8B8A8_UNORM, defaultDimensions, 1, 0), fZeroColor);
	createAttachment("Depth", GetAttachmentCreateImageInfo(VK_FORMAT_D24_UNORM_S8_UINT, defaultDimensions, 1, VK_IMAGE_USAGE_SAMPLED_BIT), depthClearValue);

	//shadow map / shadow resolve
	createAttachment("ShadowMap", GetAttachmentCreateImageInfo(VK_FORMAT_D24_UNORM_S8_UINT, shadowDimensions, SHADOWSPLITS, VK_IMAGE_USAGE_SAMPLED_BIT), depthClearValue);
	createAttachment("ShadowResolveFinal", GetAttachmentCreateImageInfo(VK_FORMAT_R32_SFLOAT, defaultDimensions, 1, VK_IMAGE_USAGE_SAMPLED_BIT), fOneColor);
	createAttachment("ShadowResolveDebug", GetAttachmentCreateImageInfo(VK_FORMAT_R16G16B16A16_SFLOAT, defaultDimensions, 1, VK_IMAGE_USAGE_SAMPLED_BIT), fZeroColor);
	createAttachment("ShadowResolveBlur", GetAttachmentCreateImageInfo(VK_FORMAT_R32_SFLOAT, defaultDimensions, 1, VK_IMAGE_USAGE_SAMPLED_BIT), fOneColor);

	//AO 
	createAttachment("AOFinal", GetAttachmentCreateImageInfo(VK_FORMAT_R16_SFLOAT, defaultDimensions, 1, VK_IMAGE_USAGE_SAMPLED_BIT), fZeroColor);
	createAttachment("AODebug", GetAttachmentCreateImageInfo(VK_FORMAT_R16G16B16A16_SFLOAT, defaultDimensions, 1, VK_IMAGE_USAGE_SAMPLED_BIT), fZeroColor); //debug
	createAttachment("AOBlurAux", GetAttachmentCreateImageInfo(VK_FORMAT_R16_SFLOAT, defaultDimensions, 1, VK_IMAGE_USAGE_SAMPLED_BIT), fZeroColor);

	//lighting
	createAttachment("DirectionalLightingFinal", GetAttachmentCreateImageInfo(VK_FORMAT_R16G16B16A16_SFLOAT, defaultDimensions, 1, VK_IMAGE_USAGE_SAMPLED_BIT), fZeroColor);

	//Sun rendering
	createAttachment("SunFinal", GetAttachmentCreateImageInfo(VK_FORMAT_R16G16B16A16_SFLOAT, halfDimensions, 1, VK_IMAGE_USAGE_SAMPLED_BIT), fZeroColor);
	createAttachment("SunSprite", GetAttachmentCreateImageInfo(VK_FORMAT_R16G16B16A16_SFLOAT, halfDimensions, 1, VK_IMAGE_USAGE_SAMPLED_BIT), fZeroColor);
	createAttachment("SunBlur1", GetAttachmentCreateImageInfo(VK_FORMAT_R16G16B16A16_SFLOAT, halfDimensions, 1, VK_IMAGE_USAGE_SAMPLED_BIT), fZeroColor);
	createAttachment("SunBlur2", GetAttachmentCreateImageInfo(VK_FORMAT_R16G16B16A16_SFLOAT, halfDimensions, 1, VK_IMAGE_USAGE_SAMPLED_BIT), fZeroColor);

	//OUTPUT attachment
	//createAttachment("FinalColorImage", GetAttachmentCreateImageInfo(VK_FORMAT_R16G16B16A16_SFLOAT, defaultDimensions, 1, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT), fZeroColor);
	createAttachment("FinalColorImage", GetAttachmentCreateImageInfo(VK_FORMAT_B8G8R8A8_UNORM, defaultDimensions, 1, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT), fZeroColor);

}

VkImageCreateInfo  GraphicEngine::GetAttachmentCreateImageInfo(VkFormat format, VkExtent3D dimensions, uint32_t layers, VkImageUsageFlags additionalUsage)
{
	VkImageUsageFlags usage = (IsColorFormat(format))? VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT : VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

	VkImageCreateInfo imgInfo;
	cleanStructure(imgInfo);
	imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imgInfo.pNext = nullptr;
	imgInfo.flags = 0;
	imgInfo.imageType = VK_IMAGE_TYPE_2D;
	imgInfo.format = format;
	imgInfo.extent.height = dimensions.height;
	imgInfo.extent.width = dimensions.width;
	imgInfo.extent.depth = dimensions.depth;
	imgInfo.arrayLayers = layers;
	imgInfo.mipLevels = 1;
	imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imgInfo.usage = usage | additionalUsage;
	imgInfo.queueFamilyIndexCount = 0;
	imgInfo.pQueueFamilyIndices = nullptr;
	imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;

	return imgInfo;
}

void GraphicEngine::InitInputMap()
{
	InputManager::GetInstance()->MapKeysPressed({ '4','T', 'G' }, InputManager::KeyPressedCallback(this, &GraphicEngine::OnKeyPressed));

	std::vector<WPARAM> cameraKeys{ 'A', 'S', 'D', 'W', VK_SPACE };
	InputManager::GetInstance()->MapKeysPressed(cameraKeys, InputManager::KeyPressedCallback(&m_activeCamera, &CCamera::OnCameraKeyPressed));

	std::vector<WPARAM> dirLightKeys{ VK_UP, VK_DOWN, VK_RIGHT, VK_LEFT, 'P', VK_OEM_PLUS, VK_OEM_MINUS };
	InputManager::GetInstance()->MapKeysPressed(dirLightKeys, InputManager::KeyPressedCallback(&m_directionalLight, &CDirectionalLight::OnKeyboardPressed));
	InputManager::GetInstance()->MapMouseButton(InputManager::MouseButtonsCallback(&m_directionalLight, &CDirectionalLight::OnMouseEvent));
}

void GraphicEngine::InitRenderGraph()
{
	m_renderGraph = new RenderGraph();

	RenderTaskGroup* gbuffer = new RenderTaskGroup("GBuffer");
	RenderTaskGroup* shadowMap = new RenderTaskGroup("ShadowMap");
	RenderTaskGroup* preLighting = new RenderTaskGroup("PreLighting");
	RenderTaskGroup* lighting = new RenderTaskGroup("Lighting");
	RenderTaskGroup* postProcess = new RenderTaskGroup("PostProcess");
	RenderTaskGroup* sunPass = new RenderTaskGroup("SunRendering");
	RenderTaskGroup* skyPass = new RenderTaskGroup("SkyRendering");

	std::vector<AttachmentInfo*> gBufferAttachments = { GraphicEngine::GetAttachment("Albedo"),
		GraphicEngine::GetAttachment("Specular"),
		GraphicEngine::GetAttachment("Normals"),
		GraphicEngine::GetAttachment("Positions"),
		GraphicEngine::GetAttachment("Depth") };

	TerrainRenderer* terrainRenderer = static_cast<TerrainRenderer*>(m_renderers["TerrainRenderer"]);
	VegetationRenderer* vegRenderer = static_cast<VegetationRenderer*>(m_renderers["VegetationRenderer"]);
	ShadowResolveRenderer* shadowResRenderer = static_cast<ShadowResolveRenderer*>(m_renderers["ShadowResolveRenderer"]);
	AORenderer* aoRenderer = static_cast<AORenderer*>(m_renderers["AORenderer"]);
	LightRenderer* lightRenderer = static_cast<LightRenderer*>(m_renderers["LightRenderer"]);
	PostProcessRenderer* postRenderer = static_cast<PostProcessRenderer*>(m_renderers["PostProcessRenderer"]);
	SunRenderer* sunRenderer = static_cast<SunRenderer*>(m_renderers["SunRenderer"]);
	SkyRenderer* skyRenderer = static_cast<SkyRenderer*>(m_renderers["SkyRenderer"]);

	//GBuffer group
	gbuffer->AddTask(new RenderTask({},
		gBufferAttachments,
		GraphicEngine::GetAttachment("Depth"),
		std::bind(&TerrainRenderer::Render, terrainRenderer),
		std::bind(&TerrainRenderer::Setup, terrainRenderer, std::placeholders::_1, std::placeholders::_2)));

	gbuffer->AddTask(new RenderTask(gBufferAttachments,
		gBufferAttachments,
		GraphicEngine::GetAttachment("Depth"),
		std::bind(&VegetationRenderer::Render, vegRenderer),
		std::bind(&VegetationRenderer::Setup, vegRenderer, std::placeholders::_1, std::placeholders::_2)));

	gbuffer->AddTask(new RenderTask(gBufferAttachments,
		gBufferAttachments,
		GraphicEngine::GetAttachment("Depth"),
		std::bind(&BatchRenderer::RenderAll, BatchRenderer::GetInstance()),
		std::bind(&BatchRenderer::Setup, BatchRenderer::GetInstance(), std::placeholders::_1, std::placeholders::_2)));

	//Shadow map group
	shadowMap->AddTask(new RenderTask({}, //ins
		{ GraphicEngine::GetAttachment("ShadowMap") }, //outs
		GraphicEngine::GetAttachment("ShadowMap"),
		std::bind(&TerrainRenderer::RenderShadows, terrainRenderer), //exec
		std::bind(&TerrainRenderer::SetupShadows, terrainRenderer, std::placeholders::_1, std::placeholders::_2)));

	shadowMap->AddTask(new RenderTask({ GraphicEngine::GetAttachment("ShadowMap") }, //ins
		{ GraphicEngine::GetAttachment("ShadowMap") }, //outs
		{ GraphicEngine::GetAttachment("ShadowMap") },
		std::bind(&BatchRenderer::RenderShadows, BatchRenderer::GetInstance()), //exec
		std::bind(&BatchRenderer::SetupShadows, BatchRenderer::GetInstance(), std::placeholders::_1, std::placeholders::_2))); //setup

	//preLighting

	//AO MAIN
	preLighting->AddTask(new RenderTask({ GraphicEngine::GetAttachment("Normals"),  GraphicEngine::GetAttachment("Positions"), GraphicEngine::GetAttachment("Depth") }, //ins
	{ GraphicEngine::GetAttachment("AOFinal"), GraphicEngine::GetAttachment("AODebug") }, //outs
		nullptr,
		std::bind(&AORenderer::RenderMain, aoRenderer),
		std::bind(&AORenderer::SetupMainPass, aoRenderer, std::placeholders::_1, std::placeholders::_2)));
	//Shadow Resolve task
	preLighting->AddTask(new RenderTask({ GraphicEngine::GetAttachment("Normals"),  GraphicEngine::GetAttachment("Positions"), GraphicEngine::GetAttachment("ShadowMap"), GraphicEngine::GetAttachment("Depth") }, //ins
		{ GraphicEngine::GetAttachment("ShadowResolveFinal"), GraphicEngine::GetAttachment("ShadowResolveDebug") }, //outs
		nullptr,
		std::bind(&ShadowResolveRenderer::Render, shadowResRenderer),
		std::bind(&ShadowResolveRenderer::Setup, shadowResRenderer, std::placeholders::_1, std::placeholders::_2)));

	//AO blur
	preLighting->AddTask(new RenderTask({ GraphicEngine::GetAttachment("AOFinal") }, //ins
		{ GraphicEngine::GetAttachment("AOBlurAux") }, //outs
		nullptr,
		std::bind(&AORenderer::RenderHBlur, aoRenderer),
		std::bind(&AORenderer::SetupHBlurPass, aoRenderer, std::placeholders::_1, std::placeholders::_2)));

	preLighting->AddTask(new RenderTask({ GraphicEngine::GetAttachment("AOBlurAux") }, //ins
		{ GraphicEngine::GetAttachment("AOFinal") }, //outs
		nullptr,
		std::bind(&AORenderer::RenderVBlur, aoRenderer),
		std::bind(&AORenderer::SetupVBlurPass, aoRenderer, std::placeholders::_1, std::placeholders::_2)));

	//lighting
	lighting->AddTask(new RenderTask({ GraphicEngine::GetAttachment("Albedo"), GraphicEngine::GetAttachment("Specular"), GraphicEngine::GetAttachment("Normals"),  GraphicEngine::GetAttachment("Positions"), GraphicEngine::GetAttachment("ShadowResolveFinal"),  GraphicEngine::GetAttachment("AOFinal") },
		{ GraphicEngine::GetAttachment("DirectionalLightingFinal"), GraphicEngine::GetAttachment("DefferedDebug") },
		nullptr,
		std::bind(&LightRenderer::Render, lightRenderer),
		std::bind(&LightRenderer::Setup, lightRenderer, std::placeholders::_1, std::placeholders::_2)));

	//sun renderpass
	sunPass->AddTask(new RenderTask(
		{ GraphicEngine::GetAttachment("Depth") },
		{ GraphicEngine::GetAttachment("SunSprite") },
		nullptr,
		std::bind(&SunRenderer::RenderSunSubpass, sunRenderer),
		std::bind(&SunRenderer::SetupSunSubpass, sunRenderer, std::placeholders::_1, std::placeholders::_2)));

	sunPass->AddTask(new RenderTask(
		{ GraphicEngine::GetAttachment("SunSprite") },
		{ GraphicEngine::GetAttachment("SunBlur1") },
		nullptr,
		std::bind(&SunRenderer::RenderBlurVSubpass, sunRenderer),
		std::bind(&SunRenderer::SetupBlurVSubpass, sunRenderer, std::placeholders::_1, std::placeholders::_2)));

	sunPass->AddTask(new RenderTask(
		{ GraphicEngine::GetAttachment("SunBlur1") },
		{ GraphicEngine::GetAttachment("SunBlur2") },
		nullptr,
		std::bind(&SunRenderer::RenderBlurHSubpass, sunRenderer),
		std::bind(&SunRenderer::SetupBlurHSubpass, sunRenderer, std::placeholders::_1, std::placeholders::_2)));

	sunPass->AddTask(new RenderTask(
		{ GraphicEngine::GetAttachment("SunBlur2") },
		{ GraphicEngine::GetAttachment("SunFinal") },
		nullptr,
		std::bind(&SunRenderer::RenderRadialBlurSubpass, sunRenderer),
		std::bind(&SunRenderer::SetupBlurRadialSubpass, sunRenderer, std::placeholders::_1, std::placeholders::_2)));

	skyPass->AddTask(new RenderTask({ GraphicEngine::GetAttachment("DirectionalLightingFinal") },
		{ GraphicEngine::GetAttachment("DirectionalLightingFinal") },
		GraphicEngine::GetAttachment("Depth"),
		std::bind(&SkyRenderer::RenderSkyboxPass, skyRenderer),
		std::bind(&SkyRenderer::SetupSkyBoxSubpass, skyRenderer, std::placeholders::_1, std::placeholders::_2)));

	skyPass->AddTask(new RenderTask({ GraphicEngine::GetAttachment("DirectionalLightingFinal"), GraphicEngine::GetAttachment("SunFinal") },
		{ GraphicEngine::GetAttachment("DirectionalLightingFinal") },
		nullptr,
		std::bind(&SkyRenderer::BlendSunPass, skyRenderer),
		std::bind(&SkyRenderer::SetupBlendSunSubpass, skyRenderer, std::placeholders::_1, std::placeholders::_2)));

	//Post process	
	postProcess->AddTask(new RenderTask({ GraphicEngine::GetAttachment("DirectionalLightingFinal") }, //inputs
		{ GraphicEngine::GetAttachment("FinalColorImage") },
		nullptr,
		std::bind(&PostProcessRenderer::Render, postRenderer),
		std::bind(&PostProcessRenderer::Setup, postRenderer, std::placeholders::_1, std::placeholders::_2)));

	m_renderGraph->AddTaskGroup(gbuffer);
	m_renderGraph->AddTaskGroup(shadowMap);
	m_renderGraph->AddTaskGroup(preLighting);
	m_renderGraph->AddTaskGroup(lighting);
	m_renderGraph->AddTaskGroup(sunPass);
	m_renderGraph->AddTaskGroup(skyPass);
	m_renderGraph->AddTaskGroup(postProcess);

	m_renderGraph->Init();
}

void GraphicEngine::InitPreRenderGraph()
{
	m_preRenderTaskGraph = new UpdateTaskGraph();
	UpdateTaskGroup* highPrio = new UpdateTaskGroup("PreRenderHigh");
	UpdateTaskGroup* normalPrio = new UpdateTaskGroup("PreRenderNormal");

	highPrio->AddTask(new Task(std::bind(&GraphicEngine::UpdateFrameConstants, GetInstance())));

	normalPrio->AddTask(new Task(std::bind(&BatchRenderer::PreRender, BatchRenderer::GetInstance())));
	normalPrio->AddTask(new Task(std::bind(&VegetationRenderer::PreRender, static_cast<VegetationRenderer*>(m_renderers["VegetationRenderer"]))));
	normalPrio->AddTask(new Task(std::bind(&TerrainRenderer::PreRender, static_cast<TerrainRenderer*>(m_renderers["TerrainRenderer"]))));
	normalPrio->AddTask(new Task(std::bind(&ShadowResolveRenderer::PreRender, static_cast<ShadowResolveRenderer*>(m_renderers["ShadowResolveRenderer"]))));
	normalPrio->AddTask(new Task(std::bind(&AORenderer::PreRender, static_cast<AORenderer*>(m_renderers["AORenderer"]))));
	normalPrio->AddTask(new Task(std::bind(&LightRenderer::PreRender, static_cast<LightRenderer*>(m_renderers["LightRenderer"]))));
	normalPrio->AddTask(new Task(std::bind(&PostProcessRenderer::UpdateShaderParams, static_cast<PostProcessRenderer*>(m_renderers["PostProcessRenderer"]))));
	normalPrio->AddTask(new Task(std::bind(&SunRenderer::PreRender, static_cast<SunRenderer*>(m_renderers["SunRenderer"]))));
	normalPrio->AddTask(new Task(std::bind(&SkyRenderer::PreRender, static_cast<SkyRenderer*>(m_renderers["SkyRenderer"]))));

	normalPrio->AddDependencies({ highPrio });

	m_preRenderTaskGraph->AddTaskGroup(highPrio);
	m_preRenderTaskGraph->AddTaskGroup(normalPrio);
	m_preRenderTaskGraph->Prepare();
}

void GraphicEngine::InitUpdateGraph()
{
	m_updateTaskGraph = new UpdateTaskGraph();
	UpdateTaskGroup* normalPrio = new UpdateTaskGroup("UpdateNormal");

	normalPrio->AddTask(new Task(std::bind(&CTextureManager::Update, CTextureManager::GetInstance()))); //50000 paranthesis
	normalPrio->AddTask(new Task(std::bind(&MeshManager::Update, MeshManager::GetInstance())));
	normalPrio->AddTask(new Task(std::bind(&BatchRenderer::Update, BatchRenderer::GetInstance())));

	m_updateTaskGraph->AddTaskGroup(normalPrio);
	m_updateTaskGraph->Prepare();
}

void GraphicEngine::InitRenderers()
{
	m_renderers.emplace("TerrainRenderer", new TerrainRenderer());
	m_renderers.emplace("VegetationRenderer", new VegetationRenderer());
	m_renderers.emplace("BatchRenderer", BatchRenderer::GetInstance());
	m_renderers.emplace("ShadowResolveRenderer", new ShadowResolveRenderer());
	m_renderers.emplace("AORenderer", new AORenderer());
	m_renderers.emplace("LightRenderer", new LightRenderer());
	m_renderers.emplace("PostProcessRenderer", new PostProcessRenderer());
	m_renderers.emplace("SunRenderer", new SunRenderer());
	m_renderers.emplace("SkyRenderer", new SkyRenderer());

	MemoryManager::GetInstance()->MapMemoryContext(EMemoryContextType::UniformBuffers);

	for (auto& it : m_renderers)
		it.second->Init();

	MemoryManager::GetInstance()->UnmapMemoryContext(EMemoryContextType::UniformBuffers);

}

void GraphicEngine::CreateCommandBuffer()
{
	VkCommandPoolCreateInfo cmdPoolCi;
	cleanStructure(cmdPoolCi);
	cmdPoolCi.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cmdPoolCi.pNext = nullptr;
	cmdPoolCi.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	cmdPoolCi.queueFamilyIndex = vk::g_vulkanContext.m_queueFamilyIndex;

	VULKAN_ASSERT(vk::CreateCommandPool(vk::g_vulkanContext.m_device, &cmdPoolCi, nullptr, &m_commandPool));

	VkCommandBufferAllocateInfo cmdAlocInfo;
	cleanStructure(cmdAlocInfo);
	cmdAlocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cmdAlocInfo.pNext = nullptr;
	cmdAlocInfo.commandPool = m_commandPool;
	cmdAlocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cmdAlocInfo.commandBufferCount = 1;

	VULKAN_ASSERT(vk::AllocateCommandBuffers(vk::g_vulkanContext.m_device, &cmdAlocInfo, &m_mainCommandBuffer));

	vk::g_vulkanContext.m_mainCommandBuffer = m_mainCommandBuffer;
}

void GraphicEngine::CreateSurface(HWND windowHandle, HINSTANCE appInstance)
{
	VkWin32SurfaceCreateInfoKHR surfCrtInfo;
	cleanStructure(surfCrtInfo);
	surfCrtInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	surfCrtInfo.flags = 0;
	surfCrtInfo.pNext = nullptr;
	surfCrtInfo.hinstance = appInstance;
	surfCrtInfo.hwnd = windowHandle;

	VkResult result = vk::CreateWin32SurfaceKHR(vk::g_vulkanContext.m_instance, &surfCrtInfo, nullptr, &m_surface);
	TRAP(result >= VK_SUCCESS);
	VkBool32 support;
	vk::GetPhysicalDeviceSurfaceSupportKHR(vk::g_vulkanContext.m_physicalDevice, vk::g_vulkanContext.m_queueFamilyIndex, m_surface, &support);
	TRAP(support);
}

void GraphicEngine::CreateSwapChains()
{
	TRAP(m_surface != VK_NULL_HANDLE);
	vk::SVUlkanContext& context = vk::g_vulkanContext;
	VkPhysicalDevice physDevice = context.m_physicalDevice;

	uint32_t modesCnt;
	std::vector<VkPresentModeKHR> presentModes;
	vk::GetPhysicalDeviceSurfacePresentModesKHR(physDevice, m_surface, &modesCnt, nullptr);
	presentModes.resize(modesCnt);
	vk::GetPhysicalDeviceSurfacePresentModesKHR(physDevice, m_surface, &modesCnt, presentModes.data());

	unsigned int formatCnt;
	VULKAN_ASSERT(vk::GetPhysicalDeviceSurfaceFormatsKHR(physDevice, m_surface, &formatCnt, nullptr));

	std::vector<VkSurfaceFormatKHR> formats;
	formats.resize(formatCnt);
	VULKAN_ASSERT(vk::GetPhysicalDeviceSurfaceFormatsKHR(physDevice, m_surface, &formatCnt, formats.data()));
	VkSurfaceFormatKHR formatUsed = formats[1];

	//TRAP(OUT_FORMAT == formatUsed.format);

	VkSurfaceCapabilitiesKHR capabilities;
	VULKAN_ASSERT(vk::GetPhysicalDeviceSurfaceCapabilitiesKHR(physDevice, m_surface, &capabilities));
	
	VkExtent2D extent = capabilities.currentExtent;
	
	VkSwapchainCreateInfoKHR swapChainCrtInfo;
	cleanStructure(swapChainCrtInfo);
	swapChainCrtInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapChainCrtInfo.pNext = nullptr;
	swapChainCrtInfo.flags = 0;
	swapChainCrtInfo.surface = m_surface;
	swapChainCrtInfo.minImageCount = 2;
	swapChainCrtInfo.imageFormat = formatUsed.format;
	swapChainCrtInfo.imageColorSpace = formatUsed.colorSpace;
	swapChainCrtInfo.imageExtent = extent;
	swapChainCrtInfo.imageArrayLayers = 1;
	swapChainCrtInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	swapChainCrtInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	swapChainCrtInfo.queueFamilyIndexCount = 1;
	swapChainCrtInfo.pQueueFamilyIndices = &context.m_queueFamilyIndex;
	swapChainCrtInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	swapChainCrtInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapChainCrtInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
	swapChainCrtInfo.clipped = VK_TRUE;
	swapChainCrtInfo.oldSwapchain = VK_NULL_HANDLE;

	VULKAN_ASSERT(vk::CreateSwapchainKHR(context.m_device, &swapChainCrtInfo, nullptr, &m_swapChain));

	unsigned int imageCnt;
	VULKAN_ASSERT(vk::GetSwapchainImagesKHR(vk::g_vulkanContext.m_device, m_swapChain, &imageCnt, nullptr));
	TRAP(imageCnt == 2);
	m_presentImages.resize(imageCnt);
	VULKAN_ASSERT(vk::GetSwapchainImagesKHR(vk::g_vulkanContext.m_device, m_swapChain, &imageCnt, m_presentImages.data()));
}

void GraphicEngine::CreateSynchronizationHelpers()
{
	VkFenceCreateInfo fenceCreateInfo;
	cleanStructure(fenceCreateInfo);
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.pNext = nullptr;
	fenceCreateInfo.flags = 0;

	VULKAN_ASSERT(vk::CreateFence(vk::g_vulkanContext.m_device, &fenceCreateInfo, nullptr, &m_aquireImageFence));
	VULKAN_ASSERT(vk::CreateFence(vk::g_vulkanContext.m_device, &fenceCreateInfo, nullptr, &m_renderFence));

	VkSemaphoreCreateInfo semaphoreCreateInfo;
	cleanStructure(semaphoreCreateInfo);
	semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	semaphoreCreateInfo.pNext = nullptr;
	semaphoreCreateInfo.flags = 0;

	VULKAN_ASSERT(vk::CreateSemaphore(vk::g_vulkanContext.m_device, &semaphoreCreateInfo, nullptr, &m_renderSemaphore));
}

void GraphicEngine::GetQueue()
{
	vk::GetDeviceQueue(vk::g_vulkanContext.m_device, vk::g_vulkanContext.m_queueFamilyIndex, 0, &m_queue);
	TRAP(m_queue != VK_NULL_HANDLE);

	vk::g_vulkanContext.m_graphicQueue = m_queue;
}

void GraphicEngine::StartFrame()
{
	vk::AcquireNextImageKHR(vk::g_vulkanContext.m_device, m_swapChain, 0, VK_NULL_HANDLE, m_aquireImageFence, &m_currentBuffer);
	vk::WaitForFences(vk::g_vulkanContext.m_device, 1, &m_aquireImageFence, VK_TRUE, UINT64_MAX);
	vk::ResetFences(vk::g_vulkanContext.m_device, 1, &m_aquireImageFence);

	StartCommandBuffer();
}

void GraphicEngine::EndFrame()
{
	TransferToPresentImage();

	EndCommandBuffer();

	VkSubmitInfo submitInfo;
	cleanStructure(submitInfo);
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.pNext = nullptr;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &m_mainCommandBuffer;
	VULKAN_ASSERT(vk::QueueSubmit(m_queue, 1, &submitInfo, m_renderFence));

	vk::WaitForFences(vk::g_vulkanContext.m_device, 1, &m_renderFence, VK_TRUE, UINT64_MAX);
	vk::ResetFences(vk::g_vulkanContext.m_device, 1, &m_renderFence);

	VkPresentInfoKHR presentInfo;
	cleanStructure(presentInfo);
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.pNext = nullptr;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &m_swapChain;
	presentInfo.pImageIndices = &m_currentBuffer;

	VULKAN_ASSERT(vk::QueuePresentKHR(m_queue, &presentInfo));
}

void GraphicEngine::TransferToPresentImage()
{
	BeginMarkerSection("CopyBackBuffer");
	VkDevice dev = vk::g_vulkanContext.m_device;

	VkImage presentImg = m_presentImages[m_currentBuffer];
	ImageHandle* finalImage = GetAttachment("FinalColorImage")->GetHandle();

	VkImageSubresourceLayers subRes;
	subRes.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subRes.mipLevel = 0;
	subRes.baseArrayLayer = 0;
	subRes.layerCount = 1;

	VkOffset3D offset;
	offset.x = offset.y = offset.z = 0;

	VkExtent3D extent;
	extent.width = WIDTH;
	extent.height = HEIGHT;
	extent.depth = 1;

	VkImageCopy imgCopy;
	imgCopy.srcSubresource = subRes;
	imgCopy.srcOffset = offset;
	imgCopy.dstSubresource = subRes;
	imgCopy.dstOffset = offset;
	imgCopy.extent = extent;

	std::vector<VkImageMemoryBarrier> preCopyBarrier;
	preCopyBarrier.resize(2);
	AddImageBarrier(preCopyBarrier[0], presentImg, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
	preCopyBarrier[1] = finalImage->CreateMemoryBarrier(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT);

	vk::CmdPipelineBarrier(m_mainCommandBuffer,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_DEPENDENCY_BY_REGION_BIT,
		0,
		nullptr,
		0,
		nullptr,
		preCopyBarrier.size(),
		preCopyBarrier.data());

	vk::CmdCopyImage(m_mainCommandBuffer, finalImage->Get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, presentImg, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imgCopy);

	VkImageMemoryBarrier prePresentBarrier;
	AddImageBarrier(prePresentBarrier, presentImg, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT);

	vk::CmdPipelineBarrier(m_mainCommandBuffer,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_HOST_BIT,
		VK_DEPENDENCY_BY_REGION_BIT,
		0,
		nullptr,
		0,
		nullptr,
		1,
		&prePresentBarrier);

	EndMarkerSection();
}

void GraphicEngine::StartCommandBuffer()
{
	VkCommandBufferBeginInfo bufferBeginInfo;
	cleanStructure(bufferBeginInfo);
	bufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	vk::BeginCommandBuffer(m_mainCommandBuffer, &bufferBeginInfo);
}

void GraphicEngine::EndCommandBuffer()
{
	vk::EndCommandBuffer(m_mainCommandBuffer);
}

void GraphicEngine::UpdateFrameConstants()
{
	m_frameConstants.DirectionalLightDirection = m_directionalLight.GetDirection();
	m_frameConstants.DirectionaLightIrradiance = m_directionalLight.GetLightIradiance();
	m_frameConstants.DirectionalLightIntensity = m_directionalLight.GetLightIntensity();
	m_frameConstants.ViewMatrix = m_activeCamera.GetViewMatrix();

	PerspectiveMatrix(m_frameConstants.ProjMatrix);
	ConvertToProjMatrix(m_frameConstants.ProjMatrix);
	m_frameConstants.ProjViewMatrix = m_frameConstants.ProjMatrix * m_frameConstants.ViewMatrix;

	ComputeCascadeSplitMatrices();
}

void GraphicEngine::ComputeCascadeSplitMatrices()
{
	//float alpha = 0.15f;
	float cameraNear = m_activeCamera.GetNear();
	float cameraFar = m_activeCamera.GetFar();

	float splitNumbers = float(SHADOWSPLITS);
	float splitFar = 0;
	float splitNear = cameraNear;

	auto linearizeDepth = [](float z)
	{
		float n = 0.01f;
		float f = 75.0f;
		return (2 * n) / (f + n - z * (f - n));
	};

	glm::vec3 j = glm::vec3(.0f, 1.0f, 0.0f);
	glm::vec3 lightDir = glm::vec3(m_directionalLight.GetDirection());
	glm::vec3 lightRight = glm::normalize(glm::cross(lightDir, j));
	glm::vec3 lightUp = glm::normalize(glm::cross(lightRight, lightUp));

	glm::mat4  initialProj = glm::ortho(-20.f, 20.1f, -11.f, 11.5f, 0.01f, 20.0f);
	ConvertToProjMatrix(initialProj);

	for (uint32_t s = 0; s < SHADOWSPLITS; ++s)
	{
		float splitIndex = float(s + 1.0f);//for split index we have zi as near plane and zi+1 as far plane
		glm::mat4 view;
		glm::mat4 proj;

		splitFar = m_splitsAlphaFactor * cameraNear * glm::pow(cameraFar / cameraNear, splitIndex / splitNumbers) + (1.0f - m_splitsAlphaFactor) * (cameraNear + (splitIndex / splitNumbers) * (cameraFar - cameraNear));

		CFrustum frustum(splitNear, splitFar);
		frustum.Update(m_activeCamera.GetPos(), m_activeCamera.GetFrontVector(), m_activeCamera.GetUpVector(), m_activeCamera.GetRightVector(), m_activeCamera.GetFOV()); //in worldspace

		ComputeCascadeViewMatrix(frustum, lightDir, lightUp, view);
		ComputeCascadeProjMatrix(frustum, view, initialProj, proj);

		//I will explain this for the future self, to not make stupid faces while you're trying to understand. its the same as:
		// projVec4 = ProjMatrix * viewPosVector4; //but we do the multiply only for z component
		//projVec4.z /= projVec4.w;
		//and then we bring z from -1, 1 -> 0, 1 because ProjMatrix is the old opengl format with z between -1, 1, vulkan changed that
		/*
		expanded fomulae:
		z = -splitFar * (-(cameraFar + cameraNear) / (cameraFar - cameraNear) - (2 * cameraFar * cameraNear) /(cameraFar - cameraNear);
		w = (-1) * (-splitFar);

		projZ = z / w;
		its -splitFar  beacause in Vulkan camera points towards the -z axes
		*/

		float projectedZ = ((splitFar * (cameraFar + cameraNear) - 2.0f * (cameraFar * cameraNear)) / (cameraFar - cameraNear)) / splitFar;
		projectedZ = projectedZ * 0.5f + 0.5f;

		m_frameConstants.ShadowSplits[s].NearFar = glm::vec4(splitNear, linearizeDepth(projectedZ), 0.0f, 0.0f);
		m_frameConstants.ShadowSplits[s].ProjViewMatrix = proj * view;

		splitNear = splitFar;
	}

	m_frameConstants.NumberOfShadowSplits = SHADOWSPLITS;
}

void GraphicEngine::ComputeCascadeViewMatrix(const CFrustum& splitFrustrum, const glm::vec3& lightDir, const glm::vec3& lightUp, glm::mat4& outView)
{
	const float kLightCameraDist = 2.5f;
	glm::vec3 wpMin = glm::vec3(std::numeric_limits<float>::max());
	glm::vec3 wpMax = glm::vec3(std::numeric_limits<float>::min());

	for (unsigned int i = 0; i < CFrustum::FPCount; ++i)
	{
		wpMin = glm::min(wpMin, splitFrustrum.GetPoint(i));
		wpMax = glm::max(wpMax, splitFrustrum.GetPoint(i));
	}

	BoundingBox3D bb(wpMin, wpMax);
	glm::vec3 lightCameraEye = bb.GetNegativeVertex(lightDir) - lightDir * kLightCameraDist;
	//glm::vec3 lightCameraEye = (wpMin + wpMax) / 2.0f - lightDir * kLightCameraDist;

	outView = glm::lookAt(lightCameraEye, lightCameraEye + lightDir, lightUp); //this up vector for me doesnt seem right
}

void GraphicEngine::ComputeCascadeProjMatrix(const CFrustum& splitFrustum, const glm::mat4& lightViewMatrix, const glm::mat4& lightProjMatrix, glm::mat4& outCroppedProjMatrix)
{
	glm::mat4 PV = lightProjMatrix * lightViewMatrix;
	glm::vec3 minLimits = glm::vec3(std::numeric_limits<float>::max());
	glm::vec3 maxLimits = glm::vec3(std::numeric_limits<float>::min());

	for (unsigned int i = 0; i < CFrustum::FPCount; ++i)
	{
		glm::vec4 lightPos = PV * glm::vec4(splitFrustum.GetPoint(i), 1.0f);
		minLimits = glm::min(minLimits, glm::vec3(lightPos));
		maxLimits = glm::max(maxLimits, glm::vec3(lightPos));
	}
	minLimits.z = 0.0f; //to capture all the objects of the scene even if they are out of camera

	float scaleX = 2.0f / (maxLimits.x - minLimits.x);
	float scaleY = 2.0f / (maxLimits.y - minLimits.y);
	float offsetX = (-0.5f) * (maxLimits.x + minLimits.x) * scaleX;
	float offsetY = (-0.5f) * (maxLimits.y + minLimits.y) * scaleY;
	float scaleZ = 1.0f / (maxLimits.z - minLimits.z);
	float offsetZ = -minLimits.z * scaleZ;

	glm::mat4 C(glm::vec4(scaleX, 0.0f, 0.0f, 0.0f),
		glm::vec4(0.0f, scaleY, 0.0f, 0.0f),
		glm::vec4(0.0f, 0.0f, scaleZ, 0.0f),
		glm::vec4(offsetX, offsetY, offsetZ, 1.0f)
	);

	outCroppedProjMatrix = C * lightProjMatrix;
}

bool GraphicEngine::OnKeyPressed(const KeyInput& key)
{
	/*std::string debugText = "4 - close; T/G increase/decrease alpha. Alpha: ";

	if (key.IsKeyPressed('4'))
	{
		m_isShadowDebugMode = !m_isShadowDebugMode;
		if (m_isShadowDebugMode)
			m_shadowDebugText = CUIManager::GetInstance()->CreateTextItem(debugText + std::to_string(m_splitsAlphaFactor), glm::uvec2(10, 50));
		else
			CUIManager::GetInstance()->DestroyTextItem(m_shadowDebugText);

		return true;
	}

	if (!m_isShadowDebugMode)
		return false;


	if (key.IsKeyPressed('T'))
	{
		m_splitsAlphaFactor = glm::min(m_splitsAlphaFactor + 0.05f, 0.95f);
		m_shadowDebugText->SetText(debugText + std::to_string(m_splitsAlphaFactor));
		return true;
	}
	else if (key.IsKeyPressed('G'))
	{
		m_splitsAlphaFactor = glm::max(m_splitsAlphaFactor - 0.05f, 0.05f);
		m_shadowDebugText->SetText(debugText + std::to_string(m_splitsAlphaFactor));
		return true;
	}*/

	return false;
}