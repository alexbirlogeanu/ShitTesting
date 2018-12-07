#include "Object.h"

#include "Texture.h"
#include "Mesh.h"
#include "rapidxml/rapidxml.hpp"
#include "defines.h"
#include "Utils.h"

#include "Scene.h"

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
		CScene::AddObject(obj);
		BatchManager::GetInstance()->AddObject(obj);
	}
}

void ObjectSerializer::AddObject(Object* obj)
{
	m_objects.push_back(obj);
	CScene::AddObject(obj);
	BatchManager::GetInstance()->AddObject(obj);
}

//////////////////////////////////////////////////////////////////////
//ObjectFactory
//////////////////////////////////////////////////////////////////////


//class ObjectFactoryImpl
//{
//public:
//    struct ObjectDesc
//    {
//        bool isShadowCaster;
//        bool albedosRGB;
//        bool normalmapsRGB;
//        ObjectType type;
//        float roughness;
//        float metalness;
//        float F0;
//        glm::vec3 scale;
//        glm::vec3 position;
//        std::string albedoTextFile;
//        std::string normalMapTextFile;
//        std::string meshFile;
//        std::string debugName;
//
//        ObjectDesc()
//            : isShadowCaster(false)
//            , roughness(1.0f)
//            , metalness(0.5f)
//            , F0(0.8f)
//            , albedosRGB(true)
//            , scale(1.0f)
//            , type(ObjectType::Solid)
//            , debugName("default")
//        {}
//    };
//
//    template<typename T>
//    void ClearMap(T& resourceMap)
//    {
//        for (auto it = resourceMap.begin(); it != resourceMap.end(); ++it)
//            delete it->second;
//
//        resourceMap.clear();
//    }
//
//
//    ObjectFactoryImpl()
//    {
//    }
//
//    ~ObjectFactoryImpl()
//    {
//        for (auto& o : m_objects)
//            delete o;
//
//        ClearMap(m_texturesMap);
//        ClearMap(m_meshMap);
//    }
//
//    const std::vector<Object*>& GetObjects() const { return m_objects; }
//
//    void LoadObj(const ObjectDesc& desc)
//    {
//        Object* obj = new Object();
//        CTexture* albedoText = LoadTexture(desc.albedoTextFile, desc.albedosRGB);
//        Mesh* mesh = LoadMesh(desc.meshFile);
//
//        obj->SetAlbedoTexture(albedoText);
//        obj->SetMesh(mesh);
//        obj->SetScale(desc.scale);
//        obj->SetIsShadowCaster(desc.isShadowCaster);
//        obj->SetPosition(desc.position);
//        obj->SetMaterialProperties(desc.roughness, desc.metalness, desc.F0);
//        obj->SetType(desc.type);
//        obj->SetDebugName(desc.debugName);
//
//        if (!desc.normalMapTextFile.empty())
//        {
//            CTexture* normalMap = LoadTexture(desc.normalMapTextFile, desc.normalmapsRGB);
//            obj->SetNormalMap(normalMap);
//        }
//
//        obj->ValidateResources();
//        CScene::AddObject(obj);
//        m_objects.push_back(obj);
//    }
//private:
//    CTexture* LoadTexture(const std::string& textFile, bool isRgb)
//    {
//        auto it = m_texturesMap.find(textFile);
//        
//        if (it != m_texturesMap.end())
//            return it->second;
//
//        SImageData imgData;
//        Read2DTextureData(imgData, std::string(TEXTDIR) + textFile, isRgb);
//        CTexture* text = new CTexture(imgData, true);
//        m_texturesMap.emplace(textFile, text);
//        return text;
//    }
//
//    Mesh* LoadMesh(const std::string& meshFile)
//    {
//        auto it = m_meshMap.find(meshFile);
//        
//        if (it != m_meshMap.end())
//            return it->second;
//
//        Mesh* mesh = new Mesh(meshFile);
//        m_meshMap.emplace(meshFile, mesh);
//        return mesh;
//    }
//
//private:
//    std::vector<Object*>                            m_objects;
//    std::unordered_map<std::string, CTexture*>      m_texturesMap;
//    std::unordered_map<std::string, Mesh*>          m_meshMap;
//};
//
//static ObjectFactoryImpl s_objectFactory;


