#pragma once

#include "VulkanLoader.h"
#include "defines.h"
#include <string>
#include <unordered_set>
#include <vector>

#include "Utils.h"
#include "ResourceTable.h"
#include "MemoryManager.h"

struct FBAttachment
{
    VkFormat            format;
    VkImageUsageFlags   usage;
    VkClearValue        clearValue;
    unsigned int        layers;
    //
    ImageHandle*        existingImage;
    std::string         debugName;

    FBAttachment()
        : format(VK_FORMAT_UNDEFINED)
        , usage(0)
        , existingImage(nullptr)
        , layers(0)
    {
        clearValue = VkClearValue();
    }

    FBAttachment(VkFormat f
        , VkImageUsageFlags u
        , unsigned int l
        , VkClearValue clr
        , const std::string& debug)
        : format(f)
        , usage(u)
        , clearValue(clr)
        , layers(l)
        , debugName(debug)
        , existingImage(VK_NULL_HANDLE)
    {
    }

    FBAttachment(ImageHandle* img
        , VkClearValue clr)
		: format(img->GetFormat())
        , clearValue(clr)
        , existingImage(img)
    {
    }

    bool IsValid() const { return format != VK_FORMAT_UNDEFINED; }

    bool NeedCreateImageView() const { return  existingImage == nullptr; }
};


struct FramebufferDescription
{
    void Begin(uint32_t numColors)
    {
        m_colorAttachments.resize(numColors);
        m_numColors = numColors;
    }

    void End()
    {
        bool isValid = true;
        for(unsigned int i = 0; i < m_colorAttachments.size(); ++i)
            isValid &= m_colorAttachments[i].IsValid();

        TRAP(isValid);
    }

    void AddColorAttachmentDesc(unsigned int index, VkFormat format, VkImageUsageFlags additionalUsage, const std::string& debugName = std::string(), unsigned int layers = 1, VkClearValue clr = VkClearValue())
    {
        TRAP(index < m_numColors);
        m_colorAttachments[index] = FBAttachment(format, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | additionalUsage, layers, clr, debugName);
    }

    void AddColorAttachmentDesc(unsigned int index, ImageHandle* img, VkClearValue clr = VkClearValue())
    {
        TRAP(index < m_numColors);
		m_colorAttachments[index] = FBAttachment(img, clr);
    }

    void AddDepthAttachmentDesc(VkFormat format, VkImageUsageFlags additionalUsage, const std::string& debugName = std::string(), uint8_t stencilClrValue = 0)
    {
        TRAP(IsDepthFormat(format));
        VkClearValue clrVal;
        clrVal.depthStencil.depth = 1.0f;
        clrVal.depthStencil.stencil = stencilClrValue;
        m_depthAttachments = FBAttachment(format, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | additionalUsage, 1, clrVal, debugName);
    }

     void AddDepthAttachmentDesc( ImageHandle* existingImage, uint8_t stencilClrValue = 0)
     {
		 TRAP(IsDepthFormat(existingImage->GetFormat()));
         VkClearValue clrVal;
         clrVal.depthStencil.depth = 1.0f;
         clrVal.depthStencil.stencil = stencilClrValue;
         m_depthAttachments = FBAttachment(existingImage, clrVal);
     }

    std::vector<FBAttachment>   m_colorAttachments;
    FBAttachment                m_depthAttachments;
    unsigned int                m_numColors;

};

class ImageHandle;
class CFrameBuffer
{

	struct SFramebufferAttch
	{
		SFramebufferAttch()
			: m_image(nullptr)
			, m_ownImage(false)
		{
		}

		void SetImage(ImageHandle* hImage, bool ownImage)
		{
			m_image = hImage;
			m_ownImage = ownImage;
		}

		const VkImage& GetImage() const { return m_image->Get(); }
		const VkImageView& GetView() const { return m_image->GetView(); }

		bool IsValid() const { return m_image != nullptr; }

		void Clean(VkDevice dev)
		{
			if (m_ownImage)
			{
				MemoryManager::GetInstance()->FreeHandle(m_image);
			}
		}

		ImageHandle*			m_image;
		bool					m_ownImage;
	};

public:

    CFrameBuffer(unsigned int width
        , unsigned int height
        , unsigned int layers);
    virtual ~CFrameBuffer();

    VkRect2D GetRenderArea();
    VkFramebuffer Get() const { return m_frameBuffer; }

