#include "Mesh.h"
#include "defines.h"
#include "assimp/scene.h"           // Output data structure
#include "assimp/postprocess.h"     // Post processing flags

void AllocBufferMemory(VkBuffer& buffer, VkDeviceMemory& memory, uint32_t size, VkBufferUsageFlags usage);

MeshLoader::MeshLoader()
    : m_scene(nullptr)
    , m_pVertexes(nullptr)
    , m_pIndexes(nullptr)
    , m_vertexCount(0)
    , m_indexCount(0)
{

}

MeshLoader::~MeshLoader()
{

}

void MeshLoader::LoadInto(const std::string filename, VertexContainer* outVertexes, IndexContainer* outIndexes)
{
    m_pVertexes = outVertexes;
    m_pIndexes = outIndexes;

    m_scene = m_importer.ReadFile(filename, aiProcess_Triangulate  | aiProcess_GenSmoothNormals | aiProcess_CalcTangentSpace);
    TRAP(m_scene);
    TRAP(m_scene->HasMeshes());

    CountMeshElements();
    //alloc indexes
    m_pVertexes->reserve(m_vertexCount);
    m_pIndexes->reserve(m_indexCount);

    ConvertScene();
}

void MeshLoader::CountMeshElements()
{
    unsigned int nMeshes = m_scene->mNumMeshes;
    aiMesh** meshes = m_scene->mMeshes;
    for(unsigned int i = 0; i < nMeshes; ++i)
    {
        aiMesh* currMesh = meshes[i];
        TRAP(currMesh->mPrimitiveTypes == aiPrimitiveType_TRIANGLE);
        m_vertexCount += currMesh->mNumVertices;
        m_indexCount  += (currMesh->mNumFaces * 3);
    }
}

void MeshLoader::ConvertScene()
{
    unsigned int nMeshes = m_scene->mNumMeshes;
    aiMesh** meshes = m_scene->mMeshes;
    unsigned int vertexOffset = 0;
    for(unsigned int i = 0; i < nMeshes; ++i)
    {
        aiMesh* currMesh = meshes[i];
        ConvertMesh(currMesh, vertexOffset);
        vertexOffset += currMesh->mNumVertices;
    }
}

void MeshLoader::ConvertMesh(aiMesh* currMesh, unsigned int vertexOffset)
{
    aiVector3D* vPositions = currMesh->mVertices;
    glm::vec3 pos;
    for(unsigned int v = 0; v < currMesh->mNumVertices; ++v)
    {
        pos = glm::vec3(vPositions[v].x, vPositions[v].y, vPositions[v].z);
        m_pVertexes->push_back(SVertex(pos));
    }
    if(currMesh->HasNormals())
    {
        aiVector3D* vNormals = currMesh->mNormals;
        for(unsigned int n = 0; n < currMesh->mNumVertices; ++n)
            (*m_pVertexes)[n + vertexOffset].normal = glm::vec3(vNormals[n].x, vNormals[n].y, vNormals[n].z);
    }

    if(currMesh->HasTextureCoords(0))
    {
        aiVector3D** vTextCoords = currMesh->mTextureCoords;
        for(unsigned int u = 0; u < currMesh->mNumVertices; ++u)
            (*m_pVertexes)[u + vertexOffset].uv = glm::vec2(vTextCoords[0][u].x, vTextCoords[0][u].y);
    }

    if(currMesh->HasTangentsAndBitangents())
    {
        aiVector3D* vBitangent = currMesh->mBitangents;
        aiVector3D* vTangent = currMesh->mTangents;

        for(unsigned int bt = 0; bt < currMesh->mNumVertices; ++bt)
        {
            (*m_pVertexes)[bt + vertexOffset].bitangent = glm::vec3(vBitangent[bt].x, vBitangent[bt].y, vBitangent[bt].z);
            (*m_pVertexes)[bt + vertexOffset].tangent = glm::vec3(vTangent[bt].x, vTangent[bt].y, vTangent[bt].z);
        }
    }

    aiFace* faces = currMesh->mFaces;
    for(unsigned int f = 0; f < currMesh->mNumFaces; ++f)
    {
        aiFace face = faces[f];
        for(unsigned int v = 0; v < 3; ++v)
            m_pIndexes->push_back(face.mIndices[v] + vertexOffset);
    }
}

Mesh::InputVertexDescription* Mesh::ms_vertexDescription = nullptr;

Mesh::Mesh()
    : m_vertexBuffer(VK_NULL_HANDLE)
    , m_vertexMemory(VK_NULL_HANDLE)
    , m_indexMemory(VK_NULL_HANDLE)
    , m_indexBuffer(VK_NULL_HANDLE)
{
}

Mesh::Mesh(const std::vector<SVertex>& vertexes, const std::vector<unsigned int>& indices)
    : m_vertexBuffer(VK_NULL_HANDLE)
    , m_vertexMemory(VK_NULL_HANDLE)
    , m_indexMemory(VK_NULL_HANDLE)
    , m_indexBuffer(VK_NULL_HANDLE)
    , m_vertexes(vertexes)
    , m_indices(indices)
{
    Create();
}

Mesh::Mesh(const std::string filename)
    : m_vertexBuffer(VK_NULL_HANDLE)
    , m_vertexMemory(VK_NULL_HANDLE)
    , m_indexMemory(VK_NULL_HANDLE)
    , m_indexBuffer(VK_NULL_HANDLE)
{
    MeshLoader loader;
    loader.LoadInto(filename, &m_vertexes, &m_indices);
    Create();
}