//void ExtractObjectDesc(rapidxml::xml_node<char>* xmlNode, ObjectFactoryImpl::ObjectDesc& desc)
//{
//    auto convertToFloat = []( rapidxml::xml_attribute<char>* att, float& field)
//    {
//        if (att)
//        {
//            field = (float)atof(att->value());
//            return true;
//        }
//        return false;
//    };
//
//    auto convertToBool = []( rapidxml::xml_attribute<char>* att, bool& field)
//    {
//        if (att)
//        {
//            field = atoi(att->value()) != 0;
//            return true;
//        }
//        return false;
//    };
//
//    auto convertToInt = [](rapidxml::xml_attribute<char>* att, unsigned int& val)
//    {
//        if (att)
//        {
//            val = atoi(att->value());
//            return true;
//        }
//        return false;
//    };
//
//    auto convertToVec3 = [&convertToFloat](rapidxml::xml_node<char>* node, glm::vec3& v)
//    {
//        if (node)
//        {
//            float e;
//            if (convertToFloat(node->first_attribute("value", 0, false), e))
//            {
//                v = glm::vec3(e);
//            }
//            else
//            {
//                convertToFloat(node->first_attribute("x", 0, false), v.x);
//                convertToFloat(node->first_attribute("y", 0, false), v.y);
//                convertToFloat(node->first_attribute("z", 0, false), v.z);
//            }
//            return true;
//        }
//        return false;
//    };
//
//    rapidxml::xml_attribute<char>* mesh = xmlNode->first_attribute("mesh", 0, false);
//
//    rapidxml::xml_node<char>* albedoText = xmlNode->first_node("albedoText", 0, false);
//     TRAP(albedoText);
//    rapidxml::xml_attribute<char>* albedoTextFile = albedoText->first_attribute("file", 0, false);
//    rapidxml::xml_attribute<char>* albedoTextIssRgb = albedoText->first_attribute("sRGB", 0, false);
//
//    TRAP(albedoTextFile);
//    convertToBool(albedoTextIssRgb, desc.albedosRGB);
//    desc.albedoTextFile = albedoTextFile->value();
//    TRAP(mesh)
//    desc.meshFile = mesh->value();
//
//    rapidxml::xml_node<char>* position = xmlNode->first_node("position", 0, false);
//    convertToVec3(xmlNode->first_node("position", 0, false), desc.position);
//    convertToVec3(xmlNode->first_node("scale", 0, false), desc.scale);
//
//    convertToBool(xmlNode->first_attribute("castShadow", 0, false), desc.isShadowCaster);
//    convertToFloat(xmlNode->first_attribute("roughness", 0, false), desc.roughness);
//    convertToFloat(xmlNode->first_attribute("metalness", 0, false), desc.metalness);
//    convertToFloat(xmlNode->first_attribute("F0", 0, false), desc.F0);
//    
//
//    unsigned int type = 0;
//    convertToInt(xmlNode->first_attribute("type", 0, false), type);
//
//    TRAP (type < (unsigned int)ObjectType::Count);
//    if (type < (unsigned int)ObjectType::Count)
//        desc.type = (ObjectType)type;
//
//    rapidxml::xml_attribute<char>* debugName = xmlNode->first_attribute("name", 0, false);
//    if (debugName)
//        desc.debugName = debugName->value();
//
//    rapidxml::xml_node<char>* normalMapText = xmlNode->first_node("normalmap", 0, false);
//    if (normalMapText)
//    {
//        convertToBool(normalMapText->first_attribute("sRGB", 0, false), desc.normalmapsRGB);
//        desc.normalMapTextFile = normalMapText->first_attribute("file", 0, false)->value();
//    }
//}
//
//void ObjectFactory::LoadXml(const std::string& file)
//{
//    unsigned int numObjects = 0;
//    char* xmlContent;
//    ReadXmlFile(file, &xmlContent);
//
//    rapidxml::xml_document<char> xml;
//    xml.parse<rapidxml::parse_default>(xmlContent);
//
//    rapidxml::xml_node<char>* objects = xml.first_node("objects", 0, false);
//    TRAP(objects);
//    for (auto child = objects->first_node("object", 0, false); child != nullptr; child = child->next_sibling())
//    {
//        ObjectFactoryImpl::ObjectDesc desc;
//        ExtractObjectDesc(child, desc);
//        s_objectFactory.LoadObj(desc);
//        ++numObjects;
//    }
//
//    delete xmlContent;
//    std::cout << "Number of Objects loaded: " << numObjects << std::endl;
//}
//
//const std::vector<Object*>& ObjectFactory::GetObjects()
//{
//    return s_objectFactory.GetObjects();
//}

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
    }

    return m_modelMatrix;
}

void Object::Render()
{
	if (m_ObjectMesh)
		m_ObjectMesh->Render();
}

 /*void Object::GetPickableDescription(std::vector<std::string>& texts)
{
   texts.reserve(2);
    texts.push_back("R/T Change roughness: " + std::to_string(m_roughtness));
    texts.push_back("F/G Change metallic: " + std::to_string(m_metallic));
}
*/
 /*bool Object::ChangePickableProperties(unsigned int key)
{
   if (key == 'R')
    {
        m_roughtness = glm::max(m_roughtness - 0.05f, 0.0f);
        return true;
    }
    else if(key == 'T')
    {
        m_roughtness = glm::min(m_roughtness + 0.05f, 1.0f);
        return true;
    }
    else if(key == 'F')
    {
        m_metallic = glm::max(m_metallic - 0.05f, 0.0f);
        return true;
    }
    else if(key == 'G')
    {
        m_metallic = glm::min(m_metallic + 0.05f, 1.0f);
        return true;
    }

    return false;
}
*/
void Object::ValidateResources()
{
    //if (m_type == ObjectType::NormalMap)
        //TRAP(m_normalMapTexture);
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
