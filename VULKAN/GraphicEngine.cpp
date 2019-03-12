#include "GraphicEngine.h"

#include "Utils.h"
#include "MemoryManager.h"
#include "Framebuffer.h"
#include "RenderTaskGraph.h"

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
{

}

GraphicEngine::~GraphicEngine()
{
	delete m_renderGraph;

	for (auto& att : m_framebufferAttachments)
		att.second->Destroy();
}

void GraphicEngine::Init(HWND windowHandle, HINSTANCE appInstance)
{
	CreateFramebufferAttachments();
	CreateCommandBuffer();
	InitRenderGraph();
}

void GraphicEngine::Render()
{
	m_renderGraph->Execute();
}


AttachmentInfo* GraphicEngine::GetAttachment(const std::string& name)
{
	static auto& attachments = GetInstance()->m_framebufferAttachments;
	auto it = attachments.find(name);
	TRAP(it != attachments.end() && "Attachement doesn't exists");
	return it->second;
}

void GraphicEngine::CreateFramebufferAttachments()
{
	VkExtent3D defaultDimensions { WIDTH, HEIGHT, 1 };
	VkExtent3D shadowDimensions{ SHADOWW, SHADOWH, 1 };

	VkClearValue fZeroColor;
	cleanStructure(fZeroColor);
	fZeroColor.color.float32[0] = fZeroColor.color.float32[1] = fZeroColor.color.float32[2] = fZeroColor.color.float32[3] = .0f;

	VkClearValue fOneColor;
	cleanStructure(fOneColor);
	fOneColor.color.float32[0] = fOneColor.color.float32[1] = fOneColor.color.float32[2] =  1.0f;
	fZeroColor.color.float32[3] = .0f;

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
	createAttachment("DefferedDebug", GetAttachmentCreateImageInfo(VK_FORMAT_R8G8B8A8_UNORM, defaultDimensions, 1), fZeroColor);
	createAttachment("Depth", GetAttachmentCreateImageInfo(VK_FORMAT_D24_UNORM_S8_UINT, defaultDimensions, VK_IMAGE_USAGE_SAMPLED_BIT), depthClearValue);

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

	//OUTPUT attachment
	createAttachment("FinalColorImage", GetAttachmentCreateImageInfo(VK_FORMAT_R16G16B16A16_SFLOAT, defaultDimensions, 1, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT), fZeroColor);
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

void GraphicEngine::InitRenderGraph()
{
	m_renderGraph = new RenderGraph();
	m_renderGraph->Init();
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

void GraphicEngine::TransferToPresentImage()
{
	BeginMarkerSection("CopyBackBuffer");
	VkDevice dev = vk::g_vulkanContext.m_device;

	VkImage presentImg = m_presentImages[m_currentBuffer];
	ImageHandle* finalImage = GetAttachment("Albedo")->GetHandle();

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
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
	preCopyBarrier[1] = finalImage->CreateMemoryBarrier(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT);

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
