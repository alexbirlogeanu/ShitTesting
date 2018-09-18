#include "Batch.h"

#include "Mesh.h"
#include "MemoryManager.h"
#include "Object.h"
#include "Renderer.h"
#include "Texture.h"
#include "Material.h"

BatchManager::BatchManager()
{
}

BatchManager::~BatchManager()
{

}

//we need a list of parameters here (we have to know the pipeline, how much uniform memory per batch, or do we use a fixed size. I dont know it seems not too optim)
Batch* BatchManager::CreateNewBatch(MaterialTemplateBase* materialTemplate)
{
	return new Batch(materialTemplate);
}

void BatchManager::Update()
{
	if (!m_inProgressBatches.empty())
	{
		for (auto batch : m_inProgressBatches)
		{
			batch->Cleanup();
		}

		m_inProgressBatches.clear();
		MemoryManager::GetInstance()->FreeMemory(EMemoryContextType::BatchStaggingBuffer);
	}

	VkDeviceSize memoryNeeded = 0;

	for (auto batch : m_batches)
	{
		if (batch->NeedReconstruct())
			memoryNeeded += batch->GetTotalBatchMemory();
	}

	if (memoryNeeded > 0)
	{
		MemoryManager::GetInstance()->AllocMemory(EMemoryContextType::BatchStaggingBuffer, memoryNeeded);

		for (auto batch : m_batches)
		{
			if (batch->NeedReconstruct())
			{
				batch->Construct();
				m_inProgressBatches.push_back(batch);
			}
		}
	}
}

void BatchManager::AddObject(Object* obj)
{
	MaterialTemplateBase* materialTemplate = obj->GetObjectMaterial()->GetTemplate();
	TRAP(materialTemplate);

	auto it = m_batchesCategories.find(materialTemplate);

	if (it != m_batchesCategories.end())
	{
		//get a suitable batch (for now we only have one)
		std::vector<Batch*> batches = it->second;
		for (uint32_t i = 0; i < batches.size(); ++i)
			if (true/*need a condition check like batch->CanAdd(obj)*/)
				batches[i]->AddObject(obj);
	}
	else
	{
		Batch* newBatch = CreateNewBatch(materialTemplate);
		m_batchesCategories.emplace(materialTemplate, std::vector<Batch*>(1, newBatch));
		newBatch->AddObject(obj);

		m_batches.push_back(newBatch); //keep all the batches in one place
	}
}

void BatchManager::RenderAll()
{
	for (auto category : m_batchesCategories)
	{
		const CGraphicPipeline& pipeline = category.first->GetPipeline();
		vk::CmdBindPipeline(vk::g_vulkanContext.m_mainCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.Get());

		for (Batch* batch : category.second)
		{
			batch->PrepareRendering(pipeline, false);
			batch->Render(false);
		}
	}
}

void BatchManager::RenderShadows()
{
	CGraphicPipeline* shadowPipeline = g_commonResources.GetAsPtr<CGraphicPipeline>(EResourceType_ShadowRenderPipeline);
	vk::CmdBindPipeline(vk::g_vulkanContext.m_mainCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowPipeline->Get());

	for (auto& batch : m_batches)
	{
		batch->PrepareRendering(*shadowPipeline, true);
		batch->Render(true);
	}

}

void BatchManager::PreRender()
{
	for (auto& batch : m_batches)
		batch->PreRender();
}

////////////////////////////////////////////////////////////////////
//Batch
////////////////////////////////////////////////////////////////////

struct BatchCommons
{
	glm::mat4 ModelMtx;
};

Batch::Batch(MaterialTemplateBase* materialTemplate)
	: m_batchBuffer(nullptr)
	, m_staggingBuffer(nullptr)
	, m_indirectCommandBuffer(nullptr)
	, m_batchStorageBuffer(nullptr)
	, m_batchCommonsBuffer(nullptr)
	, m_batchSpecificsBuffer(nullptr)
	, m_batchVertexBuffer(nullptr)
	, m_batchIndexBuffer(nullptr)
	, m_totalBatchMemory(0)
	, m_needReconstruct(true)
	, m_needCleanup(false)
	, m_isReady(false)
	, m_materialTemplate(materialTemplate)
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

