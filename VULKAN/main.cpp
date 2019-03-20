#include<iostream>
#include <limits>
#include <unordered_set>
#include <utility>

#include "VulkanLoader.h"
#include "defines.h"
#include <windowsx.h>
#include <cstdio>
#include <functional>
#include <vector>
#include <fstream>
#include <cstdlib>
#include <ctime>
#include <array>
#include "glm/glm.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/component_wise.hpp>

#include "Renderer.h"
#include "Particles.h"
#include "Texture.h"
#include "freeimage/FreeImage.h"
#include "Camera.h"
#include "Mesh.h"
#include <bitset>
#include "UI.h"
#include "Utils.h"
#include "PickManager.h"
#include "ao.h"
#include "Fog.h"
#include "SkyRenderer.h"
#include "3DTexture.h"
#include "ShadowRenderer.h"
#include "Object.h"
#include "PointLightRenderer2.h"
#include "ScreenSpaceReflectionRenderer.h"
#include "TerrainRenderer.h"
#include "VegetationRenderer.h"
#include "Batch.h"
#include "Material.h"
#include "Scene.h"
#include "TestRenderer.h"
#include "RenderTaskGraph.h"
#include "GraphicEngine.h"

#include "MemoryManager.h"
#include "Input.h"

#define OUT_FORMAT VK_FORMAT_R16G16B16A16_SFLOAT
//#define OUT_FORMAT VK_FORMAT_B8G8R8A8_UNORM
CCamera     ms_camera;

CDirectionalLight directionalLight;

void CleanUp()
{
    Mesh* quad = CreateFullscreenQuad();
    delete quad;
    Mesh* cube = CreateUnitCube();
    delete cube;
}

class QueryManager
{
public:
    static QueryManager& GetInstance()
    {
        static QueryManager instance;
        return instance;
    }
     
public:
    QueryManager()
        : m_queryBuffer(VK_NULL_HANDLE)
        , m_queryMemory(VK_NULL_HANDLE)
        , m_queryBufferPtr(nullptr)
        , m_bufferSize(256)
        , m_occlusionQueryPool(VK_NULL_HANDLE)
        , m_statisticsQueryPool(VK_NULL_HANDLE)
        , m_registerQueries(0)
    {
        VkDevice device = vk::g_vulkanContext.m_device;

        VkQueryPipelineStatisticFlags  pipeStatsFlags = VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT;
        std::bitset<32> bitSet (pipeStatsFlags);
        m_statsQueryNum = (uint32_t)bitSet.count();

        uint32_t stride = 2 * sizeof(uint32_t);
        m_occlusionQueryOffset = stride * m_statsQueryNum;
        m_maxQueries = (m_bufferSize - stride * m_statsQueryNum ) / stride;

        VkQueryPoolCreateInfo queryPoolInfo;
        cleanStructure(queryPoolInfo);
        queryPoolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        queryPoolInfo.pNext = nullptr;
        queryPoolInfo.pipelineStatistics = pipeStatsFlags;
        queryPoolInfo.queryCount =  1;
        queryPoolInfo.flags = 0;
        queryPoolInfo.queryType = VK_QUERY_TYPE_PIPELINE_STATISTICS;
        VULKAN_ASSERT(vk::CreateQueryPool(device, &queryPoolInfo, nullptr, &m_statisticsQueryPool));

        queryPoolInfo.queryType = VK_QUERY_TYPE_OCCLUSION;
        queryPoolInfo.pipelineStatistics = 0;
        queryPoolInfo.queryCount = m_maxQueries;
        VULKAN_ASSERT(vk::CreateQueryPool(device, &queryPoolInfo, nullptr, &m_occlusionQueryPool));

        AllocBufferMemory(m_queryBuffer, m_queryMemory, m_bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        VULKAN_ASSERT(vk::MapMemory(device,  m_queryMemory, 0, m_bufferSize, 0, &m_queryBufferPtr));
    }

    ~QueryManager()
    {
        //vk::UnmapMemory(vk::g_vulkanContext.m_device, m_queryMemory);
    }

    void Reset()
    {
        VkCommandBuffer cmdBuffer = vk::g_vulkanContext.m_mainCommandBuffer;
        vk::CmdResetQueryPool(cmdBuffer, m_statisticsQueryPool, 0, m_statsQueryNum);
        vk::CmdResetQueryPool(cmdBuffer, m_occlusionQueryPool, 0, m_registerQueries);

        vk::CmdFillBuffer(cmdBuffer, m_queryBuffer, 0, m_bufferSize, 0);

        m_registerQueries = 0;
        m_canQuery = false;
    }

    uint32_t BeginQuery()
    {
        VkCommandBuffer cmdBuffer = vk::g_vulkanContext.m_mainCommandBuffer;
        vk::CmdBeginQuery(cmdBuffer, m_occlusionQueryPool, m_registerQueries, 0);
        ++m_registerQueries;
    }

    void EndQuery(uint32_t index)
    {
        VkCommandBuffer cmdBuffer = vk::g_vulkanContext.m_mainCommandBuffer;
        vk::CmdEndQuery(cmdBuffer, m_occlusionQueryPool, index);
    }

    void GetQueries()
    {
        VkCommandBuffer cmdBuffer = vk::g_vulkanContext.m_mainCommandBuffer;

        vk::CmdCopyQueryPoolResults(cmdBuffer, m_statisticsQueryPool, 0, m_statsQueryNum, m_queryBuffer, 0, 2 * sizeof(uint32_t), VK_QUERY_RESULT_WITH_AVAILABILITY_BIT | VK_QUERY_RESULT_WAIT_BIT );
        vk::CmdCopyQueryPoolResults(cmdBuffer, m_occlusionQueryPool, 0, m_registerQueries, m_queryBuffer, m_occlusionQueryOffset, 2 * sizeof(uint32_t), VK_QUERY_RESULT_WITH_AVAILABILITY_BIT | VK_QUERY_RESULT_WAIT_BIT );
        m_canQuery = true;
    }

    uint32_t GetResult(uint32_t index)
    {
        TRAP(m_canQuery);
        uint32_t* occlusionPtr = (uint32_t*)((uint8_t*)m_queryBufferPtr + m_occlusionQueryOffset);
        bool isAvailable = (*(occlusionPtr + index + 1) != 0);
        TRAP(isAvailable);
        return *(occlusionPtr + index);
    }

    void StartStatistics()
    {
        VkCommandBuffer cmdBuffer = vk::g_vulkanContext.m_mainCommandBuffer;
        vk::CmdBeginQuery(cmdBuffer, m_statisticsQueryPool, 0, 0);
    }

    void EndStatistics()
    {
        VkCommandBuffer cmdBuffer = vk::g_vulkanContext.m_mainCommandBuffer;
        vk::CmdEndQuery(cmdBuffer, m_statisticsQueryPool, 0);
    }

private:
    VkBuffer        m_queryBuffer;
    VkDeviceMemory  m_queryMemory;
    void*           m_queryBufferPtr;

    VkQueryPool     m_occlusionQueryPool;
    VkQueryPool     m_statisticsQueryPool;

    uint32_t        m_maxQueries;
    uint32_t        m_registerQueries;
    uint32_t        m_occlusionQueryOffset;
    uint32_t        m_statsQueryNum;

    const uint32_t  m_bufferSize;
    bool            m_canQuery;
};



class ScreenshotManager
{
public:
    ScreenshotManager() 
        : m_screenshotBufferMemory(VK_NULL_HANDLE)
        , m_screenshotBuffer(VK_NULL_HANDLE)
        , m_needSaveScreenshot(false)
    {}

    void SaveScreenshot( VkImage img)
    {
        VkCommandBuffer cmdBuffer = vk::g_vulkanContext.m_mainCommandBuffer;

        VkImageSubresourceLayers subResLayer;
        cleanStructure(subResLayer);
        subResLayer.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subResLayer.mipLevel = 0;
        subResLayer.baseArrayLayer = 0;
        subResLayer.layerCount = 1;

        VkExtent3D imgExtent;
        imgExtent.width = WIDTH;
        imgExtent.height = HEIGHT;
        imgExtent.depth = 1;

        VkOffset3D imgOffset;
        imgOffset.x = 0;
        imgOffset.y = 0;
        imgOffset.z = 0;

        VkBufferImageCopy copyInfo;
        cleanStructure(copyInfo);
        copyInfo.bufferOffset = 0;
        copyInfo.bufferRowLength = 0;
        copyInfo.bufferImageHeight = 0;
        copyInfo.imageSubresource = subResLayer;
        copyInfo.imageExtent = imgExtent;
        copyInfo.imageOffset = imgOffset;

        uint32_t size = WIDTH * HEIGHT * 4; //RGBA
        AllocBufferMemory(m_screenshotBuffer, m_screenshotBufferMemory, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT);

        vk::CmdCopyImageToBuffer(cmdBuffer, img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_screenshotBuffer, 1, &copyInfo);

        VkBufferMemoryBarrier bufferBarrier;
        cleanStructure(bufferBarrier);
        bufferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        bufferBarrier.pNext = nullptr;
        bufferBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        bufferBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        bufferBarrier.buffer = m_screenshotBuffer;
        bufferBarrier.offset = 0;
        bufferBarrier.size = VK_WHOLE_SIZE;

        VkImageMemoryBarrier prePresentBarrier;
        cleanStructure(prePresentBarrier);
        prePresentBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        prePresentBarrier.pNext = NULL;
        prePresentBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        prePresentBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        prePresentBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        prePresentBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        prePresentBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        prePresentBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        prePresentBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        prePresentBarrier.subresourceRange.baseMipLevel = 0;
        prePresentBarrier.subresourceRange.levelCount = 1;
        prePresentBarrier.subresourceRange.baseArrayLayer = 0;
        prePresentBarrier.subresourceRange.layerCount = 1;
        prePresentBarrier.image = img;

        vk::CmdPipelineBarrier(cmdBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 
            VK_DEPENDENCY_BY_REGION_BIT ,
            0,
            nullptr,
            1,
            &bufferBarrier,
            1, 
            &prePresentBarrier);

        m_needSaveScreenshot = true;
    }

