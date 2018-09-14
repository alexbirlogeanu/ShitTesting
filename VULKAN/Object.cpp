#include "Object.h"

#include "Texture.h"
#include "Mesh.h"
#include "rapidxml/rapidxml.hpp"
#include "defines.h"
#include "Utils.h"
#include "rapidxml/rapidxml_print.hpp"

#include <cstdlib>
#include <iostream>
#include <fstream>

//////////////////////////////////////////TEST Seriable
BEGIN_PROPERTY_MAP(ObjectSer)
	IMPLEMENT_PROPERTY(Material*, ObjectMaterial, "Material", ObjectSer),
	IMPLEMENT_PROPERTY(glm::vec3, Position, "Position", ObjectSer)
END_PROPERTY_MAP(ObjectSer);

ObjectSer::ObjectSer()
	: SeriableImpl<ObjectSer>("Object")
{

}

ObjectSer::~ObjectSer()
{

}


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

void ObjectSerializer::Save(const std::string& filename)
{
	m_isSaving = true;
	auto root = GetNewNode(GetNewString("scene"));
	m_document.append_node(root);

	for (unsigned int i = 0; i < m_objects.size(); ++i)
		m_objects[i]->Serialize(this);

	std::fstream f(filename, std::ios_base::out);
	f << *m_document.first_node() << std::endl;
	f.close();
	m_document.clear();
}

void ObjectSerializer::Load(const std::string& filename)
{
	m_isSaving = false;
	char* xmlContent;
	ReadXmlFile(filename, &xmlContent);

	m_document.parse<rapidxml::parse_default>(xmlContent);

	while (!HasReachedEof()) //HAS reached eof is not working properly
	{
		Object* obj = new Object();
		if (obj->Serialize(this))
			m_objects.push_back(obj);
	}

	delete xmlContent;
	m_document.clear();
}

