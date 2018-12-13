#pragma once

#include "glm/glm.hpp"
#include "PickManager.h"
#include "Renderer.h"
#include "VulkanLoader.h"
#include "Batch.h"
#include "Material.h"
#include "Serializer.h"
#include "Singleton.h"

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

class Object :/* public CPickable,*/ public SeriableImpl<Object>
{
public:
    
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

    BoundingBox3D GetBoundingBox() const
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
	DECLARE_PROPERTY(bool, IsShadowCaster, Object);
	DECLARE_PROPERTY(glm::vec3, worldPosition, Object);
	DECLARE_PROPERTY(glm::vec3, scale, Object);
	DECLARE_PROPERTY(std::string, debugName, Object);

    bool                    m_needComputeModelMtx;

    float                   m_yRot;
    float                   m_xRot;

    glm::mat4               m_modelMatrix;
};

class ObjectSerializer : public Serializer, public Singleton<ObjectSerializer>
{
	friend class Singleton<ObjectSerializer>;
public:
	ObjectSerializer();
	virtual ~ObjectSerializer();

	virtual void LoadContent() override;
	virtual void SaveContent() override;

	const std::vector<Object*>& GetObjects() const { return m_objects; }
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
    ObjectRenderer(VkRenderPass renderPass);
    virtual ~ObjectRenderer();

    virtual void Render() override;
    virtual void Init() override;
	virtual void PreRender() override;
protected:
    virtual void PopulatePoolInfo(std::vector<VkDescriptorPoolSize>& poolSize, unsigned int& maxSets) override;
	void CreateDescriptorSetLayout(); //we dont need this function for this kind of renderer ?? (something is obsolete)

    void UpdateResourceTable() override;
    void UpdateGraphicInterface() override;
private:
    glm::mat4                           m_projMatrix;

};