    void WriteScreenshot()
    {
        
        if(!m_needSaveScreenshot)
            return;
        TRAP(m_screenshotBuffer != VK_NULL_HANDLE);
        TRAP(m_screenshotBufferMemory != VK_NULL_HANDLE);

        VkDevice dev = vk::g_vulkanContext.m_device;
        unsigned int size = WIDTH * HEIGHT * 4;

        FIBITMAP* outImg = FreeImage_Allocate(WIDTH, HEIGHT, 32, 8, 8, 8);
        BYTE* bits = FreeImage_GetBits(outImg);
        void* memPtr = nullptr;
        VULKAN_ASSERT(vk::MapMemory(dev, m_screenshotBufferMemory, 0, size, 0, &memPtr));
        TRAP(memPtr);

        std::memcpy(bits,  memPtr, size);
        vk::UnmapMemory(vk::g_vulkanContext.m_device, m_screenshotBufferMemory);
        
        time_t time = std::time(nullptr);
        unsigned int clampTime = (unsigned int)(0x0000000000FFFFFF & time);
        char timeStr[15];
        _itoa_s(clampTime, timeStr, 10);
        std::string screenshotFName = std::string( "screenshot") +  std::string(timeStr ) +  std::string(".png");

        //outImg = FreeImage_Rotate(outImg, 0);
        FreeImage_FlipVertical(outImg);
        TRAP(FreeImage_Save(FIF_PNG, outImg, screenshotFName.c_str()));

        vk::FreeMemory(vk::g_vulkanContext.m_device, m_screenshotBufferMemory, nullptr);
        vk::DestroyBuffer(vk::g_vulkanContext.m_device, m_screenshotBuffer, nullptr);

        m_screenshotBuffer = VK_NULL_HANDLE; 
        m_screenshotBufferMemory = VK_NULL_HANDLE;
        FreeImage_Unload(outImg);
       // FreeImage_Unload();
        m_needSaveScreenshot = false;
    }

    VkDeviceMemory  m_screenshotBufferMemory;
    VkBuffer        m_screenshotBuffer;
    private:
        bool m_needSaveScreenshot;
};
ScreenshotManager m_screenshotManager;

enum ELightSubpass
{
    ELightSubpass_Directional = 0,
    ELightSubpass_DirCount,
    ELightSubpass_PrePoint = 0,
    ELightSubpass_PointAccum,
	ELightSubpass_Blend,
    ELightSubpass_PointCount
};

enum EDirLightBuffers
{
	EDirLightBuffers_Final = 0,
	EDirLightBuffers_Debug,
	EDirLightBuffers_Count
};

enum EPointLightPassBuffers
{
    EPointLightBuffer_Final = 0,
	EPointLightBuffer_Accum,
	EPointLightBuffer_Debug,
	EPointLightBuffer_Depth,
	EPointLightBuffer_Count
};

class CSkyRenderer : public CRenderer
{
public:
    CSkyRenderer(VkRenderPass renderPass)
        : CRenderer(renderPass, "SkyRenderPass")
        , m_quadMesh(nullptr)
        , m_skyTexture(nullptr)
        , m_boxParamsBuffer(VK_NULL_HANDLE)
        , m_boxDescriptorSet(VK_NULL_HANDLE)
        , m_boxDescriptorSetLayout(VK_NULL_HANDLE)
        , m_sampler(VK_NULL_HANDLE)
        , m_sunDescriptorSet(VK_NULL_HANDLE)
        , m_sunDescriptorSetLayout(VK_NULL_HANDLE)
    {
    }

    virtual ~CSkyRenderer()
    {
        VkDevice dev = vk::g_vulkanContext.m_device;
        vk::DestroyDescriptorSetLayout(dev, m_boxDescriptorSetLayout, nullptr);

		MemoryManager::GetInstance()->FreeHandle(m_boxParamsBuffer);
    }

    virtual void Render()
    {
        TRAP(m_skyTexture);
        VkCommandBuffer cmdBuffer = vk::g_vulkanContext.m_mainCommandBuffer;
        StartRenderPass();

        BeginMarkerSection("SkyBox");
        vk::CmdBindPipeline(cmdBuffer, m_boxPipeline.GetBindPoint(), m_boxPipeline.Get());
        vk::CmdBindDescriptorSets(cmdBuffer, m_boxPipeline.GetBindPoint(), m_boxPipeline.GetLayout(), 0, 1, &m_boxDescriptorSet, 0, nullptr);
        m_quadMesh->Render();
        EndMarkerSection();

        BeginMarkerSection("BlendSun");
        vk::CmdNextSubpass(cmdBuffer, VK_SUBPASS_CONTENTS_INLINE);
        vk::CmdBindPipeline(cmdBuffer, m_sunPipeline.GetBindPoint(), m_sunPipeline.Get());
        vk::CmdBindDescriptorSets(cmdBuffer, m_sunPipeline.GetBindPoint(), m_sunPipeline.GetLayout(), 0, 1, &m_sunDescriptorSet, 0, nullptr);
        m_quadMesh->Render();
        EndMarkerSection();

        EndRenderPass();
    }

    void SetTexture(CTexture* text) { m_skyTexture = text; UpdateDescriptors(); }

    virtual void Init() override
    {
        CRenderer::Init();

        AllocDescriptorSets(m_descriptorPool, m_boxDescriptorSetLayout, &m_boxDescriptorSet);
        AllocDescriptorSets(m_descriptorPool, m_sunDescriptorSetLayout, &m_sunDescriptorSet);

		m_boxParamsBuffer = MemoryManager::GetInstance()->CreateBuffer(EMemoryContextType::UniformBuffers, sizeof(SSkyParams), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
        m_quadMesh = CreateFullscreenQuad();

        m_boxPipeline.SetVertexShaderFile("skybox.vert");
        m_boxPipeline.SetFragmentShaderFile("skybox.frag");
        m_boxPipeline.SetVertexInputState(Mesh::GetVertexDesc());
        m_boxPipeline.AddBlendState(CGraphicPipeline::CreateDefaultBlendState());
        m_boxPipeline.SetDepthTest(true);
        m_boxPipeline.SetDepthWrite(false);
        m_boxPipeline.CreatePipelineLayout(m_boxDescriptorSetLayout);
        m_boxPipeline.Init(this, m_renderPass, 0);

        VkPipelineColorBlendAttachmentState addState;
        cleanStructure(addState);
        addState.blendEnable = VK_TRUE;
        addState.colorBlendOp = VK_BLEND_OP_ADD;
        addState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
        addState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        addState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT;

        m_sunPipeline.SetVertexShaderFile("screenquad.vert");
        m_sunPipeline.SetFragmentShaderFile("passtrough.frag");
        m_sunPipeline.SetVertexInputState(Mesh::GetVertexDesc());
        m_sunPipeline.AddBlendState(addState);
        m_sunPipeline.SetDepthTest(false);
        m_sunPipeline.CreatePipelineLayout(m_sunDescriptorSetLayout);
        m_sunPipeline.Init(this, m_renderPass, 1);
    }

    void SetSkyImageView()
    {
        CreateLinearSampler(m_sampler);

        VkDescriptorImageInfo imgInfo;
        cleanStructure(imgInfo);
        imgInfo.sampler = m_sampler;
		imgInfo.imageView = g_commonResources.GetAs<ImageHandle*>(EResourceType_SunImage)->GetView();
        imgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet wDesc = InitUpdateDescriptor(m_sunDescriptorSet, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &imgInfo);

        vk::UpdateDescriptorSets(vk::g_vulkanContext.m_device, 1, &wDesc, 0, nullptr);
    }

private:
    struct SSkyParams
    {
        glm::vec4   CameraDir;
        glm::vec4   CameraRight;
        glm::vec4   CameraUp;
        glm::vec4   Frustrum;
        glm::vec4   DirLightColor;
        //maybe proj
    };
    
    virtual void PopulatePoolInfo(std::vector<VkDescriptorPoolSize>& poolSize, unsigned int& maxSets) override
    {
        AddDescriptorType(poolSize, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1);
        AddDescriptorType(poolSize, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2);

        maxSets = 2;
    }

    virtual void CreateDescriptorSetLayout()
    {
        std::vector<VkDescriptorSetLayoutBinding> bindings;
        bindings.resize(2);
        bindings[0] = CreateDescriptorBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
        bindings[1] = CreateDescriptorBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);

        VkDescriptorSetLayoutCreateInfo crtInfo;
        cleanStructure(crtInfo);
        crtInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        crtInfo.bindingCount = (uint32_t)bindings.size();
        crtInfo.pBindings = bindings.data();

        VULKAN_ASSERT(vk::CreateDescriptorSetLayout(vk::g_vulkanContext.m_device, &crtInfo, nullptr, &m_boxDescriptorSetLayout));

        {
            std::vector<VkDescriptorSetLayoutBinding> bindings;
            bindings.push_back(CreateDescriptorBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT));

            VkDescriptorSetLayoutCreateInfo crtInfo;
            cleanStructure(crtInfo);
            crtInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            crtInfo.bindingCount = (uint32_t)bindings.size();
            crtInfo.pBindings = bindings.data();

            VULKAN_ASSERT(vk::CreateDescriptorSetLayout(vk::g_vulkanContext.m_device, &crtInfo, nullptr, &m_sunDescriptorSetLayout));
        }
    }

    void PreRender()
    {
        SSkyParams* newParams = m_boxParamsBuffer->GetPtr<SSkyParams*>();
        newParams->CameraDir = glm::vec4(ms_camera.GetFrontVector(), 0.0f);
        newParams->CameraUp = glm::vec4(ms_camera.GetUpVector(), 0.0f);
        newParams->CameraRight = glm::vec4(ms_camera.GetRightVector(), 0.0f);
        newParams->Frustrum = glm::vec4(glm::radians(75.0f), 16.0f/9.0f, 100.0f, 0.0f);
        newParams->DirLightColor = directionalLight.GetLightIradiance();
    }

    void UpdateDescriptors()
    {
        VkDescriptorBufferInfo wBuffer = m_boxParamsBuffer->GetDescriptor();

        //VkDescriptorImageInfo wImage = m_cubeMapText->GetCubeMapDescriptor();
        VkDescriptorImageInfo wImage = m_skyTexture->GetTextureDescriptor();

        std::vector<VkWriteDescriptorSet> writeDesc;
        writeDesc.resize(2);
        writeDesc[0] = InitUpdateDescriptor(m_boxDescriptorSet, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &wBuffer); 
        writeDesc[1] = InitUpdateDescriptor(m_boxDescriptorSet, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &wImage);

        vk::UpdateDescriptorSets(vk::g_vulkanContext.m_device, (uint32_t)writeDesc.size(), writeDesc.data(), 0, nullptr);
    }

private:
    Mesh* m_quadMesh;

    BufferHandle*       m_boxParamsBuffer;

