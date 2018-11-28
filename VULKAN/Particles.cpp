#include "Particles.h"

#include <iostream>
#include <cstdlib>
#include <climits>
#include <ctime>

#include "UI.h"
#include "Mesh.h"
#include "Utils.h"
#include "Texture.h"
#include "Input.h"


float randFloat()
{
    return ((float)(rand() % 10) / 10.0f);
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//CParticleSpawner
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CParticleSpawner::CParticleSpawner(glm::vec3 pos, glm::vec3 normal, glm::vec3 tangent)
    : m_worldPosition(pos)
    , m_normal(normal)
    , m_tangent(tangent)
    , m_particleLifeSpan(8.0f)
    , m_particleLifeSpanTreshold(0.3f)
    , m_particleSize(0.2f)
    , m_particleSizeTreshold(0.03f)
    , m_fadeTime(2.0f)
    , m_particleSpeed(0.35f)
    , m_particleSpeedTreshold(0.05f)
{
    m_bitangent = glm::cross(m_normal, m_tangent);
    m_TBN = glm::mat3(m_tangent, m_bitangent, m_normal);
}

CParticleSpawner::~CParticleSpawner()
{
}

void CParticleSpawner::SpawnParticle(CParticle* p)
{
    float pLifeSpan = CreateRandFloat(m_particleLifeSpan - m_particleLifeSpanTreshold, m_particleLifeSpan + m_particleLifeSpanTreshold);
    float pSize = CreateRandFloat(m_particleSize - m_particleSizeTreshold, m_particleSize + m_particleSizeTreshold);

    float speed = CreateRandFloat(m_particleSpeed - m_particleSpeedTreshold , m_particleSpeed + m_particleSpeedTreshold);

    p->Velocity = glm::vec4(CreateSpawnDirection(), 0.0f) * speed;
    p->Position = glm::vec4(CreateSpawnPosition(), 1.0f);
    p->Properties = glm::vec4(pLifeSpan, pLifeSpan, m_fadeTime, pSize);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//CConeSpawner
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CConeSpawner::CConeSpawner(float alphaLimit, glm::vec3 pos, glm::vec3 normal, glm::vec3 tangent)
    : CParticleSpawner(pos, normal, tangent)
    , m_alphaLimit(alphaLimit)
{
}

CConeSpawner::~CConeSpawner()
{
}


glm::vec3 CConeSpawner::CreateSpawnDirection()
{
    //alpha si beta sunt unghiuri in coordonate sferice. Incerc sa limitez spawnarea particulelor in interiorul unui con

    float alpha = CreateRandFloat(-m_alphaLimit, m_alphaLimit);
    alpha = glm::radians(alpha);

    float beta = CreateRandFloat(0.0f, 360.0f);
    beta = glm::radians(beta);

    glm::vec3 spawnDirection = glm::vec3(sin(alpha) * cos(beta), sin(alpha) * sin(beta), cos(alpha));
    spawnDirection = m_TBN * spawnDirection;
    spawnDirection = glm::normalize(spawnDirection);

    return spawnDirection;
}

glm::vec3 CConeSpawner::CreateSpawnPosition()
{
    return m_worldPosition;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//CRectangularSpawner
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CRectangularSpawner::CRectangularSpawner(float w, float h, glm::vec3 pos, glm::vec3 normal, glm::vec3 tangent)
    : CParticleSpawner(pos, normal, tangent)
    , m_dimensions(w, h)
{
    m_maxLength = glm::length(m_dimensions); //whatever, its not correct but good enough
}

CRectangularSpawner::~CRectangularSpawner()
{
}

glm::vec3 CRectangularSpawner::CreateSpawnDirection()
{
    return m_normal; //already in world space
}

glm::vec3 CRectangularSpawner::CreateSpawnPosition()
{
    float alpha = glm::radians(CreateRandFloat(0.0f, 360.0f));
    glm::vec2 dir = glm::normalize(glm::vec2(cos(alpha), sin(alpha))); //this one is in spawner space.
    float dist = CreateRandFloat(0.0f, m_maxLength);
    dir = dir * dist;

    //we use TBN to transform in world space
    glm::vec3 worldDir = m_TBN * glm::vec3(dir, 0.0f);
    glm::vec3 newPos = m_worldPosition + worldDir;
    float dirOffset = CreateRandFloat(-1.0f, 1.0f);
    newPos += dirOffset * m_normal;

    return newPos;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//CParticleSystem
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CParticleSystem::CParticleSystem(CParticleSpawner* spawner, unsigned int maxParticles)
    : m_spawnedParticles(0)
    , m_updateStep(0.75f)
    , m_elapsedTime(m_updateStep)
    , m_maxParticles(maxParticles)
    , m_particlesBuffer(VK_NULL_HANDLE)
    , m_particlesMemory(VK_NULL_HANDLE)
    , m_updateShader(VK_NULL_HANDLE)
    , m_updatePipeline(VK_NULL_HANDLE)
    , m_computeDescriptorSet(VK_NULL_HANDLE)
    , m_graphicDescriptorSet(VK_NULL_HANDLE)
    , m_spawner(spawner)
{
    
}

CParticleSystem::~CParticleSystem()
{
    VkDevice dev = vk::g_vulkanContext.m_device;

    vk::DestroyPipeline(dev, m_updatePipeline, nullptr);

    vk::FreeMemory(dev, m_particlesMemory, nullptr);
    vk::DestroyBuffer(dev, m_particlesBuffer, nullptr);

    vk::DestroyShaderModule(dev, m_updateShader, nullptr);
}

void CParticleSystem::Update(float dt)
{
    Validate();

    m_spawnedParticles = m_maxParticles;
    void* data = nullptr;
    VULKAN_ASSERT(vk::MapMemory(vk::g_vulkanContext.m_device, m_particlesMemory, 0, VK_WHOLE_SIZE, 0, &data));
    CParticle* p = (CParticle*)data;

    if(m_elapsedTime >= m_updateStep)
    {
        std::vector<CParticle*> deadParticles;
        unsigned int factor = 3;
        unsigned int needSpawnMaxCnt = m_maxParticles / factor; //the max number of spawned particles
        deadParticles.reserve(needSpawnMaxCnt);

        for(unsigned int i = 0; i < m_maxParticles; ++i)
        {
            if (p[i].IsKaput())
                deadParticles.push_back(&p[i]);
        }

        unsigned int spawnParticlesCnt = (unsigned int)deadParticles.size() / factor; //per update step we spawn a fraction of the remaining particles
        for(unsigned int i = 0; i < spawnParticlesCnt; ++i)
            m_spawner->SpawnParticle(deadParticles[i]);
        
        m_elapsedTime = 0;
    }
    else
        m_elapsedTime += dt;
    
    /*std::sort(p, p + m_maxParticles, [](const CParticle& p1, const CParticle& p2)
    {
    return p1.Position.w > p2.Position.w;
    });*/
 

    vk::UnmapMemory(vk::g_vulkanContext.m_device, m_particlesMemory);
}

void CParticleSystem::CreateDebugElements(CUIManager* manager)
{
}

void CParticleSystem::SetUpdatePipeline(VkPipeline pipeline)
{
    m_updatePipeline = pipeline;
}

void CParticleSystem::SetDescriptorSets(VkDescriptorSet graphic, VkDescriptorSet compute)
{
    m_graphicDescriptorSet = graphic;
    m_computeDescriptorSet = compute;
}

void CParticleSystem::UpdateDescriptorSets()
{
    TRAP(m_particleTexture);

    VkDescriptorBufferInfo storageDescInfo;
    storageDescInfo.buffer = m_particlesBuffer;
    storageDescInfo.offset = 0;
    storageDescInfo.range = VK_WHOLE_SIZE;

    {
        VkWriteDescriptorSet wDesc;
        wDesc = InitUpdateDescriptor(m_computeDescriptorSet, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &storageDescInfo);        
        vk::UpdateDescriptorSets(vk::g_vulkanContext.m_device, 1, &wDesc, 0, nullptr);
    }

    {
        std::vector<VkWriteDescriptorSet> wDesc;
        wDesc.resize(2);
        wDesc[0] = InitUpdateDescriptor(m_graphicDescriptorSet, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &storageDescInfo);

        VkDescriptorImageInfo imgInfo = m_particleTexture->GetTextureDescriptor();
        wDesc[1] = InitUpdateDescriptor(m_graphicDescriptorSet, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &m_particleTexture->GetTextureDescriptor());

        vk::UpdateDescriptorSets(vk::g_vulkanContext.m_device, (uint32_t)wDesc.size(), wDesc.data(), 0, nullptr);
    }
}

void CParticleSystem::Init()
{
    unsigned int totalSize = m_maxParticles * sizeof(CParticle);
    AllocBufferMemory(m_particlesBuffer, m_particlesMemory, totalSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT); //this buffer should be device local

    TRAP(!m_updateShaderFile.empty());
    TRAP(CreateShaderModule(m_updateShaderFile, m_updateShader));
}

 void CParticleSystem::Validate()
 {
    TRAP(m_graphicDescriptorSet != VK_NULL_HANDLE);
    TRAP(m_computeDescriptorSet != VK_NULL_HANDLE);
    TRAP(m_particlesBuffer != VK_NULL_HANDLE);
    TRAP(m_particlesMemory != VK_NULL_HANDLE);
    TRAP(m_updateShader != VK_NULL_HANDLE);
    TRAP(m_updatePipeline != VK_NULL_HANDLE);
 }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//CParticlesRenderer
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CParticlesRenderer::CParticlesRenderer(VkRenderPass renderPass)
    : CRenderer(renderPass)
    , m_needUpdate(true)
    , m_quad(nullptr)
    , m_simPaused(false)
    , m_updateParticlesGlobalBuffer(nullptr)
    , m_computeGlobalDescLayout(VK_NULL_HANDLE)
    , m_computeSpecificDescLayout(VK_NULL_HANDLE)
    , m_computeGlobalDescriptor(VK_NULL_HANDLE)
    , m_renderParticlesGlobalBuffer(nullptr)
{
    PerspectiveMatrix(m_projMatrix);
    ConvertToProjMatrix(m_projMatrix);

	InputManager::GetInstance()->MapMouseButton(InputManager::MouseButtonsCallback(this, &CParticlesRenderer::OnMouseEvent));
}

CParticlesRenderer::~CParticlesRenderer()
{
    VkDevice dev = vk::g_vulkanContext.m_device;
    vk::DestroyDescriptorPool(vk::g_vulkanContext.m_device, m_descriptorPool, nullptr);

	MemoryManager::GetInstance()->FreeHandle(m_renderParticlesGlobalBuffer->GetRootParent());

    vk::DestroyDescriptorSetLayout(dev, m_computeGlobalDescLayout, nullptr);
    vk::DestroyDescriptorSetLayout(dev, m_computeSpecificDescLayout, nullptr);

    for(unsigned int i = 0; i < m_particleSystems.size(); ++i)
        delete m_particleSystems[i];
}

void CParticlesRenderer::PreRender()
{
	UpdateShaderParams();
}

void CParticlesRenderer::Render()
{
    if(m_needUpdate)
        UpdateDescSets();

    VkCommandBuffer buffer = vk::g_vulkanContext.m_mainCommandBuffer;
    VkBufferMemoryBarrier computeDoneBarrier[5];

    for(unsigned int i = 0; i < m_particleSystems.size(); ++i)
    {
        CParticleSystem* system = m_particleSystems[i];
        vk::CmdBindPipeline(buffer, VK_PIPELINE_BIND_POINT_COMPUTE, system->GetUpdatePipeline());
        vk::CmdBindDescriptorSets(buffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_computePipelineLayout, 0, 1, &m_computeGlobalDescriptor, 0, nullptr);

        vk::CmdBindDescriptorSets(buffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_computePipelineLayout, 1, 1, &system->GetComputeSet(), 0, nullptr);

        unsigned int particles = system->GetSpawnedParticles();
        if(particles && !m_simPaused)
            vk::CmdDispatch(buffer, particles, 1, 1);

        AddBufferBarier(computeDoneBarrier[i], system->GetParticlesBuffer(), VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_HOST_READ_BIT);
        
    }

    TRAP(m_particleSystems.size() < 5 && "Increase the number of compute barriers"); //oricum crapa mai sus
    vk::CmdPipelineBarrier(buffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 0, nullptr, (uint32_t)m_particleSystems.size(), computeDoneBarrier, 0, nullptr);

    if(!m_simPaused)
    {
        for(unsigned int i = 0; i < m_particleSystems.size(); ++i)
            m_particleSystems[i]->Update();   
    }

    StartRenderPass();

    vk::CmdBindPipeline(buffer, m_graphicPipeline.GetBindPoint(), m_graphicPipeline.Get());
    vk::CmdBindDescriptorSets(buffer, m_graphicPipeline.GetBindPoint(), m_graphicPipeline.GetLayout(), 0, 1, &m_graphicGlobalDescriptor, 0, nullptr);
    TRAP(m_quad);

    for(unsigned int i = 0; i < m_particleSystems.size(); ++i)
    {
        CParticleSystem* system = m_particleSystems[i];
        unsigned int particles = system->GetSpawnedParticles();
        if(particles)
        {
            vk::CmdBindDescriptorSets(buffer, m_graphicPipeline.GetBindPoint(), m_graphicPipeline.GetLayout(), 1, 1, &system->GetGraphicSet(), 0, nullptr);
            m_quad->Render(-1, particles);
        }
    }

    EndRenderPass();
}


void CParticlesRenderer::Init()
{
    CRenderer::Init();

    CreateGraphicDescriptors();

    //m_particleSystem = new CParticleSystem(glm::vec3(0.0f, 0.0f, -2.25f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    //m_particleSystem = new CParticleSystem(glm::vec3(0.0f, -1.0f, -1.25f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(-1.0f, 0.0f, 0.0f));

    CreateComputeLayouts();
    CreateComputeDescriptors();
	
	BufferHandle* mainBuffer = MemoryManager::GetInstance()->CreateBuffer(EMemoryContextType::UniformBuffers, { sizeof(SUpdateGlobalParams), sizeof(SParticlesParams) }, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
	m_updateParticlesGlobalBuffer = mainBuffer->CreateSubbuffer(sizeof(SUpdateGlobalParams));
	m_renderParticlesGlobalBuffer = mainBuffer->CreateSubbuffer(sizeof(SParticlesParams));

    SVertex vertices[] = {
        SVertex(glm::vec3(-1.0f, 1.0f, 0.0f), glm::vec2(0.0f, 0.0f)),
        SVertex(glm::vec3(1.0f, 1.0f, 0.0f), glm::vec2(1.0f, 0.0f)),
        SVertex(glm::vec3(1.0f, -1.0f, 0.0f), glm::vec2(1.0f, 1.0f)),
        SVertex(glm::vec3(-1.0f, -1.0f, 0.0f), glm::vec2(0.0f, 1.0f))
    };

    unsigned int indexes[] = {2, 1, 0, 0, 3, 2};

    m_quad = new Mesh(std::vector<SVertex> (&vertices[0], &vertices[0] + 4), std::vector<unsigned int>(&indexes[0], &indexes[0] + 6));

    m_needUpdate = true;

    m_graphicPipeline.SetVertexShaderFile("bilboard.vert");
    m_graphicPipeline.SetFragmentShaderFile("bilboard.frag");
    m_graphicPipeline.SetVertexInputState(Mesh::GetVertexDesc());
    m_graphicPipeline.SetDepthTest(true);
    m_graphicPipeline.SetDepthWrite(false);

    VkPipelineColorBlendAttachmentState blendState;
    blendState.blendEnable = VK_TRUE;
    blendState.colorBlendOp = VK_BLEND_OP_ADD;
    blendState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blendState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendState.alphaBlendOp = VK_BLEND_OP_MAX;
    blendState.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blendState.dstAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
    blendState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT;

    m_graphicPipeline.AddBlendState(blendState);

    std::vector<VkDescriptorSetLayout> layouts;
    layouts.push_back(m_graphicGlobalDescLayout);
    layouts.push_back(m_graphicSpecificDescLayout);

    m_graphicPipeline.CreatePipelineLayout(layouts);
    m_graphicPipeline.Init(this, m_renderPass, 0);
}

void CParticlesRenderer::Register(CParticleSystem* system)
{
    TRAP(system);
    auto it = std::find(m_particleSystems.begin(), m_particleSystems.end(), system);
    if(it != m_particleSystems.end())
        return;

    system->Init();
    CreateComputePipeline(system);

    VkDescriptorSet computeDescriptorSet = AllocDescriptorSet(m_computeSpecificDescLayout);
    VkDescriptorSet graphicDescriptorSet = AllocDescriptorSet(m_graphicSpecificDescLayout);

    system->SetDescriptorSets(graphicDescriptorSet, computeDescriptorSet);
    system->UpdateDescriptorSets();

    if (m_uiManager)
        system->CreateDebugElements(m_uiManager);

    m_particleSystems.push_back(system);
}

void CParticlesRenderer::RegisterDebugManager(CUIManager* manager)
{
    m_uiManager = manager;
}

bool CParticlesRenderer::OnMouseEvent(const MouseInput& mouseInput)
{
	if (mouseInput.IsButtonDown(MouseInput::Middle) && mouseInput.IsSpecialKeyPressed(SpecialKey::Shift))
	{
		ToggleSim();
		return true;
	}
	return false;
}

void CParticlesRenderer::PopulatePoolInfo(std::vector<VkDescriptorPoolSize>& poolSize, unsigned int& maxSets)
{
    const unsigned int maxSystemsCnt = 5;
    AddDescriptorType(poolSize, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2); //for global use (graphic + compute)

    AddDescriptorType(poolSize, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2 * maxSystemsCnt); //graphic + compute
    AddDescriptorType(poolSize, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 * maxSystemsCnt);

    maxSets = 2 + maxSystemsCnt * 2;
}

void CParticlesRenderer::CreateComputePipeline(CParticleSystem* system)
{
    TRAP(system);
    VkPipeline pipeline;

    VkPipelineShaderStageCreateInfo shaderStage;
    cleanStructure(shaderStage);
    shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStage.flags = 0;
    shaderStage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    shaderStage.pName = "main";
    shaderStage.module = system->GetUpdateShader();

    VkComputePipelineCreateInfo pipelineCrtInfo;
    cleanStructure(pipelineCrtInfo);
    pipelineCrtInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineCrtInfo.flags = 0;
    pipelineCrtInfo.stage = shaderStage;
    pipelineCrtInfo.layout = m_computePipelineLayout;

    VULKAN_ASSERT(vk::CreateComputePipelines(vk::g_vulkanContext.m_device, VK_NULL_HANDLE, 1, &pipelineCrtInfo, nullptr, &pipeline));

    system->SetUpdatePipeline(pipeline);
}
void CParticlesRenderer::CreateComputeLayouts()
{
    {
        std::vector<VkDescriptorSetLayoutBinding> bindings;
        bindings.resize(1);
        bindings[0] = CreateDescriptorBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);

        VkDescriptorSetLayoutCreateInfo descLayoutCrtInfo;
        cleanStructure(descLayoutCrtInfo);
        descLayoutCrtInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descLayoutCrtInfo.flags = 0;
        descLayoutCrtInfo.bindingCount = (uint32_t)bindings.size();
        descLayoutCrtInfo.pBindings = bindings.data();
        VULKAN_ASSERT(vk::CreateDescriptorSetLayout(vk::g_vulkanContext.m_device, &descLayoutCrtInfo, nullptr, &m_computeGlobalDescLayout));
    }
    {
        //this one will be used by all the specific properties of the particle systems
        std::vector<VkDescriptorSetLayoutBinding> bindings;
        bindings.resize(1);
        bindings[0] = CreateDescriptorBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);

        VkDescriptorSetLayoutCreateInfo descLayoutCrtInfo;
        cleanStructure(descLayoutCrtInfo);
        descLayoutCrtInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descLayoutCrtInfo.flags = 0;
        descLayoutCrtInfo.bindingCount = (uint32_t)bindings.size();
        descLayoutCrtInfo.pBindings = bindings.data();
        VULKAN_ASSERT(vk::CreateDescriptorSetLayout(vk::g_vulkanContext.m_device, &descLayoutCrtInfo, nullptr, &m_computeSpecificDescLayout));
    }

    VkDescriptorSetLayout layouts[2] = {m_computeGlobalDescLayout, m_computeSpecificDescLayout};

    VkPipelineLayoutCreateInfo pipeLayCrtInfo;
    cleanStructure(pipeLayCrtInfo);
    pipeLayCrtInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeLayCrtInfo.setLayoutCount = 2;
    pipeLayCrtInfo.pSetLayouts = layouts;

    VULKAN_ASSERT(vk::CreatePipelineLayout(vk::g_vulkanContext.m_device, &pipeLayCrtInfo, nullptr, &m_computePipelineLayout));
}

void CParticlesRenderer::UpdateDescSets()
{
    {
		VkDescriptorBufferInfo uniformDescInfo = m_updateParticlesGlobalBuffer->GetDescriptor();;

        std::vector<VkWriteDescriptorSet> wDesc;
        wDesc.resize(1);
        cleanStructure(wDesc[0]);
        wDesc[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        wDesc[0].dstSet = m_computeGlobalDescriptor;
        wDesc[0].dstBinding = 0;
        wDesc[0].dstArrayElement = 0;
        wDesc[0].descriptorCount = 1;
        wDesc[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        wDesc[0].pBufferInfo = &uniformDescInfo;

        vk::UpdateDescriptorSets(vk::g_vulkanContext.m_device, (uint32_t)wDesc.size(), wDesc.data(), 0, nullptr);
    }

    {
        VkDescriptorBufferInfo uniformDescInfo = m_renderParticlesGlobalBuffer->GetDescriptor();

        VkWriteDescriptorSet wDesc;
        cleanStructure(wDesc);
        wDesc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        wDesc.dstSet = m_graphicGlobalDescriptor;
        wDesc.dstBinding = 0;
        wDesc.dstArrayElement = 0;
        wDesc.descriptorCount = 1;
        wDesc.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        wDesc.pBufferInfo = &uniformDescInfo;

        vk::UpdateDescriptorSets(vk::g_vulkanContext.m_device, 1, &wDesc, 0, nullptr);
    }
    m_needUpdate = false;
}

void CParticlesRenderer::UpdateShaderParams()
{
    float dt = 1 / 60.0f; //NICE
    glm::mat4 projViewMatrix = m_projMatrix * ms_camera.GetViewMatrix();
    {
		SUpdateGlobalParams* params = m_updateParticlesGlobalBuffer->GetPtr<SUpdateGlobalParams*>();
        params->Params = glm::vec4(dt, 0.0f, 0.0f, 0.0f);
        params->ProjViewMatrix = projViewMatrix;
    }

    {
        SParticlesParams* p = m_renderParticlesGlobalBuffer->GetPtr<SParticlesParams*>();
        p->CameraPos = glm::vec4(ms_camera.GetPos(), 1.0f);
        p->CameraUp = glm::vec4(ms_camera.GetUpVector(), 0.0f);
        p->CameraRight = glm::vec4(ms_camera.GetRightVector(), 0.0f);
        p->ProjViewMatrix = projViewMatrix;
    }
}

void CParticlesRenderer::CreateDescriptorSetLayout()
{
    {
        std::vector<VkDescriptorSetLayoutBinding> bindings;
        bindings.resize(1);
        bindings[0] = CreateDescriptorBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);

        VkDescriptorSetLayoutCreateInfo descLayoutCrtInfo;
        cleanStructure(descLayoutCrtInfo);
        descLayoutCrtInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descLayoutCrtInfo.flags = 0;
        descLayoutCrtInfo.bindingCount = (uint32_t)bindings.size();
        descLayoutCrtInfo.pBindings = bindings.data();

        VULKAN_ASSERT(vk::CreateDescriptorSetLayout(vk::g_vulkanContext.m_device, &descLayoutCrtInfo, nullptr, &m_graphicGlobalDescLayout));
    }
    {
        std::vector<VkDescriptorSetLayoutBinding> bindings;
        bindings.resize(2);
        bindings[0] = CreateDescriptorBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
        bindings[1] = CreateDescriptorBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);

        VkDescriptorSetLayoutCreateInfo descLayoutCrtInfo;
        cleanStructure(descLayoutCrtInfo);
        descLayoutCrtInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descLayoutCrtInfo.flags = 0;
        descLayoutCrtInfo.bindingCount = (uint32_t)bindings.size();
        descLayoutCrtInfo.pBindings = bindings.data();
    
        VULKAN_ASSERT(vk::CreateDescriptorSetLayout(vk::g_vulkanContext.m_device, &descLayoutCrtInfo, nullptr, &m_graphicSpecificDescLayout));
    }
}

void CParticlesRenderer::CreateGraphicDescriptors()
{
    VkDescriptorSetAllocateInfo allocInfo; 
    cleanStructure(allocInfo);
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &m_graphicGlobalDescLayout;

    //m_descriptorSets.resize(1); nu mai folosi mizeria asta
    VULKAN_ASSERT(vk::AllocateDescriptorSets(vk::g_vulkanContext.m_device, &allocInfo, &m_graphicGlobalDescriptor));
}

void CParticlesRenderer::CreateComputeDescriptors()
{
    VkDescriptorSetAllocateInfo allocInfo; 
    cleanStructure(allocInfo);
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &m_computeGlobalDescLayout;

    VULKAN_ASSERT(vk::AllocateDescriptorSets(vk::g_vulkanContext.m_device, &allocInfo, &m_computeGlobalDescriptor));
}

VkDescriptorSet CParticlesRenderer::AllocDescriptorSet(VkDescriptorSetLayout layout)
{
    VkDescriptorSet descriptor;

    AllocDescriptorSets(m_descriptorPool, layout, &descriptor);
    return descriptor;
}