void Batch::Construct()
{
	OrderOjects();
	ConstructMeshes();
	ConstructBatchSpecifics();
	IndexTextures();
	UpdateGraphicsInterface();
	m_needReconstruct = false;
}

void Batch::OrderOjects()
{
	//order objects in m_objects based on the criteria of cast shadows or not. Our vector will be split in 2 parts, with the non-caster at the end
	std::vector<Object*> nonCasters;
	auto pred = [](const Object* obj){
		return !obj->GetIsShadowCaster();
	};

	auto it = std::find_if(m_objects.begin(), m_objects.end(), pred);
	while (it != m_objects.end())
	{
		nonCasters.push_back(*it);
		m_objects.erase(it);

		it = std::find_if(m_objects.begin(), m_objects.end(), pred);
	}

	for (Object* obj : nonCasters)
		m_objects.push_back(obj);
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


void Batch::ConstructBatchSpecifics()
{
	//here we need to get material specifics and allocate a suitable amount of memory. But for now we just hardcode the late functionality
	/*
		Storage buffer will look like this
		0x1eb
		C_C_C_C_C_C_C_S_S_S_S_S_S_S
		|            |			  |
		 Common part  Specific Part
	*/
	std::vector<VkDeviceSize> sizes(2);
	sizes[0] = m_objects.size() * sizeof(BatchCommons);
	sizes[1] = m_objects.size() * m_materialTemplate->GetDataStride();
	
	VkDeviceSize totalSize = MemoryManager::GetInstance()->ComputeTotalSize(sizes);

	m_batchStorageBuffer = MemoryManager::GetInstance()->CreateBuffer(EMemoryContextType::UniformBuffers, totalSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	m_batchCommonsBuffer = m_batchStorageBuffer->CreateSubbuffer(sizes[0]);
	m_batchSpecificsBuffer = m_batchStorageBuffer->CreateSubbuffer(sizes[1]);

	m_batchDescriptorSets = m_materialTemplate->GetNewDescriptorSets();
}

void Batch::UpdateGraphicsInterface()
{
	std::vector<VkWriteDescriptorSet> wDesc;

	VkDescriptorBufferInfo commonBuffInfo = m_batchCommonsBuffer->GetDescriptor();
	wDesc.push_back(InitUpdateDescriptor(m_batchDescriptorSets[DescriptorIndex::Common], 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &commonBuffInfo));
	
	VkDescriptorBufferInfo specificBuffInfo = m_batchSpecificsBuffer->GetDescriptor();
	wDesc.push_back(InitUpdateDescriptor(m_batchDescriptorSets[DescriptorIndex::Specific], 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &specificBuffInfo));

	std::vector<VkDescriptorImageInfo> imageInfo;
	for (const auto& text : m_batchTextures)
		imageInfo.push_back(text->GetTextureDescriptor());
	
	wDesc.push_back(InitUpdateDescriptor(m_batchDescriptorSets[DescriptorIndex::Specific], 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, imageInfo));

	//fill with default textures
	std::vector<VkDescriptorImageInfo> defaultTextures;
	for (uint32_t i = (uint32_t)m_batchTextures.size(); i < 12; ++i) //magic MIKE
		defaultTextures.push_back(m_batchTextures[0]->GetTextureDescriptor());

	wDesc.push_back(InitUpdateDescriptor(m_batchDescriptorSets[DescriptorIndex::Specific], 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, (uint32_t)m_batchTextures.size(), defaultTextures));

	vk::UpdateDescriptorSets(vk::g_vulkanContext.m_device, (uint32_t)wDesc.size(), wDesc.data(), 0, nullptr);
}

void Batch::IndexTextures()
{
	//idk man. this is some fucked up shit
	for (Object* obj : m_objects)
	{
		Material* mat = obj->GetObjectMaterial();
		std::vector<IndexedTexture> slots = mat->GetTextureSlots();
		std::vector<IndexedTexture> newSlots = slots; //THIS HERE
		for (unsigned int i = 0; i < slots.size(); ++i)
		{
			auto it = std::find_if(m_batchTextures.begin(), m_batchTextures.end(), [&](const CTexture* elem)
			{
				return elem == slots[i].texture;
			});

			if (it != m_batchTextures.end())
			{
				newSlots[i].index = uint32_t(it - m_batchTextures.begin());
			}
			else
			{
				newSlots[i].index = (uint32_t)m_batchTextures.size();
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
}

void Batch::PreRender()
{
	if (!m_isReady)
		return;

	BatchCommons* commonMem = m_batchCommonsBuffer->GetPtr<BatchCommons*>();
	uint8_t* materialMemory = m_batchSpecificsBuffer->GetPtr<uint8_t*>();

	uint32_t stride = m_materialTemplate->GetDataStride();
	for (unsigned int i = 0; i < m_objects.size(); ++i, ++commonMem, materialMemory += stride)
	{
		Object* obj = m_objects[i];
		TRAP(obj->GetObjectMaterial()->GetTemplate() == m_materialTemplate);
		commonMem->ModelMtx = obj->GetModelMatrix();
		memcpy(materialMemory, m_objects[i]->GetObjectMaterial()->GetData(), m_materialTemplate->GetDataStride());
	}

	glm::mat4 projMatrix;
	PerspectiveMatrix(projMatrix);
	ConvertToProjMatrix(projMatrix);

	m_batchParams.ProjViewMatrix = projMatrix * ms_camera.GetViewMatrix();
	m_batchParams.ViewPos = glm::vec4(ms_camera.GetPos(), 1.0f);
	m_batchParams.ShadowProjViewMatrix = g_commonResources.GetAs<glm::mat4>(EResourceType_ShadowProjViewMat);
}

void Batch::Render(bool shadowPass)
{
	if (!m_isReady)
		return;

	VkCommandBuffer cmdBuffer = vk::g_vulkanContext.m_mainCommandBuffer;
	uint32_t nShadowCastersObjects = (uint32_t)m_objects.size();
	//we should have the objects vector with the first objects shadow casters
	if (shadowPass)
	{
		auto firstNoShadowIt = std::find_if(m_objects.begin(), m_objects.end(), [](const Object* obj)
		{
			return !obj->GetIsShadowCaster();
		});

		if (firstNoShadowIt != m_objects.end())
			nShadowCastersObjects = uint32_t(firstNoShadowIt - m_objects.begin());
	}
	VkDeviceSize offset = 0;
	vk::CmdBindVertexBuffers(cmdBuffer, 0, 1, &m_batchVertexBuffer->Get(), &offset);
	vk::CmdBindIndexBuffer(cmdBuffer, m_batchIndexBuffer->Get(), m_batchIndexBuffer->GetOffset(), VK_INDEX_TYPE_UINT32);

	vk::CmdDrawIndexedIndirect(cmdBuffer, m_indirectCommandBuffer->Get(), 0, nShadowCastersObjects, sizeof(VkDrawIndexedIndirectCommand));
}

void Batch::PrepareRendering(const CGraphicPipeline& pipeline, bool shadowPass)
{
	if (!m_isReady)
		return;
	
	VkCommandBuffer cmdBuffer = vk::g_vulkanContext.m_mainCommandBuffer;

	if (shadowPass)
		vk::CmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.GetLayout(), 0, 1, &m_batchDescriptorSets[DescriptorIndex::Common], 0, nullptr);
	else
		vk::CmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.GetLayout(), 0, (uint32_t)m_batchDescriptorSets.size(), m_batchDescriptorSets.data(), 0, nullptr);
	
	vk::CmdPushConstants(cmdBuffer, pipeline.GetLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(BatchParams), &m_batchParams);
}
