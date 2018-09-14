#include "Batch.h"

#include "Mesh.h"
#include "MemoryManager.h"
#include "Object.h"
#include "Renderer.h"
#include "Texture.h"
#include "Material.h"

BatchBuilder::BatchBuilder()
	: m_currentPoolIndex(-1)
{
	CreateDescriptorLayout();
}

BatchBuilder::~BatchBuilder()
{

}

//we need a list of parameters here (we have to know the pipeline, how much uniform memory per batch, or do we use a fixed size. I dont know it seems not too optim)
Batch* BatchBuilder::CreateNewBatch()
{
	return nullptr;
}

VkDescriptorSet BatchBuilder::AllocNewDescriptorSet()
{
	if (m_currentPoolIndex == -1 || !m_descriptorPools[m_currentPoolIndex].CanAllocate(m_batchSpecificDescLayout))
	{
		m_descriptorPools.push_back(DescriptorPool());
		DescriptorPool& pool = m_descriptorPools[++m_currentPoolIndex];
		pool.Construct(m_batchSpecificDescLayout, 10); //magic number again
	}

	return m_descriptorPools[m_currentPoolIndex].AllocateDescriptorSet(m_batchSpecificDescLayout);
}

void BatchBuilder::CreateDescriptorLayout()
{
	m_batchSpecificDescLayout.AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
	m_batchSpecificDescLayout.AddBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 12); //magic number for now
	m_batchSpecificDescLayout.Construct();
}

////////////////////////////////////////////////////////////////////
//Batch
////////////////////////////////////////////////////////////////////
Batch::Batch()
	: m_batchBuffer(nullptr)
	, m_staggingBuffer(nullptr)
	, m_indirectCommandBuffer(nullptr)
	, m_materialBuffer(nullptr)
	, m_batchVertexBuffer(nullptr)
	, m_batchIndexBuffer(nullptr)
	, m_totalBatchMemory(0)
	, m_needReconstruct(true)
	, m_needCleanup(false)
	, m_isReady(false)
{
}

Batch::~Batch()
{

}

void Batch::AddObject(Object* obj)
{
	m_objects.push_back(obj);
	std::vector<VkDeviceSize> sizes(2);

	for (Object* obj : m_objects)
	{
		Mesh* mesh = obj->GetObjectMesh();
		sizes[0] += mesh->GetVerticesMemorySize(); //first part of memory will be vertexes
		sizes[1] += mesh->GetIndicesMemorySize();  //second part will be indexes
	}

	m_totalBatchMemory = MemoryManager::ComputeTotalSize(sizes);
	m_needReconstruct = true;
}

void Batch::Construct(CRenderer* renderer, VkRenderPass renderPass, uint32_t subpassIndex)
{
	ConstructMeshes();
	ConstructPipeline(renderer, renderPass, subpassIndex);
	ConstructBatchSpecifics();
	IndexTextures();
	UpdateGraphicsInterface();
	m_needReconstruct = false;
}

