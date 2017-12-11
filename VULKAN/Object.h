#pragma once

#include "glm/glm.hpp"
#include "PickManager.h"
#include "Renderer.h"
#include "VulkanLoader.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <array>

class Mesh;
class CTexture;
class Object;

enum class ObjectType
{
    Solid,
    NormalMap,
    Count
};

class ObjectFactory
{
public:
    static void LoadXml(const std::string& file);
    static const std::vector<Object*>& GetObjects();
private:
};

class Object : public CPickable
{
public:

    bool GetIsShadowCaster() const { return m_isShadowCaster; }
    
    void RotateX(float dir)
    {
        m_xRot += dir * glm::quarter_pi<float>();
        m_needComputeModelMtx = true;
    }

    void RotateY(float dir)
    {
        m_yRot += dir * glm::quarter_pi<float>();
        m_needComputeModelMtx = true;
    }

    void Translatez(float dir)
    {
        m_worldPosition += glm::vec3(.0f, .0f, dir);
        m_needComputeModelMtx = true;
    }

    void TranslateX(float dir)
    {
        m_worldPosition += glm::vec3(dir, .0f, .0f);
        m_needComputeModelMtx = true;
    }

    void SetScale(glm::vec3 scale)
    {
        m_scale = scale;
        m_needComputeModelMtx = true;
    }

    void SetPosition(glm::vec3 pos)
    {
        m_worldPosition = pos;
        m_needComputeModelMtx = true;
    }

    void SetMaterialProperties(float roughness = 1, float k = 0.5f, float F0 = 0.8f)
    {
        this->m_roughtness = roughness;
        this->m_metallic = k;
        this->F0 = F0;
    }
    glm::vec4 GetMaterialProperties() const { return glm::vec4(m_roughtness, m_metallic, F0, 0.0f); }

    BoundingBox GetBoundingBox() const
    {
        return m_mesh->GetBB();
    }

    ObjectType GetType() const { return m_type; }
    glm::mat4 GetModelMatrix();
    CTexture* GetAlbedoTexture() const { return m_albedoTexture; }
    CTexture* GetNormalMap() const { return m_normalMapTexture; }
    void Render();
    virtual void GetPickableDescription(std::vector<std::string>& texts) override;
    virtual bool ChangePickableProperties(unsigned int key) override;
private:
    Object();
    virtual ~Object();

    void SetIsShadowCaster(bool isShadowCaster) { m_isShadowCaster = isShadowCaster; }
    void SetAlbedoTexture(CTexture* text) { m_albedoTexture = text; }
    void SetNormalMap(CTexture* text) { m_normalMapTexture = text; }
    void SetMesh(Mesh* mesh) { m_mesh = mesh; }
    void SetType (ObjectType type) { m_type = type; }
    void SetDebugName(const std::string& debug) { m_debugName = debug; }

    void ValidateResources();

    friend class ObjectFactoryImpl;
private:
    bool                    m_isShadowCaster;
    bool                    m_needComputeModelMtx;

    CTexture*               m_albedoTexture;
    CTexture*               m_normalMapTexture;
    Mesh*                   m_mesh;

    glm::vec3               m_worldPosition;
    glm::vec3               m_scale;
    float                   m_yRot;
    float                   m_xRot;

    glm::mat4               m_modelMatrix;

    //material properties
    float                   m_roughtness;
    float                   F0;
    float                   m_metallic;

    ObjectType              m_type;
    std::string             m_debugName;
};

enum GBuffer //until refactoring change values into ao.cpp too
{
    GBuffer_Albedo,
    GBuffer_Specular,
    GBuffer_Normals,
    GBuffer_Position,
    GBuffer_Final,
    GBuffer_Debug,
    GBuffer_Count,
    GBuffer_InputCnt = GBuffer_Position - GBuffer_Albedo + 1,
};

class ObjectRenderer : public CRenderer
{
public:
    ObjectRenderer(VkRenderPass renderPass, const std::vector<Object*>& objects); //hey, it works
    virtual ~ObjectRenderer();

    virtual void Render() override;
    virtual void Init() override;
protected:
    virtual void CreateDescriptorSetLayout() override;
    virtual void PopulatePoolInfo(std::vector<VkDescriptorPoolSize>& poolSize, unsigned int& maxSets) override;

    void UpdateResourceTable() override;
    void UpdateGraphicInterface() override;
private:
    void InitDescriptorNodes();
    void InitMemoryOffsetNodes();

    void SetupPipeline(const std::string& vertexFile, const std::string& fragmentFile, CGraphicPipeline& pipeline);

    void UpdateShaderParams();
private:
    struct Node
    {
        Object*             obj;
        VkDescriptorSet     descriptorSet;
        VkDeviceSize        offset; //offset used in global buffer

        Node()
            : obj(nullptr)
            , descriptorSet(VK_NULL_HANDLE)
            , offset(0)
        {}
    };

    struct Batch
    {
        std::string         debugMarker;
        CGraphicPipeline    pipeline;
        std::vector<Node>   nodes;
    };

    unsigned int                        m_numOfObjects;
    glm::mat4                           m_projMatrix;

    //vulkan Render shit
    VkDescriptorSetLayout               m_objectDescLayout;
    VkBuffer                            m_instanceDataBuffer;
    VkDeviceMemory                      m_instaceDataMemory;
    VkSampler                           m_sampler;

    std::array<Batch, (unsigned int)ObjectType::Count>  m_batches;
};