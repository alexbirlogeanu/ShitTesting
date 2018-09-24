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

#include "MemoryManager.h"

#define OUT_FORMAT VK_FORMAT_R16G16B16A16_SFLOAT
//#define OUT_FORMAT VK_FORMAT_B8G8R8A8_UNORM
CCamera     ms_camera;

struct LightShaderParams
{
    glm::vec4       dirLight;
    glm::vec4       cameraPos;
    glm::vec4       lightIradiance;
    glm::mat4       pointVPMatrix;
    glm::mat4       pointModelMatrix;
    glm::vec4       pointRadius;
    glm::vec4       pointColor;
};
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

class CLightRenderer : public CRenderer
{
public:
    CLightRenderer(VkRenderPass renderPass)
        : CRenderer(renderPass, "LightRenderPass")
        , m_sampler(VK_NULL_HANDLE)
        , m_shaderUniformBuffer(nullptr)
        , m_depthSampler(VK_NULL_HANDLE)
        , m_descriptorSetLayout(VK_NULL_HANDLE)
        , m_descriptorSet(VK_NULL_HANDLE)
    {

    }
    virtual ~CLightRenderer()
    {
        VkDevice dev = vk::g_vulkanContext.m_device;
        vk::DestroySampler(dev, m_sampler, nullptr);
		MemoryManager::GetInstance()->FreeHandle(m_shaderUniformBuffer);
        vk::DestroyDescriptorSetLayout(dev, m_descriptorSetLayout, nullptr);
    }

    virtual void AllocDescriptorSets(VkDescriptorPool descPool)
    {
        VkDescriptorSetAllocateInfo descAllocInfo;
        cleanStructure(descAllocInfo);
        descAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        descAllocInfo.pNext = nullptr;
        descAllocInfo.descriptorPool = descPool;
        descAllocInfo.descriptorSetCount = 1;
        descAllocInfo.pSetLayouts = &m_descriptorSetLayout;

        VULKAN_ASSERT(vk::AllocateDescriptorSets(vk::g_vulkanContext.m_device, &descAllocInfo, &m_descriptorSet));
    }

    virtual void Render()
    {
        VkCommandBuffer cmdBuffer = vk::g_vulkanContext.m_mainCommandBuffer;
        StartRenderPass();
        vk::CmdBindPipeline(cmdBuffer, m_pipeline.GetBindPoint(), m_pipeline.Get());

        vk::CmdBindDescriptorSets(cmdBuffer, m_pipeline.GetBindPoint(), m_pipeline.GetLayout(), 0, 1, &m_descriptorSet, 0, nullptr);

        vk::CmdDraw(cmdBuffer, 4, 1, 0, 0);
        EndRenderPass();
    }

	void PreRender() override
    {
        LightShaderParams* newParams = m_shaderUniformBuffer->GetPtr<LightShaderParams*>();
        newParams->dirLight = directionalLight.GetDirection();
        newParams->cameraPos = glm::vec4(ms_camera.GetPos(), 1);
        newParams->lightIradiance = directionalLight.GetLightIradiance();
        newParams->lightIradiance[3] = directionalLight.GetLightIntensity();
    }

    virtual void PopulatePoolInfo(std::vector<VkDescriptorPoolSize>& poolSize, unsigned int& maxSets) override
    {
        maxSets = 1;

        AddDescriptorType(poolSize, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, GBuffer_InputCnt + 2); //shadow map and aomap
        AddDescriptorType(poolSize, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1);
    }

