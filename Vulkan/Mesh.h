#pragma once

#include "VulkanLoader.h"
#include <vector>
#include "glm/glm.hpp"
#include "assimp/Importer.hpp"

struct SVertex
{
    glm::vec3 pos;
    glm::vec2 uv;
    glm::vec3 normal;
    glm::vec3 bitangent;
    glm::vec3 tangent;
    unsigned int color;

    SVertex(){}
    SVertex(glm::vec3 p) : pos(p), uv(){}
    SVertex(glm::vec3 p, glm::vec2 tuv) : pos(p), uv(tuv){}
    SVertex(glm::vec3 p, glm::vec2 textuv, glm::vec3 n) : pos(p), uv(textuv), normal(n) {}

    void SetColor(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
    {
        color = 0;
        color |= a << 24;
        color |= b << 16;
        color |= g << 8;
        color |= r;
    }
};
struct aiScene;
struct aiMesh;
class MeshLoader 
{
public:
    typedef std::vector<SVertex> VertexContainer;
    typedef std::vector<unsigned int> IndexContainer;

    MeshLoader();
    virtual ~MeshLoader();

    void LoadInto(const std::string filename, VertexContainer* outVertexes, IndexContainer* outIndexes);

private:
    void CountMeshElements();
    void ConvertScene();

    void ConvertMesh(aiMesh* currMesh, unsigned int vertexOffset);
private:
    VertexContainer*    m_pVertexes;
    IndexContainer*     m_pIndexes;    

    unsigned int        m_vertexCount;
    unsigned int        m_indexCount;

    Assimp::Importer    m_importer;
    const aiScene*      m_scene;
};

struct BoundingBox
{
    glm::vec3 Min;
    glm::vec3 Max;

    void Construct(std::vector<glm::vec3>& points)
    {
        points.resize(8);
        points[0] = glm::vec3(Min.x, Max.y, Min.z); //L T F
        points[1] = glm::vec3(Max.x, Max.y, Min.z); // R T F
        points[2] = glm::vec3(Max.x, Min.y, Min.z); // R B F
        points[3] = glm::vec3(Min.x, Min.y, Min.z); // L B F
        points[4] = glm::vec3(Min.x, Max.y, Max.z); // L T B
        points[5] = glm::vec3(Max.x, Max.y, Max.z); // R T B
        points[6] = glm::vec3(Max.x, Min.y, Max.z); // R B B
        points[7] = glm::vec3(Min.x, Min.y, Max.z); //L B B
    }

    void Transform(const glm::mat4& tMatrix, std::vector<glm::vec3>& tPoints)
    {
        Construct(tPoints);
        for(unsigned int i = 0; i < tPoints.size(); ++i)
            tPoints[i] = glm::vec3(tMatrix * glm::vec4(tPoints[i], 1.0f));
    }
};

class Mesh
{
public:
    Mesh();
    Mesh(const std::vector<SVertex>& vertexes, const std::vector<unsigned int>& indexes);

    Mesh(const std::string filename);

    virtual ~Mesh();
    void Render(unsigned int numIndexes = -1, unsigned int instances = 1);

    static VkPipelineVertexInputStateCreateInfo& Mesh::GetVertexDesc();
    //for dynamic use of the mesh (UI)
    VkDeviceMemory  GetVertexMemory() const { return m_vertexMemory; }
    
    BoundingBox GetBB() const {return m_bbox; }
private:
    void Create();
    void CreateBoundigBox();
private:
    VkBuffer                    m_vertexBuffer;
    VkBuffer                    m_indexBuffer;

    VkDeviceMemory              m_vertexMemory;
    VkDeviceMemory              m_indexMemory;

    std::vector<SVertex>         m_vertexes;
    std::vector<unsigned int>   m_indices;

    BoundingBox                 m_bbox;
    unsigned int                m_nbOfIndexes;

    struct InputVertexDescription
    {
        VkPipelineVertexInputStateCreateInfo       vertexDescription;
        VkVertexInputBindingDescription             vibd;
        std::vector<VkVertexInputAttributeDescription>   viad;
    };

    static InputVertexDescription*            ms_vertexDescription;
};