    CTexture*           m_skyTexture;
    //CTexture*           m_sunTexture;
    CGraphicPipeline           m_boxPipeline;

    VkDescriptorSet     m_boxDescriptorSet;
    VkDescriptorSetLayout m_boxDescriptorSetLayout;

    VkDescriptorSetLayout       m_sunDescriptorSetLayout;
    VkDescriptorSet             m_sunDescriptorSet;
    CGraphicPipeline                   m_sunPipeline;

    VkSampler                   m_sampler;
};

class CApplication
{
public:
    CApplication();
    virtual ~CApplication();

    void Run();
    void Render();

	static float GetDeltaTime() { return ms_dt; }
private:
    void InitWindow();
    void CenterCursor();
    void HideCursor(bool hide);

    void SetupDeferredRendering();
    void SetupAORendering();
    void SetupDirectionalLightingRendering();
	void SetupDeferredTileShading();
    void SetupShadowMapRendering();
    void SetupShadowResolveRendering();
    void SetupPostProcessRendering();
    void SetupSunRendering();
    void SetupUIRendering();
    void SetupSkyRendering();
    void SetupParticleRendering();
    void SetupFogRendering();
    void Setup3DTextureRendering();
    void SetupVolumetricRendering();
	void SetupScreenSpaceReflectionsRendering();
	void SetupTerrainRendering();
	void SetupVegetationRendering();
	void SetupTestRendering();

    void CreateDeferredRenderPass(const FramebufferDescription& fbDesc);
    void CreateAORenderPass(const FramebufferDescription& fbDesc);
    void CreateDirLightingRenderPass(const FramebufferDescription& fbDesc);
    void CreatePointLightingRenderPass(const FramebufferDescription& fbDesc);
	void CreateDeferredTileShadingRenderPass(const FramebufferDescription& fDesc);
    void CreateShadowRenderPass(const FramebufferDescription& fbDesc);
    void CreateShadowResolveRenderPass(const FramebufferDescription& fbDesc);
    void CreatePostProcessRenderPass(const FramebufferDescription& fbDesc);
    void CreateSunRenderPass(const FramebufferDescription& fbDesc);
    void CreateUIRenderPass(const FramebufferDescription& fbDesc);
    void CreateSkyRenderPass(const FramebufferDescription& fbDesc);
    void CreateParticlesRenderPass(const FramebufferDescription& fbDesc);
    void CreateFogRenderPass(const FramebufferDescription& fbDesc);
    void Create3DTextureRenderPass(const FramebufferDescription& fbDesc);
    void CreateVolumetricRenderPass(const FramebufferDescription& fbDesc);
	void CreateSSRRenderPass(const FramebufferDescription& fbDesc);
	void CreateTerrainRenderPass(const FramebufferDescription& fbDesc);
	void CreateVegetationRenderPass(const FramebufferDescription& fbDesc);
	void CreateTestRenderPass(const FramebufferDescription& fbDesc);
  
    void CreateQueryPools();

    void SetupParticles();
	void RegisterSpecialInputListeners(); //TODO find a better solution at refactoring

    void CreateResources();
    void UpdateCameraRotation();
    static LRESULT CALLBACK WindowProc(
        HWND   hwnd,
        UINT   uMsg,
        WPARAM wParam,
        LPARAM lParam
        );

    void Reset();

    void ProcMsg(UINT uMsg, WPARAM wParam,LPARAM lParam);
    //void TransferPickData();
    //window paramsm_
    const char* m_windowName;
    const char* m_windowClass;
    HWND        m_windowHandle;
    HINSTANCE   m_appInstance;
    //rendering context

    VkRenderPass                m_deferredRenderPass;
    VkRenderPass                m_aoRenderPass;
    VkRenderPass                m_dirLightRenderPass;
    VkRenderPass                m_pointLightRenderPass;
	VkRenderPass				m_deferredTileShadingRenderPass;
    VkRenderPass                m_shadowRenderPass;
    VkRenderPass                m_shadowResolveRenderPass;
    VkRenderPass                m_postProcessPass;
    VkRenderPass                m_sunRenderPass;
    VkRenderPass                m_uiRenderPass;
    VkRenderPass                m_skyRenderPass;
    VkRenderPass                m_particlesRenderPass;
    VkRenderPass                m_fogRenderPass;
    VkRenderPass                m_3DtextureRenderPass;
    VkRenderPass                m_volumetricRenderPass;
	VkRenderPass				m_ssrRenderPass;
	VkRenderPass				m_terrainRenderPass;
	VkRenderPass				m_vegetationRenderPass;
	VkRenderPass				m_testRenderPass;

    //CCubeMapTexture*            m_skyTextureCube;
    CTexture*                   m_skyTexture2D;
    CTexture*                   m_sunTexture;

    //particles
    CConeSpawner*               m_coneSpawner;
    CRectangularSpawner*        m_rectangularSpawner;
    CParticleSystem*            m_smokeParticleSystem;
    CParticleSystem*            m_rainParticleSystem;
    CTexture*                   m_smokeTexture;
    CTexture*                   m_rainTexture;

    ObjectRenderer*             m_objectRenderer;
	PointLightRenderer2*		m_pointLightRenderer2;
    CParticlesRenderer*         m_particlesRenderer;
    CShadowResolveRenderer*     m_shadowResolveRenderer;
    CSkyRenderer*               m_skyRenderer;
    CFogRenderer*               m_fogRenderer;
    C3DTextureRenderer*         m_3dTextureRenderer;
    CVolumetricRenderer*        m_volumetricRenderer;
    CSunRenderer*               m_sunRenderer;
    CUIRenderer*                m_uiRenderer;
	ScreenSpaceReflectionsRenderer*	m_ssrRenderer;
	TestRenderer*				m_testRenderer;

    //bool                        m_pickRecorded;
    bool                        m_screenshotRequested;
    WORD                        m_xPickCoord;
    WORD                        m_yPickCoord;

    bool                        m_centerCursor;
    bool                        m_hideCursor;
    bool                        m_mouseMoved;
    float                       m_normMouseDX;
    float                       m_normMouseDY;
    static float                ms_dt;

    bool                        m_needReset;
};

float GetDeltaTime()
{
	return CApplication::GetDeltaTime();
}

float CApplication::ms_dt = 0.0f;

CApplication::CApplication()
	: m_windowClass(WNDCLASSNAME)
	, m_windowName(WNDNAME)
	, m_deferredRenderPass(VK_NULL_HANDLE)
	, m_aoRenderPass(VK_NULL_HANDLE)
	, m_dirLightRenderPass(VK_NULL_HANDLE)
	, m_pointLightRenderPass(VK_NULL_HANDLE)
	, m_deferredTileShadingRenderPass(VK_NULL_HANDLE)
	, m_shadowRenderPass(VK_NULL_HANDLE)
	, m_shadowResolveRenderPass(VK_NULL_HANDLE)
	, m_postProcessPass(VK_NULL_HANDLE)
	, m_sunRenderPass(VK_NULL_HANDLE)
	, m_uiRenderPass(VK_NULL_HANDLE)
	, m_fogRenderPass(VK_NULL_HANDLE)
	, m_3DtextureRenderPass(VK_NULL_HANDLE)
	, m_volumetricRenderPass(VK_NULL_HANDLE)
	, m_ssrRenderPass(VK_NULL_HANDLE)
	, m_terrainRenderPass(VK_NULL_HANDLE)
	, m_vegetationRenderPass(VK_NULL_HANDLE)
	//, m_skyTextureCube(nullptr)
	, m_skyTexture2D(nullptr)
	, m_sunTexture(nullptr)
	, m_smokeTexture(nullptr)
	, m_pointLightRenderer2(nullptr)
	, m_particlesRenderer(nullptr)
	, m_objectRenderer(nullptr)
	, m_shadowResolveRenderer(nullptr)
	, m_skyRenderer(nullptr)
	, m_fogRenderer(nullptr)
	, m_3dTextureRenderer(nullptr)
	, m_volumetricRenderer(nullptr)
	, m_sunRenderer(nullptr)
	, m_uiRenderer(nullptr)
	, m_ssrRenderer(nullptr)
    , m_screenshotRequested(false)
    , m_centerCursor(true)
    , m_mouseMoved(true)
    , m_hideCursor(false)
    , m_normMouseDX(0.0f)
    , m_normMouseDY(0.0f)
    , m_needReset(false)
{
    vk::Load();
    InitWindow();

	InputManager::CreateInstance();
	Scene::CreateInstance();

	GraphicEngine::CreateInstance();
	GraphicEngine::GetInstance()->Init(m_windowHandle, m_appInstance);
	
	CUIManager::CreateInstance();
	CPickManager::CreateInstance();
	ObjectSerializer::CreateInstance();
	ObjectSerializer::GetInstance()->Load("scene.xml");

    srand((unsigned int)time(NULL));

    FreeImage_Initialise();

	MoveWindow(GetConsoleWindow(), WIDTH, 0, 1920 - WIDTH, HEIGHT, TRUE);
};

CApplication::~CApplication()
{
    FreeImage_DeInitialise();

    VkDevice dev = vk::g_vulkanContext.m_device;

	CUIManager::DestroyInstance();
	ObjectSerializer::DestroyInstance();

	GraphicEngine::DestroyInstance();
	Scene::DestroyInstance();
	
	InputManager::DestroyInstance();
	
    CleanUp();
    vk::Unload();
}


void CApplication::Run()
{
    bool isRunning = true;

    CreateResources();
    CreateQueryPools();
	RegisterSpecialInputListeners();
	//RenderCameraFrustrum();

    DWORD start;
    DWORD stop;
    DWORD dtMs;
    while(isRunning)
    {
        MSG msg;
        start = GetTickCount();
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT)
            {
                isRunning = false;
                break;
            }

            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        
        CUIManager::GetInstance()->Update();
		InputManager::GetInstance()->Update();
		
		UpdateCameraRotation();
		Scene::GetInstance()->Update(ms_dt);
		//ms_camera.Update(); //ugly and i hope so temporary fix
		GraphicEngine::GetInstance()->Update(ms_dt);

        Render();

        HideCursor(m_centerCursor);
        if(m_screenshotRequested)
        {
            m_screenshotManager.WriteScreenshot();
            m_screenshotRequested = false;
        }

        if(m_needReset)
        {
            Reset();
        }

        stop = GetTickCount();
        dtMs = stop - start;
        dtMs = (dtMs > 0)? dtMs : 1;

        ms_dt = (float)(dtMs) / 1000.0f;
    };
}