    virtual void Init() override
    {
        CRenderer::Init();

        AllocDescriptorSets(m_descriptorPool);
        CreateNearestSampler(m_sampler);

        CreateNearestSampler(m_depthSampler);

		m_shaderUniformBuffer = MemoryManager::GetInstance()->CreateBuffer(EMemoryContextType::UniformBuffers, sizeof(LightShaderParams), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

        m_pipeline.SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN);
        m_pipeline.AddBlendState(CGraphicPipeline::CreateDefaultBlendState(), 2);
        m_pipeline.SetVertexShaderFile("light.vert");
        m_pipeline.SetFragmentShaderFile("light.frag");
        m_pipeline.SetDepthTest(false);

        m_pipeline.CreatePipelineLayout(m_descriptorSetLayout);
        m_pipeline.Init(this, m_renderPass, ELightSubpass_Directional);
    }

protected:
    virtual void UpdateGraphicInterface() override
    {
		ImageHandle* shadowMap = g_commonResources.GetAs<ImageHandle*>(EResourceType_ResolvedShadowImage);
		ImageHandle* aoMap = g_commonResources.GetAs<ImageHandle*>(EResourceType_AOBufferImage);

        const unsigned int descSize = GBuffer_InputCnt;
        VkDescriptorImageInfo imgInfo[descSize];
        imgInfo[GBuffer_Albedo].sampler = m_sampler;
		imgInfo[GBuffer_Albedo].imageView = g_commonResources.GetAs<ImageHandle*>(EResourceType_AlbedoImage)->GetView();
        imgInfo[GBuffer_Albedo].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        imgInfo[GBuffer_Normals].sampler = m_sampler;
		imgInfo[GBuffer_Normals].imageView = g_commonResources.GetAs<ImageHandle*>(EResourceType_NormalsImage)->GetView();
        imgInfo[GBuffer_Normals].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        imgInfo[GBuffer_Position].sampler = m_sampler;
		imgInfo[GBuffer_Position].imageView = g_commonResources.GetAs<ImageHandle*>(EResourceType_PositionsImage)->GetView();
        imgInfo[GBuffer_Position].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        imgInfo[GBuffer_Specular].sampler = m_sampler;
		imgInfo[GBuffer_Specular].imageView = g_commonResources.GetAs<ImageHandle*>(EResourceType_SpecularImage)->GetView();
        imgInfo[GBuffer_Specular].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        std::vector<VkWriteDescriptorSet> writeSets;
        writeSets.resize(4);
        cleanStructure(writeSets[0]);
        writeSets[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeSets[0].pNext = nullptr;
        writeSets[0].dstSet = m_descriptorSet;
        writeSets[0].dstBinding = 0;
        writeSets[0].dstArrayElement = 0;
        writeSets[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writeSets[0].descriptorCount = descSize; //this is a little risky. There is no array of sampler in shader. this just work
        writeSets[0].pImageInfo = imgInfo;

        VkDescriptorBufferInfo buffInfo = m_shaderUniformBuffer->GetDescriptor();
        writeSets[1] = InitUpdateDescriptor(m_descriptorSet, GBuffer_InputCnt, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &buffInfo);

        VkDescriptorImageInfo shadowMapDesc;
        shadowMapDesc.sampler = m_depthSampler;
        shadowMapDesc.imageView = shadowMap->GetView();
        shadowMapDesc.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        writeSets[2] = InitUpdateDescriptor(m_descriptorSet, GBuffer_InputCnt + 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &shadowMapDesc);

        VkDescriptorImageInfo aoMapDesc;
        aoMapDesc.sampler = m_sampler;
        aoMapDesc.imageView = aoMap->GetView();
        aoMapDesc.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        writeSets[3] = InitUpdateDescriptor(m_descriptorSet, GBuffer_InputCnt + 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &aoMapDesc);

        vk::UpdateDescriptorSets(vk::g_vulkanContext.m_device, (uint32_t)writeSets.size(), writeSets.data(), 0, nullptr);
    }

    virtual void CreateDescriptorSetLayout()
    {
        const unsigned int size = GBuffer_InputCnt + 1; //+ uniform buffer
        std::vector<VkDescriptorSetLayoutBinding> descCnt;
        descCnt.resize(size);
        cleanStructure(descCnt[GBuffer_Albedo]); //albedo
        descCnt[GBuffer_Albedo] = CreateDescriptorBinding(GBuffer_Albedo, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
        descCnt[GBuffer_Normals] = CreateDescriptorBinding(GBuffer_Normals, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
        descCnt[GBuffer_Position] = CreateDescriptorBinding(GBuffer_Position, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
        descCnt[GBuffer_Specular] = CreateDescriptorBinding(GBuffer_Specular, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
        descCnt[GBuffer_InputCnt] = CreateDescriptorBinding(GBuffer_InputCnt, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);

        VkDescriptorSetLayoutBinding shadowMapDesc = CreateDescriptorBinding(GBuffer_InputCnt + 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
        descCnt.push_back(shadowMapDesc);

        VkDescriptorSetLayoutBinding aoMap = CreateDescriptorBinding( GBuffer_InputCnt + 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
        descCnt.push_back(aoMap);

        VkDescriptorSetLayoutCreateInfo descSetLayout;
        cleanStructure(descSetLayout);
        descSetLayout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descSetLayout.pNext = nullptr;
        descSetLayout.flags = 0;
        descSetLayout.bindingCount = (uint32_t)descCnt.size();
        descSetLayout.pBindings = descCnt.data();

        VULKAN_ASSERT(vk::CreateDescriptorSetLayout(vk::g_vulkanContext.m_device, &descSetLayout, nullptr, &m_descriptorSetLayout));
    }
protected:

    VkSampler                       m_sampler;
    VkSampler                       m_depthSampler;

    BufferHandle*                   m_shaderUniformBuffer;

    VkDescriptorSetLayout           m_descriptorSetLayout;
    VkDescriptorSet                 m_descriptorSet;

    CGraphicPipeline                m_pipeline;
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


struct PostProcessParam
{
    glm::vec4       screenCoords; //x = width, y = height, z = 1/width, w = 1/height
};

class PostProcessRenderer : public CRenderer
{
public:
    PostProcessRenderer(VkRenderPass renderPass)
        : CRenderer(renderPass, "HDRRenderPass")
        , m_sampler(VK_NULL_HANDLE)
        , m_uniformBuffer(VK_NULL_HANDLE)
        , m_quadMesh(nullptr)
        , m_descriptorSetLayout(VK_NULL_HANDLE)
        , m_descriptorSet(VK_NULL_HANDLE)
        , m_linearSampler(VK_NULL_HANDLE)
    {

    }

    virtual ~PostProcessRenderer()
    {
        VkDevice device = vk::g_vulkanContext.m_device;
        vk::DestroySampler(device, m_sampler, nullptr);

        vk::UnmapMemory(device, m_uniformMemory);
        vk::FreeMemory(device, m_uniformMemory, nullptr);
        vk::DestroyBuffer(device, m_uniformBuffer, nullptr);
        vk::DestroySampler(device, m_linearSampler, nullptr);

        vk::DestroyDescriptorSetLayout(device, m_descriptorSetLayout, nullptr);
    }

    virtual void Render()
    {
        VkCommandBuffer cmdBuffer = vk::g_vulkanContext.m_mainCommandBuffer;
        vk::CmdBindPipeline(cmdBuffer, m_pipeline.GetBindPoint(), m_pipeline.Get());
        vk::CmdBindDescriptorSets(cmdBuffer, m_pipeline.GetBindPoint(), m_pipeline.GetLayout(), 0, 1, &m_descriptorSet, 0, nullptr);

        m_quadMesh->Render();
    }

    
    virtual void UpdateShaderParams()
    {
        PostProcessParam params;
        params.screenCoords.x = WIDTH;
        params.screenCoords.y = HEIGHT;
        params.screenCoords.z = 1.0f / WIDTH;
        params.screenCoords.w = 1.0f / HEIGHT;

        memcpy(m_uniformPtr, &params, sizeof(PostProcessParam));
    }
    
    virtual void Init() override
    {
        CRenderer::Init();
        AllocDescriptorSets(m_descriptorPool, m_descriptorSetLayout, &m_descriptorSet);
        CreateNearestSampler(m_sampler);
        CreateLinearSampler(m_linearSampler);

        AllocBufferMemory(m_uniformBuffer, m_uniformMemory, sizeof(PostProcessParam), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
        VULKAN_ASSERT(vk::MapMemory(vk::g_vulkanContext.m_device, m_uniformMemory, 0, sizeof(PostProcessParam), 0, &m_uniformPtr));

        m_quadMesh = CreateFullscreenQuad();

        m_pipeline.SetVertexInputState(Mesh::GetVertexDesc());
        m_pipeline.SetViewport(WIDTH, HEIGHT);
        m_pipeline.SetScissor(WIDTH, HEIGHT);
        m_pipeline.SetVertexShaderFile("screenquad.vert");
        m_pipeline.SetFragmentShaderFile("hdrgamma.frag");
        m_pipeline.SetDepthTest(false);
        m_pipeline.SetDepthWrite(false);
        VkPipelineColorBlendAttachmentState blendAtt = CGraphicPipeline::CreateDefaultBlendState();
        m_pipeline.AddBlendState(blendAtt);

        m_pipeline.CreatePipelineLayout(m_descriptorSetLayout);
        m_pipeline.Init(this, m_renderPass, 0);

        SImageData lutData;
        ReadLUTTextureData(lutData, std::string(TEXTDIR) + "LUT_Warm.png", true);

        m_lut = new CTexture(lutData, true);

    }

protected:

    virtual void PopulatePoolInfo(std::vector<VkDescriptorPoolSize>& poolSize, unsigned int& maxSets) override
    {
        maxSets = 1;
        AddDescriptorType(poolSize, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1);
        AddDescriptorType(poolSize, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2);
    }

    virtual void UpdateResourceTable() override
    {
        UpdateResourceTableForColor(0, EResourceType_AfterPostProcessImage);
    }

    virtual void CreateDescriptorSetLayout()
    {
        std::vector<VkDescriptorSetLayoutBinding> descCnt;
        descCnt.reserve(3);
        descCnt.push_back(CreateDescriptorBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT));
        descCnt.push_back(CreateDescriptorBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT));
        descCnt.push_back(CreateDescriptorBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT));

        VkDescriptorSetLayoutCreateInfo descLayoutCrtInfo;
        cleanStructure(descLayoutCrtInfo);
        descLayoutCrtInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descLayoutCrtInfo.pNext = nullptr;
        descLayoutCrtInfo.flags = 0;
        descLayoutCrtInfo.bindingCount = (uint32_t)descCnt.size();
        descLayoutCrtInfo.pBindings = descCnt.data();

        VULKAN_ASSERT(vk::CreateDescriptorSetLayout(vk::g_vulkanContext.m_device, &descLayoutCrtInfo, nullptr, &m_descriptorSetLayout));
    }

    virtual void UpdateGraphicInterface() override
    {
        VkDescriptorImageInfo imgInfo;
        cleanStructure(imgInfo);
        imgInfo.sampler = m_sampler;
		imgInfo.imageView = g_commonResources.GetAs<ImageHandle*>(EResourceType_FinalImage)->GetView();
        imgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkDescriptorBufferInfo buffInfo;
        cleanStructure(buffInfo);
        buffInfo.buffer = m_uniformBuffer;
        buffInfo.offset = 0;
        buffInfo.range = sizeof(PostProcessParam);

        VkDescriptorImageInfo lutInfo = m_lut->GetTextureDescriptor();

        std::vector<VkWriteDescriptorSet> wDescSet;
        wDescSet.push_back(InitUpdateDescriptor(m_descriptorSet, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &buffInfo));
        wDescSet.push_back(InitUpdateDescriptor(m_descriptorSet, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &imgInfo));
        wDescSet.push_back(InitUpdateDescriptor(m_descriptorSet, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &lutInfo));

        vk::UpdateDescriptorSets(vk::g_vulkanContext.m_device, (uint32_t)wDescSet.size(), wDescSet.data(), 0, nullptr);
    }
private:
    VkSampler       m_sampler;
    VkSampler       m_linearSampler;

    VkBuffer        m_uniformBuffer;
    VkDeviceMemory  m_uniformMemory;
    void*           m_uniformPtr;

    Mesh*                   m_quadMesh;
    CGraphicPipeline        m_pipeline;

    VkDescriptorSet m_descriptorSet;
    VkDescriptorSetLayout m_descriptorSetLayout;

    CTexture*       m_lut;
};

class CApplication
{
public:
    CApplication();
    virtual ~CApplication();

    void Run();
    void Render();
    void SwapBuffers();

    
private:
    void InitWindow();
    void CenterCursor();
    void HideCursor(bool hide);

    VkImage GetFinalOutput();
    void TransferToPresentImage();
    void BeginFrame();

    void CreateSurface();
    void CreateSwapChains();

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

    void CreateCommandBuffer();
  
    void StartDeferredRender();
    void RenderShadows();
    void RenderPostProcess();

    void WaitForFinalImageToComplete();

    void StartCommandBuffer();
    void EndCommandBuffer();
    //pipeline
    void GetQueue();
    void CreateSynchronizationHelpers();

    //void CreateDescriptorPool();
    void CreateQueryPools();

    void SetupParticles();

	//TESTING
	void RenderCameraFrustrum();

    void CreateResources();
    void UpdateCamera();
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
    VkSurfaceKHR                m_surface;
    VkSwapchainKHR              m_swapChain;
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

    VkQueue                     m_queue;

    VkCommandPool               m_commandPool;
    VkCommandBuffer             m_mainCommandBuffer;

    std::vector<VkImage>            m_presentImages;

    unsigned int                    m_currentBuffer;

    //synchronization objects
    VkFence                     m_aquireImageFence;
    VkFence                     m_renderFence;
    VkSemaphore                 m_renderSemaphore;

    CCubeMapTexture*            m_skyTextureCube;
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
    CAORenderer*                m_aoRenderer;
    CLightRenderer*             m_lightRenderer;
	PointLightRenderer2*		m_pointLightRenderer2;
    CParticlesRenderer*         m_particlesRenderer;
    ShadowMapRenderer*             m_shadowRenderer;
    CShadowResolveRenderer*     m_shadowResolveRenderer;
    CSkyRenderer*               m_skyRenderer;
    CFogRenderer*               m_fogRenderer;
    C3DTextureRenderer*         m_3dTextureRenderer;
    CVolumetricRenderer*        m_volumetricRenderer;
    PostProcessRenderer*        m_postProcessRenderer;
    CSunRenderer*               m_sunRenderer;
    CUIRenderer*                m_uiRenderer;
	ScreenSpaceReflectionsRenderer*	m_ssrRenderer;
	TerrainRenderer*			m_terrainRenderer;

    CUIManager*                 m_uiManager;

    //bool                        m_pickRecorded;
    bool                        m_screenshotRequested;
    WORD                        m_xPickCoord;
    WORD                        m_yPickCoord;

    bool                        m_centerCursor;
    bool                        m_hideCursor;
    bool                        m_mouseMoved;
    bool                        m_enableFog;
    float                       m_normMouseDX;
    float                       m_normMouseDY;
    float                       dt;

    bool                        m_needReset;
};

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
    , m_swapChain(VK_NULL_HANDLE)
    , m_surface(VK_NULL_HANDLE)
    , m_queue(VK_NULL_HANDLE)
    , m_mainCommandBuffer(VK_NULL_HANDLE)
    , m_commandPool(VK_NULL_HANDLE)
    , m_aquireImageFence(VK_NULL_HANDLE)
    , m_renderSemaphore(VK_NULL_HANDLE)
    , m_renderFence(VK_NULL_HANDLE)
    , m_currentBuffer(-1)
    , m_skyTextureCube(nullptr)
    , m_skyTexture2D(nullptr)
    , m_sunTexture(nullptr)
    , m_smokeTexture(nullptr)
    , m_lightRenderer(nullptr)
	, m_pointLightRenderer2(nullptr)
    , m_particlesRenderer(nullptr)
    , m_objectRenderer(nullptr)
    , m_aoRenderer(nullptr)
    , m_shadowRenderer(nullptr)
    , m_shadowResolveRenderer(nullptr)
    , m_skyRenderer(nullptr)
    , m_fogRenderer(nullptr)
    , m_3dTextureRenderer(nullptr)
    , m_volumetricRenderer(nullptr)
    , m_postProcessRenderer(nullptr)
    , m_sunRenderer(nullptr)
    , m_uiRenderer(nullptr)
	, m_ssrRenderer(nullptr)
    , m_uiManager(nullptr)
    , m_screenshotRequested(false)
    , m_centerCursor(true)
    , m_mouseMoved(true)
    , m_hideCursor(false)
    , m_normMouseDX(0.0f)
    , m_normMouseDY(0.0f)
    , m_needReset(false)
    , m_enableFog(false)
{
    vk::Load();
    InitWindow();
    GetQueue();
    CreateSurface();
    CreateSwapChains();

	MemoryManager::CreateInstance();
	MeshManager::CreateInstance();
	CTextureManager::CreateInstance();
	ResourceLoader::CreateInstance();
	BatchManager::CreateInstance();
	MaterialLibrary::CreateInstance();

    CreateCommandBuffer();
    CPickManager::CreateInstance();
	ObjectSerializer::CreateInstance();
	ObjectSerializer::GetInstance()->Load("scene.xml");

	MemoryManager::GetInstance()->MapMemoryContext(EMemoryContextType::UniformBuffers);

    SetupDeferredRendering();
    SetupAORendering();
	SetupDirectionalLightingRendering();
	SetupDeferredTileShading();
    SetupShadowMapRendering();
    SetupShadowResolveRendering();
    SetupPostProcessRendering();
    SetupSunRendering();
    SetupUIRendering();
    SetupSkyRendering();
    SetupParticleRendering();
    SetupFogRendering();
    Setup3DTextureRendering();
    SetupVolumetricRendering();
	SetupScreenSpaceReflectionsRendering();
	SetupTerrainRendering();

    GetPickManager()->Setup();

	MemoryManager::GetInstance()->UnmapMemoryContext(EMemoryContextType::UniformBuffers);

    CreateSynchronizationHelpers();
    srand((unsigned int)time(NULL));

    FreeImage_Initialise();
};

CApplication::~CApplication()
{
    FreeImage_DeInitialise();

    delete m_uiManager;

    delete m_smokeTexture;


    delete m_objectRenderer;
    delete m_aoRenderer;
    delete m_lightRenderer;
	delete m_pointLightRenderer2;
    delete m_particlesRenderer;
    delete m_uiRenderer;
	delete m_ssrRenderer;
    delete m_postProcessRenderer;
    delete m_sunRenderer;
    delete m_shadowRenderer;
    delete m_shadowResolveRenderer;
    delete m_skyRenderer;
    delete m_fogRenderer;
    delete m_3dTextureRenderer;
    delete m_volumetricRenderer;

    VkDevice dev = vk::g_vulkanContext.m_device;
    vk::DestroySemaphore(dev, m_renderSemaphore, nullptr);
    vk::DestroyFence(dev, m_renderFence, nullptr);
    vk::DestroyFence(dev, m_aquireImageFence, nullptr);

    vk::DestroySwapchainKHR(dev, m_swapChain, nullptr);
    vk::DestroyRenderPass(dev, m_deferredRenderPass, nullptr);
    vk::DestroyRenderPass(dev, m_aoRenderPass, nullptr);
    vk::DestroyRenderPass(dev, m_dirLightRenderPass, nullptr);
    vk::DestroyRenderPass(dev, m_pointLightRenderPass, nullptr);
	vk::DestroyRenderPass(dev, m_deferredTileShadingRenderPass, nullptr);
    vk::DestroyRenderPass(dev, m_shadowRenderPass, nullptr);
    vk::DestroyRenderPass(dev, m_shadowResolveRenderPass, nullptr);
    vk::DestroyRenderPass(dev, m_postProcessPass, nullptr);
    vk::DestroyRenderPass(dev, m_sunRenderPass, nullptr);
    vk::DestroyRenderPass(dev, m_uiRenderPass, nullptr);
    vk::DestroyRenderPass(dev, m_skyRenderPass, nullptr);
    vk::DestroyRenderPass(dev, m_particlesRenderPass, nullptr);
    vk::DestroyRenderPass(dev, m_fogRenderPass, nullptr);
    vk::DestroyRenderPass(dev, m_3DtextureRenderPass, nullptr);
    vk::DestroyRenderPass(dev, m_volumetricRenderPass, nullptr);
	vk::DestroyRenderPass(dev, m_ssrRenderPass, nullptr);

	ObjectSerializer::DestroyInstance();
	MaterialLibrary::DestroyInstance();
	BatchManager::DestroyInstance();
	ResourceLoader::DestroyInstance();
	CTextureManager::DestroyInstance();
	MeshManager::DestroyInstance();
	MemoryManager::DestroyInstance();

    vk::DestroyCommandPool(dev, m_commandPool, nullptr);
    vk::DestroySurfaceKHR(vk::g_vulkanContext.m_instance, m_surface, nullptr);

    CleanUp();
    vk::Unload();
}


void CApplication::Run()
{
    bool isRunning = true;
	MaterialLibrary::GetInstance()->Initialize(m_objectRenderer);
    CreateResources();
    CreateQueryPools();
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
        UpdateCamera();
        m_uiManager->Update();

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

        dt = (float)(dtMs) / 1000.0f;
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

void CApplication::CreateSurface()
{
    VkWin32SurfaceCreateInfoKHR surfCrtInfo;
    cleanStructure(surfCrtInfo);
    surfCrtInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surfCrtInfo.flags = 0;
    surfCrtInfo.pNext = nullptr;
    surfCrtInfo.hinstance = m_appInstance;
    surfCrtInfo.hwnd = m_windowHandle;

    VkResult result = vk::CreateWin32SurfaceKHR(vk::g_vulkanContext.m_instance, &surfCrtInfo, nullptr, &m_surface);
    TRAP(result >= VK_SUCCESS);
    VkBool32 support;
    vk::GetPhysicalDeviceSurfaceSupportKHR(vk::g_vulkanContext.m_physicalDevice, vk::g_vulkanContext.m_queueFamilyIndex, m_surface, &support);
    TRAP(support);
}


VkImage CApplication::GetFinalOutput()
{
    //return m_objectRenderer->GetFramebuffer()->GetColorImage(GBuffer_DirLight);

    return m_postProcessRenderer->GetFramebuffer()->GetColorImage(0);

}

void CApplication::TransferToPresentImage()
{
    BeginMarkerSection("CopyBackBuffer");
    VkDevice dev = vk::g_vulkanContext.m_device;
    VkCommandBuffer cmdBuffer = vk::g_vulkanContext.m_mainCommandBuffer;

    VkImage presentImg = m_presentImages[m_currentBuffer];
    VkImage finalImg = GetFinalOutput();

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

    VkImageMemoryBarrier preCopyBarrier[2];
    AddImageBarrier(preCopyBarrier[0], presentImg, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0,  VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
    AddImageBarrier(preCopyBarrier[1], finalImg, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,  VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT);

    vk::CmdPipelineBarrier(cmdBuffer,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT, 
        VK_DEPENDENCY_BY_REGION_BIT ,
        0,
        nullptr,
        0,
        nullptr,
        2, 
        preCopyBarrier);

    vk::CmdCopyImage(m_mainCommandBuffer, finalImg, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, presentImg, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imgCopy);

    VkImageMemoryBarrier prePresentBarrier;
    cleanStructure(prePresentBarrier);
    prePresentBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    prePresentBarrier.pNext = NULL;
    prePresentBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    prePresentBarrier.dstAccessMask =  VK_ACCESS_MEMORY_READ_BIT;
    prePresentBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    prePresentBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    prePresentBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    prePresentBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    prePresentBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    prePresentBarrier.subresourceRange.baseMipLevel = 0;
    prePresentBarrier.subresourceRange.levelCount = 1;
    prePresentBarrier.subresourceRange.baseArrayLayer = 0;
    prePresentBarrier.subresourceRange.layerCount = 1;
    prePresentBarrier.image = presentImg;

    vk::CmdPipelineBarrier(cmdBuffer,
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

void CApplication::BeginFrame()
{
    VkDevice dev = vk::g_vulkanContext.m_device;
    VkCommandBuffer cmdBuffer = vk::g_vulkanContext.m_mainCommandBuffer;
    VkImage presentImg = m_presentImages[m_currentBuffer];

    VkImageMemoryBarrier beginFrameBarrier;
    cleanStructure(beginFrameBarrier);
    beginFrameBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    beginFrameBarrier.pNext = NULL;
    beginFrameBarrier.srcAccessMask = 0;
    beginFrameBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    beginFrameBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    beginFrameBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    beginFrameBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    beginFrameBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    beginFrameBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    beginFrameBarrier.subresourceRange.baseMipLevel = 0;
    beginFrameBarrier.subresourceRange.levelCount = 1;
    beginFrameBarrier.subresourceRange.baseArrayLayer = 0;
    beginFrameBarrier.subresourceRange.layerCount = 1;
    beginFrameBarrier.image = presentImg;

    vk::CmdPipelineBarrier(cmdBuffer,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 
        VK_PIPELINE_STAGE_TRANSFER_BIT ,
        VK_DEPENDENCY_BY_REGION_BIT,
        0,
        nullptr,
        0,
        nullptr,
        1, 
        &beginFrameBarrier);
}

void CApplication::CreateSwapChains()
{
    TRAP(m_surface != VK_NULL_HANDLE);
    vk::SVUlkanContext& context = vk::g_vulkanContext;
    VkPhysicalDevice physDevice = context.m_physicalDevice;

    uint32_t modesCnt;
    std::vector<VkPresentModeKHR> presentModes;
    vk::GetPhysicalDeviceSurfacePresentModesKHR(physDevice, m_surface, &modesCnt, nullptr);
    presentModes.resize(modesCnt);
    vk::GetPhysicalDeviceSurfacePresentModesKHR(physDevice, m_surface, &modesCnt, presentModes.data());

    VkResult res;

    unsigned int formatCnt;
    res = vk::GetPhysicalDeviceSurfaceFormatsKHR(physDevice, m_surface, &formatCnt, nullptr);
    TRAP(res >= VK_SUCCESS);

    std::vector<VkSurfaceFormatKHR> formats;
    formats.resize(formatCnt);
    res = vk::GetPhysicalDeviceSurfaceFormatsKHR(physDevice, m_surface, &formatCnt, formats.data());
    TRAP(res >= VK_SUCCESS);
    VkSurfaceFormatKHR formatUsed = formats[1];

    //TRAP(OUT_FORMAT == formatUsed.format);

    VkSurfaceCapabilitiesKHR capabilities;
    res = vk::GetPhysicalDeviceSurfaceCapabilitiesKHR(physDevice, m_surface, &capabilities);
    TRAP(res >= VK_SUCCESS);
    VkExtent2D extent = capabilities.currentExtent;

    VkSwapchainCreateInfoKHR swapChainCrtInfo;
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

    res = vk::CreateSwapchainKHR(context.m_device, &swapChainCrtInfo, nullptr, &m_swapChain);
    TRAP(res >= VK_SUCCESS);
    
    unsigned int imageCnt;
    VULKAN_ASSERT(vk::GetSwapchainImagesKHR(vk::g_vulkanContext.m_device, m_swapChain, &imageCnt, nullptr));
    TRAP(imageCnt == 2);

    m_presentImages.resize(imageCnt);

    VULKAN_ASSERT(vk::GetSwapchainImagesKHR(vk::g_vulkanContext.m_device, m_swapChain, &imageCnt, m_presentImages.data()));
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
    FramebufferDescription fbDesc;
    fbDesc.Begin(3);
    fbDesc.AddColorAttachmentDesc(0, VK_FORMAT_R16_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT, "AOFinal");
    fbDesc.AddColorAttachmentDesc(1, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT, "AODebug"); //debug
    fbDesc.AddColorAttachmentDesc(2, VK_FORMAT_R16_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT, "AOBlurAux");
    fbDesc.End();

    CreateAORenderPass(fbDesc);

    m_aoRenderer = new CAORenderer(m_aoRenderPass);
    m_aoRenderer->Init();
    m_aoRenderer->CreateFramebuffer(fbDesc, WIDTH, HEIGHT);
}

void CApplication::SetupDirectionalLightingRendering()
{
	FramebufferDescription fbDesc;
	fbDesc.Begin(EDirLightBuffers_Count);

	fbDesc.AddColorAttachmentDesc(EDirLightBuffers_Final, g_commonResources.GetAs<ImageHandle*>(EResourceType_FinalImage));
	fbDesc.AddColorAttachmentDesc(EDirLightBuffers_Debug, VK_FORMAT_R16G16B16A16_SFLOAT, 0, "DirLightDebug");
	fbDesc.End();

	CreateDirLightingRenderPass(fbDesc);

	// Lighting passes
	m_lightRenderer = new CLightRenderer(m_dirLightRenderPass);
	m_lightRenderer->Init();
	m_lightRenderer->CreateFramebuffer(fbDesc, WIDTH, HEIGHT);
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
    std::vector<VkAttachmentDescription> ad;
    ad.resize(3);
    AddAttachementDesc(ad[0], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, fbDesc.m_colorAttachments[0].format);
    AddAttachementDesc(ad[1], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, fbDesc.m_colorAttachments[1].format);
    AddAttachementDesc(ad[2], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, fbDesc.m_colorAttachments[2].format);


    std::vector<VkAttachmentReference> attRef;
    attRef.push_back(CreateAttachmentReference(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));
    attRef.push_back(CreateAttachmentReference(1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));

    VkAttachmentReference blurRef = CreateAttachmentReference(2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    std::vector<VkSubpassDescription> sd;
    sd.resize(ESSAOPass_Count);
    sd[ESSAOPass_Main] = CreateSubpassDesc(attRef.data(), (uint32_t)attRef.size());
    sd[ESSAOPass_HBlur] = CreateSubpassDesc(&blurRef, 1);
    sd[ESSAOPass_VBlur] = CreateSubpassDesc(&attRef[0], 1);

    std::vector<VkSubpassDependency> subDeps;
    //subDeps.resize(4);
    subDeps.push_back(CreateSubpassDependency(VK_SUBPASS_EXTERNAL, ESSAOPass_Main, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
       VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,  VK_ACCESS_SHADER_READ_BIT, VK_DEPENDENCY_BY_REGION_BIT));

    subDeps.push_back(CreateSubpassDependency(ESSAOPass_Main, ESSAOPass_HBlur, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,  VK_ACCESS_SHADER_READ_BIT, VK_DEPENDENCY_BY_REGION_BIT));

    subDeps.push_back(CreateSubpassDependency(ESSAOPass_HBlur, ESSAOPass_VBlur, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,  VK_ACCESS_SHADER_READ_BIT, VK_DEPENDENCY_BY_REGION_BIT));

    subDeps.push_back(CreateSubpassDependency(ESSAOPass_Main, ESSAOPass_VBlur, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,  VK_ACCESS_SHADER_READ_BIT, VK_DEPENDENCY_BY_REGION_BIT));

    VkRenderPassCreateInfo rpci;
    cleanStructure(rpci);
    rpci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpci.attachmentCount = (uint32_t)ad.size();
    rpci.pAttachments = ad.data();
    rpci.subpassCount = (uint32_t)sd.size();
    rpci.pSubpasses = sd.data();
    rpci.dependencyCount = (uint32_t)subDeps.size();
    rpci.pDependencies = subDeps.data();

    VULKAN_ASSERT(vk::CreateRenderPass(vk::g_vulkanContext.m_device, &rpci, nullptr, &m_aoRenderPass));
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
    FramebufferDescription fbDesc;
    fbDesc.Begin(0);
    fbDesc.AddDepthAttachmentDesc(VK_FORMAT_D24_UNORM_S8_UINT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, "ShadowMap");
    fbDesc.End();

    CreateShadowRenderPass(fbDesc);
    m_shadowRenderer = new ShadowMapRenderer(m_shadowRenderPass);
    m_shadowRenderer->Init();

    m_shadowRenderer->CreateFramebuffer(fbDesc, SHADOWW, SHADOWH);
}

 void CApplication::SetupShadowResolveRendering()
 {
     FramebufferDescription fbDesc;
     fbDesc.Begin(3);
     fbDesc.AddColorAttachmentDesc(0, VK_FORMAT_R32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT, "ShadowResolveFinal");
     fbDesc.AddColorAttachmentDesc(1, VK_FORMAT_R32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT, "ShadowResolveDebug");
     fbDesc.AddColorAttachmentDesc(2, VK_FORMAT_R32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT, "ShadowResolveBlur");
     fbDesc.End();

     CreateShadowResolveRenderPass(fbDesc);
     m_shadowResolveRenderer = new CShadowResolveRenderer(m_shadowResolveRenderPass);
     m_shadowResolveRenderer->CreateFramebuffer(fbDesc, WIDTH, HEIGHT);
     m_shadowResolveRenderer->Init();
 }

void CApplication::SetupPostProcessRendering()
{
    VkFormat format = VK_FORMAT_B8G8R8A8_UNORM;
    //VkFormat format = VK_FORMAT_B8G8R8A8_SRGB;
    FramebufferDescription fbDesc;
    fbDesc.Begin(1);
    fbDesc.AddColorAttachmentDesc(0, format, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, "PostProcess");
    fbDesc.End();

    CreatePostProcessRenderPass(fbDesc);

    m_postProcessRenderer = new PostProcessRenderer(m_postProcessPass);
    m_postProcessRenderer->Init();

    m_postProcessRenderer->CreateFramebuffer(fbDesc, WIDTH, HEIGHT);
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
	 FramebufferDescription fbDesc;
	 fbDesc.Begin(4);
	 fbDesc.AddColorAttachmentDesc(0, g_commonResources.GetAs<ImageHandle*>(EResourceType_AlbedoImage));
	 fbDesc.AddColorAttachmentDesc(1, g_commonResources.GetAs<ImageHandle*>(EResourceType_SpecularImage));
	 fbDesc.AddColorAttachmentDesc(2, g_commonResources.GetAs<ImageHandle*>(EResourceType_NormalsImage));
	 fbDesc.AddColorAttachmentDesc(3, g_commonResources.GetAs<ImageHandle*>(EResourceType_PositionsImage));
	 fbDesc.AddDepthAttachmentDesc(g_commonResources.GetAs<ImageHandle*>(EResourceType_DepthBufferImage));
	 fbDesc.End();

	 CreateTerrainRenderPass(fbDesc);
	 
	 m_terrainRenderer = new TerrainRenderer(m_terrainRenderPass);
	 m_terrainRenderer->CreateFramebuffer(fbDesc, WIDTH, HEIGHT);
	 m_terrainRenderer->Init();
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

	atRef.push_back(CreateAttachmentReference(fbDesc.m_colorAttachments.size(), VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL));

	std::vector<VkSubpassDescription> subpasses;
	subpasses.push_back(CreateSubpassDesc(atRef.data(), (uint32_t)fbDesc.m_colorAttachments.size(), &atRef[fbDesc.m_colorAttachments.size()]));

	std::vector<VkSubpassDependency> dependencies;
	//dependencies.push_back(CreateSubpassDependency(VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_DEPENDENCY_BY_REGION_BIT));

	NewRenderPass(&m_terrainRenderPass, ad, subpasses);
}

void CApplication::CreateCommandBuffer()
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

void CApplication::GetQueue()
{
    vk::GetDeviceQueue(vk::g_vulkanContext.m_device, vk::g_vulkanContext.m_queueFamilyIndex, 0, &m_queue);
    TRAP(m_queue != VK_NULL_HANDLE);

    vk::g_vulkanContext.m_graphicQueue = m_queue;
}

void CApplication::CreateSynchronizationHelpers()
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

CCubeMapTexture* CreateSkyTexture()
{
    std::vector<std::string> facesFilename;
    facesFilename.push_back("text/side.png");
    facesFilename.push_back("text/side.png");
    facesFilename.push_back("text/side.png");
    facesFilename.push_back("text/side.png");
    facesFilename.push_back("text/side.png");
    facesFilename.push_back("text/side.png");

    return CreateCubeMapTexture(facesFilename);
}

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

void CApplication::CreateResources()
{
    bool isSrgb = true;

    m_skyTexture2D = Create2DSkyTexture();

    SImageData sun;
    Read2DTextureData(sun, std::string(TEXTDIR) + "sun2.png", false);
    m_sunTexture = new CTexture(sun, true);

    CRenderer::UpdateAll();

    m_uiManager = new CUIManager();
    m_uiManager->SetupRenderer(m_uiRenderer);

    m_particlesRenderer->RegisterDebugManager(m_uiManager);

    m_sunRenderer->SetSunTexture(m_sunTexture);
    m_sunRenderer->CreateEditInfo(m_uiManager);

    m_skyRenderer->SetTexture(m_skyTexture2D);
    m_skyRenderer->SetSkyImageView();

    GetPickManager()->CreateDebug(m_uiManager);
    directionalLight.CreateDebug(m_uiManager);
    //SetupParticles();

    //SetupPointLights();
}

void CApplication::UpdateCamera()
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

        ms_camera.Rotate(m_normMouseDX, m_normMouseDY);
        CenterCursor();
    }

    ms_camera.Update();
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
    //PULA
    vk::AcquireNextImageKHR(vk::g_vulkanContext.m_device, m_swapChain, 0, VK_NULL_HANDLE, m_aquireImageFence, &m_currentBuffer);
    vk::WaitForFences(vk::g_vulkanContext.m_device, 1, &m_aquireImageFence,VK_TRUE, UINT64_MAX);
    vk::ResetFences(vk::g_vulkanContext.m_device, 1, &m_aquireImageFence);

	MemoryManager::GetInstance()->MapMemoryContext(EMemoryContextType::UniformBuffers);
	CRenderer::PrepareAll();
	BatchManager::GetInstance()->PreRender();
	MemoryManager::GetInstance()->UnmapMemoryContext(EMemoryContextType::UniformBuffers);

    StartCommandBuffer();
	
    CTextureManager::GetInstance()->Update();
	MeshManager::GetInstance()->Update();
	BatchManager::GetInstance()->Update();

    //BeginFrame();
    QueryManager::GetInstance().Reset();
    QueryManager::GetInstance().StartStatistics();
    RenderShadows();

    m_objectRenderer->Render();
	m_terrainRenderer->Render();//TODO object renderer clear the GBuffer. I have to move it at the start of frame

    vk::CmdPipelineBarrier(vk::g_vulkanContext.m_mainCommandBuffer, 
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, 
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0,
        0, nullptr, 0, nullptr, 0, nullptr); //WHY??

    m_aoRenderer->Render();
    m_shadowResolveRenderer->Render();

    m_lightRenderer->Render();
	m_pointLightRenderer2->Render();
    m_sunRenderer->Render();
    m_skyRenderer->Render();
    m_particlesRenderer->Render();

    if (m_enableFog)
    {
        m_3dTextureRenderer->Render();
        m_volumetricRenderer->Render();
    }

    GetPickManager()->Update();

    WaitForFinalImageToComplete();

    RenderPostProcess();

    m_uiRenderer->Render();

    //RenderPostProcess();

    TransferToPresentImage();

    QueryManager::GetInstance().EndStatistics();
    QueryManager::GetInstance().GetQueries();
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


float RandomFloat()
{
    return (float)rand() / (float) RAND_MAX;
}

void CApplication::StartDeferredRender()
{
}

void CApplication::RenderShadows()
{
    m_shadowRenderer->StartRenderPass();
    m_shadowRenderer->Render();
    m_shadowRenderer->EndRenderPass();
}

void CApplication::WaitForFinalImageToComplete()
{
	ImageHandle* img = g_commonResources.GetAs<ImageHandle*>(EResourceType_FinalImage);
	VkImageMemoryBarrier before = img->CreateMemoryBarrier(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
    vk::CmdPipelineBarrier(vk::g_vulkanContext.m_mainCommandBuffer, 
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 
        VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 0, nullptr, 1, &before);
}

void CApplication::RenderPostProcess()
{
    VkCommandBuffer buff = vk::g_vulkanContext.m_mainCommandBuffer;

	m_ssrRenderer->Render();

    m_postProcessRenderer->UpdateShaderParams();
    m_postProcessRenderer->StartRenderPass();

    m_postProcessRenderer->Render();
    m_postProcessRenderer->EndRenderPass();
}

void CApplication::StartCommandBuffer()
{
    VkCommandBufferBeginInfo bufferBeginInfo;
    cleanStructure(bufferBeginInfo);
    bufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    vk::BeginCommandBuffer(m_mainCommandBuffer, &bufferBeginInfo);
}

void CApplication::EndCommandBuffer()
{
    vk::EndCommandBuffer(m_mainCommandBuffer);
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

    if(uMsg == WM_RBUTTONDOWN )
    {
        WORD key = GET_KEYSTATE_WPARAM(wParam);
        if (key == MK_SHIFT)
            m_particlesRenderer->ToggleSim();
       /* else 
        {
           if (key == MK_CONTROL)
               directionalLight.Shift(1.0f);
           else
               directionalLight.Rotate(1.0f);
        }*/
        return;
    }

    if(uMsg == WM_LBUTTONUP )
    {
        GetPickManager()->RegisterPick(glm::uvec2(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)));
        return;
    }

    if(uMsg == WM_MBUTTONDOWN )
    {
        directionalLight.ToggleDebug();
        return;
    }

    /* if(uMsg == WM_MOUSEWHEEL)
    {
    WORD key = GET_KEYSTATE_WPARAM(wParam);
    float step = float(GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA);
    if (key == MK_CONTROL)
    directionalLight.Shift(step);
    else
    directionalLight.Rotate(step);
    }*/

    if(uMsg == WM_COPYDATA)
    {
        PCOPYDATASTRUCT pData = (PCOPYDATASTRUCT) lParam;
        if (pData->dwData == MSGSHADERCOMPILED)
            m_needReset = true;

        return;
    }

    if(uMsg == WM_KEYDOWN)
    {
        float lightMoveFactor = 0.5f;
        GetPickManager()->RegisterKey((unsigned int)wParam);

        if (m_sunRenderer->RegisterPick((unsigned int)wParam))
            return;

        if(wParam == VK_UP)
        {
            directionalLight.Shift(lightMoveFactor);
            return;
        }
        else if(wParam == VK_DOWN)
        {
            directionalLight.Shift(-lightMoveFactor);
            return;
        }
        else if(wParam == VK_RIGHT)
        {
            directionalLight.Rotate(lightMoveFactor);
            return;
        }
        else if(wParam == VK_LEFT)
        {
            directionalLight.Rotate(-lightMoveFactor);
            return;
        }

        if (wParam == VK_SPACE)
        {
            ms_camera.Reset();
            return;
        }

        if (wParam == 'A')
        {
            ms_camera.Translate(glm::vec3(-1, 0.0f, 0.0f));
        }
        if (wParam == 'D')
        {
            ms_camera.Translate(glm::vec3(1, 0.0f, 0.0f));
        }

        if (wParam == 'W')
        {
             ms_camera.Translate(glm::vec3(0.0f, 0.0f, 1));
        }

        if (wParam == 'S')
        {
            ms_camera.Translate(glm::vec3(0.0f, 0.0f, -1));
        }

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

        if (wParam == VK_F4)
        {
            GetPickManager()->ToggleEditMode();
        }

        if (wParam == VK_F5)
        {
            m_enableFog = !m_enableFog;
        }

        if (wParam == VK_TAB)
        {
            m_uiManager->ToggleDisplayInfo();
        }

        if (wParam == VK_OEM_PLUS)
        {
            directionalLight.ChangeLightIntensity(0.5f);
        }

        if (wParam == VK_OEM_MINUS)
        {
            directionalLight.ChangeLightIntensity(-0.5f);
        }

        if (wParam == 'P')
        {
            directionalLight.ChangeLightColor();
        }
    }
}


glm::vec4 GetPlaneFrom(glm::vec4 p1, glm::vec4 p2, glm::vec4 p3)
{
	glm::vec3 normal = glm::cross(glm::vec3(p2 - p1), glm::vec3(p3 - p1));
	normal = glm::normalize(normal);
	float d = glm::dot(normal, glm::vec3(p1));
	return glm::vec4(normal, d);
}

void CApplication::RenderCameraFrustrum()
{
	glm::mat4 proj = glm::perspective(glm::radians(45.0f), 16.0f / 9.f, 0.1f, 3.0f);
	PerspectiveMatrix(proj);
	ConvertToProjMatrix(proj);

	glm::mat4 view = glm::lookAt(glm::vec3(10.f, 1.f, 0.0f), glm::vec3(0.f, 1.f, -1.f), glm::vec3(0.f, 1.f, 0.f));
	glm::mat4 projViewMatrix = proj * view;
	glm::mat4 invProjViewMatrix = glm::inverse(projViewMatrix);

	glm::vec4 colors[] = { glm::vec4(1.0f, 0.f, 0.0f, 1.0f),
		glm::vec4(0.0f, 1.f, 0.0f, 1.0f),
		glm::vec4(0.0f, 0.f, 1.0f, 1.0f),
		glm::vec4(1.0f, 1.f, 0.0f, 1.0f) };

	unsigned int n = 8;
	float l = 2 / float(n);
	for (unsigned int x = 0; x < n; ++x)
		for (unsigned int y = 0; y < n; ++y)
		{
			glm::vec4 ndcPoints[] = { glm::vec4(-1.f + x * l, -1.f + y * l, 0.f, 1.f),
									glm::vec4(-1.f + (x + 1.f) * l, -1.f + y * l, 0.f, 1.f),
									glm::vec4(-1.f + (x + 1.f) * l, -1.f + (y + 1.f) * l, 0.f, 1.f),
									glm::vec4(-1.f + x * l, -1.f + (y + 1.f) * l, 0.f, 1.f),
									glm::vec4(-1.f + x * l, -1.f + y * l, 1.f, 1.f),
									glm::vec4(-1.f + (x + 1.f) * l, -1.f + y * l, 1.f, 1.f),
									glm::vec4(-1.f + (x + 1.f) * l, -1.f + (y + 1.f) * l, 1.f, 1.f),
									glm::vec4(-1.f + x * l, -1.f + (y + 1.f) * l, 1.f, 1.f) };

			glm::vec4 worldPoints[8];
			for (unsigned int i = 0; i < 8; ++i)
			{
				worldPoints[i] = invProjViewMatrix * ndcPoints[i];
				//worldPoints[i] = glm::inverse(proj) * ndcPoints[i];
				worldPoints[i] /= worldPoints[i].w;
			}

			glm::vec4 color = colors[(x + y) % 4];

			//build near/far planes
			for (unsigned int i = 0; i < 3; ++i)
			{
				m_uiManager->CreateVectorItem(glm::vec3(worldPoints[i]), glm::vec3(worldPoints[i + 1] - worldPoints[i]), color);
			}

			//close the loop
			m_uiManager->CreateVectorItem(glm::vec3(worldPoints[3]), glm::vec3(worldPoints[0] - worldPoints[3]), color);

			for (unsigned int i = 4; i < 7; ++i)
			{
				m_uiManager->CreateVectorItem(glm::vec3(worldPoints[i]), glm::vec3(worldPoints[i + 1] - worldPoints[i]), color);
			}

			//close the loop
			m_uiManager->CreateVectorItem(glm::vec3(worldPoints[7]), glm::vec3(worldPoints[4] - worldPoints[7]), color);

			//build the sides
			for (unsigned int i = 0; i < 4; ++i)
			{
				m_uiManager->CreateVectorItem(glm::vec3(worldPoints[i]), glm::vec3(worldPoints[i + 4] - worldPoints[i]), color);
			}

			glm::vec4 planes[6];
			//near
			planes[0] = GetPlaneFrom(worldPoints[2], worldPoints[0], worldPoints[1]);
			//far
			planes[1] = GetPlaneFrom(worldPoints[6], worldPoints[5], worldPoints[4]);
			//left
			planes[2] = GetPlaneFrom(worldPoints[3], worldPoints[7], worldPoints[4]);
			//right
			planes[3] = GetPlaneFrom(worldPoints[5], worldPoints[6], worldPoints[2]);
			//top
			planes[4] = GetPlaneFrom(worldPoints[5], worldPoints[1], worldPoints[0]);
			//bot
			planes[5] = GetPlaneFrom(worldPoints[3], worldPoints[2], worldPoints[6]);

			//m_uiManager->CreateVectorItem(glm::vec3(worldPoints[0]), glm::vec3(planes[0]), glm::vec4(0.f, 1.f, 0.f, 1.0f));
			//m_uiManager->CreateVectorItem(glm::vec3(worldPoints[4]), glm::vec3(planes[1]), glm::vec4(0.f, 1.f, 0.f, 1.0f)); //because of inverse y
			//m_uiManager->CreateVectorItem(glm::vec3(worldPoints[3]), glm::vec3(planes[2]), glm::vec4(0.f, 1.f, 0.f, 1.0f));
			//m_uiManager->CreateVectorItem(glm::vec3(worldPoints[5]), glm::vec3(planes[3]), glm::vec4(0.f, 1.f, 0.f, 1.0f));
			//m_uiManager->CreateVectorItem(glm::vec3(worldPoints[1]), glm::vec3(planes[4]), glm::vec4(0.f, 1.f, 0.f, 1.0f));
			//m_uiManager->CreateVectorItem(glm::vec3(worldPoints[6]), glm::vec3(planes[5]), glm::vec4(0.f, 1.f, 0.f, 1.0f));

		}
}

int main(int argc, char* arg[])
{
	/*ResourceLoader::CreateInstance();
	ObjectSerializer serializer;
	Object* obj = new Object();
	DefaultMaterial* mat = (DefaultMaterial*)s_TestTemplate->Create();
	CTexture* texture = new CTexture();
	Mesh* mesh = new Mesh();

	mesh->SetFilename("obj\\\\monkey.mb");
	texture->SetFilename("pussy.png");
	texture->SetIsSRGB(true);

	mat->SetRoughness(20);
	mat->SetF0(4);
	mat->SetK(1);
	mat->SetAlbedoText(texture);
	obj->SetisShadowCaster(true);
	obj->SetScale(glm::vec3(1.0));

	obj->SetObjectMaterial(mat);
	obj->SetObjectMesh(mesh);

	serializer.AddObject(obj);
	serializer.Save("scene.xml");

	ObjectSerializer loadSerializer;
	loadSerializer.Load("scene.xml");*/

	CApplication app;

    app.Run();
    return 0;
}