void Batch::ConstructMeshes()
{
	std::vector<VkDeviceSize> subBuffersSizes(2);

	subBuffersSizes[0] = subBuffersSizes[1] = 0;//unnecessary i think
	for (Object* obj : m_objects)
	{
		Mesh* mesh = obj->GetObjectMesh();
		subBuffersSizes[0] += (VkDeviceSize)mesh->GetVerticesMemorySize(); //in first part of the memory we keep the vertices
		subBuffersSizes[1] += (VkDeviceSize)mesh->GetIndicesMemorySize(); //in the second part of the memory we keep the indices
	}

	m_batchBuffer = MemoryManager::GetInstance()->CreateBuffer(EMemoryContextType::DeviceLocalBuffer, subBuffersSizes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	m_staggingBuffer = MemoryManager::GetInstance()->CreateBuffer(EMemoryContextType::BatchStaggingBuffer, subBuffersSizes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
	m_indirectCommandBuffer = MemoryManager::GetInstance()->CreateBuffer(EMemoryContextType::IndirectDrawCmdBuffer, m_objects.size() * sizeof(VkDrawIndexedIndirectCommand), VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);

	MemoryManager::GetInstance()->MapMemoryContext(EMemoryContextType::IndirectDrawCmdBuffer);
	MemoryManager::GetInstance()->MapMemoryContext(EMemoryContextType::BatchStaggingBuffer);

	m_batchVertexBuffer = m_batchBuffer->CreateSubbuffer(subBuffersSizes[0]);
	m_batchIndexBuffer = m_batchBuffer->CreateSubbuffer(subBuffersSizes[1]);
	BufferHandle* staggingVertexBuffer = m_staggingBuffer->CreateSubbuffer(subBuffersSizes[0]);
	BufferHandle* staggingIndexBuffer = m_staggingBuffer->CreateSubbuffer(subBuffersSizes[1]);

	SVertex* vertexMemory = staggingVertexBuffer->GetPtr<SVertex*>();
	uint32_t* indexMemory = staggingIndexBuffer->GetPtr<uint32_t*>();
	VkDrawIndexedIndirectCommand* indirectCommand = m_indirectCommandBuffer->GetPtr<VkDrawIndexedIndirectCommand*>();

	uint32_t vertexOffset = 0;
	uint32_t indexOffset = 0;

	for (Object* obj : m_objects)
	{
		Mesh* mesh = obj->GetObjectMesh();

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
	VkCommandBuffer cmdBuffer = vk::g_vulkanContext.m_mainCommandBuffer;
	
	VkBufferMemoryBarrier copyBarrier;
	copyBarrier = m_batchVertexBuffer->CreateMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_INDEX_READ_BIT);

	VkBufferCopy copyRegion;
	cleanStructure(copyRegion);
	copyRegion.srcOffset = m_staggingBuffer->GetOffset();
	copyRegion.dstOffset = m_batchBuffer->GetOffset();
	copyRegion.size = m_staggingBuffer->GetSize();

	vk::CmdCopyBuffer(cmdBuffer, m_staggingBuffer->Get(), m_batchBuffer->Get(), 1, &copyRegion);
	vk::CmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 0, nullptr, 1, &copyBarrier, 0, nullptr);

	m_needCleanup = true;
}

void Batch::ConstructPipeline(CRenderer* renderer, VkRenderPass renderPass, uint32_t subpassIndex)
{
	m_materialTemplate = new MaterialTemplate<StandardMaterial>("batch.vert", "batch.frag");

	VkPushConstantRange pushConstRange;
	pushConstRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	pushConstRange.offset = 0;
	pushConstRange.size = sizeof(BatchParams);

	m_pipeline.SetVertexInputState(Mesh::GetVertexDesc());
	m_pipeline.AddBlendState(CGraphicPipeline::CreateDefaultBlendState(), GBuffer_InputCnt);
	m_pipeline.SetVertexShaderFile(m_materialTemplate->GetVertexShader());
	m_pipeline.SetFragmentShaderFile(m_materialTemplate->GetFragmentShader());
	m_pipeline.SetCullMode(VK_CULL_MODE_BACK_BIT);
	m_pipeline.AddPushConstant(pushConstRange);
	m_pipeline.CreatePipelineLayout(BatchBuilder::GetInstance()->GetDescriptorSetLayout());
	m_pipeline.Init(renderer, renderPass, 0); //0 is a "magic" number. This means that renderpass has just a subpass and the m_pipelines are used in that subpass
}

void Batch::ConstructBatchSpecifics()
{
	//here we need to get material specifics and allocate a suitable amount of memory. But for now we just hardcode the late functionality
	VkDeviceSize totalSize = m_objects.size() * (sizeof(glm::mat4) + m_materialTemplate->GetDataStride());
	m_materialBuffer = MemoryManager::GetInstance()->CreateBuffer(EMemoryContextType::UniformBuffers, totalSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	m_batchSpecificDescSet = BatchBuilder::GetInstance()->AllocNewDescriptorSet();
}

void Batch::UpdateGraphicsInterface()
{
	VkDescriptorBufferInfo buffInfo = m_materialBuffer->GetDescriptor();
	std::vector<VkWriteDescriptorSet> wDesc;
	wDesc.push_back(InitUpdateDescriptor(m_batchSpecificDescSet, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &buffInfo));
	std::vector<VkDescriptorImageInfo> imageInfo;

	for (const auto& text : m_batchTextures)
	{
		imageInfo.push_back(text->GetTextureDescriptor());
	}

	wDesc.push_back(InitUpdateDescriptor(m_batchSpecificDescSet, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, imageInfo));

	vk::UpdateDescriptorSets(vk::g_vulkanContext.m_device, (uint32_t)wDesc.size(), wDesc.data(), 0, nullptr);
}

void Batch::IndexTextures()
{
	m_materials.resize(m_objects.size());
	for (unsigned int i = 0; i < m_objects.size(); ++i)
	{
		Object* obj = m_objects[i];
		m_materials[i] = m_materialTemplate->Create();
		StandardMaterial* mat = (StandardMaterial*)m_materials[i];
		//mat->SetAlbedoTexture(obj->GetAlbedoTexture());
		//mat->SetSpecularProperties(obj->GetMaterialProperties());
	}

	//idk man. this is some fucked up shit
	for (Material* mat : m_materials)
	{
		std::vector<IndexedTexture> slots = mat->GetTextureSlots();
		std::vector<IndexedTexture> newSlots = slots;
		for (unsigned int i = 0; i < slots.size(); ++i)
		{
			auto it = std::find_if(m_batchTextures.begin(), m_batchTextures.end(), [&](const CTexture* elem)
			{
				return elem == slots[i].texture;
			});

			if (it != m_batchTextures.end())
			{
				newSlots[i].index = it - m_batchTextures.begin();
			}
			else
			{
				newSlots[i].index = m_batchTextures.size();
				m_batchTextures.push_back(slots[i].texture);
			}
		}

		mat->SetTextureSlots(newSlots);
	}
}

void Batch::Cleanup()
{
	MemoryManager::GetInstance()->FreeHandle( m_staggingBuffer);

	m_staggingBuffer = nullptr;

	m_needCleanup = false;
	m_isReady = true;
}

void Batch::Destruct()
{
	//TODO
	m_isReady = false;
	delete m_materialTemplate;
}

void Batch::PreRender()
{
	if (!m_isReady)
		return;

	uint8_t* mem = m_materialBuffer->GetPtr<uint8_t*>();
	uint32_t offset = m_materialTemplate->GetDataStride() + sizeof(glm::mat4);
	for (unsigned int i = 0; i < m_objects.size(); ++i, mem += offset)
	{
		Object* obj = m_objects[i];
		//this one, too, is GOOOD SHIT
		*((glm::mat4*)mem) = obj->GetModelMatrix();
		memcpy(mem + sizeof(glm::mat4), m_materials[i]->GetData(), m_materials[i]->GetDataStride());
	}

	glm::mat4 projMatrix;
	PerspectiveMatrix(projMatrix);
	ConvertToProjMatrix(projMatrix);

	m_batchParams.ProjViewMatrix = projMatrix * ms_camera.GetViewMatrix();
	m_batchParams.ViewPos = glm::vec4(ms_camera.GetPos(), 1.0f);
}
void Batch::Render()
{
	if (!m_isReady)
		return;

	VkCommandBuffer cmdBuffer = vk::g_vulkanContext.m_mainCommandBuffer;

	vk::CmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline.Get());
	vk::CmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline.GetLayout(), 0, 1, &m_batchSpecificDescSet, 0, nullptr);
	vk::CmdPushConstants(cmdBuffer, m_pipeline.GetLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(BatchParams), &m_batchParams);
	
	VkDeviceSize offset = 0;
	vk::CmdBindVertexBuffers(cmdBuffer, 0, 1, &m_batchVertexBuffer->Get(), &offset);
	vk::CmdBindIndexBuffer(cmdBuffer, m_batchIndexBuffer->Get(), m_batchIndexBuffer->GetOffset(), VK_INDEX_TYPE_UINT32);

	vk::CmdDrawIndexedIndirect(cmdBuffer, m_indirectCommandBuffer->Get(), 0, (uint32_t)m_objects.size(), sizeof(VkDrawIndexedIndirectCommand));
}