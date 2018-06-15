#include "Mesh.h"

#include <fstream>

#include "defines.h"
#include "MeshLoader.h"
#include "MemoryManager.h"

void AllocBufferMemory(VkBuffer& buffer, VkDeviceMemory& memory, uint32_t size, VkBufferUsageFlags usage);

////////////////////////////////////////////////////////////////////////////////////////
//TransferMeshInfo
////////////////////////////////////////////////////////////////////////////////////////

MeshManager::TransferMeshInfo::TransferMeshInfo(){}
MeshManager::TransferMeshInfo::TransferMeshInfo(Mesh* mesh)
	: m_stagginVertexBuffer(nullptr)
	, m_toVertexBuffer(nullptr)
	, m_staggingIndexBuffer(nullptr)
	, m_toIndexBuffer(nullptr)
	, m_meshBuffer(nullptr)
	, m_staggingBuffer(nullptr)
	, m_mesh(mesh)
{

	m_meshBuffer = MemoryManager::GetInstance()->CreateBuffer(EMemoryContextType::DeviceLocalBuffer, m_mesh->MemorySizeNeeded(), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
	m_staggingBuffer = MemoryManager::GetInstance()->CreateBuffer(EMemoryContextType::StaggingBuffer, m_mesh->MemorySizeNeeded(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

	m_toVertexBuffer = m_meshBuffer->CreateSubbuffer(m_mesh->GetVerticesMemorySize());
	m_toIndexBuffer = m_meshBuffer->CreateSubbuffer(m_mesh->GetIndicesMemorySize());

	m_stagginVertexBuffer = m_staggingBuffer->CreateSubbuffer(m_mesh->GetVerticesMemorySize());
	m_staggingIndexBuffer = m_staggingBuffer->CreateSubbuffer(m_mesh->GetIndicesMemorySize());
}

void MeshManager::TransferMeshInfo::BeginTransfer(VkCommandBuffer cmdBuffer)
{
	VkBufferCopy regions[2];
	//copy vertexes
	regions[0].size = m_stagginVertexBuffer->GetSize();
	regions[0].srcOffset = m_stagginVertexBuffer->GetOffset();
	regions[0].dstOffset = m_toVertexBuffer->GetOffset();

	//copy indices
	regions[1].size = m_staggingIndexBuffer->GetSize();
	regions[1].srcOffset = m_staggingIndexBuffer->GetOffset();
	regions[1].dstOffset = m_toIndexBuffer->GetOffset();

	vk::CmdCopyBuffer(cmdBuffer, m_staggingBuffer->Get(), m_meshBuffer->Get(), 2, regions);
}

void MeshManager::TransferMeshInfo::EndTransfer()
{
	m_mesh->m_meshBuffer = m_meshBuffer;
	m_mesh->m_indexSubBuffer = m_toIndexBuffer;
	m_mesh->m_vertexSubBuffer = m_toVertexBuffer;
}
////////////////////////////////////////////////////////////////////////////////////////
//MeshManager
////////////////////////////////////////////////////////////////////////////////////////


MeshManager::MeshManager()
{

}

MeshManager::~MeshManager()
{

}

void MeshManager::RegisterForUploading(Mesh* m)
{
	m_pendingMeshes.push_back(m);
}

void MeshManager::Update()
{
	if (!m_transferInProgress.empty()) //if I will use 2 different queue this is not a valid condition. I have to check if the transfers are finished
	{
		for (unsigned int i = 0; i < m_transferInProgress.size(); ++i)
			m_transferInProgress[i].EndTransfer();

		m_transferInProgress.clear();
		MemoryManager::GetInstance()->FreeMemory(EMemoryContextType::StaggingBuffer);
	}

	if (!m_pendingMeshes.empty())
	{
		unsigned int staggingMemoryTotalSize = CalculateStagginMemorySize();
		MemoryManager::GetInstance()->AllocMemory(EMemoryContextType::StaggingBuffer, staggingMemoryTotalSize);
		//
		
		TRAP(m_transferInProgress.empty() && "All transfer should be processed"); 
		m_transferInProgress.reserve(m_pendingMeshes.size());

		for (auto m : m_pendingMeshes)
		{
			m_transferInProgress.push_back(TransferMeshInfo(m));
		}

		MemoryManager::GetInstance()->MapMemoryContext(EMemoryContextType::StaggingBuffer);
		MappedMemory* memoryMap = MemoryManager::GetInstance()->GetMappedMemory(EMemoryContextType::StaggingBuffer);
		for (auto info : m_transferInProgress)
		{
			info.m_mesh->CopyLocalData(memoryMap, info.m_stagginVertexBuffer, info.m_staggingIndexBuffer);
		}

		MemoryManager::GetInstance()->UnmapMemoryContext(EMemoryContextType::StaggingBuffer);
		std::vector<VkBufferMemoryBarrier> copyBarriers;
		copyBarriers.reserve(m_transferInProgress.size());

		VkCommandBuffer cmdBuffer = vk::g_vulkanContext.m_mainCommandBuffer; //for now use this command buffer and the same queue. At home, try to create a transfer queue, with a different command buffer
		for (unsigned int i = 0; i < m_transferInProgress.size(); ++i)
		{
			TransferMeshInfo& transInfo = m_transferInProgress[i];
			copyBarriers.push_back(transInfo.m_toVertexBuffer->CreateMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT));
			copyBarriers.push_back(transInfo.m_toIndexBuffer->CreateMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_INDEX_READ_BIT));

			transInfo.BeginTransfer(cmdBuffer);
		}

		vk::CmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 0, nullptr, (uint32_t)copyBarriers.size(), copyBarriers.data(), 0, nullptr);
		m_pendingMeshes.clear();
	}
}

