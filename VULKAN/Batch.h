#pragma once

#include "VulkanLoader.h"
#include <vector>

class BufferHandle;
class Mesh;
class Batch
{
public:
	Batch();
	virtual ~Batch();

	//construct
	void AddMesh(Mesh* mesh);
	void Construct();
	void Destruct();

	void Render();

	bool NeedReconstruct() const { return m_needReconstruct; }
	VkDeviceSize GetTotalBatchMemory() const { return m_totalBatchMemory; }
private:
	BufferHandle*			m_batchBuffer;
	BufferHandle*			m_staggingBuffer;
	BufferHandle*			m_indirectCommandBuffer;

	std::vector<Mesh*>		m_meshes; //have to save meshes somewhere before construction
	VkDeviceSize			m_totalBatchMemory;

	bool					m_needReconstruct;
	//need a buffer for uniforms. Also need to pack descriptors??
};