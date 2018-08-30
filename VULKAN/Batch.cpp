#include "Batch.h"

#include "Mesh.h"
#include "MemoryManager.h"

Batch::Batch()
	: m_batchBuffer(nullptr)
	, m_staggingBuffer(nullptr)
	, m_indirectCommandBuffer(nullptr)
	, m_totalBatchMemory(0)
	, m_needReconstruct(true)
{
}

Batch::~Batch()
{

}

void Batch::AddMesh(Mesh* mesh)
{
	m_meshes.push_back(mesh);
	std::vector<VkDeviceSize> sizes(2);

	for (Mesh* mesh : m_meshes)
	{
		sizes[0] += mesh->GetVerticesMemorySize(); //first part of memory will be vertexes
		sizes[1] += mesh->GetIndicesMemorySize();  //second part will be indexes
	}

	m_totalBatchMemory = MemoryManager::ComputeTotalSize(sizes);
	m_needReconstruct = true;
}

void Batch::Construct()
{
	std::vector<VkDeviceSize> subBuffersSizes(2);

	subBuffersSizes[0] = subBuffersSizes[1] = 0;//unnecessary i think
	for (unsigned int i = 0; i < m_meshes.size(); ++i)
		subBuffersSizes[0] += (VkDeviceSize)m_meshes[i]->GetVerticesMemorySize(); //in first part of the memory we keep the vertices

	for (unsigned int i = 0; i < m_meshes.size(); ++i)
		subBuffersSizes[1] += (VkDeviceSize)m_meshes[i]->GetIndicesMemorySize(); //in the second part of the memory we keep the indices

	m_batchBuffer = MemoryManager::GetInstance()->CreateBuffer(EMemoryContextType::DeviceLocalBuffer, subBuffersSizes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	m_staggingBuffer = MemoryManager::GetInstance()->CreateBuffer(EMemoryContextType::BatchStaggingBuffer, subBuffersSizes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
	m_indirectCommandBuffer = MemoryManager::GetInstance()->CreateBuffer(EMemoryContextType::IndirectDrawCmdBuffer, m_meshes.size() * sizeof(VkDrawIndexedIndirectCommand), VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
	
	MemoryManager::GetInstance()->MapMemoryContext(EMemoryContextType::IndirectDrawCmdBuffer);
	MemoryManager::GetInstance()->MapMemoryContext(EMemoryContextType::BatchStaggingBuffer);

	BufferHandle* batchVertexBuffer = m_batchBuffer->CreateSubbuffer(subBuffersSizes[0]);
	BufferHandle* batchIndexBuffer = m_batchBuffer->CreateSubbuffer(subBuffersSizes[1]);
	BufferHandle* staggingVertexBuffer = m_staggingBuffer->CreateSubbuffer(subBuffersSizes[0]);
	BufferHandle* staggingIndexBuffer = m_staggingBuffer->CreateSubbuffer(subBuffersSizes[1]);

	SVertex* vertexMemory = staggingVertexBuffer->GetPtr<SVertex*>();
	uint32_t* indexMemory = staggingIndexBuffer->GetPtr<uint32_t*>();
	VkDrawIndexedIndirectCommand* indirectCommand = m_indirectCommandBuffer->GetPtr<VkDrawIndexedIndirectCommand*>();

	uint32_t vertexOffset = 0;
	uint32_t indexOffset = 0;

	for (Mesh* mesh : m_meshes)
	{
		mesh->CopyLocalData(vertexMemory, indexMemory);
		indirectCommand->firstIndex = indexOffset;
		indirectCommand->vertexOffset = vertexOffset;
		indirectCommand->indexCount = mesh->GetIndexCount();
		indirectCommand->instanceCount = 1;
		indirectCommand->firstInstance = 0;

		++indirectCommand;
		vertexOffset += mesh->GetVertexCount();
		indexOffset += mesh->GetIndexCount();
		vertexMemory += mesh->GetVertexCount();
		indexMemory += mesh->GetIndexCount();
	}

	MemoryManager::GetInstance()->UnmapMemoryContext(EMemoryContextType::IndirectDrawCmdBuffer);
	MemoryManager::GetInstance()->UnmapMemoryContext(EMemoryContextType::BatchStaggingBuffer);

	m_needReconstruct = false;
}

void Batch::Destruct()
{
	//TODO
}

void Batch::Render()
{

}