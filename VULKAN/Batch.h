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
class MaterialTemplateBase;
class Batch;
class Material;
class CTexture;

class BatchManager : public Singleton<BatchManager>
{
public:
	BatchManager();
	virtual ~BatchManager();

	void AddObject(Object* obj);
	//we need a list of parameters here (we have to know the pipeline, how much uniform memory per batch, or do we use a fixed size. I dont know it seems not too optim)
	Batch* CreateNewBatch(MaterialTemplateBase* materialTemplate);

	void Update();

	void RenderAll();
	void RenderShadows();
	void PreRender();
private:
private:
	std::vector<Batch*>				m_batches;
	std::vector<Batch*>				m_inProgressBatches;

	typedef std::unordered_map<MaterialTemplateBase*, std::vector<Batch*>> TBatchMap;
	TBatchMap						m_batchesCategories;
};

class Batch
{
public:
	Batch(MaterialTemplateBase* materialTemplate);
	virtual ~Batch();

	void AddObject(Object* obj);

	void Construct();
	void Destruct();
	void Cleanup();

	void PreRender();
	void Render(bool shadowPass);
	void PrepareRendering(const CGraphicPipeline& pipeline, bool shadowPass);

	bool NeedReconstruct() const { return m_needReconstruct; }
	bool NeedCleanup() const { return m_needCleanup; }

	VkDeviceSize GetTotalBatchMemory() const { return m_totalBatchMemory; }

private:
	struct MeshBufferInfo
	{
		uint32_t firstIndex;
		uint32_t vertexOffset;
		uint32_t indexCount;
	};
	void CreateIndirectCommandBuffer(const std::unordered_map<Mesh*, MeshBufferInfo>& meshCommands);
	void BuildMeshBuffers(std::unordered_map<Mesh*, MeshBufferInfo>& meshCommands);
	void ConstructBatchSpecifics();
	void UpdateGraphicsInterface();
	void IndexTextures();
private:
	struct BatchParams
	{
		glm::mat4 ProjViewMatrix;
		glm::mat4 ShadowProjViewMatrix;
		glm::vec4 ViewPos;
	} m_batchParams;

	BufferHandle*			m_batchBuffer;
	BufferHandle*			m_staggingBuffer;
	BufferHandle*			m_indirectCommandBuffer;
	BufferHandle*			m_batchVertexBuffer;
	BufferHandle*			m_batchIndexBuffer;
	
	BufferHandle*			m_batchStorageBuffer;
	BufferHandle*			m_batchCommonsBuffer;
	BufferHandle*			m_batchSpecificsBuffer;

	MaterialTemplateBase*	m_materialTemplate;

	std::vector<Object*>	m_objects;
	VkDeviceSize			m_totalBatchMemory;

	bool					m_needReconstruct;
	bool					m_needCleanup;
	bool					m_isReady;

	//need a buffer for uniforms. Also need to pack descriptors??
	std::vector<VkDescriptorSet>			m_batchDescriptorSets;
	std::vector<CTexture*>					m_batchTextures;
	uint32_t								m_shadowCasterCommands;
	std::vector<VkDrawIndexedIndirectCommand> m_indirectCommands;
};