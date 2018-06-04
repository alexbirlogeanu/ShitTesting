#include "PickManager.h"
#include <algorithm>
#include "UI.h"

#define PICKBUFFERFORMAT VK_FORMAT_R8_UINT

CPickManager* GetPickManager()
{
    CPickManager* manager = CPickManager::GetInstance();
    TRAP(manager);
    return manager;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//CPickable
///////////////////////////////////////////////////////////////////////////////////////////////////
unsigned int CPickable::ms_nextId = 1; //0 means nothing is selected

CPickable::CPickable()
    : m_id(ms_nextId++)
{
   GetPickManager()->Register(this);
}

CPickable::~CPickable()
{
    GetPickManager()->Unregister(this);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//CPickRenderer
///////////////////////////////////////////////////////////////////////////////////////////////////

CPickRenderer::CPickRenderer(VkRenderPass renderPass)
    : CRenderer(renderPass, "PickingRenderPass")
    , m_uniformBuffer(VK_NULL_HANDLE)
    , m_uniformMemory(VK_NULL_HANDLE)
    , m_pickedNode(nullptr)
    , m_bbMesh(nullptr)
    , m_mouseCoords(glm::uvec2(-1, -1))
    , m_copyMemory(VK_NULL_HANDLE)
    , m_copyBuffer(VK_NULL_HANDLE)
    , m_descriptorSetLayout(VK_NULL_HANDLE)
    , m_selectedId(0)
{
    PerspectiveMatrix(m_projection);
    ConvertToProjMatrix(m_projection);
}

CPickRenderer::~CPickRenderer()
{
    VkDevice dev = vk::g_vulkanContext.m_device;
    delete m_bbMesh;

    vk::DestroyDescriptorSetLayout(dev, m_descriptorSetLayout, nullptr);
}

void CPickRenderer::Init()
{
    CRenderer::Init();

    VkDeviceSize minOffsetAlign = vk::g_vulkanContext.m_limits.minUniformBufferOffsetAlignment;
    TRAP(sizeof(SPickParams) < minOffsetAlign);
    AllocBufferMemory(m_uniformBuffer, m_uniformMemory, uint32_t(MAXPICKABLES * minOffsetAlign), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

    TRAP(PICKBUFFERFORMAT == VK_FORMAT_R8_UINT && "Set the coresponding size");
    VkDeviceSize size = WIDTH * HEIGHT * sizeof(unsigned char);
    AllocBufferMemory(m_copyBuffer, m_copyMemory, (uint32_t)size, VK_BUFFER_USAGE_TRANSFER_DST_BIT);

    CreateBBMesh();

    m_idPipeline.SetVertexShaderFile("pickid.vert");
    m_idPipeline.SetFragmentShaderFile("pickid.frag");
    m_idPipeline.SetVertexInputState(Mesh::GetVertexDesc());
    m_idPipeline.SetCullMode(VK_CULL_MODE_BACK_BIT);
    m_idPipeline.SetDepthTest(true);
    m_idPipeline.SetScissor(WIDTH, HEIGHT);
    m_idPipeline.SetViewport(WIDTH, HEIGHT);
    m_idPipeline.SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    m_idPipeline.AddBlendState(CGraphicPipeline::CreateDefaultBlendState());
    m_idPipeline.CreatePipelineLayout(m_descriptorSetLayout);
    m_idPipeline.Init(this, m_renderPass, 0);

    m_bbPipeline.SetVertexShaderFile("pickbb.vert");
    m_bbPipeline.SetFragmentShaderFile("pickbb.frag");
    m_bbPipeline.SetVertexInputState(Mesh::GetVertexDesc());
    m_bbPipeline.SetCullMode(VK_CULL_MODE_BACK_BIT);
    m_bbPipeline.SetDepthTest(true);
    m_bbPipeline.SetScissor(WIDTH, HEIGHT);
    m_bbPipeline.SetViewport(WIDTH, HEIGHT);
    m_bbPipeline.SetTopology(VK_PRIMITIVE_TOPOLOGY_LINE_LIST);
    m_bbPipeline.AddBlendState(CGraphicPipeline::CreateDefaultBlendState());
    m_bbPipeline.CreatePipelineLayout(m_descriptorSetLayout);
    m_bbPipeline.Init(this, m_renderPass, 1);
}

void CPickRenderer::Render()
{
    UpdateObjects();

    VkCommandBuffer cmd = vk::g_vulkanContext.m_mainCommandBuffer;

    StartRenderPass();
    vk::CmdBindPipeline(cmd, m_idPipeline.GetBindPoint(), m_idPipeline.Get());
    if (AreCoordsValid(m_mouseCoords) || m_pickedNode)
    {
        for(unsigned int i = 0; i < m_nodes.size(); ++i)
        {
            vk::CmdBindDescriptorSets(cmd, m_idPipeline.GetBindPoint(), m_idPipeline.GetLayout(), 0, 1, &m_nodes[i].Set, 0, nullptr);
            m_nodes[i].Pickable->Render();
        }
    }

    vk::CmdNextSubpass(cmd, VK_SUBPASS_CONTENTS_INLINE);
    if (m_pickedNode)
    {
        vk::CmdBindPipeline(cmd, m_bbPipeline.GetBindPoint(), m_bbPipeline.Get());
        vk::CmdBindDescriptorSets(cmd, m_bbPipeline.GetBindPoint(), m_bbPipeline.GetLayout(), 0, 1, &m_pickedNode->Set, 0, nullptr);

        VkImage outImg =  m_framebuffer->GetColorImage(1);
        /* VkImageMemoryBarrier preRenderBarrier;
        AddImageBarrier(preRenderBarrier, outImg, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT);*/

        //vk::CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 0, nullptr, 1, &preRenderBarrier);
        m_bbMesh->Render();
        
        /*VkImageMemoryBarrier postRenderBarrier;
        AddImageBarrier(postRenderBarrier, outImg, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT);

        vk::CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 0, nullptr, 1, &postRenderBarrier);*/

    }

    EndRenderPass();

    if (AreCoordsValid(m_mouseCoords))
        GetPickedId();
    m_mouseCoords = glm::uvec2(-1, -1);
}

void CPickRenderer::AddNode(CPickable* p)
{
    SNode node;
    node.Pickable = p;

    unsigned int i = 0;
    for(; i < m_memoryPool.size(); ++i)
    {
        if (m_memoryPool[i] == 0)
            break;
    }

    TRAP(i < m_memoryPool.size());
    m_memoryPool.set(i);
    VkDeviceSize minOffsetAlign = vk::g_vulkanContext.m_limits.minUniformBufferOffsetAlignment;
    node.offset = i * minOffsetAlign;
    node.Set = AllocateDescSet();

    VkDescriptorBufferInfo buffInfo;
    cleanStructure(buffInfo);
    buffInfo.buffer = m_uniformBuffer;
    buffInfo.offset = node.offset;
    buffInfo.range = sizeof(SPickParams);

    VkWriteDescriptorSet wDesc;
    wDesc = InitUpdateDescriptor(node.Set, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &buffInfo);

    vk::UpdateDescriptorSets(vk::g_vulkanContext.m_device, 1, &wDesc, 0, nullptr);
    m_nodes.push_back(node);
}

void CPickRenderer::RemoveNode(CPickable* p)
{
    auto it = std::find_if(m_nodes.begin(), m_nodes.end(), [p](const SNode& el)
    {
        return el.Pickable == p;
    });

    if (it != m_nodes.end())
    {
        SNode node = *it; //node to remove
        VkDeviceSize minOffsetAlign = vk::g_vulkanContext.m_limits.minUniformBufferOffsetAlignment;
        unsigned int i = uint32_t(node.offset / minOffsetAlign);
        m_memoryPool.reset(i);
        vk::FreeDescriptorSets(vk::g_vulkanContext.m_device, m_descriptorPool, 1, &node.Set);
        m_nodes.erase(it);
    }
}

void CPickRenderer::CreateDescriptorSetLayout()
{
    {
        std::vector<VkDescriptorSetLayoutBinding> bindings;
        bindings.resize(1);
        bindings[0] = CreateDescriptorBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);

        VkDescriptorSetLayoutCreateInfo crtInfo;
        cleanStructure(crtInfo);
        crtInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        crtInfo.flags = 0;
        crtInfo.bindingCount = (uint32_t)bindings.size();
        crtInfo.pBindings = bindings.data();

        VULKAN_ASSERT(vk::CreateDescriptorSetLayout(vk::g_vulkanContext.m_device, &crtInfo, nullptr, &m_descriptorSetLayout));
    }
}

void CPickRenderer::PopulatePoolInfo(std::vector<VkDescriptorPoolSize>& poolSize, unsigned int& maxSets)
{
    AddDescriptorType(poolSize, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAXPICKABLES);
    maxSets = MAXPICKABLES;
}

VkDescriptorSet CPickRenderer::AllocateDescSet()
{
    VkDescriptorSetAllocateInfo allocInfo;
    cleanStructure(allocInfo);
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &m_descriptorSetLayout;

    VkDescriptorSet descSet;
    VULKAN_ASSERT(vk::AllocateDescriptorSets(vk::g_vulkanContext.m_device, &allocInfo, &descSet));

    return descSet;
}

void CPickRenderer::UpdateObjects()
{
    unsigned char* memPtr = nullptr;
    glm::mat4 projViewMatrix = m_projection * ms_camera.GetViewMatrix();

    vk::MapMemory(vk::g_vulkanContext.m_device, m_uniformMemory, 0, VK_WHOLE_SIZE, 0, (void**)&memPtr);
    for(unsigned int i = 0; i < m_nodes.size(); ++i)
    {
        CPickable* p = m_nodes[i].Pickable;
        BoundingBox bb = p->GetBoundingBox();

        SPickParams* params = (SPickParams*)(memPtr + m_nodes[i].offset);
        params->ID = glm::uvec4(p->GetId());
        params->BBMin = glm::vec4(bb.Min, 1.0f);
        params->BBMax = glm::vec4(bb.Max, 1.0f);
        params->MVPMatrix = projViewMatrix * p->GetModelMatrix();
    }
    vk::UnmapMemory(vk::g_vulkanContext.m_device, m_uniformMemory);
}

void CPickRenderer::CreateBBMesh()
{
    SVertex vertices[] = {
        SVertex(glm::vec3(-1, 1, 1)),
        SVertex(glm::vec3(1, 1, 1)),
        SVertex(glm::vec3(1, -1, 1)),
        SVertex(glm::vec3(-1, -1, 1)),
        SVertex(glm::vec3(-1, 1, -1)),
        SVertex(glm::vec3(1, 1, -1)),
        SVertex(glm::vec3(1, -1, -1)),
        SVertex(glm::vec3(-1, -1, -1))
    };

    //lines
    unsigned int indices[] = {
        0, 1, 1, 2, 2, 3, 3, 0,
        0, 3, 3, 7, 7, 4, 4, 0,
        4, 5, 5, 6, 6, 7, 7, 4,
        1, 5, 5, 6, 6, 2, 2, 1,
        2, 3, 3, 7, 7, 6, 6, 2,
        0, 1, 1, 5, 5, 4, 4, 0
    };

    m_bbMesh = new Mesh(std::vector<SVertex>(vertices, vertices + 8), std::vector<unsigned int>(indices, indices + sizeof(indices) / sizeof(unsigned int)));

}
void CPickRenderer::GetPickedId()
{
    VkCommandBuffer cmd = vk::g_vulkanContext.m_mainCommandBuffer;

    VkImage outImg =  m_framebuffer->GetColorImage(0);
    VkImageMemoryBarrier preCopyBarrier;
    AddImageBarrier(preCopyBarrier, outImg, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
    //wait for render to be complete
    vk::CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 0, nullptr, 1, &preCopyBarrier);

    VkOffset3D offset = {0, 0, 0};
    VkExtent3D extent = {WIDTH, HEIGHT, 1};
    VkImageSubresourceLayers layers;
    layers.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    layers.mipLevel = 0;
    layers.baseArrayLayer = 0;
    layers.layerCount = 1;

    VkBufferImageCopy region;
    cleanStructure(region);
    region.bufferOffset = 0;
    region.imageSubresource = layers;
    region.imageOffset = offset;
    region.imageExtent = extent;

    vk::CmdCopyImageToBuffer(cmd, outImg, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_copyBuffer, 1, &region);

    VkImageMemoryBarrier postCopyImgBarrier;
    VkBufferMemoryBarrier postCopyBufBarrier;
    AddImageBarrier(postCopyImgBarrier, outImg, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT);

    AddBufferBarier(postCopyBufBarrier, m_copyBuffer, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);


    vk::CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 1, &postCopyBufBarrier, 1, &postCopyImgBarrier);

    unsigned char* mem;
    VULKAN_ASSERT(vk::MapMemory(vk::g_vulkanContext.m_device, m_copyMemory, 0, VK_WHOLE_SIZE, 0, (void**)&mem));

    m_selectedId = mem[m_mouseCoords.y * WIDTH + m_mouseCoords.x];

    vk::UnmapMemory(vk::g_vulkanContext.m_device, m_copyMemory);

    if ( m_selectedId != 0)
    {
        auto it = std::find_if(m_nodes.begin(), m_nodes.end(), [&](const SNode& node){
            return node.Pickable->GetId() == m_selectedId;
        });

        TRAP(it != m_nodes.end());
        m_pickedNode = &(*it);
    }
    else
    {
        m_pickedNode = nullptr;
    }
}

bool CPickRenderer::AreCoordsValid(glm::uvec2 coords)
{
    return coords.x != -1 && coords.y != -1;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//CPickManager
///////////////////////////////////////////////////////////////////////////////////////////////////
CPickManager::CPickManager()
    : m_selectedID(-1)
    , m_renderPass(VK_NULL_HANDLE)
    , m_pickRenderer(nullptr)
    , m_editMode(false)
    , m_needRefresh(true)
{
}

CPickManager::~CPickManager()
{
}

void CPickManager::Setup()
{
    VkImage outImg = g_commonResources.GetAs<VkImage>(EResourceType_FinalImage);
    VkImageView outImgView = g_commonResources.GetAs<VkImageView>(EResourceType_FinalImageView);
    FramebufferDescription fbDesc;
    fbDesc.Begin(2);
    fbDesc.AddColorAttachmentDesc(0, PICKBUFFERFORMAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    fbDesc.AddColorAttachmentDesc(1, VK_FORMAT_R16G16B16A16_SFLOAT, outImg, outImgView);
    fbDesc.AddDepthAttachmentDesc(VK_FORMAT_D16_UNORM, 0);
    fbDesc.End();

    SetupRenderpass(fbDesc);

    m_pickRenderer = new CPickRenderer(m_renderPass);
    m_pickRenderer->CreateFramebuffer(fbDesc, WIDTH, HEIGHT);
    m_pickRenderer->Init();
}

void CPickManager::RegisterPick(glm::uvec2 coords) 
{ 
    if (m_editMode)
        m_pickRenderer->SetCoords(coords); 
}

void CPickManager::RegisterKey(unsigned int key)
{
    if (m_editMode && m_pickedObject)
        if(m_pickedObject->ChangePickableProperties(key))
            UpdateEditInfo();
}

void CPickManager::Update()
{
    if (!m_editMode)
        return;

    if (m_needRefresh)
    {
        for (auto pickObj : m_tempPickableObjects)
        {
            m_pickableObjects.push_back(pickObj);
            m_pickRenderer->AddNode(pickObj);
        }

        m_tempPickableObjects.clear();
        m_needRefresh = false;
    }

    m_pickRenderer->Render();

    if (m_selectedID != m_pickRenderer->GetSelectedID())
    {
        m_selectedID = m_pickRenderer->GetSelectedID();

        if(m_selectedID != 0)
        {
            auto it = std::find_if(m_pickableObjects.begin(), m_pickableObjects.end(), [&](const CPickable* p){
                return p->GetId() == m_selectedID;
            });

            TRAP(it != m_pickableObjects.end());
            TRAP(*it == m_pickRenderer->m_pickedNode->Pickable); //NICE SHIIEET

            m_pickedObject = *it;
        }
        else
            m_pickedObject = nullptr;

        UpdateEditInfo();
    }


}

 void CPickManager::CreateDebug(CUIManager* manager)
 {
     std::vector<std::string> infoTexts;
     infoTexts.resize(Count);

     infoTexts[Title] = std::string("Object Edit Mode");
     //infoTexts[Selected] = std::string("Current selected: ");

     m_editModeInfo = manager->CreateTextContainerItem(infoTexts, glm::uvec2(1000, 200), 5, 36);
     m_editModeInfo->SetVisible(m_editMode);
 }

 void CPickManager::UpdateEditInfo()
 {
     m_editModeInfo->SetTextItem(Selected, "Current selected: " + std::to_string(m_selectedID));
     for(unsigned int i = SlotStart; i <= SlotsEnd; ++i)
        m_editModeInfo->SetTextItem(i, "");
     if(m_pickedObject)
     {
        std::vector<std::string> desc;
        m_pickedObject->GetPickableDescription(desc);
        for(unsigned int i = 0; i < desc.size() && (i + SlotStart < Count); ++i)
            m_editModeInfo->SetTextItem(i + SlotStart, desc[i]);
     }

 }

 void CPickManager::ToggleEditMode()
 {
     m_editMode = !m_editMode;
     m_editModeInfo->SetVisible(m_editMode);
 }

void CPickManager::SetupRenderpass(const FramebufferDescription& fbDesc)
{
    std::vector<VkAttachmentDescription> ad;
    ad.resize(3);
    AddAttachementDesc(ad[0], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, fbDesc.m_colorAttachments[0].format);
    AddAttachementDesc(ad[1], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, fbDesc.m_colorAttachments[1].format, VK_ATTACHMENT_LOAD_OP_LOAD);
    AddAttachementDesc(ad[2], VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, fbDesc.m_depthAttachments.format);

    std::vector<VkAttachmentReference> attachment_ref;
    attachment_ref.push_back(CreateAttachmentReference(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));
    attachment_ref.push_back(CreateAttachmentReference(1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));
    attachment_ref.push_back(CreateAttachmentReference(2, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL));
 

    std::vector<VkSubpassDescription> sd;
    sd.push_back(CreateSubpassDesc(&attachment_ref[0], 1, &attachment_ref[2]));
    sd.push_back(CreateSubpassDesc(&attachment_ref[1], 1, &attachment_ref[2]));
    
    std::vector<VkSubpassDependency> subpass_deps;
    
    subpass_deps.push_back(CreateSubpassDependency(0, 1, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT, VK_DEPENDENCY_BY_REGION_BIT));

    VkRenderPassCreateInfo rpci;
    cleanStructure(rpci);
    rpci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpci.pNext = nullptr;
    rpci.flags = 0;
    rpci.attachmentCount = (uint32_t)ad.size(); 
    rpci.pAttachments = ad.data();
    rpci.subpassCount = (uint32_t)sd.size();
    rpci.pSubpasses = sd.data();
    rpci.dependencyCount = (uint32_t)subpass_deps.size();
    rpci.pDependencies =  subpass_deps.data(); 

    VULKAN_ASSERT(vk::CreateRenderPass(vk::g_vulkanContext.m_device, &rpci, nullptr, &m_renderPass));
}

void CPickManager::Register(CPickable* p)
{
    auto it = std::find(m_pickableObjects.begin(), m_pickableObjects.end(), p);
    if (it == m_pickableObjects.end())
    {
        m_tempPickableObjects.push_back(p);
    }
    m_needRefresh = true;
}

void CPickManager::Unregister(CPickable* p)
{
    auto it = std::find(m_pickableObjects.begin(), m_pickableObjects.end(), p);
    if (it != m_pickableObjects.end())
    {
        m_pickableObjects.erase(it);
        m_pickRenderer->RemoveNode(p);
    }
}

