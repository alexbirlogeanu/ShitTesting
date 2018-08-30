#pragma once

#include "VulkanLoader.h"
#include <vector>
#include "glm/glm.hpp"
#include "SVertex.h"
#include "Singleton.h"

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
class Mesh;
class BufferHandle;
class MeshManager : public Singleton<MeshManager>
{
	friend class Singleton<MeshManager>;
public:
	void RegisterForUploading(Mesh* m);
	void Update();

private:
	MeshManager();
	virtual ~MeshManager();

	MeshManager(const MeshManager& other);
	MeshManager& operator=(const MeshManager& other);

	//
	unsigned int CalculateStagginMemorySize();
private:
	class TransferMeshInfo
	{
	public:
		TransferMeshInfo();
		TransferMeshInfo(Mesh* mesh);

		void BeginTransfer(VkCommandBuffer cmdBuffer);
		void EndTransfer();

		Mesh*			m_mesh;
		BufferHandle*	m_meshBuffer;
		BufferHandle*	m_staggingBuffer;
		BufferHandle*	m_stagginVertexBuffer;
		BufferHandle*	m_toVertexBuffer;
		BufferHandle*	m_staggingIndexBuffer;
		BufferHandle*	m_toIndexBuffer;
	};
private:
	std::vector<Mesh*>					m_pendingMeshes;
	std::vector<TransferMeshInfo>		m_transferInProgress;
};


class Mesh
{
	friend class MeshManager;
	friend class TransferMeshInfo;
public:
    Mesh();
    Mesh(const std::vector<SVertex>& vertexes, const std::vector<unsigned int>& indexes);

    Mesh(const std::string filename);

    virtual ~Mesh();
    void Render(unsigned int numIndexes = -1, unsigned int instances = 1);

    static VkPipelineVertexInputStateCreateInfo& Mesh::GetVertexDesc();
    //for dynamic use of the mesh (UI)
    //VkDeviceMemory  GetVertexMemory() const { return m_vertexMemory; }
    
    BoundingBox GetBB() const {return m_bbox; }

	unsigned int MemorySizeNeeded() const;
	unsigned int GetVerticesMemorySize() const;
	unsigned int GetIndicesMemorySize() const;
	uint32_t GetVertexCount() const { return (uint32_t)m_vertexes.size(); }
	uint32_t GetIndexCount() const { return (uint32_t)m_indices.size(); }

	void CopyLocalData(BufferHandle* stagginVertexBuffer, BufferHandle* staggingIndexBuffer);
	void CopyLocalData(void* vboMemory, void* iboMemory);

private:
    void Create();
    void CreateBoundigBox();
private:

    std::vector<SVertex>			m_vertexes;
    std::vector<unsigned int>		m_indices;

	BufferHandle*					m_meshBuffer;
	BufferHandle*					m_vertexSubBuffer;
	BufferHandle*					m_indexSubBuffer;

    BoundingBox						m_bbox;
    unsigned int					m_nbOfIndexes;

    struct InputVertexDescription
    {
        VkPipelineVertexInputStateCreateInfo       vertexDescription;
        VkVertexInputBindingDescription             vibd;
        std::vector<VkVertexInputAttributeDescription>   viad;
    };

    static InputVertexDescription*            ms_vertexDescription;
};