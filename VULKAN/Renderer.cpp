#include "Renderer.h"

ResourceTable   g_commonResources;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//CFrameBuffer
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CFrameBuffer::CFrameBuffer(unsigned int width
    , unsigned int height
    , unsigned int layers)

    : m_frameBuffer(VK_NULL_HANDLE)
    , m_width(width)
    , m_height(height)
    , m_layers(layers)
{       

}

CFrameBuffer::~CFrameBuffer()
{
    VkDevice dev = vk::g_vulkanContext.m_device;

    for(auto att = m_attachments.begin(); att != m_attachments.end(); ++att)
        att->Clean(dev);

    m_attachments.clear();

    vk::DestroyFramebuffer(dev, m_frameBuffer, nullptr);
}

VkRect2D CFrameBuffer::GetRenderArea() 
{
    VkRect2D renderArea;
    renderArea.offset.x = 0;
    renderArea.offset.y = 0;
    renderArea.extent.height = m_height;
    renderArea.extent.width = m_width;

    return renderArea;
}

VkImageCreateInfo CFrameBuffer::CreateImageInfo(const FBAttachment& fbDesc)
{
	VkImageCreateInfo imgInfo;
    cleanStructure(imgInfo);
    imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imgInfo.pNext = nullptr;
    imgInfo.flags = 0;
    imgInfo.imageType = VK_IMAGE_TYPE_2D;
	imgInfo.format = fbDesc.format;
    imgInfo.extent.height = m_height;
    imgInfo.extent.width = m_width;
    imgInfo.extent.depth = 1;
	imgInfo.arrayLayers = fbDesc.layers;
    imgInfo.mipLevels = 1;
    imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imgInfo.usage = fbDesc.usage;
    imgInfo.queueFamilyIndexCount = 0;
    imgInfo.pQueueFamilyIndices = nullptr;
    imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;

	return imgInfo;
}

void CFrameBuffer::CreateFramebuffer(VkRenderPass renderPass, const FramebufferDescription& fbDesc)
{
    m_colorsAttNum = fbDesc.m_numColors;
    m_attachments.resize(m_colorsAttNum);
    m_clearValues.resize(m_colorsAttNum);

    const std::vector<FBAttachment>& colorAtt = fbDesc.m_colorAttachments;
    for(unsigned int i = 0; i < m_colorsAttNum; ++i)
    {
        VkImageCreateInfo imageInfo =  CreateImageInfo(colorAtt[i]);
        m_clearValues[i] = colorAtt[i].clearValue;

        if(colorAtt[i].NeedCreateImageView())
        {
            std::string debugName;
            if (!colorAtt[i].debugName.empty())
                debugName = colorAtt[i].debugName + std::string("ColorAtt");

			m_attachments[i].SetImage(MemoryManager::GetInstance()->CreateImage(EMemoryContextType::Framebuffers, imageInfo, debugName), true);
        }
        else
        {
            m_attachments[i].SetImage(colorAtt[i].existingImage, false);
        }
    }

    if(fbDesc.m_depthAttachments.IsValid())
    {
		VkImageCreateInfo imageInfo = CreateImageInfo(fbDesc.m_depthAttachments);
        if(fbDesc.m_depthAttachments.NeedCreateImageView())
        {
            std::string debugName;
            if (!fbDesc.m_depthAttachments.debugName.empty())
                debugName = fbDesc.m_depthAttachments.debugName + std::string("DepthAtt");

			m_depthAttachment.SetImage(MemoryManager::GetInstance()->CreateImage(EMemoryContextType::Framebuffers, imageInfo, debugName), true);
        }
        else
        {
			m_depthAttachment.SetImage(fbDesc.m_depthAttachments.existingImage, false);
        }
        m_clearValues.push_back(fbDesc.m_depthAttachments.clearValue);
    }

    std::vector<VkImageView> imageViews;
    ConvertTo<VkImageView>(m_attachments, imageViews, [](const SFramebufferAttch& att) {
        return att.GetView();
    });

    if (m_depthAttachment.IsValid())
        imageViews.push_back(m_depthAttachment.GetView());

    VkFramebufferCreateInfo fbCreateInfo;
    cleanStructure(fbCreateInfo);
    fbCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbCreateInfo.pNext = nullptr;
    fbCreateInfo.flags = 0;
    fbCreateInfo.renderPass = renderPass;
    fbCreateInfo.attachmentCount = (uint32_t)imageViews.size();
    fbCreateInfo.pAttachments = imageViews.data();
    fbCreateInfo.width = m_width;
    fbCreateInfo.height = m_height;
    fbCreateInfo.layers = m_layers;

    VULKAN_ASSERT(vk::CreateFramebuffer(vk::g_vulkanContext.m_device, &fbCreateInfo, nullptr, &m_frameBuffer));
}