	const VkImage& GetColorImage(unsigned int index) const { TRAP(index < m_colorsAttNum && m_attachments[index].IsValid()); return m_attachments[index].GetImage(); }
	const VkImageView& GetColorImageView(unsigned int index) const { TRAP(index < m_colorsAttNum && m_attachments[index].IsValid()); return m_attachments[index].GetView(); }
	ImageHandle*& GetColorImageHandle(unsigned int index)  { TRAP(index < m_colorsAttNum && m_attachments[index].IsValid()); return m_attachments[index].m_image; }
	const VkImage& GetDepthImage() const { TRAP(m_depthAttachment.IsValid());  return m_depthAttachment.GetImage(); }
	const VkImageView& GetDepthImageView() const { TRAP(m_depthAttachment.IsValid()); return m_depthAttachment.GetView(); }
	ImageHandle*& GetDepthImageHandle()  { TRAP(m_depthAttachment.IsValid()); return m_depthAttachment.m_image; }

    bool HasDepth() { return m_depthAttachment.IsValid(); }

    const std::vector<VkClearValue>& GetClearValues() const { return m_clearValues; }

    void CreateFramebuffer(VkRenderPass renderPass, const FramebufferDescription& fbDesc);

    void Finalize(); //refactor this shit method
    unsigned int GetWidth() const { return m_width; }
    unsigned int GetHeight() const { return m_height; }
    unsigned int GetLayers() const { return m_layers; }

private:
	VkImageCreateInfo CreateImageInfo(const FBAttachment& fbDesc);

    template<typename T>
    void ConvertTo(const std::vector<SFramebufferAttch>& inVec, std::vector<T>& outVec, T (*getter)(const SFramebufferAttch& ))
    {
        outVec.resize(inVec.size());
        for(unsigned int i = 0; i < inVec.size(); ++i)
        {
            outVec[i] = getter(inVec[i]);
        }
    }

    SFramebufferAttch                   m_depthAttachment;
	VkFramebuffer                       m_frameBuffer;
	std::vector<SFramebufferAttch>      m_attachments;

    std::vector<VkClearValue>           m_clearValues;

    unsigned int                        m_colorsAttNum;
    unsigned int                        m_width;
    unsigned int                        m_height;
    unsigned int                        m_layers;
};
class CRenderer;

class CPipeline
{
public:
    CPipeline();
    virtual ~CPipeline();

    void Init(CRenderer* renderer, VkRenderPass renderPass, unsigned int subpassId); //this is different for graphic and compute pipelines // TODO find a solution
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
    void SetStencilOp (VkCompareOp op);
    void AddDynamicState(VkDynamicState state);
	void SetTesselationPatchSize(uint32_t size);
	void SetWireframeSupport(bool allowWireframe);
	void SetFrontFace(VkFrontFace face);
	void SwitchWireframe(bool isWireframe);
	void SetRasterizerDiscard(bool value); //if set true, pipeline creation crashes in render doc.dll. Maybe it's because of the driver. 

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
    CGraphicPipeline(const CGraphicPipeline& other) ;
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

class CRenderer
{
public:
    CRenderer(VkRenderPass renderPass, std::string renderPassMarker = std::string());
    virtual ~CRenderer();

    virtual void Init(); //this is an anti pattern. Fix it
    virtual void Render() = 0;
	virtual void PreRender(){};
	virtual void RenderShadows() {} //need to refactor this thing

    void StartRenderPass();
    void EndRenderPass();

    virtual void CreateFramebuffer(FramebufferDescription& fbDesc, unsigned int width, unsigned int height, unsigned int layers = 1);
    void SetFramebuffer(CFrameBuffer* fb) { m_framebuffer = fb; m_ownFramebuffer = false; }
    CFrameBuffer* GetFramebuffer() const { return m_framebuffer; }

    void RegisterPipeline(CPipeline* pipeline);

    static void ReloadAll();
    static void UpdateAll();
	static void PrepareAll();

	VkRenderPass GetRenderPass() const { return m_renderPass; }
protected:
    virtual void CreateDescriptorSetLayout()=0;
    virtual void PopulatePoolInfo(std::vector<VkDescriptorPoolSize>& poolSize, unsigned int& maxSets)=0;

    virtual void UpdateResourceTable() {}
    virtual void UpdateGraphicInterface() {}

    void Reload();
    void CreateDescPool(std::vector<VkDescriptorPoolSize>& poolSize, unsigned int maxSets);
    void UpdateResourceTableForColor(unsigned int fbIndex, EResourceType tableType);
    void UpdateResourceTableForDepth( EResourceType tableType);

protected:
    CFrameBuffer*                                       m_framebuffer;
    VkRenderPass                                        m_renderPass;

    VkDescriptorPool                                    m_descriptorPool; //change to class DescriptorPool
private:
    bool                                                m_initialized;
    bool                                                m_ownFramebuffer;
    std::string                                         m_renderPassMarker;

    std::unordered_set<CPipeline*>                      m_ownPipelines;
    static std::vector<CRenderer*>						ms_Renderers;
};