void Mesh::Create()
{
    m_nbOfIndexes = (unsigned int)m_indices.size();

    unsigned int size = (uint32_t)m_vertexes.size() * sizeof(SVertex);
    AllocBufferMemory(m_vertexBuffer, m_vertexMemory, size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    TRAP(size);

    void* memPtr = nullptr;
    VULKAN_ASSERT(vk::MapMemory(vk::g_vulkanContext.m_device, m_vertexMemory, 0, size, 0, &memPtr));
    memcpy(memPtr, m_vertexes.data(), size);
    vk::UnmapMemory(vk::g_vulkanContext.m_device, m_vertexMemory);

    size = (uint32_t)m_indices.size() * sizeof(unsigned int);
    TRAP(size);

    AllocBufferMemory(m_indexBuffer, m_indexMemory, size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

    VULKAN_ASSERT(vk::MapMemory(vk::g_vulkanContext.m_device, m_indexMemory, 0, size, 0, &memPtr));
    memcpy(memPtr, m_indices.data(), size);
    vk::UnmapMemory(vk::g_vulkanContext.m_device, m_indexMemory);
    
    CreateBoundigBox();
}

void Mesh::CreateBoundigBox()
{
    m_bbox.Min = glm::vec3(99.0f);
    m_bbox.Max = glm::vec3(-99.0f);

    for(unsigned int i = 0; i < m_vertexes.size(); ++i)
    {
        m_bbox.Min = glm::vec3(glm::min(m_bbox.Min.x, m_vertexes[i].pos.x), glm::min(m_bbox.Min.y, m_vertexes[i].pos.y), glm::min(m_bbox.Min.z, m_vertexes[i].pos.z));
        m_bbox.Max = glm::vec3(glm::max(m_bbox.Max.x, m_vertexes[i].pos.x), glm::max(m_bbox.Max.y, m_vertexes[i].pos.y), glm::max(m_bbox.Max.z, m_vertexes[i].pos.z));
    }
}

void Mesh::Render(unsigned int numIndexes, unsigned int instances)
{
    unsigned int indexesToRender = (numIndexes == -1)? m_nbOfIndexes : numIndexes;

    VkCommandBuffer cmdBuff = vk::g_vulkanContext.m_mainCommandBuffer;
    const VkDeviceSize offsets[1] = {0};
    vk::CmdBindVertexBuffers(cmdBuff, 0, 1, &m_vertexBuffer, offsets);
    vk::CmdBindIndexBuffer(cmdBuff, m_indexBuffer, 0, VK_INDEX_TYPE_UINT32);
    vk::CmdDrawIndexed(cmdBuff, 
        indexesToRender,
        instances,
        0,
        0,
        0);
}

VkPipelineVertexInputStateCreateInfo& Mesh::GetVertexDesc()  
{ 
    if(!ms_vertexDescription)
    {
        ms_vertexDescription = new InputVertexDescription();
        VkVertexInputBindingDescription& vibd = ms_vertexDescription->vibd;
        cleanStructure(vibd);
        vibd.binding = 0;
        vibd.stride = sizeof(SVertex); 
        vibd.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        std::vector<VkVertexInputAttributeDescription>& viad = ms_vertexDescription->viad;

        viad.resize(6);
        cleanStructure(viad[0]);
        viad[0].location = 0;
        viad[0].binding = 0;
        viad[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        viad[0].offset = 0;

        cleanStructure(viad[1]);
        viad[1].location = 1;
        viad[1].binding = 0;
        viad[1].format = VK_FORMAT_R32G32_SFLOAT;
        viad[1].offset = sizeof(glm::vec3);

        cleanStructure(viad[2]);
        viad[2].location = 2;
        viad[2].binding = 0;
        viad[2].format = VK_FORMAT_R32G32B32_SFLOAT;
        viad[2].offset = sizeof(glm::vec3) + sizeof(glm::vec2);

        cleanStructure(viad[3]);
        viad[3].location = 3;
        viad[3].binding = 0;
        viad[3].format = VK_FORMAT_R32G32B32_SFLOAT;
        viad[3].offset = 2 * sizeof(glm::vec3) + sizeof(glm::vec2);

        cleanStructure(viad[4]);
        viad[4].location = 4;
        viad[4].binding = 0;
        viad[4].format = VK_FORMAT_R32G32B32_SFLOAT;
        viad[4].offset = 3 * sizeof(glm::vec3) + sizeof(glm::vec2);

        cleanStructure(viad[5]);
        viad[5].location = 5;
        viad[5].binding = 0;
        viad[5].format = VK_FORMAT_R8G8B8A8_UNORM;
        viad[5].offset = 4 * sizeof(glm::vec3) + sizeof(glm::vec2);

        VkPipelineVertexInputStateCreateInfo& vertexDescription = ms_vertexDescription->vertexDescription;
        cleanStructure(vertexDescription);
        vertexDescription.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexDescription.pNext = nullptr;
        vertexDescription.flags = 0;
        vertexDescription.vertexBindingDescriptionCount = 1;
        vertexDescription.pVertexBindingDescriptions = &vibd;
        vertexDescription.vertexAttributeDescriptionCount = (uint32_t)viad.size();
        vertexDescription.pVertexAttributeDescriptions = viad.data();

    }
    return ms_vertexDescription->vertexDescription; 
}

Mesh::~Mesh()
{
    VkDevice dev = vk::g_vulkanContext.m_device;

    vk::DestroyBuffer(dev, m_indexBuffer, nullptr);
    vk::FreeMemory(dev, m_indexMemory, nullptr);

    vk::DestroyBuffer(vk::g_vulkanContext.m_device, m_vertexBuffer, nullptr);
    vk::FreeMemory(vk::g_vulkanContext.m_device, m_vertexMemory, nullptr);
}