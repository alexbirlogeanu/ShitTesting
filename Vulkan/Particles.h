#pragma once 

#include "Renderer.h"
#include "glm/glm.hpp"
#include "glm/gtc/constants.hpp"
#include "VulkanLoader.h"

#include <vector>
#include <array>
#include <string>

#define MAX_PARTICLES_PER_SYSTEM 100

class CParticle
{
public:
    CParticle(){};
    ~CParticle(){};
    
    bool IsKaput() const { return Properties.y <= glm::epsilon<float>(); }

    glm::vec4   Velocity;
    glm::vec4   Position;
    glm::vec4   Color;
    glm::vec4   Properties; //x - life spawn, y - current life span, z - fade time, w - size
};

struct SUpdateGlobalParams
{
    glm::mat4 ProjViewMatrix;
    glm::vec4 Params;
};
class CUIManager;
class CUIVector;
class CUIAxisSystem;
class CUIText;
class CTexture;

class CParticleSpawner
{
public:
    CParticleSpawner(glm::vec3 pos = glm::vec3(), glm::vec3 normal = glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3 tangent = glm::vec3(1.0f, 0.0f, 0.0f));
    virtual ~CParticleSpawner();

    virtual void SpawnParticle(CParticle* p);
    virtual void Update(float dt = 1.0f/60.0f){}
    virtual glm::vec3 CreateSpawnDirection() = 0;
    virtual glm::vec3 CreateSpawnPosition() = 0;

    float GetParticleSizeTreshold() const { return m_particleSizeTreshold; }
    void SetParticleSizeTreshold(float val) { m_particleSizeTreshold = val; }

    float GetParticleSize() const { return m_particleSize; }
    void SetParticleSize(float val) { m_particleSize = val; }

    float GetParticleLifeSpawn() const { return m_particleLifeSpan; }
    void SetParticleLifeSpawn(float val) { m_particleLifeSpan = val; }

    float GetParticleLifeSpawnTreshold() const { return m_particleLifeSpanTreshold; }
    void SetParticleLifeSpawnTreshold(float val) { m_particleLifeSpanTreshold = val; }

    float GetFadeTime() const { return m_fadeTime; }
    void SetFadeTime(float val) { m_fadeTime = val; }

    float GetParticleSpeed() const { return m_particleSpeed; }
    void SetParticleSpeed(float val) { m_particleSpeed = val; }

    float GetParticleSpeedTreshold() const { return m_particleSpeedTreshold; }
    void SetParticleSpeedTreshold(float val) { m_particleSpeedTreshold = val; }

    void CreateDebugElements(CUIManager* manager);
protected:
    glm::mat3           m_TBN;
    glm::vec3           m_normal;
    glm::vec3           m_tangent;
    glm::vec3           m_bitangent;
    glm::vec3           m_worldPosition;

    float               m_particleSize;
    float               m_particleSizeTreshold;
    float               m_particleLifeSpan;
    float               m_particleLifeSpanTreshold;
    float               m_fadeTime;
    float               m_particleSpeed;
    float               m_particleSpeedTreshold;

    //debug
    CUIAxisSystem*      m_debugAxis; //move debug to spawner
};

class CConeSpawner : public CParticleSpawner
{
public:
    CConeSpawner(float alphaLimit = 90, glm::vec3 pos = glm::vec3(), glm::vec3 normal = glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3 tangent = glm::vec3(1.0f, 0.0f, 0.0f));
    virtual ~CConeSpawner();

    virtual glm::vec3 CreateSpawnDirection() override;
    virtual glm::vec3 CreateSpawnPosition() override;
private:
    float       m_alphaLimit;
};

class CRectangularSpawner : public CParticleSpawner
{
public:
    CRectangularSpawner(float w, float h, glm::vec3 pos = glm::vec3(), glm::vec3 normal = glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3 tangent = glm::vec3(1.0f, 0.0f, 0.0f));
    virtual ~CRectangularSpawner();
    virtual glm::vec3 CreateSpawnDirection() override;
    virtual glm::vec3 CreateSpawnPosition() override;
private:
    //world position is in the center of the plane
    glm::vec2   m_dimensions;
    float       m_maxLength;
};

