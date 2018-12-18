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

enum class SubpassIndex
{
	ShadowPass,
	Solid,
	Count
};

class Batch
{
public:
	Batch(MaterialTemplateBase* materialTemplate);
	virtual ~Batch();

	void AddObject(Object* obj);
	//TODO CanAddObject should return true if the batch has already an object with the same mesh as obj
	bool CanAddObject(Object* obj);

	void Construct();
	void Destruct();
	void Cleanup();

	void PreRender();
	void Render(SubpassIndex subpassIndex);
	void PrepareRendering(const CGraphicPipeline& pipeline, SubpassIndex subpassIndex);

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

	struct SubpassInfo
	{
		std::vector<VkDescriptorSet>						DescriptorSets;
		BufferHandle*										CommonBuffer;
		BufferHandle*										SpecificBuffer;
		BufferHandle*										IndirectCommands;
		std::vector<Object*>								RenderedObjects;
	};

	void BuildMeshBuffers();
	void InitSubpasses();
	void UpdateGraphicsInterface();
	void IndexTextures();

	void UpdateIndirectCmdBuffer(SubpassInfo& subpass);

	std::string GetSubpassDebugMarker(SubpassIndex subpassIndex);
private:
	typedef std::unordered_map<Mesh*, MeshBufferInfo> TMeshMap;

	struct BatchParams
	{
		glm::mat4 ProjViewMatrix;
		glm::mat4 ShadowProjViewMatrix;
		glm::vec4 ViewPos;
	} m_batchParams;

	BufferHandle*			m_batchBuffer;
	BufferHandle*			m_staggingBuffer;
	BufferHandle*			m_batchVertexBuffer;
	BufferHandle*			m_batchIndexBuffer;
	
	//global handles for the memory
	BufferHandle*			m_batchStorageBuffer;
	BufferHandle*			m_indirectCommandBuffer;


	MaterialTemplateBase*	m_materialTemplate;

	std::vector<Object*>	m_objects;
	VkDeviceSize			m_totalBatchMemory;

	bool					m_needReconstruct;
	bool					m_needCleanup;
	bool					m_isReady;

	//need a buffer for uniforms. Also need to pack descriptors??
	std::vector<CTexture*>						m_batchTextures;
	std::array<SubpassInfo, uint32_t(SubpassIndex::Count)> m_subpasses;

	TMeshMap									m_batchMeshes;

	static uint32_t								ms_memoryLimit;
	static uint32_t								ms_texturesLimit;

	std::string									m_debugMarkerName;

};
