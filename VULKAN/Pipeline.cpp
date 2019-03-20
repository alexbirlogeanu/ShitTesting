#include "Pipeline.h"

#include "Renderer.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//CPipeline
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CPipeline::CPipeline()
	: m_pipeline(VK_NULL_HANDLE)
	, m_pipelineLayout(VK_NULL_HANDLE)
	, m_initialized(false)
	, m_renderPass(VK_NULL_HANDLE)
	, m_subpassIndex(VK_NULL_HANDLE)
{
}

CPipeline::~CPipeline()
{
	if (!m_initialized)
		return;

	VkDevice dev = vk::g_vulkanContext.m_device;

	if (m_pipeline)
		vk::DestroyPipeline(dev, m_pipeline, nullptr);
	if (m_pipelineLayout)
		vk::DestroyPipelineLayout(dev, m_pipelineLayout, nullptr);
}

void CPipeline::Init(CRenderer* renderer, VkRenderPass renderPass, unsigned int subpassId)
{
	m_renderPass = renderPass;
	m_subpassIndex = subpassId;
	TRAP(m_pipelineLayout != VK_NULL_HANDLE);
	CreatePipeline();
	renderer->RegisterPipeline(this);
	m_initialized = true;
}

void CPipeline::Reload()
{
	if (!m_initialized)
		return;

	VkDevice dev = vk::g_vulkanContext.m_device;
	vk::DestroyPipeline(dev, m_pipeline, nullptr);

	CleanInternal();
	CreatePipeline();
}

void CPipeline::CreatePipelineLayout(VkDescriptorSetLayout layout)
{
	std::vector<VkDescriptorSetLayout> layouts(1, layout);
	CreatePipelineLayout(layouts);
}

void CPipeline::CreatePipelineLayout(const std::vector<VkDescriptorSetLayout>& layouts)
{
	VkPipelineLayoutCreateInfo plci;
	cleanStructure(plci);
	plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	plci.pNext = nullptr;
	plci.flags = 0;
	plci.setLayoutCount = (uint32_t)layouts.size();
	plci.pSetLayouts = layouts.data();
	plci.pushConstantRangeCount = (uint32_t)m_pushConstantRanges.size();
	plci.pPushConstantRanges = (m_pushConstantRanges.empty()) ? nullptr : m_pushConstantRanges.data();

	VULKAN_ASSERT(vk::CreatePipelineLayout(vk::g_vulkanContext.m_device, &plci, nullptr, &m_pipelineLayout));
}

