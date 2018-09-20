#include "Batch.h"

#include "Mesh.h"
#include "MemoryManager.h"
#include "Object.h"
#include "Renderer.h"
#include "Texture.h"
#include "Material.h"

#include <iostream>
#include <unordered_set>

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
			if (batches[i]->CanAddObject(obj))
			{
				batches[i]->AddObject(obj);
				return;
			}

		Batch* newBatch = CreateNewBatch(materialTemplate);
		newBatch->AddObject(obj);
		it->second.push_back(newBatch);

		m_batches.push_back(newBatch);//keep all the batches in one place
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

uint32_t Batch::ms_memoryLimit = 4 << 20; //4 MB per batch
uint32_t Batch::ms_texturesLimit = BATCH_MAX_TEXTURE;

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
	, m_shadowCasterCommands(0)
{
}

Batch::~Batch()
{
	Destruct();
}

void Batch::AddObject(Object* obj)
{
	m_objects.push_back(obj);
	std::vector<VkDeviceSize> sizes(2);
	std::unordered_set<Mesh*> meshes;

	for (Object* obj : m_objects)
		meshes.insert(obj->GetObjectMesh());
		
	for (Mesh* mesh : meshes)
	{
		sizes[0] += mesh->GetVerticesMemorySize(); //first part of memory will be vertexes
		sizes[1] += mesh->GetIndicesMemorySize();  //second part will be indexes
	}

	m_totalBatchMemory = MemoryManager::ComputeTotalSize(sizes);
	m_needReconstruct = true;

	if (m_totalBatchMemory > ms_memoryLimit)
		std::cout << "Warning!! Batch uses too much memory!! Maybe the batch contains only one detailed object. In this case we create descriptor sets for texture that are not used" << std::endl;
}

bool Batch::CanAddObject(Object* obj)
{
	std::vector<VkDeviceSize> newObjMeshSizes(2);
	newObjMeshSizes[0] = obj->GetObjectMesh()->GetVerticesMemorySize();
	newObjMeshSizes[1] = obj->GetObjectMesh()->GetIndicesMemorySize();

	VkDeviceSize newMeshSize = MemoryManager::ComputeTotalSize(newObjMeshSizes);

	if (newMeshSize + m_totalBatchMemory > ms_memoryLimit)
		return false;

	Material* material = obj->GetObjectMaterial();
	TRAP(material->GetTemplate() == m_materialTemplate);

	std::unordered_set<CTexture*> textures; 
	for (Object* obj : m_objects)
	{
		const auto& textureSlots = obj->GetObjectMaterial()->GetTextureSlots();
		for (const auto& slot : textureSlots)
			textures.insert(slot.texture);
	}

	const auto& objTextSlots = material->GetTextureSlots();
	for (const auto& slot : objTextSlots)
		textures.insert(slot.texture);

	return textures.size() <= ms_texturesLimit;
}

void Batch::Construct()
{
	std::unordered_map<Mesh*, MeshBufferInfo> meshCommands;
	BuildMeshBuffers(meshCommands);
	CreateIndirectCommandBuffer(meshCommands);

	ConstructBatchSpecifics();
	IndexTextures();
	UpdateGraphicsInterface();
	m_needReconstruct = false;

	m_debugMarkerName = m_materialTemplate->GetName() + "_" + std::to_string(m_totalBatchMemory % 100);
}

void Batch::CreateIndirectCommandBuffer(const std::unordered_map<Mesh*, MeshBufferInfo>& meshCommands)
{
	//order objects in m_objects based on the criteria of cast shadows or not. Our vector will be split in 2 parts, with the non-caster at the end
	std::vector<Object*> nonCasters;
	std::vector<Object*> casters;

	for (Object* obj : m_objects)
	{
		if (obj->GetIsShadowCaster())
			casters.push_back(obj);
		else
			nonCasters.push_back(obj);
	}
	m_objects.clear();

	//after the shadow caster/ non shadow casters split, move the objects that use same mesh adjancently, by caster no caster category. We have to have the objects ordered that way because uniform buffer expects that object in the same draw command to be one after the other. 
	//we order per category, because some objects can be set not to be draw in render shadows pass and we will add the indirect draw command at the back of the command vector
	std::unordered_map<Mesh*, std::vector<Object*>> casterSplitter;
	std::unordered_map<Mesh*, std::vector<Object*>> nonCasterSplitter;

	auto orderer = [](const std::vector<Object*>& objects, std::unordered_map<Mesh*, std::vector<Object*>>& splitter)
	{
		for (Object* obj : objects)
		{
			auto& split = splitter[obj->GetObjectMesh()];
			split.push_back(obj);
		}
	};

	orderer(casters, casterSplitter);
	orderer(nonCasters, nonCasterSplitter);

	uint32_t instances = 0;
	auto createIndirectCommand = [&, meshCommands](const std::unordered_map<Mesh*, std::vector<Object*>> splitter)
	{
		for (const auto& elem : splitter)
		{
			auto it = meshCommands.find(elem.first);
			TRAP(it != meshCommands.end());

			const MeshBufferInfo& info = it->second;
			const auto& objects = elem.second;
			VkDrawIndexedIndirectCommand indirectCommand;

			indirectCommand.firstIndex = info.firstIndex;
			indirectCommand.vertexOffset = info.vertexOffset;
			indirectCommand.indexCount = info.indexCount;
			indirectCommand.firstInstance = instances; //??
			indirectCommand.instanceCount = (uint32_t)objects.size(); //this are the vectors

			instances += (uint32_t)objects.size();
			m_indirectCommands.push_back(indirectCommand);

			m_objects.insert(m_objects.end(), objects.begin(), objects.end());
		}
	};
	createIndirectCommand(casterSplitter);
	//save the number of commands that will be called in render shadow pass
	m_shadowCasterCommands = (uint32_t)m_indirectCommands.size();
	//append the non caster commands
	createIndirectCommand(nonCasterSplitter);

	m_indirectCommandBuffer = MemoryManager::GetInstance()->CreateBuffer(EMemoryContextType::IndirectDrawCmdBuffer, m_indirectCommands.size() * sizeof(VkDrawIndexedIndirectCommand), VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);

	MemoryManager::GetInstance()->MapMemoryContext(EMemoryContextType::IndirectDrawCmdBuffer);
	void* mem = m_indirectCommandBuffer->GetPtr<void*>();
	memcpy(mem, m_indirectCommands.data(), m_indirectCommands.size() * sizeof(VkDrawIndexedIndirectCommand));
	MemoryManager::GetInstance()->UnmapMemoryContext(EMemoryContextType::IndirectDrawCmdBuffer);
}

