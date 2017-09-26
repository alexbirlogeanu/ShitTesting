#pragma once

#include "VulkanLoader.h"
#include "defines.h"
#include <string>
#include <vector>
#include "Utils.h"
#include <unordered_set>

struct FBAttachment
{
    VkFormat            format;
    VkImageUsageFlags   usage;
    VkClearValue        clearValue;
    unsigned int        layers;
    //
    VkImage             existingImage;
    VkImageView         existingImageView;

    FBAttachment()
        : format(VK_FORMAT_UNDEFINED)
        , usage(0)
        , existingImage(VK_NULL_HANDLE)
        , existingImageView(VK_NULL_HANDLE)
        , layers(0)
    {
        clearValue = VkClearValue();
    }

    FBAttachment(VkFormat f
        , VkImageUsageFlags u
        , unsigned int l
        , VkClearValue clr)
        : format(f)
        , usage(u)
        , clearValue(clr)
        , layers(l)
        , existingImage(VK_NULL_HANDLE)
        , existingImageView(VK_NULL_HANDLE)
    {
    }

    FBAttachment(VkFormat f
        , VkImage img
        , VkImageView imgView
        , VkClearValue clr)
        : format(f)
        , clearValue(clr)
        , existingImage(img)
        , existingImageView(imgView)
    {
    }

    bool IsValid() const { return format != VK_FORMAT_UNDEFINED; }

    bool NeedCreateImageView() const { return existingImageView == VK_NULL_HANDLE || existingImage == VK_NULL_HANDLE; }
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

    void AddColorAttachmentDesc(unsigned int index, VkFormat format, VkImageUsageFlags additionalUsage, unsigned int layers = 1, VkClearValue clr = VkClearValue())
    {
        TRAP(index < m_numColors);
        m_colorAttachments[index] = FBAttachment(format, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | additionalUsage, layers, clr);
    }

    void AddColorAttachmentDesc(unsigned int index, VkFormat format, VkImage existingImage, VkImageView existingImgView, VkClearValue clr = VkClearValue())
    {
        TRAP(index < m_numColors);
        m_colorAttachments[index] = FBAttachment(format, existingImage, existingImgView, clr);
    }

    void AddDepthAttachmentDesc(VkFormat format, VkImageUsageFlags additionalUsage, uint8_t stencilClrValue = 0)
    {
        TRAP(IsDepthFormat(format));
        VkClearValue clrVal;
        clrVal.depthStencil.depth = 1.0f;
        clrVal.depthStencil.stencil = stencilClrValue;
        m_depthAttachments = FBAttachment(format, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | additionalUsage, 1, clrVal);
    }

     void AddDepthAttachmentDesc(VkFormat format, VkImage existingImage, VkImageView existingImgView, uint8_t stencilClrValue = 0)
     {
         TRAP(IsDepthFormat(format));
         VkClearValue clrVal;
         clrVal.depthStencil.depth = 1.0f;
         clrVal.depthStencil.stencil = stencilClrValue;
         m_depthAttachments = FBAttachment(format, existingImage, existingImgView, clrVal);
     }

    std::vector<FBAttachment>   m_colorAttachments;
    FBAttachment                m_depthAttachments;
    unsigned int                m_numColors;

};

class CFrameBuffer
{
public:

    CFrameBuffer(unsigned int width
        , unsigned int height
        , unsigned int layers);
    virtual ~CFrameBuffer();

    VkRect2D GetRenderArea();
    VkFramebuffer Get() const { return m_frameBuffer; }
    VkImage GetColorImage(unsigned int index) { TRAP(index < m_colorsAttNum); return m_attachments[index].image; }
    VkImageView GetColorImageView(unsigned int index) { TRAP(index < m_colorsAttNum); return m_attachments[index].imageView; }
    VkDeviceMemory GetColorImageMemory(unsigned int index) { TRAP(index < m_colorsAttNum); return m_attachments[index].imageMemory; }
    //void SetDepth(unsigned int index) {TRAP(index < m_size); m_depthAttachment = m_attachments[index]; }

    VkImage GetDepthImage() { return m_depthAttachment.image; }
    VkImageView GetDepthImageView() { return m_depthAttachment.imageView; }
    bool HasDepth() { return m_depthAttachment.IsValid(); }

    void AddAttachment(VkImageCreateInfo& imgInfo, VkFormat format, VkImageUsageFlags usage, unsigned int layers, VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL);

    const std::vector<VkClearValue>& GetClearValues() const { return m_clearValues; }

    void CreateFramebuffer(VkRenderPass renderPass, const FramebufferDescription& fbDesc);

    void Finalize();
    VkFramebuffer                       m_frameBuffer;
    unsigned int GetWidth() const { return m_width; }
    unsigned int GetHeight() const { return m_height; }
    unsigned int GetLayers() const { return m_layers; }

    struct SFramebufferAttch
    {
        VkImage             image;
        VkImageView         imageView;
        VkDeviceMemory      imageMemory;
        VkImageCreateInfo   imageInfo;

        SFramebufferAttch()
            : image(VK_NULL_HANDLE)
            , imageView(VK_NULL_HANDLE)
            , imageMemory(VK_NULL_HANDLE)
        {
            cleanStructure(imageInfo);
        }

        bool IsCreateInfoValid() { return imageInfo.sType == VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO; }

        bool IsValid() { return (image != VK_NULL_HANDLE) && (imageView != VK_NULL_HANDLE) /*&& (imageMemory != VK_NULL_HANDLE)*/;  }