void CPipeline::AddPushConstant(VkPushConstantRange range)
{
	m_pushConstantRanges.push_back(range);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//CGraphicPipeline
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CGraphicPipeline::CGraphicPipeline()
	: m_vertexShader(VK_NULL_HANDLE)
	, m_fragmentShader(VK_NULL_HANDLE)
	, m_geometryShader(VK_NULL_HANDLE)
	, m_tesselationControlShader(VK_NULL_HANDLE)
	, m_tesselationEvaluationShader(VK_NULL_HANDLE)
	, m_wireframePipeline(VK_NULL_HANDLE)
	, m_allowWireframe(false)
	, m_isWireframe(false)
{
	CreateVertexInput();
	CreateInputAssemblyState();
	CreateViewportInfo();
	CreateRasterizationInfo();
	CreateMultisampleInfo();
	CreateDepthInfo();
	CreateTesselationInfo();
}

CGraphicPipeline::~CGraphicPipeline()
{
	if (!m_initialized)
		return;

	VkDevice dev = vk::g_vulkanContext.m_device;

	if (m_vertexShader)
		vk::DestroyShaderModule(dev, m_vertexShader, nullptr);
	if (m_fragmentShader != VK_NULL_HANDLE)
		vk::DestroyShaderModule(dev, m_fragmentShader, nullptr);
	if (m_geometryShader != VK_NULL_HANDLE)
		vk::DestroyShaderModule(dev, m_geometryShader, nullptr);
}

void CGraphicPipeline::CreatePipeline()
{
	CompileShaders();

	CreatePipelineStages();
	CreateColorBlendInfo();
	CreateDynamicStateInfo();

	VkPipelineCreateFlags createFlags = (m_allowWireframe) ? VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT : 0;

	VkGraphicsPipelineCreateInfo gpci;
	cleanStructure(gpci);
	gpci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	gpci.pNext = nullptr;
	gpci.flags = createFlags;
	gpci.stageCount = (uint32_t)m_pipelineStages.size();
	gpci.pStages = m_pipelineStages.data();
	gpci.pVertexInputState = &m_pipelineVertexInfo;
	gpci.pInputAssemblyState = &m_pipelineInputAssemblyInfo;
	gpci.pTessellationState = &m_pipelineTesselationInfo;
	gpci.pViewportState = &m_pipelineViewportInfo;
	gpci.pRasterizationState = &m_pipelineRasterizationInfo;
	gpci.pMultisampleState = &m_pipelineMultisampleInfo;
	gpci.pDepthStencilState = &m_pipelineDepthStencilInfo;
	gpci.pColorBlendState = &m_pipelineBlendStateInfo;
	gpci.pDynamicState = (m_dynamicStates.empty()) ? nullptr : &m_pipelineDynamicState;
	gpci.layout = m_pipelineLayout;
	gpci.renderPass = m_renderPass;
	gpci.subpass = m_subpassIndex;
	gpci.basePipelineHandle = VK_NULL_HANDLE;
	gpci.basePipelineIndex = -1;

	VULKAN_ASSERT(vk::CreateGraphicsPipelines(vk::g_vulkanContext.m_device, VK_NULL_HANDLE, 1, &gpci, nullptr, &m_solidPipeline));

	if (m_allowWireframe)
	{
		VkPipelineRasterizationStateCreateInfo wireRasterizationInfo = m_pipelineRasterizationInfo;
		wireRasterizationInfo.polygonMode = VK_POLYGON_MODE_LINE;

		gpci.flags = VK_PIPELINE_CREATE_DERIVATIVE_BIT;
		gpci.pRasterizationState = &wireRasterizationInfo;
		gpci.basePipelineHandle = m_solidPipeline;

		VULKAN_ASSERT(vk::CreateGraphicsPipelines(vk::g_vulkanContext.m_device, VK_NULL_HANDLE, 1, &gpci, nullptr, &m_wireframePipeline));
	}

	SwitchWireframe(m_isWireframe);
};

void CGraphicPipeline::CreateVertexInput()
{
	cleanStructure(m_pipelineVertexInfo);
	m_pipelineVertexInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	m_pipelineVertexInfo.pNext = nullptr;
	m_pipelineVertexInfo.flags = 0;

}
void CGraphicPipeline::CreateInputAssemblyState()
{
	cleanStructure(m_pipelineInputAssemblyInfo);
	m_pipelineInputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	m_pipelineInputAssemblyInfo.pNext = nullptr;
	m_pipelineInputAssemblyInfo.flags = 0;
	m_pipelineInputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	m_pipelineInputAssemblyInfo.primitiveRestartEnable = false;
}

void CGraphicPipeline::CreateViewportInfo()
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
void CGraphicPipeline::CreateRasterizationInfo()
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

void CGraphicPipeline::CreateMultisampleInfo()
{
	cleanStructure(m_pipelineMultisampleInfo);
	m_pipelineMultisampleInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	m_pipelineMultisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	m_pipelineMultisampleInfo.sampleShadingEnable = false;
	m_pipelineMultisampleInfo.pSampleMask = nullptr;
	m_pipelineMultisampleInfo.alphaToCoverageEnable = false;
	m_pipelineMultisampleInfo.alphaToOneEnable = false;
}
void CGraphicPipeline::CreateDepthInfo()
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

void CGraphicPipeline::CreateColorBlendInfo()
{
	cleanStructure(m_pipelineBlendStateInfo);
	m_pipelineBlendStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	m_pipelineBlendStateInfo.logicOpEnable = VK_FALSE;
	m_pipelineBlendStateInfo.attachmentCount = (uint32_t)m_blendAttachmentState.size();
	m_pipelineBlendStateInfo.pAttachments = m_blendAttachmentState.data();

}

void CGraphicPipeline::CreateDynamicStateInfo() //before creation update the struct
{
	cleanStructure(m_pipelineDynamicState);
	m_pipelineDynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	m_pipelineDynamicState.pNext = nullptr;
	m_pipelineDynamicState.flags = 0;
	m_pipelineDynamicState.dynamicStateCount = (uint32_t)m_dynamicStates.size();
	m_pipelineDynamicState.pDynamicStates = m_dynamicStates.data();
}

void CGraphicPipeline::CreateTesselationInfo()
{
	cleanStructure(m_pipelineTesselationInfo);
	m_pipelineTesselationInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
	m_pipelineTesselationInfo.patchControlPoints = 3;
}

void CGraphicPipeline::CleanInternal()
{
	VkDevice dev = vk::g_vulkanContext.m_device;

	vk::DestroyShaderModule(dev, m_vertexShader, nullptr);
	if (m_fragmentShader != VK_NULL_HANDLE)
		vk::DestroyShaderModule(dev, m_fragmentShader, nullptr);

	if (m_geometryShader != VK_NULL_HANDLE)
		vk::DestroyShaderModule(dev, m_geometryShader, nullptr);

	if (m_tesselationControlShader != VK_NULL_HANDLE)
		vk::DestroyShaderModule(dev, m_tesselationControlShader, nullptr);

	if (m_tesselationEvaluationShader != VK_NULL_HANDLE)
		vk::DestroyShaderModule(dev, m_tesselationEvaluationShader, nullptr);

	if (m_isWireframe)
		vk::DestroyPipeline(dev, m_solidPipeline, nullptr);
	else if (m_wireframePipeline != VK_NULL_HANDLE)
		vk::DestroyPipeline(dev, m_wireframePipeline, nullptr);

}

void CGraphicPipeline::SetVertexInputState(VkPipelineVertexInputStateCreateInfo& state)
{
	m_pipelineVertexInfo = state;
}

void CGraphicPipeline::SetTopology(VkPrimitiveTopology topoplogy)
{
	m_pipelineInputAssemblyInfo.topology = topoplogy;
}

void CGraphicPipeline::SetDepthTest(bool enable)
{
	m_pipelineDepthStencilInfo.depthTestEnable = enable;
}

void CGraphicPipeline::SetDepthWrite(bool enable)
{
	m_pipelineDepthStencilInfo.depthWriteEnable = enable;
}

void CGraphicPipeline::SetDepthOp(VkCompareOp op)
{
	m_pipelineDepthStencilInfo.depthCompareOp = op;
}

void CGraphicPipeline::SetStencilTest(bool enable)
{
	m_pipelineDepthStencilInfo.stencilTestEnable = enable;
}

void CGraphicPipeline::SetViewport(unsigned int widht, unsigned int height)
{
	m_viewport.width = (float)widht;
	m_viewport.height = (float)height;
}

void CGraphicPipeline::SetScissor(unsigned int widht, unsigned int height)
{
	m_scissorRect.extent.width = widht;
	m_scissorRect.extent.height = height;
}

void CGraphicPipeline::SetVertexShaderFile(const std::string& file)
{
	m_vertexFilename = file;
}

void CGraphicPipeline::SetFragmentShaderFile(const std::string file)
{
	m_fragmentFilename = file;
}

void CGraphicPipeline::SetGeometryShaderFile(const std::string& file)
{
	m_geometryFilename = file;
}

void CGraphicPipeline::SetTesselationControlShaderFile(const std::string& file)
{
	m_tesselationControlFilename = file;
}

void CGraphicPipeline::SetTesselationEvaluationShaderFile(const std::string& file)
{
	m_tesselationEvaluationFilename = file;
}

void CGraphicPipeline::SetCullMode(VkCullModeFlagBits cullmode)
{
	m_pipelineRasterizationInfo.cullMode = cullmode;
}

void CGraphicPipeline::AddBlendState(VkPipelineColorBlendAttachmentState blendState, unsigned int cnt)
{
	for (unsigned int i = 0; i < cnt; ++i)
		m_blendAttachmentState.push_back(blendState);
}

void CGraphicPipeline::SetStencilOperations(VkStencilOp depthFail, VkStencilOp passOp, VkStencilOp failOp)
{
	VkStencilOpState& state = m_pipelineDepthStencilInfo.front;
	state.depthFailOp = depthFail;
	state.passOp = passOp;
	state.failOp = failOp;
	m_pipelineDepthStencilInfo.back = state;
}

void CGraphicPipeline::SetStencilValues(unsigned char comapreMask, unsigned char writeMask, unsigned char ref)
{
	VkStencilOpState& state = m_pipelineDepthStencilInfo.front;
	state.compareMask = comapreMask;
	state.writeMask = writeMask;
	state.reference = ref;
	m_pipelineDepthStencilInfo.back = state;

}

void CGraphicPipeline::SetLineWidth(float width)
{
	m_pipelineRasterizationInfo.lineWidth = width;
}

void CGraphicPipeline::SetStencilOp(VkCompareOp op)
{
	VkStencilOpState& state = m_pipelineDepthStencilInfo.front;
	state.compareOp = op;
	m_pipelineDepthStencilInfo.back = state;
}

void CGraphicPipeline::AddDynamicState(VkDynamicState state)
{
	m_dynamicStates.push_back(state);
}

void CGraphicPipeline::SetTesselationPatchSize(uint32_t size)
{
	m_pipelineTesselationInfo.patchControlPoints = size;
}

void CGraphicPipeline::SetWireframeSupport(bool allowWireframe)
{
	m_allowWireframe = allowWireframe;
}

void CGraphicPipeline::SetFrontFace(VkFrontFace face)
{
	m_pipelineRasterizationInfo.frontFace = face;
}

void CGraphicPipeline::SwitchWireframe(bool isWireframe)
{
	m_isWireframe = isWireframe;
	m_pipeline = (m_isWireframe && m_allowWireframe) ? m_wireframePipeline : m_solidPipeline;
}

void CGraphicPipeline::SetRasterizerDiscard(bool value)
{
	m_pipelineRasterizationInfo.rasterizerDiscardEnable = value;
}

void CGraphicPipeline::Setup(VkRenderPass renderPass, unsigned int subpassId)
{
	m_renderPass = renderPass;
	m_subpassIndex = subpassId;

	//ALL under this comment should be in InitInternal Method and create a new Init method different for compute and graphic pipelines
	//ALSO add every pipeline to a list to be reloaded
	TRAP(m_pipelineLayout != VK_NULL_HANDLE);
	CreatePipeline();
	m_initialized = true;
}

void CGraphicPipeline::CompileShaders()
{
	TRAP(!m_vertexFilename.empty());
	TRAP(CreateShaderModule(m_vertexFilename, m_vertexShader));
	if (!m_fragmentFilename.empty())
		TRAP(CreateShaderModule(m_fragmentFilename, m_fragmentShader));
	if (!m_geometryFilename.empty())
		TRAP(CreateShaderModule(m_geometryFilename, m_geometryShader));

	if (!m_tesselationControlFilename.empty())
		TRAP(CreateShaderModule(m_tesselationControlFilename, m_tesselationControlShader));

	if (!m_tesselationEvaluationFilename.empty())
		TRAP(CreateShaderModule(m_tesselationEvaluationFilename, m_tesselationEvaluationShader));
}

void CGraphicPipeline::CreatePipelineStages()
{
	m_pipelineStages.clear();

	m_pipelineStages.push_back(CreatePipelineStage(m_vertexShader, VK_SHADER_STAGE_VERTEX_BIT));

	if (m_fragmentShader != VK_NULL_HANDLE)
		m_pipelineStages.push_back(CreatePipelineStage(m_fragmentShader, VK_SHADER_STAGE_FRAGMENT_BIT));

	if (m_geometryShader != VK_NULL_HANDLE)
		m_pipelineStages.push_back(CreatePipelineStage(m_geometryShader, VK_SHADER_STAGE_GEOMETRY_BIT));

	if (m_tesselationControlShader != VK_NULL_HANDLE)
		m_pipelineStages.push_back(CreatePipelineStage(m_tesselationControlShader, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT));

	if (m_tesselationEvaluationShader != VK_NULL_HANDLE)
		m_pipelineStages.push_back(CreatePipelineStage(m_tesselationEvaluationShader, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT));
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//CGraphicPipeline
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CComputePipeline::CComputePipeline()
	: CPipeline()
	, m_computeShader(VK_NULL_HANDLE)
{
}

CComputePipeline::~CComputePipeline()
{
	if (m_computeShader)
		vk::DestroyShaderModule(vk::g_vulkanContext.m_device, m_computeShader, nullptr);
}

void CComputePipeline::Setup()
{
	//ALL under this comment should be in InitInternal Method and create a new Init method different for compute and graphic pipelines
	//ALSO add every pipeline to a list to be reloaded
	TRAP(m_pipelineLayout != VK_NULL_HANDLE);
	CreatePipeline();
	m_initialized = true;
}

void CComputePipeline::CreatePipeline()
{
	TRAP(!m_computeFilename.empty());
	TRAP(m_pipelineLayout);
	CreateShaderModule(m_computeFilename, m_computeShader);

	VkComputePipelineCreateInfo crtInfo;
	cleanStructure(crtInfo);
	crtInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	crtInfo.stage = CreatePipelineStage(m_computeShader, VK_SHADER_STAGE_COMPUTE_BIT);
	crtInfo.layout = m_pipelineLayout;

	VULKAN_ASSERT(vk::CreateComputePipelines(vk::g_vulkanContext.m_device, VK_NULL_HANDLE, 1, &crtInfo, nullptr, &m_pipeline));
}

void CComputePipeline::CleanInternal()
{
	if (m_computeShader)
		vk::DestroyShaderModule(vk::g_vulkanContext.m_device, m_computeShader, nullptr);
}
