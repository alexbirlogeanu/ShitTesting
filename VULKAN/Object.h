#pragma once

#include "glm/glm.hpp"
#include "PickManager.h"
#include "Renderer.h"
#include "VulkanLoader.h"
#include "Batch.h"
#include "Material.h"
#include "Serializer.h"

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

//////////////////////////////////////////TEST Seriable

class ObjectSer : public SeriableImpl<ObjectSer>
{
public:
	ObjectSer();
	virtual ~ObjectSer();

private:
	DECLARE_PROPERTY(Material*, ObjectMaterial, ObjectSer);
	DECLARE_PROPERTY(glm::vec3, Position, ObjectSer);
};




///////////////////////////////////////////

class ObjectFactory
{
public:
	static void LoadXml(const std::string& file){}
	static const std::vector<Object*>& GetObjects() { return m_objects; };
private:
	static std::vector<Object*> m_objects;
};

class Object :/* public CPickable,*/ public SeriableImpl<Object>
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

    BoundingBox GetBoundingBox() const
    {
        return GetObjectMesh()->GetBB();
    }

    glm::mat4 GetModelMatrix();

    void Render();
    //virtual void GetPickableDescription(std::vector<std::string>& texts) override;
    //virtual bool ChangePickableProperties(unsigned int key) override;
	Object();
	virtual ~Object();
private:
   

    void ValidateResources();

    friend class ObjectSerializer;
private:
	DECLARE_PROPERTY(Mesh*, ObjectMesh, Object);
	DECLARE_PROPERTY(Material*, ObjectMaterial, Object);
	DECLARE_PROPERTY(bool, isShadowCaster, Object);
	DECLARE_PROPERTY(glm::vec3, worldPosition, Object);
	DECLARE_PROPERTY(glm::vec3, scale, Object);
	DECLARE_PROPERTY(std::string, debugName, Object);

    bool                    m_needComputeModelMtx;

    float                   m_yRot;
    float                   m_xRot;

    glm::mat4               m_modelMatrix;
};

class ObjectSerializer : public Serializer
{
public:
	ObjectSerializer();
	virtual ~ObjectSerializer();

	virtual void Save(const std::string& filename) override;
	virtual void Load(const std::string& filename) override;

	void AddObject(Object* obj);
private:
	std::vector<Object*>		m_objects;
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
	virtual void PreRender() override;
protected:
    virtual void CreateDescriptorSetLayout() override;
    virtual void PopulatePoolInfo(std::vector<VkDescriptorPoolSize>& poolSize, unsigned int& maxSets) override;

    void UpdateResourceTable() override;
    void UpdateGraphicInterface() override;
private:
    //these 2 methods are duplicate code. See ShadowRenderer
    void InitDescriptorNodes();
    void InitMemoryOffsetNodes();

    void SetupPipeline(const std::string& vertexFile, const std::string& fragmentFile, CGraphicPipeline& pipeline);

    void UpdateShaderParams();
private:
    struct Node
    {
        Object*             obj;
        VkDescriptorSet     descriptorSet;
		BufferHandle*       buffer;

        Node()
            : obj(nullptr)
            , descriptorSet(VK_NULL_HANDLE)
            , buffer(nullptr)
        {}

        Node(Object* o)
            : obj(o)
            , descriptorSet(VK_NULL_HANDLE)
            , buffer(nullptr)
        {}
    };

    struct SBatch
    {
        std::string         debugMarker;
        CGraphicPipeline    pipeline;
        std::vector<Node>   nodes;
    };

    unsigned int                        m_numOfObjects;
    glm::mat4                           m_projMatrix;

    //vulkan Render shit
    VkDescriptorSetLayout               m_objectDescLayout;
    BufferHandle*                       m_instanceDataBuffer;
    VkSampler                           m_sampler;

	std::array<SBatch, (unsigned int)ObjectType::Count>  m_batches;
	Batch													m_solidBatch;
};

class CScene
{
public:
    static void AddObject(Object* obj);

    static BoundingBox GetBoundingBox() { return ms_sceneBoundingBox; };
private:
    static void UpdateBoundingBox();
private:
    static std::unordered_set<Object*>      ms_sceneObjects;
    static BoundingBox                      ms_sceneBoundingBox;
};
