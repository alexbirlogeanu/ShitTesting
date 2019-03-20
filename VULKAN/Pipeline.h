#pragma once

#include "VulkanLoader.h"
#include "defines.h"

#include <vector>

class CRenderer;
class CPipeline
{
public:
	CPipeline();
	virtual ~CPipeline();

	void Init(CRenderer* renderer, VkRenderPass renderPass, unsigned int subpassId); //this is different for graphic and compute pipelines // TODO find a solution DELETE
	
	void Reload();
	void CreatePipelineLayout(VkDescriptorSetLayout layout);
	void CreatePipelineLayout(const std::vector<VkDescriptorSetLayout>& layouts);

	void AddPushConstant(VkPushConstantRange range);
	bool IsValid() const { return m_pipeline != VK_NULL_HANDLE; }

	VkPipeline Get() const { return m_pipeline; }
	VkPipelineLayout GetLayout() const { return m_pipelineLayout; }

	virtual VkPipelineBindPoint GetBindPoint() const = 0;
protected:
	virtual void CreatePipeline() = 0;
	virtual void CleanInternal() = 0;
protected:
	VkPipeline                  m_pipeline;
	VkPipelineLayout            m_pipelineLayout;

	VkRenderPass                m_renderPass;
	unsigned int                m_subpassIndex;

	std::vector<VkPushConstantRange> m_pushConstantRanges;
	bool                        m_initialized;
};

class CGraphicPipeline : public CPipeline
{
public:
	CGraphicPipeline();
	virtual ~CGraphicPipeline();

	VkPipelineBindPoint GetBindPoint() const final { return VK_PIPELINE_BIND_POINT_GRAPHICS; };

	void SetVertexInputState(VkPipelineVertexInputStateCreateInfo& state);
	void SetTopology(VkPrimitiveTopology topoplogy);
	void SetDepthTest(bool enable);
	void SetDepthWrite(bool enable);
	void SetDepthOp(VkCompareOp op);
	void SetStencilTest(bool enable);
	void SetViewport(unsigned int widht, unsigned int height);
	void SetScissor(unsigned int widht, unsigned int height);
	void SetVertexShaderFile(const std::string& file);
	void SetFragmentShaderFile(const std::string file);
	void SetGeometryShaderFile(const std::string& file);
	void SetTesselationControlShaderFile(const std::string& file);
	void SetTesselationEvaluationShaderFile(const std::string& file);
	void SetCullMode(VkCullModeFlagBits cullmode);
	void AddBlendState(VkPipelineColorBlendAttachmentState blendState, unsigned int cnt = 1); //this should be in order
	void SetStencilOperations(VkStencilOp depthFail, VkStencilOp passOp, VkStencilOp failOp);
	void SetStencilValues(unsigned char comapreMask, unsigned char writeMask, unsigned char ref);
	void SetLineWidth(float width);
	void SetStencilOp(VkCompareOp op);
	void AddDynamicState(VkDynamicState state);
	void SetTesselationPatchSize(uint32_t size);
	void SetWireframeSupport(bool allowWireframe);
	void SetFrontFace(VkFrontFace face);
	void SwitchWireframe(bool isWireframe);
	void SetRasterizerDiscard(bool value); //if set true, pipeline creation crashes in render doc.dll. Maybe it's because of the driver. 

	void Setup(VkRenderPass renderPass, unsigned int subpassId);

	static VkPipelineColorBlendAttachmentState CreateDefaultBlendState()
	{
		VkPipelineColorBlendAttachmentState blendAttachs;
		cleanStructure(blendAttachs);
		blendAttachs.blendEnable = VK_FALSE;
		blendAttachs.srcColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
		blendAttachs.dstColorBlendFactor = VK_BLEND_FACTOR_CONSTANT_ALPHA;
		blendAttachs.colorBlendOp = VK_BLEND_OP_ADD;
		blendAttachs.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		blendAttachs.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		blendAttachs.alphaBlendOp = VK_BLEND_OP_ADD;
		blendAttachs.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
			VK_COLOR_COMPONENT_G_BIT |
			VK_COLOR_COMPONENT_B_BIT |
			VK_COLOR_COMPONENT_A_BIT;

		return blendAttachs;
	}
private:
	CGraphicPipeline(const CGraphicPipeline& other);
	CGraphicPipeline& operator=(const CGraphicPipeline& other);

	virtual void CleanInternal() override;
	virtual void CreatePipeline() override;

protected:
	void CompileShaders();
	void CreatePipelineStages();

private:
	void CreateVertexInput();
	void CreateInputAssemblyState();
	void CreateViewportInfo();
	void CreateRasterizationInfo();
	void CreateMultisampleInfo();
	void CreateDepthInfo();
	void CreateColorBlendInfo();
	void CreateDynamicStateInfo();
	void CreateTesselationInfo();

private:
	VkShaderModule                                      m_vertexShader;
	VkShaderModule                                      m_fragmentShader;
	VkShaderModule                                      m_geometryShader;
	VkShaderModule										m_tesselationControlShader;
	VkShaderModule										m_tesselationEvaluationShader;

	std::string                                         m_vertexFilename;
	std::string                                         m_fragmentFilename;
	std::string                                         m_geometryFilename;
	std::string											m_tesselationControlFilename;
	std::string											m_tesselationEvaluationFilename;

	bool												m_allowWireframe;
	bool												m_isWireframe;
private:
	VkPipelineVertexInputStateCreateInfo                m_pipelineVertexInfo;
	VkPipelineInputAssemblyStateCreateInfo              m_pipelineInputAssemblyInfo;

	VkPipelineViewportStateCreateInfo                   m_pipelineViewportInfo;
	VkViewport                                          m_viewport;
	VkRect2D                                            m_scissorRect;

	VkPipelineRasterizationStateCreateInfo              m_pipelineRasterizationInfo;
	VkPipelineMultisampleStateCreateInfo                m_pipelineMultisampleInfo;
	VkPipelineDepthStencilStateCreateInfo               m_pipelineDepthStencilInfo;
	VkPipelineTessellationStateCreateInfo				m_pipelineTesselationInfo;

	VkPipelineColorBlendStateCreateInfo                 m_pipelineBlendStateInfo;
	std::vector<VkPipelineColorBlendAttachmentState>    m_blendAttachmentState;

	VkPipelineDynamicStateCreateInfo                    m_pipelineDynamicState;
	std::vector<VkDynamicState>                         m_dynamicStates;

	std::vector<VkPipelineShaderStageCreateInfo>        m_pipelineStages;

	VkPipeline											m_wireframePipeline;
	VkPipeline											m_solidPipeline;
};

class CComputePipeline : public CPipeline
{
public:
	CComputePipeline();
	virtual ~CComputePipeline();

	VkPipelineBindPoint GetBindPoint() const final { return VK_PIPELINE_BIND_POINT_COMPUTE; }
	void Setup();

	void SetComputeShaderFile(const std::string& file) { m_computeFilename = file; };
protected:
	CComputePipeline(const CComputePipeline& other);
	CComputePipeline& operator=(const CComputePipeline& other);

	virtual void CreatePipeline() override;
	virtual void CleanInternal() override;
private:
	VkShaderModule              m_computeShader;
	std::string                 m_computeFilename;
};