void CFrameBuffer::Finalize() //TODO fix this shit method
{
    if(!m_depthAttachment.IsValid())
        return;

    VkFormat depthFormat = m_depthAttachment.m_image->GetFormat();

    TRAP(IsDepthFormat(depthFormat));

    VkDevice dev = vk::g_vulkanContext.m_device;
    VkCommandBuffer cmdBuff = vk::g_vulkanContext.m_mainCommandBuffer;
    VkQueue queue = vk::g_vulkanContext.m_graphicQueue;

    VkFence transFence;
    VkFenceCreateInfo fenceInfo;
    cleanStructure(fenceInfo);
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.pNext = nullptr;
    fenceInfo.flags = 0;

    VULKAN_ASSERT(vk::CreateFence(dev, &fenceInfo, nullptr, &transFence));
    VkCommandBufferBeginInfo bufferBeginInfo;
    cleanStructure(bufferBeginInfo);
    bufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    const unsigned int barrierCnt = 1;
    VkImageMemoryBarrier image_memory_barrier[barrierCnt];
    VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    aspectMask |= (IsStencilFormat(depthFormat))? VK_IMAGE_ASPECT_STENCIL_BIT : 0;
    //AddImageBarrier(image_memory_barrier[0], m_depthAttachment.GetImage(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 
        //0, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, aspectMask); //TODO create a CreateImageBarrier in ImageHandle

	image_memory_barrier[0] = m_depthAttachment.m_image->CreateMemoryBarrier(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		0, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, aspectMask);

    vk::BeginCommandBuffer(cmdBuff, &bufferBeginInfo);
    vk::CmdPipelineBarrier(cmdBuff, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, 0, 0, NULL, 0, nullptr, 1, &image_memory_barrier[0]); 
    vk::EndCommandBuffer(cmdBuff);

    VkSubmitInfo submitInfo;
    cleanStructure(submitInfo);
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pNext = nullptr;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdBuff;

    vk::QueueSubmit(queue, 1, &submitInfo, transFence);
    VULKAN_ASSERT(vk::WaitForFences(dev, 1, &transFence, VK_TRUE, UINT64_MAX));
    vk::DestroyFence(dev, transFence, nullptr);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//CRenderer
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::vector<CRenderer*> CRenderer::ms_Renderers;

CRenderer::CRenderer(VkRenderPass renderPass, std::string renderPassMarker)
    : m_initialized(false)
    , m_renderPass(renderPass)
    , m_framebuffer(nullptr)
    , m_ownFramebuffer(true)
    , m_descriptorPool(VK_NULL_HANDLE)
    , m_renderPassMarker(renderPassMarker)

{
    ms_Renderers.push_back(this);
}


void CRenderer::CreateFramebuffer(FramebufferDescription& fbDesc, unsigned int width, unsigned int height, unsigned int layers)
{
    m_framebuffer = new CFrameBuffer(width, height, layers);
    m_framebuffer->CreateFramebuffer(m_renderPass, fbDesc);
    m_framebuffer->Finalize();
    m_ownFramebuffer = true;

    UpdateResourceTable();
}

void CRenderer::StartRenderPass()
{
    VkRect2D renderArea = m_framebuffer->GetRenderArea();  
    const std::vector<VkClearValue>& clearValues = m_framebuffer->GetClearValues();

    VkRenderPassBeginInfo renderBeginInfo;
    cleanStructure(renderBeginInfo);
    renderBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderBeginInfo.pNext = nullptr;
    renderBeginInfo.renderPass = m_renderPass;
    renderBeginInfo.framebuffer = m_framebuffer->Get();
    renderBeginInfo.renderArea = renderArea;
    renderBeginInfo.clearValueCount = (uint32_t)clearValues.size();
    renderBeginInfo.pClearValues = clearValues.data();

    if (!m_renderPassMarker.empty())
        StartDebugMarker(m_renderPassMarker);

    vk::CmdBeginRenderPass(vk::g_vulkanContext.m_mainCommandBuffer, &renderBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
}

void CRenderer::EndRenderPass()
{
    vk::CmdEndRenderPass(vk::g_vulkanContext.m_mainCommandBuffer);
    if (!m_renderPassMarker.empty())
        EndDebugMarker(m_renderPassMarker);
}

CRenderer::~CRenderer()
{
    VkDevice dev = vk::g_vulkanContext.m_device;

    if(m_descriptorPool != VK_NULL_HANDLE)
        vk::DestroyDescriptorPool(dev, m_descriptorPool, nullptr);

    if(m_ownFramebuffer)
        delete m_framebuffer;

	auto it = std::find(ms_Renderers.begin(), ms_Renderers.end(), this);
	if (it != ms_Renderers.end())
		ms_Renderers.erase(it);

    m_ownPipelines.clear();
}

void CRenderer::Reload()
{
    for(auto it = m_ownPipelines.begin(); it != m_ownPipelines.end(); ++it)
        (*it)->Reload();
}

void CRenderer::RegisterPipeline(CPipeline* pipeline)
{
    m_ownPipelines.insert(pipeline);
}

void CRenderer::ReloadAll()
{
    for(auto it = ms_Renderers.begin(); it != ms_Renderers.end(); ++it)
        (*it)->Reload();
}

void CRenderer::UpdateAll()
{
    for(auto it = ms_Renderers.begin(); it != ms_Renderers.end(); ++it)
        (*it)->UpdateGraphicInterface();
}

void CRenderer::PrepareAll()
{
	for (auto renderer : ms_Renderers)
		renderer->PreRender();
}

void CRenderer::ComputeAll()
{
	for (auto renderer : ms_Renderers)
		renderer->Compute();
}

void CRenderer::Init()
{
    std::vector<VkDescriptorPoolSize> poolSize;
    uint32_t maxSets = 0;
    PopulatePoolInfo(poolSize, maxSets);
    CreateDescPool(poolSize, maxSets);
    CreateDescriptorSetLayout();
}

void CRenderer::CreateDescPool(std::vector<VkDescriptorPoolSize>& poolSize, unsigned int maxSets)
{
	if (maxSets == 0)
		return;

    VkDescriptorPoolCreateInfo descPoolCi;
    cleanStructure(descPoolCi);
    descPoolCi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descPoolCi.pNext = nullptr;
    descPoolCi.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    descPoolCi.maxSets = maxSets;
    descPoolCi.poolSizeCount = (uint32_t)poolSize.size();
    descPoolCi.pPoolSizes = poolSize.data();

    VULKAN_ASSERT(vk::CreateDescriptorPool(vk::g_vulkanContext.m_device, &descPoolCi, nullptr, &m_descriptorPool));
}

void CRenderer::UpdateResourceTableForColor(unsigned int fbIndex, EResourceType tableType)
{
	g_commonResources.SetAs<ImageHandle*>(&m_framebuffer->GetColorImageHandle(fbIndex), tableType);
}

void CRenderer::UpdateResourceTableForDepth(EResourceType tableType)
{
	g_commonResources.SetAs<ImageHandle*>(&m_framebuffer->GetDepthImageHandle(), tableType);

}

//////////////////////////////////////////////////////
//Renderer
//////////////////////////////////////////////////////

Renderer::Renderer()
{
}

Renderer::~Renderer()
{

}

void Renderer::Init()
{
	InitInternal();
	CreateDescriptorSetLayouts();
	ConstructDescriptorPool();
	AllocateDescriptorSets();
	UpdateGraphicInterface();
}


void Renderer::Reload()
{

	for (auto& pipeline : m_ownedPipelines)
		pipeline->Reload();
}

void Renderer::RegisterPipeline(CPipeline* pipeline)
{
	m_ownedPipelines.insert(pipeline);
}

void Renderer::RegisterDescriptorSetLayout(DescriptorSetLayout* layout)
{
	m_descriptorSetLayouts.push_back(layout);
}

void Renderer::ConstructDescriptorPool()
{
	m_descriptorPool.Construct(m_descriptorSetLayouts, m_descriptorSetLayouts.size()); //TODO find a fix for this hack, eats memory
}