unsigned int MeshManager::CalculateStagginMemorySize()
{
	unsigned int totalSize = 0;
	for (auto m : m_pendingMeshes)
		totalSize += m->MemorySizeNeeded();

	return totalSize + m_pendingMeshes.size() * vk::g_vulkanContext.m_limits.minUniformBufferOffsetAlignment;
}

////////////////////////////////////////////////////////////////////////////////////////
//Mesh
////////////////////////////////////////////////////////////////////////////////////////
Mesh::InputVertexDescription* Mesh::ms_vertexDescription = nullptr;

Mesh::Mesh()
	: m_meshBuffer(nullptr)
	, m_vertexSubBuffer(nullptr)
	, m_indexSubBuffer(nullptr)
{
}

Mesh::Mesh(const std::vector<SVertex>& vertexes, const std::vector<unsigned int>& indices)
	: m_meshBuffer(nullptr)
	, m_vertexSubBuffer(nullptr)
	, m_indexSubBuffer(nullptr)
    , m_vertexes(vertexes)
    , m_indices(indices)
{
    Create();
}

Mesh::Mesh(const std::string filename) //use the binarized version
	: m_meshBuffer(nullptr)
	, m_vertexSubBuffer(nullptr)
	, m_indexSubBuffer(nullptr)
{
	std::fstream inFile(filename, std::ios_base::in | std::ios_base::binary);

	TRAP(inFile.is_open());
	unsigned int nbVertices;
	unsigned int nbIndexes;
	unsigned int bytesToRead;

	inFile >> nbVertices;
	m_vertexes.resize(nbVertices);
	bytesToRead = nbVertices * sizeof(SVertex);
	inFile.read((char*)m_vertexes.data(), bytesToRead);
	TRAP(inFile.gcount() == (std::streamsize)bytesToRead);

	inFile >> nbIndexes;
	m_indices.resize(nbIndexes);
	bytesToRead = nbIndexes * sizeof(unsigned int);
	inFile.read((char*)m_indices.data(), bytesToRead);
	TRAP(inFile.gcount() == (std::streamsize)bytesToRead);

	inFile.close();

	Create();
}

void Mesh::Create()
{
   m_nbOfIndexes = (unsigned int)m_indices.size();
	MeshManager::GetInstance()->RegisterForUploading(this);

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

unsigned int Mesh::MemorySizeNeeded() const
{
	return GetVerticesMemorySize() + GetIndicesMemorySize();
}

unsigned int Mesh::GetVerticesMemorySize() const
{
	return m_vertexes.size() * sizeof(SVertex);
}
unsigned int Mesh::GetIndicesMemorySize() const
{
	return m_indices.size() * sizeof(unsigned int);
}

void Mesh::CopyLocalData(MappedMemory* mapMemory, BufferHandle* stagginVertexBuffer, BufferHandle* staggingIndexBuffer)
{
	void* vertexMem = mapMemory->GetPtr<void*>(stagginVertexBuffer); //for debug purpose only
	void* indexMem = mapMemory->GetPtr<void*>(staggingIndexBuffer);
	memcpy(vertexMem, m_vertexes.data(), GetVerticesMemorySize());
	memcpy(indexMem, m_indices.data(), GetIndicesMemorySize());
}


void Mesh::Render(unsigned int numIndexes, unsigned int instances)
{
	if (!m_meshBuffer)
		return;

    unsigned int indexesToRender = (numIndexes == -1)? m_nbOfIndexes : numIndexes;

    VkCommandBuffer cmdBuff = vk::g_vulkanContext.m_mainCommandBuffer;
    const VkDeviceSize offsets[1] = {m_vertexSubBuffer->GetOffset()};
    vk::CmdBindVertexBuffers(cmdBuff, 0, 1, &m_vertexSubBuffer->Get(), offsets);
    vk::CmdBindIndexBuffer(cmdBuff, m_indexSubBuffer->Get(), m_indexSubBuffer->GetOffset(), VK_INDEX_TYPE_UINT32);
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
}