class CParticleSystem
{
public:
    CParticleSystem(CParticleSpawner* spawner, unsigned int maxParticles = MAX_PARTICLES_PER_SYSTEM);
    virtual ~CParticleSystem();

    void Update(float dt = 1.0f/60.0f);

    unsigned int GetSpawnedParticles() const { return m_spawnedParticles; }
    VkBuffer& GetParticlesBuffer() { return m_particlesBuffer; }
    VkShaderModule GetUpdateShader() const { return m_updateShader; }

    void SetParticleTexture(CTexture* texture) { m_particleTexture = texture; }
    void SetUpdateShaderFile(const std::string& file) { m_updateShaderFile = file; }

protected:
    void SetUpdatePipeline(VkPipeline pipeline);
    void SetDescriptorSets(VkDescriptorSet graphic, VkDescriptorSet compute);
    void UpdateDescriptorSets();

    void Init();
    void Validate();
    void CreateDebugElements(CUIManager* manager);

    const VkDescriptorSet& GetGraphicSet() const { return m_graphicDescriptorSet; }
    const VkDescriptorSet& GetComputeSet() const { return m_computeDescriptorSet; }
    VkPipeline      GetUpdatePipeline() const { return m_updatePipeline; }
    friend class CParticlesRenderer;
protected:
    float               m_updateStep;
    float               m_elapsedTime;

    const uint32_t      m_maxParticles;
    unsigned int        m_spawnedParticles;

    //particle system world space

    CParticleSpawner*                                   m_spawner;
    CTexture*                                           m_particleTexture;

    VkShaderModule                                      m_updateShader;
    std::string                                         m_updateShaderFile;
    VkPipeline                                          m_updatePipeline;

    VkBuffer                                            m_particlesBuffer;
    VkDeviceMemory                                      m_particlesMemory;

    VkDescriptorSet                                     m_graphicDescriptorSet;
    VkDescriptorSet                                     m_computeDescriptorSet;
};
class Mesh;
//WARNING!! NU foloseste mizeria de array de descriptorsets
class CParticlesRenderer : public CRenderer
{
public:
    CParticlesRenderer(VkRenderPass renderPass);
    virtual ~CParticlesRenderer();

    void Render() override;
    virtual void Init() override;

    void ToggleSim() { m_simPaused = !m_simPaused; }

    void Register(CParticleSystem* system);
    void RegisterDebugManager(CUIManager* manager);
protected:
    struct SParticlesParams
    {
        glm::vec4 CameraPos;
        glm::vec4 CameraUp;
        glm::vec4 CameraRight;
        glm::mat4 ProjViewMatrix;
    };

    virtual void PopulatePoolInfo(std::vector<VkDescriptorPoolSize>&, unsigned int& maxSets);
    void CreateComputePipeline(CParticleSystem* system);
    void CreateComputeLayouts();

    void CreateGraphicDescriptors();
    void CreateComputeDescriptors();

    void UpdateDescSets();
    void UpdateShaderParams();

    VkDescriptorSet AllocDescriptorSet(VkDescriptorSetLayout layout);

    virtual void CreateDescriptorSetLayout() override;
protected:
    glm::mat4                           m_projMatrix;

    VkDescriptorSetLayout               m_computeGlobalDescLayout;
    VkDescriptorSetLayout               m_computeSpecificDescLayout;
    VkDescriptorSet                     m_computeGlobalDescriptor;
    
    VkDescriptorSetLayout               m_graphicGlobalDescLayout;
    VkDescriptorSetLayout               m_graphicSpecificDescLayout;
    VkDescriptorSet                     m_graphicGlobalDescriptor;

    VkPipelineLayout                    m_computePipelineLayout;
    CPipeline                           m_graphicPipeline;

    VkBuffer                            m_updateParticlesGlobalBuffer;
    VkDeviceMemory                      m_updateParticlesGlobalMemory;

    VkBuffer                            m_renderParticlesGlobalBuffer;
    VkDeviceMemory                      m_renderParticlesGlobalMemory;

    Mesh*                               m_quad;

    std::vector<CParticleSystem*>       m_particleSystems;


    CUIManager*                         m_uiManager; //debug purpose

    bool                                m_needUpdate;
    bool                                m_simPaused;
};