void ObjectSerializer::AddObject(Object* obj)
{
	m_objects.push_back(obj);
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

std::vector<Object*> ObjectFactory::m_objects;

//////////////////////////////////////////////////////////////////////
//Object
//////////////////////////////////////////////////////////////////////

BEGIN_PROPERTY_MAP(Object)
	IMPLEMENT_PROPERTY(Mesh*, ObjectMesh, "mesh", Object),
	IMPLEMENT_PROPERTY(Material*, ObjectMaterial, "material", Object),
	IMPLEMENT_PROPERTY(bool, isShadowCaster, "castShadows", Object),
	IMPLEMENT_PROPERTY(glm::vec3, worldPosition, "position", Object),
	IMPLEMENT_PROPERTY(glm::vec3, scale, "scale", Object),
	IMPLEMENT_PROPERTY(std::string, debugName, "name", Object)
END_PROPERTY_MAP(Object)

Object::Object()
	: SeriableImpl<Object>("object")
	, m_needComputeModelMtx(true)
    , m_isShadowCaster(false)
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

ObjectRenderer::ObjectRenderer(VkRenderPass renderPass, const std::vector<Object*>& objects)
    : CRenderer(renderPass, "SolidRenderPass")
    , m_numOfObjects((unsigned int)objects.size())
    , m_objectDescLayout(VK_NULL_HANDLE)
    , m_instanceDataBuffer(nullptr)
    , m_sampler(VK_NULL_HANDLE)
{

	BatchBuilder::CreateInstance();

    for (unsigned int i = 0; i < (unsigned int)objects.size(); ++i)
    {

		/*if (objects[i]->GetType() == ObjectType::Solid)
		{
			m_solidBatch.AddObject(objects[i]);
			continue;
		}

        unsigned int batchIndex = (unsigned int)objects[i]->GetType();
        std::vector<Node>& nodes = m_batches[batchIndex].nodes;
        nodes.push_back(Node(objects[i]));*/
		
    }

    m_batches[(unsigned int)ObjectType::Solid].debugMarker = "Solid";
    m_batches[(unsigned int)ObjectType::NormalMap].debugMarker = "NormalMapping";
}

ObjectRenderer::~ObjectRenderer()
{
    for (unsigned int i = 0; i < (unsigned int)ObjectType::Count; ++i)
    {
        m_batches[i].nodes.clear(); //no need i think. doesnt free the m
    }

    VkDevice dev = vk::g_vulkanContext.m_device;
	MemoryManager::GetInstance()->FreeHandle(m_instanceDataBuffer);

	BatchBuilder::DestroyInstance();
}

void ObjectRenderer::Render()
{
	if (m_solidBatch.NeedCleanup())
	{
		m_solidBatch.Cleanup();
		MemoryManager::GetInstance()->FreeMemory(EMemoryContextType::BatchStaggingBuffer);
	}

	if (m_solidBatch.NeedReconstruct())
	{
		MemoryManager::GetInstance()->AllocMemory(EMemoryContextType::BatchStaggingBuffer, m_solidBatch.GetTotalBatchMemory());

		m_solidBatch.Construct(this, m_renderPass, 0);
	}

    VkCommandBuffer cmd = vk::g_vulkanContext.m_mainCommandBuffer;
    StartRenderPass();

	for (unsigned int i = (unsigned int)ObjectType::NormalMap; i < (unsigned int)ObjectType::Count; ++i)
	{
		const SBatch& batch = m_batches[i];

		StartDebugMarker(batch.debugMarker);
		const CGraphicPipeline& currPipeline = batch.pipeline;
		vk::CmdBindPipeline(cmd, currPipeline.GetBindPoint(), currPipeline.Get());
		for (const auto& node : batch.nodes)
		{
			vk::CmdBindDescriptorSets(cmd, currPipeline.GetBindPoint(), currPipeline.GetLayout(), 0, 1, &node.descriptorSet, 0, nullptr); //try to bind just once
			node.obj->Render();
		}

		EndDebugMarker(batch.debugMarker);
	}
	
	m_solidBatch.Render();

    EndRenderPass();
}

void ObjectRenderer::PreRender()
{
	m_solidBatch.PreRender();
	UpdateShaderParams();
}

void ObjectRenderer::Init()
{
    CRenderer::Init();

    CreateNearestSampler(m_sampler);
    PerspectiveMatrix(m_projMatrix);
    ConvertToProjMatrix(m_projMatrix);

    InitDescriptorNodes();
    InitMemoryOffsetNodes();

    //setup pipelines for batches
    SetupPipeline("vert.spv", "frag.spv", m_batches[(unsigned int)ObjectType::Solid].pipeline);
    SetupPipeline("normal.vert", "normal.frag", m_batches[(unsigned int)ObjectType::NormalMap].pipeline);
}

void ObjectRenderer::CreateDescriptorSetLayout()
{
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    bindings.push_back(CreateDescriptorBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS));
    bindings.push_back(CreateDescriptorBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT));
    bindings.push_back(CreateDescriptorBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT));

    NewDescriptorSetLayout(bindings, &m_objectDescLayout);
}

void ObjectRenderer::PopulatePoolInfo(std::vector<VkDescriptorPoolSize>& poolSize, unsigned int& maxSets)
{
    maxSets = m_numOfObjects;
    AddDescriptorType(poolSize, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, m_numOfObjects);
    AddDescriptorType(poolSize, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, m_numOfObjects); //albedo
    AddDescriptorType(poolSize, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, m_numOfObjects); //normal map
}

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
    //this section is to declare all the variables used for creating the update info in no particular order
    std::vector<VkWriteDescriptorSet> wDesc;
    //these have to be cached otherwise vk::UpdateDescriptorSets will crash, pointers will become invalid
    std::vector<VkDescriptorBufferInfo> buffInfos;
    buffInfos.reserve(m_numOfObjects);

    for (auto& batch : m_batches)
    {
        for (auto& node : batch.nodes)
        {
   //         buffInfos.push_back(node.buffer->GetDescriptor());
			//CTexture* albedo = node.obj->GetAlbedoTexture();
   //         wDesc.push_back(InitUpdateDescriptor(node.descriptorSet, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &buffInfos.back()));
   //         TRAP(albedo);
   //         wDesc.push_back(InitUpdateDescriptor(node.descriptorSet, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &albedo->GetTextureDescriptor()));
   //         //CTexture* normalmap = node.obj->GetNormalMap();
   //         if (normalmap)
   //         {
   //             wDesc.push_back(InitUpdateDescriptor(node.descriptorSet, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &normalmap->GetTextureDescriptor()));
   //         }
        }
    }

    vk::UpdateDescriptorSets(vk::g_vulkanContext.m_device, (uint32_t)wDesc.size(), wDesc.data(), 0, nullptr);
}