void Batch::BuildMeshBuffers(std::unordered_map<Mesh*, MeshBufferInfo>& meshCommands)
{
	std::vector<VkDeviceSize> subBuffersSizes(2);
	std::unordered_set<Mesh*> meshes;

	subBuffersSizes[0] = subBuffersSizes[1] = 0;//unnecessary i think
	for (Object* obj : m_objects)
		meshes.insert(obj->GetObjectMesh());

	for (Mesh* mesh : meshes)
	{
		subBuffersSizes[0] += (VkDeviceSize)mesh->GetVerticesMemorySize(); //in first part of the memory we keep the vertices
		subBuffersSizes[1] += (VkDeviceSize)mesh->GetIndicesMemorySize(); //in the second part of the memory we keep the indices
	}

	m_batchBuffer = MemoryManager::GetInstance()->CreateBuffer(EMemoryContextType::DeviceLocalBuffer, subBuffersSizes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	m_staggingBuffer = MemoryManager::GetInstance()->CreateBuffer(EMemoryContextType::BatchStaggingBuffer, subBuffersSizes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

	MemoryManager::GetInstance()->MapMemoryContext(EMemoryContextType::BatchStaggingBuffer);
	
	m_batchVertexBuffer = m_batchBuffer->CreateSubbuffer(subBuffersSizes[0]);
	m_batchIndexBuffer = m_batchBuffer->CreateSubbuffer(subBuffersSizes[1]);
	BufferHandle* staggingVertexBuffer = m_staggingBuffer->CreateSubbuffer(subBuffersSizes[0]);
	BufferHandle* staggingIndexBuffer = m_staggingBuffer->CreateSubbuffer(subBuffersSizes[1]);

	SVertex* vertexMemory = staggingVertexBuffer->GetPtr<SVertex*>();
	uint32_t* indexMemory = staggingIndexBuffer->GetPtr<uint32_t*>();

	uint32_t vertexOffset = 0;
	uint32_t indexOffset = 0;

	for (Mesh* mesh : meshes)
	{
		mesh->CopyLocalData(vertexMemory, indexMemory);
		
		//we partially fill indirect command structure
		MeshBufferInfo info;
		info.firstIndex = indexOffset;
		info.vertexOffset = vertexOffset;
		info.indexCount = mesh->GetIndexCount();

		meshCommands.emplace(mesh, info);

		vertexOffset += mesh->GetVertexCount();
		indexOffset += mesh->GetIndexCount();
		vertexMemory += mesh->GetVertexCount();
		indexMemory += mesh->GetIndexCount();
	}

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
	/*
		Storage buffer will look like this
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
	for (uint32_t i = (uint32_t)m_batchTextures.size(); i < ms_texturesLimit; ++i) //magic MIKE
		defaultTextures.push_back(m_batchTextures[0]->GetTextureDescriptor());

	if (!defaultTextures.empty())
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

	if (!m_batchBuffer)
		MemoryManager::GetInstance()->FreeHandle(m_batchBuffer);

	if (!m_indirectCommandBuffer)
		MemoryManager::GetInstance()->FreeHandle(m_indirectCommandBuffer);

	if (!m_batchStorageBuffer)
		MemoryManager::GetInstance()->FreeHandle(m_batchStorageBuffer);

	m_materialTemplate = nullptr;

	m_indirectCommands.clear();
	m_objects.size();

	// TODO m_batchDescriptorSets i dont know how to approach the problem with descriptor sets being cleared and new construction being allocated. even if a batch is deleted, descriptors remain untill de descriptor pool is deleted
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
	uint32_t nShadowCastersObjects = (shadowPass) ? m_shadowCasterCommands : (uint32_t)m_indirectCommands.size();

	VkDeviceSize offset = 0;
	vk::CmdBindVertexBuffers(cmdBuffer, 0, 1, &m_batchVertexBuffer->Get(), &offset);
	vk::CmdBindIndexBuffer(cmdBuffer, m_batchIndexBuffer->Get(), m_batchIndexBuffer->GetOffset(), VK_INDEX_TYPE_UINT32);

	StartDebugMarker(m_debugMarkerName);
	vk::CmdDrawIndexedIndirect(cmdBuffer, m_indirectCommandBuffer->Get(), 0, nShadowCastersObjects, sizeof(VkDrawIndexedIndirectCommand));
	EndDebugMarker(m_debugMarkerName);
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