        void Clean(VkDevice dev)
        {
            if(imageMemory != VK_NULL_HANDLE) //means that the framebuffer "owns" the image, otherwise it got an image from a previous framebuffer
            {
                vk::FreeMemory(dev, imageMemory, nullptr);
                vk::DestroyImageView(dev, imageView, nullptr);
                vk::DestroyImage(dev, image, nullptr);
            }
        }
    };

    template<typename T>
    void ConvertTo(const std::vector<SFramebufferAttch>& inVec, std::vector<T>& outVec, T (*getter)(const SFramebufferAttch& ))
    {
        outVec.resize(inVec.size());
        for(unsigned int i = 0; i < inVec.size(); ++i)
        {
            outVec[i] = getter(inVec[i]);
        }
    }

    std::vector<SFramebufferAttch>      m_attachments;
    SFramebufferAttch                   m_depthAttachment;

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

    void Init(CRenderer* renderer, VkRenderPass renderPass, unsigned int subpassId);
    void Reload();
    void CreatePipelineLayout(VkDescriptorSetLayout layout);
    void CreatePipelineLayout(const std::vector<VkDescriptorSetLayout>& layouts);

    VkPipeline Get() { return m_pipeline; }
    VkPipelineLayout GetLayout() { return m_pipelineLayout; }

    virtual VkPipelineBindPoint GetBindPoint() const = 0;
protected:
    virtual void CreatePipeline() = 0;
    virtual void CleanInternal() = 0;
protected:
    VkPipeline                  m_pipeline;
    VkPipelineLayout            m_pipelineLayout;
    
    VkRenderPass                m_renderPass;
    unsigned int                m_subpassIndex;

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
    void SetCullMode(VkCullModeFlagBits cullmode);
    void AddBlendState(VkPipelineColorBlendAttachmentState blendState, unsigned int cnt = 1); //this should be in order
    void SetStencilOperations(VkStencilOp depthFail, VkStencilOp passOp, VkStencilOp failOp);
    void SetStencilValues(unsigned char comapreMask, unsigned char writeMask, unsigned char ref);
    void SetLineWidth(float width);
    void SetStencilOp (VkCompareOp op);
    void AddDynamicState(VkDynamicState state);

    static VkPipelineColorBlendAttachmentState CreateDefaultBlendState()
    {
        VkPipelineColorBlendAttachmentState blendAttachs;
        cleanStructure(blendAttachs);
        blendAttachs.blendEnable = VK_FALSE; //TODO
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

private:
    VkShaderModule                                      m_vertexShader;
    VkShaderModule                                      m_fragmentShader;
    VkShaderModule                                      m_geometryShader;

    std::string                                         m_vertexFilename;
    std::string                                         m_fragmentFilename;
    std::string                                         m_geometryFilename;
private:
    VkPipelineVertexInputStateCreateInfo                m_pipelineVertexInfo;
    VkPipelineInputAssemblyStateCreateInfo              m_pipelineInputAssemblyInfo;

    VkPipelineViewportStateCreateInfo                   m_pipelineViewportInfo;
    VkViewport                                          m_viewport;
    VkRect2D                                            m_scissorRect;

    VkPipelineRasterizationStateCreateInfo              m_pipelineRasterizationInfo;
    VkPipelineMultisampleStateCreateInfo                m_pipelineMultisampleInfo;
    VkPipelineDepthStencilStateCreateInfo               m_pipelineDepthStencilInfo;

    VkPipelineColorBlendStateCreateInfo                 m_pipelineBlendStateInfo;
    std::vector<VkPipelineColorBlendAttachmentState>    m_blendAttachmentState;

    VkPipelineDynamicStateCreateInfo                    m_pipelineDynamicState;
    std::vector<VkDynamicState>                         m_dynamicStates;

    std::vector<VkPipelineShaderStageCreateInfo>        m_pipelineStages;
};

class CComputePipeline : public CPipeline
{
public:
    CComputePipeline();
    virtual ~CComputePipeline();

    VkPipelineBindPoint GetBindPoint() const final { return VK_PIPELINE_BIND_POINT_COMPUTE; }

    void SetComputeShaderFile(const std::string& file) { m_computeFilename = file; };
protected:
    virtual void CreatePipeline() override;
    virtual void CleanInternal() override;
private:
    VkShaderModule              m_computeShader;
    std::string                 m_computeFilename;
};

class CRenderer
{
public:
    CRenderer(VkRenderPass renderPass);
    virtual ~CRenderer();

    virtual void Init();
    virtual void Render() = 0;

    void StartRenderPass();
    void EndRenderPass();

    virtual void CreateFramebuffer(FramebufferDescription& fbDesc, unsigned int width, unsigned int height, unsigned int layers = 1);
    void SetFramebuffer(CFrameBuffer* fb) { m_framebuffer = fb; m_ownFramebuffer = false; }
    CFrameBuffer* GetFramebuffer() const { return m_framebuffer; }

    void RegisterPipeline(CPipeline* pipeline);

    static void ReloadAll();
protected:
    virtual void CreateDescriptorSetLayout()=0;
    virtual void PopulatePoolInfo(std::vector<VkDescriptorPoolSize>& poolSize, unsigned int& maxSets)=0;

    void Reload();
    void CreateDescPool(std::vector<VkDescriptorPoolSize>& poolSize, unsigned int maxSets);
protected:
    CFrameBuffer*                                       m_framebuffer;
    VkRenderPass                                        m_renderPass;

    VkDescriptorPool                                    m_descriptorPool;
private:
    bool                                                m_initialized;
    bool                                                m_ownFramebuffer;

    std::unordered_set<CPipeline*>                      m_ownPipelines;
    static std::unordered_set<CRenderer*>               ms_Renderers;
};

