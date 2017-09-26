#include "Renderer.h"

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

void CFrameBuffer::AddAttachment(VkImageCreateInfo& imgInfo, VkFormat format, VkImageUsageFlags usage, unsigned int layers, VkImageTiling tiling) 
{
    cleanStructure(imgInfo);
    imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imgInfo.pNext = nullptr;
    imgInfo.flags = 0;
    imgInfo.imageType = VK_IMAGE_TYPE_2D;
    imgInfo.format = format;
    imgInfo.extent.height = m_height;
    imgInfo.extent.width = m_width;
    imgInfo.extent.depth = 1;
    imgInfo.arrayLayers = layers;
    imgInfo.mipLevels = 1;
    imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imgInfo.usage = usage;
    imgInfo.queueFamilyIndexCount = 0;
    imgInfo.pQueueFamilyIndices = nullptr;
    imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imgInfo.tiling = tiling;
}

void CFrameBuffer::CreateFramebuffer(VkRenderPass renderPass, const FramebufferDescription& fbDesc)
{
    m_colorsAttNum = fbDesc.m_numColors;
    m_attachments.resize(m_colorsAttNum);
    m_clearValues.resize(m_colorsAttNum);

    const std::vector<FBAttachment>& colorAtt = fbDesc.m_colorAttachments;
    for(unsigned int i = 0; i < m_colorsAttNum; ++i)
    {
        AddAttachment(m_attachments[i].imageInfo, colorAtt[i].format, colorAtt[i].usage, colorAtt[i].layers);
        m_clearValues[i] = colorAtt[i].clearValue;

        VkFormat format = m_attachments[i].imageInfo.format;
        if(colorAtt[i].NeedCreateImageView())
        {
            AllocImageMemory(m_attachments[i].imageInfo, m_attachments[i].image, m_attachments[i].imageMemory);
            CreateImageView(m_attachments[i].imageView, m_attachments[i].image, m_attachments[i].imageInfo);
        }
        else
        {
            m_attachments[i].image = colorAtt[i].existingImage;
            m_attachments[i].imageView = colorAtt[i].existingImageView;
        }
    }

    /*for(auto att = m_attachments.begin(); att != m_attachments.end(); ++att)
    {
    VkFormat format = att->imageInfo.format;
    AllocImageMemory(att->imageInfo, att->image, att->imageMemory);
    CreateImageView(att->imageView, att->image, format);
    }*/

    if(fbDesc.m_depthAttachments.IsValid())
    {
        AddAttachment(m_depthAttachment.imageInfo, fbDesc.m_depthAttachments.format, fbDesc.m_depthAttachments.usage, fbDesc.m_depthAttachments.layers);

        VkFormat format = m_depthAttachment.imageInfo.format;
        if(fbDesc.m_depthAttachments.NeedCreateImageView())
        {
            AllocImageMemory(m_depthAttachment.imageInfo, m_depthAttachment.image, m_depthAttachment.imageMemory);
            CreateImageView(m_depthAttachment.imageView, m_depthAttachment.image, m_depthAttachment.imageInfo);
        }
        else
        {
            m_depthAttachment.image = fbDesc.m_depthAttachments.existingImage;
            m_depthAttachment.imageView = fbDesc.m_depthAttachments.existingImageView;
        }
        m_clearValues.push_back(fbDesc.m_depthAttachments.clearValue);
    }

    std::vector<VkImageView> imageViews;
    ConvertTo<VkImageView>(m_attachments, imageViews, [](const SFramebufferAttch& att) {
        return att.imageView;
    });

    if (m_depthAttachment.IsValid())
        imageViews.push_back(m_depthAttachment.imageView);

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

void CFrameBuffer::Finalize()
{
    if(!m_depthAttachment.IsValid())
        return;

    VkFormat depthFormat = m_depthAttachment.imageInfo.format;

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
    AddImageBarrier(image_memory_barrier[0], m_depthAttachment.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 
        0, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, aspectMask);


    vk::BeginCommandBuffer(cmdBuff, &bufferBeginInfo);
    vk::CmdPipelineBarrier(cmdBuff, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 0, NULL, 0, nullptr, 1, &image_memory_barrier[0]); 
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
//CPipeline
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CPipeline::CPipeline()
    : m_pipeline(VK_NULL_HANDLE)
    , m_pipelineLayout(VK_NULL_HANDLE)
    , m_vertexShader(VK_NULL_HANDLE)
    , m_fragmentShader(VK_NULL_HANDLE)
    , m_geometryShader(VK_NULL_HANDLE)
    , m_initialized(false)
    , m_renderPass(VK_NULL_HANDLE)
    , m_subpassIndex(VK_NULL_HANDLE)
{
    CreateVertexInput();
    CreateInputAssemblyState();
    CreateViewportInfo();
    CreateRasterizationInfo();
    CreateMultisampleInfo();
    CreateDepthInfo();
}

CPipeline::~CPipeline()
{
    VkDevice dev = vk::g_vulkanContext.m_device;

    if(m_pipeline)
        vk::DestroyPipeline(dev, m_pipeline, nullptr);
    if(m_pipelineLayout)
        vk::DestroyPipelineLayout(dev, m_pipelineLayout, nullptr);

    if(m_vertexShader)
        vk::DestroyShaderModule(dev, m_vertexShader, nullptr);
    if(m_fragmentShader != VK_NULL_HANDLE)
        vk::DestroyShaderModule(dev, m_fragmentShader, nullptr);
    if(m_geometryShader != VK_NULL_HANDLE)
        vk::DestroyShaderModule(dev, m_geometryShader, nullptr);
}

void CPipeline::Init(CRenderer* renderer, VkRenderPass renderPass, unsigned int subpassId)
{
    m_renderPass = renderPass;
    m_subpassIndex = subpassId;
    TRAP(m_pipelineLayout != VK_NULL_HANDLE);
    CreatePipeline();
    renderer->RegisterPipeline(this);
}

void CPipeline::CreatePipeline()
{
    CompileShaders();

    CreatePipelineStages();
    CreateColorBlendInfo();
    CreateDynamicStateInfo();

    VkGraphicsPipelineCreateInfo gpci;
    cleanStructure(gpci);
    gpci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    gpci.pNext = nullptr;
    gpci.flags = 0;
    gpci.stageCount = (uint32_t)m_pipelineStages.size();
    gpci.pStages = m_pipelineStages.data();
    gpci.pVertexInputState = &m_pipelineVertexInfo;
    gpci.pInputAssemblyState = &m_pipelineInputAssemblyInfo;
    gpci.pTessellationState = nullptr; //no tesselation
    gpci.pViewportState = &m_pipelineViewportInfo;
    gpci.pRasterizationState = &m_pipelineRasterizationInfo;
    gpci.pMultisampleState = &m_pipelineMultisampleInfo;
    gpci.pDepthStencilState = &m_pipelineDepthStencilInfo;
    gpci.pColorBlendState = &m_pipelineBlendStateInfo;
    gpci.pDynamicState = (m_dynamicStates.empty())? nullptr : &m_pipelineDynamicState;
    gpci.layout = m_pipelineLayout;
    gpci.renderPass = m_renderPass;
    gpci.subpass = m_subpassIndex;
    gpci.basePipelineHandle = VK_NULL_HANDLE;
    gpci.basePipelineIndex = 0;

    VULKAN_ASSERT(vk::CreateGraphicsPipelines(vk::g_vulkanContext.m_device, VK_NULL_HANDLE, 1, &gpci, nullptr, &m_pipeline));
    m_initialized = true;
};

void CPipeline::CreateVertexInput()
{
    cleanStructure(m_pipelineVertexInfo);
    m_pipelineVertexInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    m_pipelineVertexInfo.pNext = nullptr;
    m_pipelineVertexInfo.flags = 0;

}
void CPipeline::CreateInputAssemblyState()
{
    cleanStructure(m_pipelineInputAssemblyInfo);
    m_pipelineInputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    m_pipelineInputAssemblyInfo.pNext = nullptr;
    m_pipelineInputAssemblyInfo.flags = 0;
    m_pipelineInputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    m_pipelineInputAssemblyInfo.primitiveRestartEnable = false;
}

void CPipeline::CreateViewportInfo()
{
    cleanStructure(m_viewport);
    m_viewport.width = WIDTH;
    m_viewport.height = HEIGHT;
    m_viewport.x = 0;
    m_viewport.y = 0;
    m_viewport.minDepth = 0.0f;
    m_viewport.maxDepth = 1.0f;

    m_scissorRect.extent.width = WIDTH;
    m_scissorRect.extent.height = HEIGHT;
    m_scissorRect.offset.x = 0;
    m_scissorRect.offset.y = 0;

    cleanStructure(m_pipelineViewportInfo);
    m_pipelineViewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    m_pipelineViewportInfo.pNext = nullptr;
    m_pipelineViewportInfo.flags = 0;
    m_pipelineViewportInfo.viewportCount = 1;
    m_pipelineViewportInfo.pViewports = &m_viewport;
    m_pipelineViewportInfo.scissorCount = 1;
    m_pipelineViewportInfo.pScissors = &m_scissorRect;

}
void CPipeline::CreateRasterizationInfo()
{
    cleanStructure(m_pipelineRasterizationInfo);
    m_pipelineRasterizationInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    m_pipelineRasterizationInfo.depthClampEnable = false;
    m_pipelineRasterizationInfo.rasterizerDiscardEnable = false;
    m_pipelineRasterizationInfo.polygonMode = VK_POLYGON_MODE_FILL;
    m_pipelineRasterizationInfo.cullMode = VK_CULL_MODE_NONE;
    m_pipelineRasterizationInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    m_pipelineRasterizationInfo.depthBiasEnable = false;
    m_pipelineRasterizationInfo.lineWidth = 1.0f;
}

void CPipeline::CreateMultisampleInfo()
{
    cleanStructure(m_pipelineMultisampleInfo);
    m_pipelineMultisampleInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    m_pipelineMultisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    m_pipelineMultisampleInfo.sampleShadingEnable = false;
    m_pipelineMultisampleInfo.pSampleMask = nullptr;
    m_pipelineMultisampleInfo.alphaToCoverageEnable = false;
    m_pipelineMultisampleInfo.alphaToOneEnable = false;
}
void CPipeline::CreateDepthInfo()
{
    cleanStructure(m_pipelineDepthStencilInfo);
    m_pipelineDepthStencilInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    m_pipelineDepthStencilInfo.pNext = NULL;
    m_pipelineDepthStencilInfo.flags = 0;
    m_pipelineDepthStencilInfo.depthTestEnable = VK_TRUE;
    m_pipelineDepthStencilInfo.depthWriteEnable = VK_TRUE;
    m_pipelineDepthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    m_pipelineDepthStencilInfo.depthBoundsTestEnable = VK_FALSE;
    m_pipelineDepthStencilInfo.stencilTestEnable = VK_FALSE;
    m_pipelineDepthStencilInfo.back.failOp = VK_STENCIL_OP_KEEP;
    m_pipelineDepthStencilInfo.back.passOp = VK_STENCIL_OP_KEEP;
    m_pipelineDepthStencilInfo.back.compareOp = VK_COMPARE_OP_ALWAYS;
    m_pipelineDepthStencilInfo.back.compareMask = 0;
    m_pipelineDepthStencilInfo.back.reference = 0;
    m_pipelineDepthStencilInfo.back.depthFailOp = VK_STENCIL_OP_KEEP;
    m_pipelineDepthStencilInfo.back.writeMask = 0;
    m_pipelineDepthStencilInfo.minDepthBounds = 0;
    m_pipelineDepthStencilInfo.maxDepthBounds = 0;
    m_pipelineDepthStencilInfo.stencilTestEnable = VK_FALSE;
    m_pipelineDepthStencilInfo.front = m_pipelineDepthStencilInfo.back;
}

void CPipeline::CreateColorBlendInfo() 
{
    cleanStructure(m_pipelineBlendStateInfo);
    m_pipelineBlendStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    m_pipelineBlendStateInfo.logicOpEnable = VK_FALSE;
    m_pipelineBlendStateInfo.attachmentCount = (uint32_t)m_blendAttachmentState.size();
    m_pipelineBlendStateInfo.pAttachments = m_blendAttachmentState.data();

}

void CPipeline::CreateDynamicStateInfo() //before creation update the struct
{
    cleanStructure( m_pipelineDynamicState);
    m_pipelineDynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    m_pipelineDynamicState.pNext = nullptr;
    m_pipelineDynamicState.flags = 0;
    m_pipelineDynamicState.dynamicStateCount = (uint32_t)m_dynamicStates.size();
    m_pipelineDynamicState.pDynamicStates = m_dynamicStates.data();
}


void CPipeline::Reload()
{
    if(!m_initialized)
        return;

    CleanForReload();
    CreatePipeline();
}

void CPipeline::CleanForReload()
{
    VkDevice dev = vk::g_vulkanContext.m_device;

    vk::DestroyPipeline(dev, m_pipeline, nullptr);

    vk::DestroyShaderModule(dev, m_vertexShader, nullptr);
    if(m_fragmentShader != VK_NULL_HANDLE)
        vk::DestroyShaderModule(dev, m_fragmentShader, nullptr);

    if(m_geometryShader != VK_NULL_HANDLE)
        vk::DestroyShaderModule(dev, m_geometryShader, nullptr);
}

void CPipeline::SetVertexInputState(VkPipelineVertexInputStateCreateInfo& state)
{
    m_pipelineVertexInfo = state;
}

void CPipeline::SetTopology(VkPrimitiveTopology topoplogy)
{
    m_pipelineInputAssemblyInfo.topology = topoplogy;
}

void CPipeline::SetDepthTest(bool enable)
{
    m_pipelineDepthStencilInfo.depthTestEnable = enable;
}

void CPipeline::SetDepthWrite(bool enable)
{
    m_pipelineDepthStencilInfo.depthWriteEnable = enable;
}

void CPipeline::SetDepthOp(VkCompareOp op)
{
    m_pipelineDepthStencilInfo.depthCompareOp = op;
}

void CPipeline::SetStencilTest(bool enable)
{
    m_pipelineDepthStencilInfo.stencilTestEnable = enable;
}

void CPipeline::SetViewport(unsigned int widht, unsigned int height)
{
    m_viewport.width = (float)widht;
    m_viewport.height = (float)height;
}

void CPipeline::SetScissor(unsigned int widht, unsigned int height)
{
    m_scissorRect.extent.width = widht;
    m_scissorRect.extent.height = height;
}

void CPipeline::SetVertexShaderFile(const std::string& file)
{
    m_vertexFilename = file;
}

void CPipeline::SetFragmentShaderFile(const std::string file)
{
    m_fragmentFilename = file;
}

void CPipeline::SetGeometryShaderFile(const std::string& file)
{
    m_geometryFilename = file;
}

void CPipeline::SetCullMode(VkCullModeFlagBits cullmode)
{
    m_pipelineRasterizationInfo.cullMode = cullmode;
}

void CPipeline::AddBlendState(VkPipelineColorBlendAttachmentState blendState, unsigned int cnt)
{
    for(unsigned int i = 0; i < cnt; ++i)
        m_blendAttachmentState.push_back(blendState);
}

void CPipeline::SetStencilOperations(VkStencilOp depthFail, VkStencilOp passOp, VkStencilOp failOp)
{
    VkStencilOpState& state = m_pipelineDepthStencilInfo.front;
    state.depthFailOp = depthFail;
    state.passOp = passOp;
    state.failOp = failOp;
    m_pipelineDepthStencilInfo.back = state;
}

void CPipeline::SetStencilValues(unsigned char comapreMask, unsigned char writeMask, unsigned char ref)
{
    VkStencilOpState& state = m_pipelineDepthStencilInfo.front;
    state.compareMask = comapreMask;
    state.writeMask = writeMask;
    state.reference = ref;
    m_pipelineDepthStencilInfo.back = state;

}

void CPipeline::SetLineWidth(float width)
{
    m_pipelineRasterizationInfo.lineWidth = width;
}

void CPipeline::SetStencilOp (VkCompareOp op)
{
    VkStencilOpState& state = m_pipelineDepthStencilInfo.front;
    state.compareOp = op;
    m_pipelineDepthStencilInfo.back = state;
}

void CPipeline::AddDynamicState(VkDynamicState state)
{
    m_dynamicStates.push_back(state);
}

void CPipeline::CompileShaders()
{
    TRAP(!m_vertexFilename.empty());
    TRAP(CreateShaderModule(m_vertexFilename, m_vertexShader));
    if(!m_fragmentFilename.empty())
        TRAP(CreateShaderModule(m_fragmentFilename, m_fragmentShader));
    if(!m_geometryFilename.empty())
        TRAP(CreateShaderModule(m_geometryFilename, m_geometryShader));
}

void CPipeline::CreatePipelineStages()
{
    m_pipelineStages.clear();

    m_pipelineStages.push_back(CreatePipelineStage(m_vertexShader, VK_SHADER_STAGE_VERTEX_BIT));

    if (m_fragmentShader != VK_NULL_HANDLE)
        m_pipelineStages.push_back(CreatePipelineStage(m_fragmentShader, VK_SHADER_STAGE_FRAGMENT_BIT));

    if(m_geometryShader != VK_NULL_HANDLE)
        m_pipelineStages.push_back(CreatePipelineStage(m_geometryShader, VK_SHADER_STAGE_GEOMETRY_BIT));
}

void CPipeline::CreatePipelineLayout(VkDescriptorSetLayout layout)
{
    VkPipelineLayoutCreateInfo plci;
    cleanStructure(plci);
    plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plci.pNext = nullptr;
    plci.flags = 0;
    plci.setLayoutCount = 1;
    plci.pSetLayouts = &layout;

    VULKAN_ASSERT(vk::CreatePipelineLayout(vk::g_vulkanContext.m_device, &plci, nullptr, &m_pipelineLayout));
}

void CPipeline::CreatePipelineLayout(std::vector<VkDescriptorSetLayout> layouts)
{
    VkPipelineLayoutCreateInfo plci;
    cleanStructure(plci);
    plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plci.pNext = nullptr;
    plci.flags = 0;
    plci.setLayoutCount = (uint32_t)layouts.size();
    plci.pSetLayouts = layouts.data();

    VULKAN_ASSERT(vk::CreatePipelineLayout(vk::g_vulkanContext.m_device, &plci, nullptr, &m_pipelineLayout));
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//CRenderer
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::unordered_set<CRenderer*> CRenderer::ms_Renderers;

CRenderer::CRenderer(VkRenderPass renderPass)
    : m_initialized(false)
    , m_renderPass(renderPass)
    , m_framebuffer(nullptr)
    , m_ownFramebuffer(true)
    , m_descriptorPool(VK_NULL_HANDLE)

{
    ms_Renderers.insert(this);
}


void CRenderer::CreateFramebuffer(FramebufferDescription& fbDesc, unsigned int width, unsigned int height, unsigned int layers)
{
    m_framebuffer = new CFrameBuffer(width, height, layers);
    m_framebuffer->CreateFramebuffer(m_renderPass, fbDesc);
    m_framebuffer->Finalize();
    m_ownFramebuffer = true;
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

    vk::CmdBeginRenderPass(vk::g_vulkanContext.m_mainCommandBuffer, &renderBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
}

void CRenderer::EndRenderPass()
{
    vk::CmdEndRenderPass(vk::g_vulkanContext.m_mainCommandBuffer);
}

CRenderer::~CRenderer()
{
    VkDevice dev = vk::g_vulkanContext.m_device;

    if(m_descriptorPool != VK_NULL_HANDLE)
        vk::DestroyDescriptorPool(dev, m_descriptorPool, nullptr);

    if(m_ownFramebuffer)
        delete m_framebuffer;

    ms_Renderers.erase(this);
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
