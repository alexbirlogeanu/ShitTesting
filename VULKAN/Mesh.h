#pragma once

#include "VulkanLoader.h"
#include <vector>
#include "glm/glm.hpp"
#include "SVertex.h"
#include "Singleton.h"
#include "Serializer.h"
#include "ResourceLoader.h"
#include "Geometry.h"

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


class Mesh : public SeriableImpl<Mesh>
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
    
    BoundingBox3D GetBB() const {return m_bbox; }

	unsigned int MemorySizeNeeded() const;
	unsigned int GetVerticesMemorySize() const;
	unsigned int GetIndicesMemorySize() const;
	uint32_t GetVertexCount() const { return (uint32_t)m_vertexes.size(); }
	uint32_t GetIndexCount() const { return (uint32_t)m_indices.size(); }

	void CopyLocalData(BufferHandle* stagginVertexBuffer, BufferHandle* staggingIndexBuffer);
	void CopyLocalData(void* vboMemory, void* iboMemory);

	void LoadFromFile(const std::string filename);
private:
    void Create();
    void CreateBoundigBox();
private:
	DECLARE_PROPERTY(std::string, Filename, Mesh);

    std::vector<SVertex>			m_vertexes;
    std::vector<unsigned int>		m_indices;

	BufferHandle*					m_meshBuffer;
	BufferHandle*					m_vertexSubBuffer;
	BufferHandle*					m_indexSubBuffer;

	BoundingBox3D					m_bbox;
    unsigned int					m_nbOfIndexes;

	bool							m_usedInBatching;

    struct InputVertexDescription
    {
        VkPipelineVertexInputStateCreateInfo       vertexDescription;
        VkVertexInputBindingDescription             vibd;
        std::vector<VkVertexInputAttributeDescription>   viad;
    };

    static InputVertexDescription*            ms_vertexDescription;
};

template<typename BASE>
class Property<Mesh*, BASE> : public PropertyGeneric
{
public:
	typedef Mesh* BASE::* PtmType;
	Property()
	{}
	Property(PtmType offset, const std::string& label)
		: m_ptm(offset)
		, m_label(label)
	{}

	virtual void Save(rapidxml::xml_node<char>* objNode, Serializer* serializer, ISeriable* obj)
	{
		BASE* cobj = dynamic_cast<BASE*>(obj);
		TRAP(cobj);

		(cobj->*m_ptm)->SetName(m_label);
		(cobj->*m_ptm)->Serialize(serializer);
	};

	virtual void Load(rapidxml::xml_node<char>* objNode, Serializer* serializer, ISeriable* obj)
	{
		auto prop = objNode->first_attribute(m_label.c_str(), 0, false);
		BASE* cobj = dynamic_cast<BASE*>(obj);
		TRAP(cobj);

		cobj->*m_ptm = new Mesh();

		(cobj->*m_ptm)->SetName(m_label);
		(cobj->*m_ptm)->Serialize(serializer);

		ResourceLoader::GetInstance()->LoadMesh(&(cobj->*m_ptm));
	};
private:
	PtmType						m_ptm;
	std::string					m_label;
};