void ObjectRenderer::InitDescriptorNodes()
{
    std::vector<VkDescriptorSetLayout> layouts (m_numOfObjects, m_objectDescLayout);
    std::vector<VkDescriptorSet> sets(m_numOfObjects);

    AllocDescriptorSets(m_descriptorPool, layouts, sets);
    //here we give a descriptor set to every graphic node in batches
    unsigned int setIndex = 0;
    for (auto& batch : m_batches)
    {
        std::vector<Node>& nodes = batch.nodes;
        for(unsigned int i = 0; i < nodes.size(); ++i, ++setIndex)
        {
            nodes[i].descriptorSet = sets[setIndex];
        }
    }
}

void ObjectRenderer::InitMemoryOffsetNodes()
{
	m_instanceDataBuffer = MemoryManager::GetInstance()->CreateBuffer(EMemoryContextType::UniformBuffers, std::vector<VkDeviceSize>(m_numOfObjects, sizeof(ObjectShaderParams)), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

    VkDeviceSize currentOffset = 0;
    for (auto& batch : m_batches)
    {
        std::vector<Node>& nodes = batch.nodes;
		for (unsigned int i = 0; i < nodes.size(); ++i)
            nodes[i].buffer = m_instanceDataBuffer->CreateSubbuffer(sizeof(ObjectShaderParams));
    }
}

void ObjectRenderer::SetupPipeline(const std::string& vertex, const std::string& fragment, CGraphicPipeline& pipeline)
{
    pipeline.SetVertexInputState(Mesh::GetVertexDesc());
    pipeline.AddBlendState(CGraphicPipeline::CreateDefaultBlendState(), GBuffer_InputCnt);
    pipeline.SetVertexShaderFile(vertex);
    pipeline.SetFragmentShaderFile(fragment);
    pipeline.SetCullMode(VK_CULL_MODE_BACK_BIT);
    pipeline.CreatePipelineLayout(m_objectDescLayout);
    pipeline.Init(this, m_renderPass, 0); //0 is a "magic" number. This means that renderpass has just a subpass and the pipelines are used in that subpass
}

void ObjectRenderer::UpdateShaderParams()
{
    void* memPtr = nullptr;
    glm::mat4 projView = m_projMatrix * ms_camera.GetViewMatrix();

    for (auto& batch : m_batches)
    {
        for (auto& node : batch.nodes)
        {
            Object* obj = node.obj;
            glm::mat4 model = obj->GetModelMatrix();

			ObjectShaderParams* params = node.buffer->GetPtr<ObjectShaderParams*>();
            params->Mvp = projView * model;
            params->WorldMatrix = model;
            //params->MaterialProp = obj->GetMaterialProperties();
            params->ViewPos = glm::vec4(ms_camera.GetFrontVector(), 0.0f);
        }
    }

}

//////////////////////////////////////////////////////////////////////////
//CScene
//////////////////////////////////////////////////////////////////////////
std::unordered_set<Object*> CScene::ms_sceneObjects; //i got the objects in 2 places: here and in ObjectFactory. BUt fuck it. not important
BoundingBox CScene::ms_sceneBoundingBox;

void CScene::AddObject(Object* obj)
{
    auto result = ms_sceneObjects.insert(obj);
    TRAP(result.second == true);

    CScene::UpdateBoundingBox();
}

void CScene::UpdateBoundingBox()
{
    ms_sceneBoundingBox.Max = ms_sceneBoundingBox.Min = glm::vec3();
    if (ms_sceneObjects.empty())
        return;

    glm::vec3 maxLimits (std::numeric_limits<float>::min());
    glm::vec3 minLimits (std::numeric_limits<float>::max());

    for(auto o = ms_sceneObjects.begin(); o != ms_sceneObjects.end(); ++o)
    {
        BoundingBox bb = (*o)->GetBoundingBox();
        std::vector<glm::vec3> bbPoints;
        bb.Transform((*o)->GetModelMatrix(), bbPoints);
        for(unsigned int i = 0; i < bbPoints.size(); ++i)
        {
            maxLimits = glm::max(maxLimits, bbPoints[i]);
            minLimits = glm::min(minLimits, bbPoints[i]);
        }
    }

    ms_sceneBoundingBox.Max = maxLimits;
    ms_sceneBoundingBox.Min = minLimits;
}