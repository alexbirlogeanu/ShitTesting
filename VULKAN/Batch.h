#pragma once

#include "VulkanLoader.h"
#include "Renderer.h"
#include "DescriptorsUtils.h"
#include "Singleton.h"

#include <vector>

class BufferHandle;
class Mesh;
class Object;
class CRenderer;

class Batch;
class BatchBuilder : public Singleton<BatchBuilder>
{
public:
	BatchBuilder();
	virtual ~BatchBuilder();

	//we need a list of parameters here (we have to know the pipeline, how much uniform memory per batch, or do we use a fixed size. I dont know it seems not too optim)
	Batch* CreateNewBatch();

	//TODO temporary function
	VkDescriptorSet AllocNewDescriptorSet();

	VkDescriptorSetLayout GetDescriptorSetLayout() const { return m_batchSpecificDescLayout.Get(); }
private:
	void CreateDescriptorLayout();
private:
	std::vector<DescriptorPool>		m_descriptorPools;
	uint32_t						m_currentPoolIndex;

	DescriptorSetLayout				m_batchSpecificDescLayout;
};

class Batch
{
public:
	Batch();
	virtual ~Batch();

	void AddObject(Object* obj);
	void Construct(CRenderer* renderer, VkRenderPass renderPass, uint32_t subpassIndex/*TODO remove*/);
	void Destruct();
	void Cleanup();

	void PreRender();
	void Render();

	bool NeedReconstruct() const { return m_needReconstruct; }
	bool NeedCleanup() const { return m_needCleanup; }

	VkDeviceSize GetTotalBatchMemory() const { return m_totalBatchMemory; }
private:
	void ConstructMeshes();
	void ConstructPipeline(CRenderer* renderer, VkRenderPass renderPass, uint32_t subpassIndex); //TODO remove
	void ConstructBatchSpecifics();
	void UpdateGraphicsInterface();
private:
	struct BatchParams
	{
		glm::mat4 ProjViewMatrix;
		glm::vec4 ViewPos;
	} m_batchParams;

	BufferHandle*			m_batchBuffer;
	BufferHandle*			m_staggingBuffer;
	BufferHandle*			m_indirectCommandBuffer;
	BufferHandle*			m_batchVertexBuffer;
	BufferHandle*			m_batchIndexBuffer;

	BufferHandle*			m_materialBuffer; //this should be coming from a material template

	std::vector<Object*>	m_objects;
	VkDeviceSize			m_totalBatchMemory;

	bool					m_needReconstruct;
	bool					m_needCleanup;

	//HACK
	bool					m_canRender;
	bool					m_needUpdate;

	//need a buffer for uniforms. Also need to pack descriptors??
	CGraphicPipeline		m_pipeline; //Batch class shouldn't onw a pipline. material should construct the pipeline. and we bind only once per multiple batches
	VkDescriptorSet			m_batchSpecificDescSet;
};