#pragma once
#include "VulkanLoader.h"

#include "Singleton.h"
#include "defines.h"

#include <vector>
#include <set>
#include <utility>
#include <array>
#include <algorithm>

enum class EMemoryContextType
{
	DeviceLocal,
	Stagging,
	Count
};

class BufferHandle
{
	friend class MemoryContext;
public:
	VkBuffer& GetBuffer() { return m_buffer; }
	VkDeviceSize GetSize() const { return m_size; }
	VkDeviceSize GetOffset() const { return m_offset; }

	BufferHandle* CreateSubbuffer(VkDeviceSize size);
private:
	BufferHandle(VkBuffer buffer, VkDeviceSize size, VkDeviceSize alignment, VkDeviceSize offset = 0);
	virtual ~BufferHandle();

	BufferHandle(const BufferHandle& other);
	BufferHandle& operator=(const BufferHandle& other);

	void SetMemoryContext(EMemoryContextType type) { m_memoryContext = type; }
private:
	VkBuffer		m_buffer;
	VkDeviceSize	m_size;
	VkDeviceSize	m_offset;
	VkDeviceSize	m_alignment; //if alignment is 0, then is not considered when creating sub buffers.

	EMemoryContextType	m_memoryContext;

	//used for creating subbuffers
	std::vector<BufferHandle*>	m_subbuffers;
	VkDeviceSize				m_freeOffset;
};

class MappedMemory
{
	friend class MemoryContext;
public:
	template<class T>
	T GetPtr(BufferHandle* handle);


private:
	MappedMemory(void* memPtr, MemoryContext* parentContext);
	virtual ~MappedMemory();

private:
	MemoryContext*	m_parentMemoryContext;
	void*			m_memPtr;
};

class MemoryContext
{
	friend class MappedMemory;
public:
	MemoryContext(EMemoryContextType type);
	virtual ~MemoryContext();

	BufferHandle* CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage);
	void AllocateMemory(VkDeviceSize size, VkMemoryPropertyFlags flags);
	void FreeMemory();
	
	bool CanMapMemory();
	bool MapMemory();
	void UnmapMemory();
	bool IsMapped() const { return m_mappedMemory != nullptr; }
	MappedMemory* GetMappedMemory() const { return (IsMapped())? m_mappedMemory : nullptr; }
private:
private:
	std::set<std::pair<BufferHandle*, VkDeviceSize>>	m_buffersToOffset;
	VkDeviceMemory										m_memory;
	VkDeviceSize										m_freeOffset;
	VkDeviceSize										m_totalSize;
	uint32_t											m_memoryTypeIndex;
	MappedMemory*										m_mappedMemory;
	VkMemoryPropertyFlags								m_memoryFlags;

	EMemoryContextType									m_contextType;
};

template<class T>
T MappedMemory::GetPtr(BufferHandle* handle)
{
	//change this function is shit
	auto& handles = m_parentMemoryContext->m_buffersToOffset;
	auto it = std::find_if(handles.begin(), handles.end(), [&](std::pair<BufferHandle*, VkDeviceSize> other)
	{
		return handle->GetBuffer() == other.first->GetBuffer();
	});

	if (it == handles.end())
	{
		TRAP(false && " This buffer is not from this memory context");
		return nullptr;
	}
	VkDeviceSize subBufferOffset = handle->GetOffset(); //handle can be a subbuffer from a bigger buffer;
	VkDeviceSize bufferOffset = it->second; //this is the offset inside the VkDeviceMemory that is mapped to the buffer
	uint8_t* bufferMemPtr = (uint8_t*)m_memPtr + bufferOffset + subBufferOffset;
	return (T)bufferMemPtr;
}

class MemoryManager : public Singleton<MemoryManager>
{
	friend class Singleton<MemoryManager>;
public:
	BufferHandle* CreateBuffer(EMemoryContextType context, VkDeviceSize size, VkBufferUsageFlags usage);
	void AllocStaggingMemory(VkDeviceSize size);
	void FreeStagginMemory();

	bool MapMemoryContext(EMemoryContextType context);
	void UnmapMemoryContext(EMemoryContextType context);

	MappedMemory* GetMappedMemory(EMemoryContextType context);

protected:
	MemoryManager();
	virtual ~MemoryManager();

	MemoryManager(const MemoryManager&);
	MemoryManager& operator= (const MemoryManager&);

	
private:
	std::array<MemoryContext*, (size_t)EMemoryContextType::Count>	m_memoryContexts;
};