void CApplication::InitWindow()
{
    UINT width = WIDTH;
    UINT height = HEIGHT;
    WNDCLASSEX wndClassReg;
    cleanStructure(wndClassReg);

    m_appInstance = GetModuleHandle(nullptr);

    wndClassReg.cbSize = sizeof(WNDCLASSEX);
    wndClassReg.style = CS_HREDRAW | CS_VREDRAW;
    wndClassReg.lpfnWndProc = &CApplication::WindowProc;
    wndClassReg.hInstance = m_appInstance;
    wndClassReg.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wndClassReg.lpszClassName = m_windowClass;

    RegisterClassEx(&wndClassReg);

    const DWORD win_style = WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_VISIBLE | WS_OVERLAPPEDWINDOW /*| ~WS_THICKFRAME ^ WS_MAXIMIZEBOX*/;

    RECT win_rect;
    win_rect.top = 0;
    win_rect.left = 0;
    win_rect.bottom = height;
    win_rect.right = width;

    AdjustWindowRect(&win_rect, win_style, false);

    m_windowHandle = CreateWindowEx(WS_EX_APPWINDOW ,
        m_windowClass,
        m_windowName,
        win_style,
        0,
        0,
        win_rect.right - win_rect.left,
        win_rect.bottom - win_rect.top,
        nullptr,
        nullptr,
        m_appInstance,
        nullptr);

    TRAP(m_windowHandle);
    SetForegroundWindow(m_windowHandle);

    SetWindowLongPtr(m_windowHandle, GWLP_USERDATA, (LONG_PTR)this);
    CenterCursor();
}

void CApplication::CenterCursor()
{

    POINT center;
    center.x = WIDTH / 2;
    center.y =  HEIGHT / 2;

    ClientToScreen(m_windowHandle, &center);
    SetCursorPos(center.x, center.y);
    m_mouseMoved = true;
}

void CApplication::HideCursor(bool hide)
{
    if(hide != m_hideCursor)
    {
        ShowCursor(!hide);
        m_hideCursor = hide;
    }
}


void CApplication::SetupDeferredRendering()
{
    FramebufferDescription fbDesc;
    fbDesc.Begin(GBuffer_Count);

    fbDesc.AddColorAttachmentDesc(GBuffer_Albedo, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, "Albedo");
    fbDesc.AddColorAttachmentDesc(GBuffer_Specular, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, "Specular");
    fbDesc.AddColorAttachmentDesc(GBuffer_Normals, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, "Normals");
    fbDesc.AddColorAttachmentDesc(GBuffer_Position, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, "Positions");
    fbDesc.AddColorAttachmentDesc(GBuffer_Debug, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, "DefferedDebug");
    fbDesc.AddColorAttachmentDesc(GBuffer_Final, OUT_FORMAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, "DefferedFinal");

    fbDesc.AddDepthAttachmentDesc(VK_FORMAT_D24_UNORM_S8_UINT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, "Main");
    fbDesc.End();

    CreateDeferredRenderPass(fbDesc);

    m_objectRenderer = new ObjectRenderer(m_deferredRenderPass);
    m_objectRenderer->CreateFramebuffer(fbDesc, WIDTH, HEIGHT);
    m_objectRenderer->Init();

}

void CApplication::SetupAORendering()
{
}

void CApplication::SetupDirectionalLightingRendering()
{
};

void CApplication::SetupDeferredTileShading()
{
	FramebufferDescription fbDesc;
	fbDesc.Begin(1);

	fbDesc.AddColorAttachmentDesc(0, g_commonResources.GetAs<ImageHandle*>(EResourceType_FinalImage));
	fbDesc.End();

	CreateDeferredTileShadingRenderPass(fbDesc);

	m_pointLightRenderer2 = new PointLightRenderer2(m_deferredTileShadingRenderPass);
	m_pointLightRenderer2->Init();
	m_pointLightRenderer2->CreateFramebuffer(fbDesc, WIDTH, HEIGHT);
	m_pointLightRenderer2->InitializeLightGrid();
}

