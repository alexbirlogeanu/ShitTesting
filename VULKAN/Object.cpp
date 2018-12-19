#include "Object.h"

#include "Texture.h"
#include "Mesh.h"
#include "rapidxml/rapidxml.hpp"
#include "defines.h"
#include "Utils.h"
#include "Batch.h"
#include "Scene.h"
#include "Material.h"

#include <cstdlib>


//////////////////////////////////////////////////////////////////////
//ObjectSerializer
//////////////////////////////////////////////////////////////////////

ObjectSerializer::ObjectSerializer()
{
	
}

ObjectSerializer::~ObjectSerializer()
{
	for (unsigned int i = 0; i < m_objects.size(); ++i)
		delete m_objects[i];
}

void ObjectSerializer::SaveContent()
{
	for (unsigned int i = 0; i < m_objects.size(); ++i)
		m_objects[i]->Serialize(this);
}

void ObjectSerializer::LoadContent()
{
	while (!HasReachedEof()) //HAS reached eof is not working properly
	{
		Object* obj = new Object();
		if (obj->Serialize(this))
			m_objects.push_back(obj);
	}

	for (Object* obj : m_objects)
	{
		Scene::GetInstance()->AddObject(obj);
		BatchManager::GetInstance()->AddObject(obj);
	}
}

void ObjectSerializer::AddObject(Object* obj)
{
	m_objects.push_back(obj);
	Scene::GetInstance()->AddObject(obj);
	BatchManager::GetInstance()->AddObject(obj);
}

//////////////////////////////////////////////////////////////////////
//Object
//////////////////////////////////////////////////////////////////////

BEGIN_PROPERTY_MAP(Object)
	IMPLEMENT_PROPERTY(Mesh*, ObjectMesh, "mesh", Object),
	IMPLEMENT_PROPERTY(Material*, ObjectMaterial, "material", Object),
	IMPLEMENT_PROPERTY(bool, IsShadowCaster, "castShadows", Object),
	IMPLEMENT_PROPERTY(glm::vec3, worldPosition, "position", Object),
	IMPLEMENT_PROPERTY(glm::vec3, scale, "scale", Object),
	IMPLEMENT_PROPERTY(std::string, debugName, "name", Object)
END_PROPERTY_MAP(Object)

Object::Object()
	: SeriableImpl<Object>("object")
	, m_needComputeModelMtx(true)
    , m_xRot(0.0f)
    , m_yRot(0.0f)
    , m_scale(1.0f)
	, m_ObjectMesh(nullptr)
{
}

Object::~Object()
{
}

glm::mat4 Object::GetModelMatrix()
{
    if (m_needComputeModelMtx)
    {
        m_modelMatrix = glm::mat4(1.0f);

        m_modelMatrix = glm::translate(m_modelMatrix, m_worldPosition);
        m_modelMatrix = glm::scale(m_modelMatrix, m_scale);

        glm::mat4 rotateMatY = glm::rotate(glm::mat4(1.0), m_yRot, glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 rotateMatX = glm::rotate(glm::mat4(1.0), m_xRot, glm::vec3(1.0f, 0.0f, 0.0f));
        glm::mat4 rotateMat = rotateMatX * rotateMatY;

        m_modelMatrix = m_modelMatrix * rotateMat;
        m_needComputeModelMtx = false;

		BoundingBox3D meshBB = GetObjectMesh()->GetBB();
		m_boundingBox.Max = glm::vec3(m_modelMatrix * glm::vec4(meshBB.Max, 1.0f));
		m_boundingBox.Min = glm::vec3(m_modelMatrix * glm::vec4(meshBB.Min, 1.0f));
    }

    return m_modelMatrix;
}

void Object::Render()
{
	if (m_ObjectMesh)
		m_ObjectMesh->Render();
}
void Object::ValidateResources()
{
}

//////////////////////////////////////////////////////////////////////////
//ObjectRenderer
//////////////////////////////////////////////////////////////////////////
struct ObjectShaderParams
{
    glm::mat4 Mvp;
    glm::mat4 WorldMatrix;
    glm::vec4 MaterialProp; //x = roughness, y = k, z = F0
    glm::vec4 ViewPos; //??
};

ObjectRenderer::ObjectRenderer(VkRenderPass renderPass)
	: CRenderer(renderPass, "SolidRenderPass")
{
}

ObjectRenderer::~ObjectRenderer()
{
}

void ObjectRenderer::Render()
{
    VkCommandBuffer cmd = vk::g_vulkanContext.m_mainCommandBuffer;
    StartRenderPass();
	
	BatchManager::GetInstance()->RenderAll();

    EndRenderPass();
}

void ObjectRenderer::PreRender()
{
}

void ObjectRenderer::Init()
{
    CRenderer::Init();

    PerspectiveMatrix(m_projMatrix);
    ConvertToProjMatrix(m_projMatrix);
}

void ObjectRenderer::CreateDescriptorSetLayout()
{
}

void ObjectRenderer::PopulatePoolInfo(std::vector<VkDescriptorPoolSize>& poolSize, unsigned int& maxSets)
{
    maxSets = 1;
    AddDescriptorType(poolSize, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1);}

void ObjectRenderer::UpdateResourceTable()
{
    UpdateResourceTableForColor(GBuffer_Final, EResourceType_FinalImage);
    UpdateResourceTableForColor(GBuffer_Albedo, EResourceType_AlbedoImage);
    UpdateResourceTableForColor(GBuffer_Specular, EResourceType_SpecularImage);
    UpdateResourceTableForColor(GBuffer_Normals, EResourceType_NormalsImage);
    UpdateResourceTableForColor(GBuffer_Position, EResourceType_PositionsImage);
    UpdateResourceTableForDepth(EResourceType_DepthBufferImage);
}

void ObjectRenderer::UpdateGraphicInterface()
{
}