void CApplication::CreateDeferredRenderPass(const FramebufferDescription& fbDesc)
{
    TRAP(fbDesc.m_depthAttachments.IsValid());
    unsigned int size = fbDesc.m_numColors + 1;
    unsigned int depthIndex = size - 1;

    const std::vector<FBAttachment>& colors = fbDesc.m_colorAttachments;
    TRAP(colors.size() == GBuffer_Count);

    std::vector<VkAttachmentDescription> ad;
    ad.resize(size);
    AddAttachementDesc(ad[GBuffer_Final], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, colors[GBuffer_Final].format);
    AddAttachementDesc(ad[GBuffer_Albedo], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, colors[GBuffer_Albedo].format);
    AddAttachementDesc(ad[GBuffer_Specular], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, colors[GBuffer_Specular].format);
    AddAttachementDesc(ad[GBuffer_Normals], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, colors[GBuffer_Normals].format);
    AddAttachementDesc(ad[GBuffer_Position], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, colors[GBuffer_Position].format);
    AddAttachementDesc(ad[GBuffer_Debug], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, colors[GBuffer_Debug].format);
    AddAttachementDesc(ad[depthIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, fbDesc.m_depthAttachments.format); //Depth

    std::vector<VkAttachmentReference> attachment_ref;
    attachment_ref.resize(size);
    
    attachment_ref[GBuffer_Final] = CreateAttachmentReference(GBuffer_Final, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    attachment_ref[GBuffer_Albedo] = CreateAttachmentReference(GBuffer_Albedo, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    attachment_ref[GBuffer_Specular] = CreateAttachmentReference(GBuffer_Specular, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    attachment_ref[GBuffer_Normals] = CreateAttachmentReference(GBuffer_Normals, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    attachment_ref[GBuffer_Position] = CreateAttachmentReference(GBuffer_Position, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    attachment_ref[depthIndex] = CreateAttachmentReference(depthIndex, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    attachment_ref[GBuffer_Debug] = CreateAttachmentReference(GBuffer_Debug, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    std::vector<VkAttachmentReference> defAtts (&attachment_ref[GBuffer_Albedo], &attachment_ref[GBuffer_Albedo] + GBuffer_InputCnt) ;
    std::vector<VkSubpassDescription> sd;
    sd.resize(1);
    sd[0] = CreateSubpassDesc(defAtts.data(), (uint32_t)defAtts.size(), &attachment_ref[depthIndex]);

    VkRenderPassCreateInfo rpci;
    cleanStructure(rpci);
    rpci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpci.pNext = nullptr;
    rpci.flags = 0;
    rpci.attachmentCount = (uint32_t)ad.size(); 
    rpci.pAttachments = ad.data();
    rpci.subpassCount = (uint32_t)sd.size();
    rpci.pSubpasses = sd.data();
    rpci.dependencyCount = 0;
    rpci.pDependencies =  nullptr; 

    VULKAN_ASSERT(vk::CreateRenderPass(vk::g_vulkanContext.m_device, &rpci, nullptr, &m_deferredRenderPass));
}

void CApplication::CreateAORenderPass(const FramebufferDescription& fbDesc)
{
  
}


void CApplication::CreateDirLightingRenderPass(const FramebufferDescription& fbDesc)
{
    std::vector<VkAttachmentDescription> ad;
    ad.resize(EDirLightBuffers_Count);

    AddAttachementDesc(ad[EDirLightBuffers_Final], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, fbDesc.m_colorAttachments[EDirLightBuffers_Final].format, VK_ATTACHMENT_LOAD_OP_LOAD);
    AddAttachementDesc(ad[EDirLightBuffers_Debug], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, fbDesc.m_colorAttachments[EDirLightBuffers_Debug].format);

    std::vector<VkAttachmentReference> attachment_ref;
    attachment_ref.resize(EDirLightBuffers_Count);

    attachment_ref[EDirLightBuffers_Final] = CreateAttachmentReference(EDirLightBuffers_Final, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    attachment_ref[EDirLightBuffers_Debug] = CreateAttachmentReference(EDirLightBuffers_Debug, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    std::vector<VkAttachmentReference> dirLightAtt;
    dirLightAtt.reserve(2);
    dirLightAtt.push_back(attachment_ref[EDirLightBuffers_Final]);
    dirLightAtt.push_back(attachment_ref[EDirLightBuffers_Debug]);

    std::vector<VkSubpassDescription> sd;
    sd.resize(ELightSubpass_DirCount);
    sd[ELightSubpass_Directional] = CreateSubpassDesc(dirLightAtt.data(), (uint32_t)dirLightAtt.size());
    
    std::vector<VkSubpassDependency> subDeps;
	subDeps.push_back(CreateSubpassDependency(VK_SUBPASS_EXTERNAL, ELightSubpass_Directional, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT));

    VkRenderPassCreateInfo rpci;
    cleanStructure(rpci);
    rpci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpci.pNext = nullptr;
    rpci.flags = 0;
    rpci.attachmentCount = (uint32_t)ad.size(); 
    rpci.pAttachments = ad.data();
    rpci.subpassCount = (uint32_t)sd.size();
    rpci.pSubpasses = sd.data();
    rpci.dependencyCount = (uint32_t)subDeps.size();
    rpci.pDependencies =  subDeps.data();

    VULKAN_ASSERT(vk::CreateRenderPass(vk::g_vulkanContext.m_device, &rpci, nullptr, &m_dirLightRenderPass));
}

void CApplication::CreatePointLightingRenderPass(const FramebufferDescription& fbDesc)
{
    std::vector<VkAttachmentDescription> ad;
    ad.resize(EPointLightBuffer_Count);

    AddAttachementDesc(ad[EPointLightBuffer_Final], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, fbDesc.m_colorAttachments[EPointLightBuffer_Final].format, VK_ATTACHMENT_LOAD_OP_LOAD);
    AddAttachementDesc(ad[EPointLightBuffer_Debug], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, fbDesc.m_colorAttachments[EPointLightBuffer_Debug].format);
	AddAttachementDesc(ad[EPointLightBuffer_Accum], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, fbDesc.m_colorAttachments[EPointLightBuffer_Accum].format);
    AddAttachementDesc(ad[EPointLightBuffer_Depth], VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, fbDesc.m_depthAttachments.format, VK_ATTACHMENT_LOAD_OP_LOAD);

    std::vector<VkAttachmentReference> attachment_ref;
    attachment_ref.resize(EPointLightBuffer_Count);

    attachment_ref[EPointLightBuffer_Final] = CreateAttachmentReference(EPointLightBuffer_Final, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	attachment_ref[EPointLightBuffer_Accum] = CreateAttachmentReference(EPointLightBuffer_Accum, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    attachment_ref[EPointLightBuffer_Debug] = CreateAttachmentReference(EPointLightBuffer_Debug, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    attachment_ref[EPointLightBuffer_Depth] = CreateAttachmentReference(EPointLightBuffer_Depth, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

	std::vector<VkAttachmentReference> pointLightAtt;
	pointLightAtt.push_back(attachment_ref[EPointLightBuffer_Accum]);
	pointLightAtt.push_back(attachment_ref[EPointLightBuffer_Debug]);

    std::vector<VkSubpassDescription> sd;
    sd.resize(ELightSubpass_PointCount);

    sd[ELightSubpass_PrePoint] = CreateSubpassDesc(nullptr, 0, &attachment_ref[EPointLightBuffer_Depth]);
    sd[ELightSubpass_PointAccum] = CreateSubpassDesc(pointLightAtt.data(), (uint32_t)pointLightAtt.size(), &attachment_ref[EPointLightBuffer_Depth]);
	sd[ELightSubpass_Blend] = CreateSubpassDesc(&attachment_ref[EPointLightBuffer_Final], 1, nullptr);

    std::vector<VkSubpassDependency> subDeps;
    subDeps.push_back(CreateSubpassDependency(VK_SUBPASS_EXTERNAL, ELightSubpass_PointAccum, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT));

	subDeps.push_back(CreateSubpassDependency(VK_SUBPASS_EXTERNAL, ELightSubpass_PrePoint, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT));

    subDeps.push_back(CreateSubpassDependency(ELightSubpass_PrePoint, ELightSubpass_PointAccum, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT));

	subDeps.push_back(CreateSubpassDependency(ELightSubpass_PointAccum, ELightSubpass_Blend, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT));


    VkRenderPassCreateInfo rpci;
    cleanStructure(rpci);
    rpci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpci.pNext = nullptr;
    rpci.flags = 0;
    rpci.attachmentCount = (uint32_t)ad.size(); 
    rpci.pAttachments = ad.data();
    rpci.subpassCount = (uint32_t)sd.size();
    rpci.pSubpasses = sd.data();
    rpci.dependencyCount = (uint32_t)subDeps.size();
    rpci.pDependencies =  subDeps.data();

    VULKAN_ASSERT(vk::CreateRenderPass(vk::g_vulkanContext.m_device, &rpci, nullptr, &m_pointLightRenderPass));
}

void CApplication::CreateDeferredTileShadingRenderPass(const FramebufferDescription& fbDesc)
{
	std::vector<VkAttachmentDescription> ad(1);
	AddAttachementDesc(ad[0], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, fbDesc.m_colorAttachments[0].format, VK_ATTACHMENT_LOAD_OP_LOAD);

	std::vector<VkAttachmentReference> attRef(1);
	attRef[0] = CreateAttachmentReference(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

	std::vector<VkSubpassDescription> sd;
	sd.push_back(CreateSubpassDesc(attRef.data(), (uint32_t)attRef.size(), nullptr));

	std::vector<VkSubpassDependency> subDeps;
	subDeps.push_back(CreateSubpassDependency(VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_DEPENDENCY_BY_REGION_BIT));

	NewRenderPass(&m_deferredTileShadingRenderPass, ad, sd, subDeps);
}

void CApplication::SetupShadowMapRendering()
{
}

 void CApplication::SetupShadowResolveRendering()
 {
     FramebufferDescription fbDesc;
     fbDesc.Begin(3);
     fbDesc.AddColorAttachmentDesc(0, VK_FORMAT_R32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT, "ShadowResolveFinal");
     fbDesc.AddColorAttachmentDesc(1, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT, "ShadowResolveDebug");
     fbDesc.AddColorAttachmentDesc(2, VK_FORMAT_R32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT, "ShadowResolveBlur");
     fbDesc.End();

     CreateShadowResolveRenderPass(fbDesc);
     m_shadowResolveRenderer = new CShadowResolveRenderer(m_shadowResolveRenderPass);
     m_shadowResolveRenderer->CreateFramebuffer(fbDesc, WIDTH, HEIGHT);
     m_shadowResolveRenderer->Init();
 }

void CApplication::SetupPostProcessRendering()
{
}

void CApplication::SetupSunRendering()
{
    FramebufferDescription fbDesc;
    fbDesc.Begin(ESunFB_Count);

    fbDesc.AddColorAttachmentDesc(ESunFB_Final, VK_FORMAT_R16G16B16A16_SFLOAT,  VK_IMAGE_USAGE_SAMPLED_BIT, "SunFinal");
    fbDesc.AddColorAttachmentDesc(ESunFB_Sun, VK_FORMAT_R16G16B16A16_SFLOAT,  VK_IMAGE_USAGE_SAMPLED_BIT, "SunSprite");
    fbDesc.AddColorAttachmentDesc(ESunFB_Blur1, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT, "SunBlur1");
    fbDesc.AddColorAttachmentDesc(ESunFB_Blur2, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT, "SunBlur2");
    fbDesc.End();

    CreateSunRenderPass(fbDesc);

    m_sunRenderer = new CSunRenderer(m_sunRenderPass);
    unsigned int div = 2;
    m_sunRenderer->CreateFramebuffer(fbDesc, WIDTH / div, HEIGHT / div);
    m_sunRenderer->Init();
    
}

void CApplication::SetupUIRendering()
{
    FramebufferDescription fbDesc;
    fbDesc.Begin(1);

	fbDesc.AddColorAttachmentDesc(0, g_commonResources.GetAs<ImageHandle*>(EResourceType_AfterPostProcessImage));
	fbDesc.AddDepthAttachmentDesc(g_commonResources.GetAs<ImageHandle*>(EResourceType_DepthBufferImage));
    fbDesc.End();

    CreateUIRenderPass(fbDesc);

    m_uiRenderer = new CUIRenderer(m_uiRenderPass);
    m_uiRenderer->Init();
    m_uiRenderer->CreateFramebuffer(fbDesc, WIDTH, HEIGHT);
}

void CApplication::SetupSkyRendering()
{
    FramebufferDescription fbDesc;
    fbDesc.Begin(1);

	fbDesc.AddColorAttachmentDesc(0, g_commonResources.GetAs<ImageHandle*>(EResourceType_FinalImage));
	fbDesc.AddDepthAttachmentDesc(g_commonResources.GetAs<ImageHandle*>(EResourceType_DepthBufferImage));
    fbDesc.End();

    CreateSkyRenderPass(fbDesc);

    m_skyRenderer = new CSkyRenderer(m_skyRenderPass);
    m_skyRenderer->Init();
    m_skyRenderer->CreateFramebuffer(fbDesc, WIDTH, HEIGHT);
}

void CApplication::SetupParticleRendering()
{
    FramebufferDescription fbDesc;
    fbDesc.Begin(1);

	fbDesc.AddColorAttachmentDesc(0, g_commonResources.GetAs<ImageHandle*>(EResourceType_FinalImage));
	fbDesc.AddDepthAttachmentDesc(g_commonResources.GetAs<ImageHandle*>(EResourceType_DepthBufferImage));
    fbDesc.End();

    CreateParticlesRenderPass(fbDesc);

    m_particlesRenderer = new CParticlesRenderer(m_particlesRenderPass);
    m_particlesRenderer->Init();
    m_particlesRenderer->CreateFramebuffer(fbDesc, WIDTH, HEIGHT);
}

 void CApplication::SetupFogRendering()
 {
     FramebufferDescription fbDesc;
     fbDesc.Begin(2);
	 fbDesc.AddColorAttachmentDesc(0, g_commonResources.GetAs<ImageHandle*>(EResourceType_FinalImage));
     fbDesc.AddColorAttachmentDesc(1, VK_FORMAT_R16G16B16A16_SFLOAT, 0, "FogDebug"); 
     fbDesc.End();

     CreateFogRenderPass(fbDesc);

     m_fogRenderer = new CFogRenderer(m_fogRenderPass);
     m_fogRenderer->Init();
     m_fogRenderer->CreateFramebuffer(fbDesc, WIDTH, HEIGHT);
 }

 void CApplication::Setup3DTextureRendering()
 {
     FramebufferDescription fbDesc;
     fbDesc.Begin(2);
     fbDesc.AddColorAttachmentDesc(0, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, "3DTexture", TEXTURE3DLAYERS);
     fbDesc.AddColorAttachmentDesc(1, VK_FORMAT_R16G16B16A16_SFLOAT, 0, "3DTextureDebug", TEXTURE3DLAYERS);
     fbDesc.End();

     Create3DTextureRenderPass(fbDesc);

	 //TODO FIX IT. NO NEED FOR FRAMEBUFFER
     unsigned int fbSize = 2;
     m_3dTextureRenderer = new C3DTextureRenderer(m_3DtextureRenderPass);
     m_3dTextureRenderer->CreateFramebuffer(fbDesc, fbSize, fbSize);
     m_3dTextureRenderer->Init();
 }

 void CApplication::SetupVolumetricRendering()
 {
    FramebufferDescription fbDesc;

    fbDesc.Begin(4);
	fbDesc.AddDepthAttachmentDesc(g_commonResources.GetAs<ImageHandle*>(EResourceType_DepthBufferImage));
    fbDesc.AddColorAttachmentDesc(0, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT, "VolumetricFrontCull");
    fbDesc.AddColorAttachmentDesc(1, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT, "VolumetricBackCull");
	fbDesc.AddColorAttachmentDesc(2, g_commonResources.GetAs<ImageHandle*>(EResourceType_FinalImage));
    fbDesc.AddColorAttachmentDesc(3, VK_FORMAT_R16G16B16A16_SFLOAT, 0, "VolumetricDebug");
    fbDesc.End();

    CreateVolumetricRenderPass(fbDesc);
    m_volumetricRenderer = new CVolumetricRenderer(m_volumetricRenderPass);
    m_volumetricRenderer->CreateFramebuffer(fbDesc, WIDTH, HEIGHT);
    m_volumetricRenderer->Init();
 }
 
 void CApplication::SetupScreenSpaceReflectionsRendering()
 {
	 FramebufferDescription fbDesc;
	 fbDesc.Begin(4);
	 fbDesc.AddColorAttachmentDesc(0, g_commonResources.GetAs<ImageHandle*>(EResourceType_FinalImage));
	 fbDesc.AddColorAttachmentDesc(1, VK_FORMAT_R16G16B16A16_SFLOAT, 0, "SSRDebug");
	 fbDesc.AddColorAttachmentDesc(2, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT, "SSRLightBlurV");
	 fbDesc.AddColorAttachmentDesc(3, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT, "SSRLightBlurH");

	 fbDesc.End();

	 CreateSSRRenderPass(fbDesc);
	 m_ssrRenderer = new ScreenSpaceReflectionsRenderer(m_ssrRenderPass);
	 m_ssrRenderer->CreateFramebuffer(fbDesc, WIDTH, HEIGHT );
	 m_ssrRenderer->Init();
 }

 void CApplication::SetupTerrainRendering()
 {
 }

void CApplication::SetupVegetationRendering()
{
}

void CApplication::SetupTestRendering()
{
	FramebufferDescription fbDesc;
	fbDesc.Begin(0);
	fbDesc.AddDepthAttachmentDesc(VK_FORMAT_D24_UNORM_S8_UINT, 0, "TestDepth", 3);
	fbDesc.End();

	CreateTestRenderPass(fbDesc);

	m_testRenderer = new TestRenderer(m_testRenderPass);
	m_testRenderer->CreateFramebuffer(fbDesc, WIDTH, HEIGHT, 3);
	m_testRenderer->Init();
}

void CApplication::CreateShadowRenderPass(const FramebufferDescription& fbDesc)
{
    TRAP(fbDesc.m_depthAttachments.IsValid());
    VkAttachmentDescription ad;
    AddAttachementDesc(ad, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, fbDesc.m_depthAttachments.format);

    VkAttachmentReference attachment_ref = CreateAttachmentReference(0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    VkSubpassDescription sd = CreateSubpassDesc(nullptr, 0, &attachment_ref);

    VkRenderPassCreateInfo rpci;
    cleanStructure(rpci);
    rpci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpci.pNext = nullptr;
    rpci.flags = 0;
    rpci.attachmentCount =  1; //depth
    rpci.pAttachments = &ad;
    rpci.subpassCount = 1;
    rpci.pSubpasses = &sd;
    rpci.dependencyCount = 0;
    rpci.pDependencies =  nullptr; 

    VULKAN_ASSERT(vk::CreateRenderPass(vk::g_vulkanContext.m_device, &rpci, nullptr, &m_shadowRenderPass));
}

void CApplication::CreateShadowResolveRenderPass(const FramebufferDescription& fbDesc)
{
    std::vector<VkAttachmentDescription> ad;
    ad.resize(3);
    AddAttachementDesc(ad[0], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, fbDesc.m_colorAttachments[0].format);
    AddAttachementDesc(ad[1], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, fbDesc.m_colorAttachments[1].format);
    AddAttachementDesc(ad[2], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, fbDesc.m_colorAttachments[2].format);

    std::vector<VkAttachmentReference> attachment_ref;
    attachment_ref.push_back(CreateAttachmentReference(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));
    attachment_ref.push_back(CreateAttachmentReference(1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));
    attachment_ref.push_back(CreateAttachmentReference(2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));

    std::vector<VkSubpassDescription> sd;
    sd.push_back(CreateSubpassDesc(attachment_ref.data(), 2));
#ifdef USE_SHADOW_BLUR
    sd.push_back(CreateSubpassDesc(&attachment_ref[2], 1));
    sd.push_back(CreateSubpassDesc(&attachment_ref[0], 1));
#endif

    std::vector<VkSubpassDependency> sub_deps;
	sub_deps.push_back(CreateSubpassDependency(VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT));
#ifdef USE_SHADOW_BLUR
    sub_deps.push_back(CreateSubpassDependency(0, 1, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_DEPENDENCY_BY_REGION_BIT));

    sub_deps.push_back(CreateSubpassDependency(1, 2, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_DEPENDENCY_BY_REGION_BIT));
#endif
    VkRenderPassCreateInfo rpci;
    cleanStructure(rpci);
    rpci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpci.pNext = nullptr;
    rpci.flags = 0;
    rpci.attachmentCount =  (uint32_t)ad.size();
    rpci.pAttachments = ad.data();
    rpci.subpassCount = (uint32_t)sd.size();
    rpci.pSubpasses = sd.data();
    rpci.dependencyCount = (uint32_t)sub_deps.size();
    rpci.pDependencies =  sub_deps.data(); 

    VULKAN_ASSERT(vk::CreateRenderPass(vk::g_vulkanContext.m_device, &rpci, nullptr, &m_shadowResolveRenderPass));
}

void CApplication::CreatePostProcessRenderPass(const FramebufferDescription& fbDesc)
{
    TRAP(fbDesc.m_colorAttachments.size() == 1);
    VkAttachmentDescription ad;
    AddAttachementDesc(ad, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, fbDesc.m_colorAttachments[0].format);

    VkAttachmentReference attachment_ref = CreateAttachmentReference(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkSubpassDescription sd = CreateSubpassDesc(&attachment_ref, 1);
    std::vector<VkSubpassDependency> subpassDeps; 
    subpassDeps.push_back(CreateSubpassDependency(VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_DEPENDENCY_BY_REGION_BIT));
    
    VkRenderPassCreateInfo rpci;
    cleanStructure(rpci);
    rpci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpci.pNext = nullptr;
    rpci.flags = 0;
    rpci.attachmentCount =  1;
    rpci.pAttachments = &ad;
    rpci.subpassCount = 1;
    rpci.pSubpasses = &sd;
    rpci.dependencyCount = (uint32_t)subpassDeps.size();
    rpci.pDependencies =  subpassDeps.data();

    VULKAN_ASSERT(vk::CreateRenderPass(vk::g_vulkanContext.m_device, &rpci, nullptr, &m_postProcessPass));
}

void CApplication::CreateSunRenderPass(const FramebufferDescription& fbDesc)
{
    VkAttachmentDescription ad[ESunFB_Count];
    AddAttachementDesc(ad[ESunFB_Final], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, fbDesc.m_colorAttachments[ESunFB_Final].format);
    AddAttachementDesc(ad[ESunFB_Sun], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, fbDesc.m_colorAttachments[ESunFB_Sun].format);
    AddAttachementDesc(ad[ESunFB_Blur1], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, fbDesc.m_colorAttachments[ESunFB_Blur1].format);
    AddAttachementDesc(ad[ESunFB_Blur2], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, fbDesc.m_colorAttachments[ESunFB_Blur2].format);
    //AddAttachementDesc(ad[EBloomFB_Depth], VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, fbDesc.m_depthAttachments.format, VK_ATTACHMENT_LOAD_OP_LOAD);

    VkAttachmentReference attRef[ESunFB_Count];
    for(unsigned int i = 0 ; i < ESunFB_Count; ++i)
        attRef[i] = CreateAttachmentReference(i, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    VkSubpassDescription sd[ESunPass_Count];
    sd[ESunPass_Sun] = CreateSubpassDesc(&attRef[ESunFB_Sun], 1);
    sd[ESunPass_BlurV] = CreateSubpassDesc(&attRef[ESunFB_Blur1], 1);
    sd[ESunPass_BlurH] = CreateSubpassDesc(&attRef[ESunFB_Blur2], 1);
    sd[ESunPass_BlurRadial] = CreateSubpassDesc(&attRef[ESunFB_Final], 1);

    std::vector<VkSubpassDependency> subDep;
    subDep.push_back(CreateSubpassDependency(ESunPass_BlurH, ESunPass_BlurRadial, 
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_DEPENDENCY_BY_REGION_BIT));

    subDep.push_back(CreateSubpassDependency(ESunPass_Sun, ESunPass_BlurV, 
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_DEPENDENCY_BY_REGION_BIT));

    subDep.push_back(CreateSubpassDependency(ESunPass_BlurV, ESunPass_BlurH, 
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT, VK_DEPENDENCY_BY_REGION_BIT));


    VkRenderPassCreateInfo rpci;
    cleanStructure(rpci);
    rpci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpci.pNext = nullptr;
    rpci.flags = 0;
    rpci.attachmentCount =  ESunFB_Count;
    rpci.pAttachments = ad;
    rpci.subpassCount = ESunPass_Count;
    rpci.pSubpasses = sd;
    rpci.dependencyCount = (uint32_t)subDep.size();
    rpci.pDependencies =  subDep.data();

    VULKAN_ASSERT(vk::CreateRenderPass(vk::g_vulkanContext.m_device, &rpci, nullptr, &m_sunRenderPass));
}

void CApplication::CreateUIRenderPass(const FramebufferDescription& fbDesc)
{
    VkAttachmentDescription ad[2];
    AddAttachementDesc(ad[0], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, fbDesc.m_colorAttachments[0].format, VK_ATTACHMENT_LOAD_OP_LOAD);
    AddAttachementDesc(ad[1], VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, fbDesc.m_depthAttachments.format, VK_ATTACHMENT_LOAD_OP_LOAD);

    VkAttachmentReference attachment_ref[2];
    attachment_ref[0] = CreateAttachmentReference(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    attachment_ref[1] = CreateAttachmentReference(1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

    VkSubpassDescription sd = CreateSubpassDesc(&attachment_ref[0], 1, &attachment_ref[1]);
    VkSubpassDependency subpassDep = CreateSubpassDependency(VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_DEPENDENCY_BY_REGION_BIT);

    VkRenderPassCreateInfo rpci;
    cleanStructure(rpci);
    rpci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpci.pNext = nullptr;
    rpci.flags = 0;
    rpci.attachmentCount =  2;
    rpci.pAttachments = &ad[0];
    rpci.subpassCount = 1;
    rpci.pSubpasses = &sd;
    rpci.dependencyCount = 1;
    rpci.pDependencies =  &subpassDep;

    VULKAN_ASSERT(vk::CreateRenderPass(vk::g_vulkanContext.m_device, &rpci, nullptr, &m_uiRenderPass));
}

void CApplication::CreateSkyRenderPass(const FramebufferDescription& fbDesc)
{
    std::vector<VkAttachmentDescription> ad;
    ad.resize(2);

    AddAttachementDesc(ad[0], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, fbDesc.m_colorAttachments[0].format, VK_ATTACHMENT_LOAD_OP_LOAD);
    AddAttachementDesc(ad[1], VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, fbDesc.m_depthAttachments.format, VK_ATTACHMENT_LOAD_OP_LOAD);

    std::vector<VkAttachmentReference> attRef;
    attRef.push_back(CreateAttachmentReference(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));
    attRef.push_back(CreateAttachmentReference(1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL));

    std::vector<VkSubpassDescription> sd;

    sd.push_back(CreateSubpassDesc(&attRef[0], 1, &attRef[1]));
    sd.push_back(CreateSubpassDesc(&attRef[0], 1));

    std::vector<VkSubpassDependency> subpassDep;
	subpassDep.push_back(CreateSubpassDependency(VK_SUBPASS_EXTERNAL, 0, 
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
		VK_DEPENDENCY_BY_REGION_BIT));

	subpassDep.push_back(CreateSubpassDependency(0, 1, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT, VK_DEPENDENCY_BY_REGION_BIT));


    VkRenderPassCreateInfo rpci;
    cleanStructure(rpci);
    rpci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpci.pNext = nullptr;
    rpci.flags = 0;
    rpci.attachmentCount = (uint32_t)ad.size();
    rpci.pAttachments = ad.data();
    rpci.subpassCount = (uint32_t)sd.size();
    rpci.pSubpasses = sd.data();
    rpci.dependencyCount = (uint32_t)subpassDep.size();
    rpci.pDependencies =  subpassDep.data();

    VULKAN_ASSERT(vk::CreateRenderPass(vk::g_vulkanContext.m_device, &rpci, nullptr, &m_skyRenderPass));
}

void CApplication::CreateParticlesRenderPass(const FramebufferDescription& fbDesc)
{
    VkAttachmentDescription ad[2];
    AddAttachementDesc(ad[0], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, fbDesc.m_colorAttachments[0].format, VK_ATTACHMENT_LOAD_OP_LOAD);
    AddAttachementDesc(ad[1], VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, fbDesc.m_depthAttachments.format, VK_ATTACHMENT_LOAD_OP_LOAD);

    VkAttachmentReference attachment_ref[2];
    attachment_ref[0] = CreateAttachmentReference(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    attachment_ref[1] = CreateAttachmentReference(1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    VkSubpassDescription sd = CreateSubpassDesc(&attachment_ref[0], 1, &attachment_ref[1]);
	VkSubpassDependency subpassDep = CreateSubpassDependency(VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
   
    VkRenderPassCreateInfo rpci;
    cleanStructure(rpci);
    rpci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpci.pNext = nullptr;
    rpci.flags = 0;
    rpci.attachmentCount =  2;
    rpci.pAttachments = ad;
    rpci.subpassCount = 1;
    rpci.pSubpasses = &sd;
    rpci.dependencyCount = 1;
    rpci.pDependencies =  &subpassDep;

    VULKAN_ASSERT(vk::CreateRenderPass(vk::g_vulkanContext.m_device, &rpci, nullptr, &m_particlesRenderPass));
}

void CApplication::CreateFogRenderPass(const FramebufferDescription& fbDesc)
{
    std::vector<VkAttachmentDescription> ad;
    ad.resize(2);
    AddAttachementDesc(ad[0], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, fbDesc.m_colorAttachments[0].format, VK_ATTACHMENT_LOAD_OP_LOAD);
    AddAttachementDesc(ad[1], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, fbDesc.m_colorAttachments[1].format);

    std::vector<VkAttachmentReference> attRef;
    attRef.push_back(CreateAttachmentReference(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));
    attRef.push_back(CreateAttachmentReference(1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));

    VkSubpassDescription sd = CreateSubpassDesc(attRef.data(), (uint32_t)attRef.size());
    VkSubpassDependency subDep = CreateSubpassDependency(VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT, VK_DEPENDENCY_BY_REGION_BIT);

    VkRenderPassCreateInfo rpci;
    cleanStructure(rpci);
    rpci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpci.attachmentCount = (uint32_t)ad.size();
    rpci.pAttachments = ad.data();
    rpci.dependencyCount = 1;
    rpci.pDependencies = &subDep;
    rpci.subpassCount = 1;
    rpci.pSubpasses = &sd;

    VULKAN_ASSERT(vk::CreateRenderPass(vk::g_vulkanContext.m_device, &rpci, nullptr, &m_fogRenderPass));
}

void CApplication::Create3DTextureRenderPass(const FramebufferDescription& fbDesc)
{
    std::vector<VkAttachmentDescription> ad;
    ad.resize( fbDesc.m_colorAttachments.size());
    for(unsigned int i = 0; i < ad.size(); ++i)
        AddAttachementDesc(ad[i], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, fbDesc.m_colorAttachments[i].format);

    std::vector<VkAttachmentReference> attRef;
    for(unsigned int i = 0; i < ad.size(); ++i)
        attRef.push_back(CreateAttachmentReference(i, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));

    std::vector<VkSubpassDescription> subpasses;
    subpasses.push_back(CreateSubpassDesc(&attRef[0], 1));
#ifdef TEST3DTEXT
    subpasses.push_back(CreateSubpassDesc(&attRef[1], 1));
#endif


    std::vector<VkSubpassDependency> dep;
#ifdef TEST3DTEXT
    dep.push_back(CreateSubpassDependency(0, 1, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_DEPENDENCY_BY_REGION_BIT));
#endif

    NewRenderPass(&m_3DtextureRenderPass, ad, subpasses, dep);
}

void CApplication::CreateVolumetricRenderPass(const FramebufferDescription& fbDesc)
{
    std::vector<VkAttachmentDescription> ad;
    ad.push_back(AddAttachementDesc(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, fbDesc.m_colorAttachments[0].format));
    ad.push_back(AddAttachementDesc(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, fbDesc.m_colorAttachments[1].format));
    ad.push_back(AddAttachementDesc(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, fbDesc.m_colorAttachments[2].format, VK_ATTACHMENT_LOAD_OP_LOAD));
    ad.push_back(AddAttachementDesc(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, fbDesc.m_colorAttachments[3].format));
    ad.push_back(AddAttachementDesc(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, fbDesc.m_depthAttachments.format, VK_ATTACHMENT_LOAD_OP_LOAD));

    std::vector<VkAttachmentReference> atRef;
    atRef.push_back(CreateAttachmentReference(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));
    atRef.push_back(CreateAttachmentReference(1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));
    atRef.push_back(CreateAttachmentReference(2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));
    atRef.push_back(CreateAttachmentReference(3, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));
    atRef.push_back(CreateAttachmentReference(4, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL));


    std::vector<VkSubpassDescription> subpasses;
    subpasses.push_back(CreateSubpassDesc(&atRef[0], 1));
    subpasses.push_back(CreateSubpassDesc(&atRef[1], 1));
    subpasses.push_back(CreateSubpassDesc(&atRef[2], 2, &atRef[4]));

    std::vector<VkSubpassDependency> dependecies;
    dependecies.push_back(CreateSubpassDependency(VK_SUBPASS_EXTERNAL, 2, 
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_DEPENDENCY_BY_REGION_BIT));

    dependecies.push_back(CreateSubpassDependency(0, 2, 
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_DEPENDENCY_BY_REGION_BIT));

    dependecies.push_back(CreateSubpassDependency(1, 2, 
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_DEPENDENCY_BY_REGION_BIT));

    NewRenderPass(&m_volumetricRenderPass, ad, subpasses, dependecies);
}

void CApplication::CreateSSRRenderPass(const FramebufferDescription& fbDesc)
{
	std::vector<VkAttachmentDescription> ad;
	ad.resize(fbDesc.m_colorAttachments.size());
	AddAttachementDesc(ad[0], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, fbDesc.m_colorAttachments[0].format, VK_ATTACHMENT_LOAD_OP_LOAD); //final image
	for (unsigned int i = 1; i < ad.size(); ++i)
		AddAttachementDesc(ad[i], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, fbDesc.m_colorAttachments[i].format);

	VkAttachmentReference blurHRef = CreateAttachmentReference(2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	VkAttachmentReference blurVRef = CreateAttachmentReference(3, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

	std::vector<VkAttachmentReference> atRef;
	atRef.push_back(CreateAttachmentReference(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));
	atRef.push_back(CreateAttachmentReference(1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));

	std::vector<VkSubpassDescription> subpasses;
	subpasses.push_back(CreateSubpassDesc(&blurHRef, 1));
	subpasses.push_back(CreateSubpassDesc(&blurVRef, 1));
	subpasses.push_back(CreateSubpassDesc(atRef.data(), 2));

	std::vector<VkSubpassDependency> dependencies;
	dependencies.push_back(CreateSubpassDependency(VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_DEPENDENCY_BY_REGION_BIT));

	dependencies.push_back(CreateSubpassDependency(0, 1, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_DEPENDENCY_BY_REGION_BIT));

	dependencies.push_back(CreateSubpassDependency(1, 2, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_DEPENDENCY_BY_REGION_BIT));

	NewRenderPass(&m_ssrRenderPass, ad, subpasses, dependencies);
}

void CApplication::CreateTerrainRenderPass(const FramebufferDescription& fbDesc)
{
	std::vector<VkAttachmentDescription> ad;
	ad.resize(fbDesc.m_colorAttachments.size() + 1);
	
	for (uint32_t i = 0; i < fbDesc.m_colorAttachments.size(); ++i)
		AddAttachementDesc(ad[i], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, fbDesc.m_colorAttachments[i].format, VK_ATTACHMENT_LOAD_OP_LOAD);

	AddAttachementDesc(ad[fbDesc.m_colorAttachments.size()], VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, fbDesc.m_depthAttachments.format, VK_ATTACHMENT_LOAD_OP_LOAD);

	std::vector<VkAttachmentReference> atRef;
	for (unsigned int i = 0; i < fbDesc.m_colorAttachments.size(); ++i)
		atRef.push_back(CreateAttachmentReference(i, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));

	atRef.push_back(CreateAttachmentReference((uint32_t)fbDesc.m_colorAttachments.size(), VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL));

	std::vector<VkSubpassDescription> subpasses;
	subpasses.push_back(CreateSubpassDesc(atRef.data(), (uint32_t)fbDesc.m_colorAttachments.size(), &atRef[fbDesc.m_colorAttachments.size()]));

	std::vector<VkSubpassDependency> dependencies;
	dependencies.push_back(CreateSubpassDependency(VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_DEPENDENCY_BY_REGION_BIT));

	NewRenderPass(&m_terrainRenderPass, ad, subpasses, dependencies);
}

void CApplication::CreateVegetationRenderPass(const FramebufferDescription& fbDesc)
{
	std::vector<VkAttachmentDescription> ad;
	ad.resize(fbDesc.m_colorAttachments.size() + 1);

	for (uint32_t i = 0; i < fbDesc.m_colorAttachments.size(); ++i)
		AddAttachementDesc(ad[i], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, fbDesc.m_colorAttachments[i].format, VK_ATTACHMENT_LOAD_OP_LOAD);

	AddAttachementDesc(ad[fbDesc.m_colorAttachments.size()], VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, fbDesc.m_depthAttachments.format, VK_ATTACHMENT_LOAD_OP_LOAD);

	std::vector<VkAttachmentReference> atRef;
	for (unsigned int i = 0; i < fbDesc.m_colorAttachments.size(); ++i)
		atRef.push_back(CreateAttachmentReference(i, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));

	atRef.push_back(CreateAttachmentReference((uint32_t)fbDesc.m_colorAttachments.size(), VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL));

	std::vector<VkSubpassDescription> subpasses;
	subpasses.push_back(CreateSubpassDesc(atRef.data(), (uint32_t)fbDesc.m_colorAttachments.size(), &atRef[fbDesc.m_colorAttachments.size()]));

	std::vector<VkSubpassDependency> dependencies;
	dependencies.push_back(CreateSubpassDependency(VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_DEPENDENCY_BY_REGION_BIT));

	NewRenderPass(&m_vegetationRenderPass, ad, subpasses, dependencies);
}

void CApplication::CreateTestRenderPass(const FramebufferDescription& fbDesc)
{
	VkAttachmentDescription depth;
	AddAttachementDesc(depth, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, fbDesc.m_depthAttachments.format);

	VkAttachmentReference depthRef = CreateAttachmentReference(0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

	VkSubpassDescription subpass = CreateSubpassDesc(nullptr, 0, &depthRef);

	NewRenderPass(&m_testRenderPass, { depth }, { subpass });
}

void CApplication::CreateQueryPools()
{
    //VkDevice device = vk::g_vulkanContext.m_device;

    //VkQueryPoolCreateInfo queryInfo;
    //cleanStructure(queryInfo);
    //queryInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    //queryInfo.flags = 0;
    //queryInfo.pNext = nullptr;
    //queryInfo.queryType = VK_QUERY_TYPE_OCCLUSION;
    //queryInfo.queryCount = g_queryCnt;

    //VULKAN_ASSERT(vk::CreateQueryPool(device, &queryInfo, nullptr, &g_occlusionQuerryPool));
    //uint32_t bufferSize = g_queryCnt * 2 * sizeof(uint32_t); //2 pt ca incerc cu availability_bit
    //AllocBufferMemory(g_queryBuffer, g_queryMemory, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    //VULKAN_ASSERT(vk::MapMemory(vk::g_vulkanContext.m_device, g_queryMemory, 0, VK_WHOLE_SIZE, 0, &g_queryBufferPtr)); 
}

//CCubeMapTexture* CreateSkyTexture()
//{
//    std::vector<std::string> facesFilename;
//    facesFilename.push_back("text/side.png");
//    facesFilename.push_back("text/side.png");
//    facesFilename.push_back("text/side.png");
//    facesFilename.push_back("text/side.png");
//    facesFilename.push_back("text/side.png");
//    facesFilename.push_back("text/side.png");
//
//    return CreateCubeMapTexture(facesFilename);
//}

CTexture* Create2DSkyTexture()
{
    SImageData imgData;
    Read2DTextureData(imgData, std::string(TEXTDIR) + "side.png", true);
    return new CTexture(imgData, true);
}
void CApplication::SetupParticles()
{
    SImageData smokeImg;
    Read2DTextureData(smokeImg, std::string(TEXTDIR) + "smoke0.png", true);
    m_smokeTexture = new CTexture(smokeImg, true);

    SImageData rainImg;
    Read2DTextureData(rainImg, std::string(TEXTDIR) + "raindrop.png", true);
    m_rainTexture = new CTexture(rainImg, true);

    m_coneSpawner = new CConeSpawner(30.0f, glm::vec3(0.0f, -1.0f, -1.25f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(-1.0f, 0.0f, 0.0f)); //pentru asta am valori default potrivite
    m_smokeParticleSystem = new CParticleSystem(m_coneSpawner);
    m_smokeParticleSystem->SetUpdateShaderFile("smokeupdate.comp");
    m_smokeParticleSystem->SetParticleTexture(m_smokeTexture);

    {
        m_rectangularSpawner = new CRectangularSpawner(2.5f, 5.0f, glm::vec3(0.0f, 3.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        m_rectangularSpawner->SetFadeTime(0.1f);
        m_rectangularSpawner->SetParticleLifeSpawn(5.0f);
        m_rectangularSpawner->SetParticleLifeSpawnTreshold(0.5f);
        m_rectangularSpawner->SetParticleSize(0.05f);
        m_rectangularSpawner->SetParticleSizeTreshold(0.001f);
        m_rectangularSpawner->SetParticleSpeed(2.5f);
        m_rectangularSpawner->SetParticleSpeedTreshold(0.0f);

        m_rainParticleSystem = new CParticleSystem(m_rectangularSpawner, 2000);
        m_rainParticleSystem->SetUpdateShaderFile("smokeupdate.comp");
        m_rainParticleSystem->SetParticleTexture(m_rainTexture);

    }
    //m_particlesRenderer->Register(m_rainParticleSystem);
    m_particlesRenderer->Register(m_smokeParticleSystem);
}

void CApplication::RegisterSpecialInputListeners()
{
}

void CApplication::CreateResources()
{
    bool isSrgb = true;

    m_skyTexture2D = Create2DSkyTexture();

    SImageData sun;
    Read2DTextureData(sun, std::string(TEXTDIR) + "sun2.png", false);
    m_sunTexture = new CTexture(sun, true);

    //CRenderer::UpdateAll();

    //CUIManager::GetInstance()->SetupRenderer(m_uiRenderer);

    /*m_particlesRenderer->RegisterDebugManager();

    m_sunRenderer->SetSunTexture(m_sunTexture);
    m_sunRenderer->CreateEditInfo();

    m_skyRenderer->SetTexture(m_skyTexture2D);
    m_skyRenderer->SetSkyImageView();

    GetPickManager()->CreateDebug();
    directionalLight.CreateDebug();*/
    //SetupParticles();

    //SetupPointLights();
}

void CApplication::UpdateCameraRotation() //this function need to be addressed
{
    if(!m_centerCursor)
        return;

    int treshold = 0;
    static int centerx = WIDTH / 2;
    static int centery = HEIGHT / 2;

    POINT p;
    GetCursorPos(&p);
    ScreenToClient(m_windowHandle, &p);

    int x = p.x;
    int y = p.y;

    int dx = centerx - (int)x;
    int dy = centery - (int)y;

    if(abs(dx) > 0 || abs(dy) > 0)
    {
        m_normMouseDX = (float)dx / (float)centerx;
        m_normMouseDY = (float)dy / float(centery);

        GraphicEngine::GetActiveCamera().Rotate(m_normMouseDX, m_normMouseDY);
        CenterCursor();
    }
}
LRESULT CApplication::WindowProc(
    HWND   hwnd,
    UINT   uMsg,
    WPARAM wParam,
    LPARAM lParam
    )
{
    CApplication* app = reinterpret_cast<CApplication*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    if(app)
    {
        app->ProcMsg(uMsg, wParam, lParam);
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void CApplication::Render()
{
	GraphicEngine* ge = GraphicEngine::GetInstance();
	ge->PreRender();
	ge->Render();
}


float RandomFloat()
{
    return (float)rand() / (float) RAND_MAX;
}

void CApplication::Reset()
{
    CRenderer::ReloadAll();
    m_needReset = false;
}

void CApplication::ProcMsg(UINT uMsg, WPARAM wParam,LPARAM lParam)
{
    if(uMsg == WM_CLOSE)
    {
        PostQuitMessage(0);
        return;
    }

    
    if(uMsg == WM_ACTIVATE)
    {
        if(wParam == WA_ACTIVE)
        {
            m_centerCursor = true;
        }

        if(wParam == WA_INACTIVE)
        {
            m_centerCursor = false;
        }
        return;
    }


	if (uMsg > WM_MOUSEFIRST && uMsg <= WM_MOUSELAST)
    {
		switch (uMsg)
		{
		case WM_LBUTTONDOWN:
			InputManager::GetInstance()->RegisterMouseEvent(MouseInput::Left, MouseInput::ButtonState::Down, wParam, lParam);
			break;
		case WM_LBUTTONUP:
			InputManager::GetInstance()->RegisterMouseEvent(MouseInput::Left, MouseInput::ButtonState::Up, wParam, lParam);
			break;
		case WM_MBUTTONDOWN:
			InputManager::GetInstance()->RegisterMouseEvent(MouseInput::Middle, MouseInput::ButtonState::Down, wParam, lParam);
			break;
		case WM_MBUTTONUP:
			InputManager::GetInstance()->RegisterMouseEvent(MouseInput::Middle, MouseInput::ButtonState::Up, wParam, lParam);
			break;
		case WM_RBUTTONDOWN:
			InputManager::GetInstance()->RegisterMouseEvent(MouseInput::Right, MouseInput::ButtonState::Down, wParam, lParam);
			break;
		case WM_RBUTTONUP:
			InputManager::GetInstance()->RegisterMouseEvent(MouseInput::Right, MouseInput::ButtonState::Up, wParam, lParam);
			break;
		case WM_MOUSEWHEEL:
			InputManager::GetInstance()->RegisterMouseEvent(MouseInput::Wheel, MouseInput::ButtonState::None, wParam, lParam);
			break;
		}
        return;
    }

    if(uMsg == WM_COPYDATA)
    {
        PCOPYDATASTRUCT pData = (PCOPYDATASTRUCT) lParam;
        if (pData->dwData == MSGSHADERCOMPILED)
            m_needReset = true;

        return;
    }

    if(uMsg == WM_KEYDOWN)
    {
        if (wParam == VK_F2)
        {
            m_centerCursor = !m_centerCursor;
        }

        if (wParam == VK_F1)
        {
            m_needReset = true;
        }

        if (wParam == VK_F5)
        {
            m_screenshotRequested = true;
        }

       /* if (wParam == VK_TAB)
        {
            m_uiManager->ToggleDisplayInfo();
        }*/

		InputManager::GetInstance()->RegisterKeyboardEvent(wParam);
    }
}


glm::vec4 GetPlaneFrom(glm::vec4 p1, glm::vec4 p2, glm::vec4 p3)
{
	glm::vec3 normal = glm::cross(glm::vec3(p2 - p1), glm::vec3(p3 - p1));
	normal = glm::normalize(normal);
	float d = glm::dot(normal, glm::vec3(p1));
	return glm::vec4(normal, d);
}

int main(int argc, char* arg[])
{
	CApplication app;
    app.Run();
    return 0;
}