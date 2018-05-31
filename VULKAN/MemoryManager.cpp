#include "MemoryManager.h"

#include <iostream>
///////////////////////////////////////////////////////////////////////////////////
//BufferHandle
///////////////////////////////////////////////////////////////////////////////////
BufferHandle::BufferHandle(VkBuffer buffer, VkDeviceSize size, VkDeviceSize alignment, VkDeviceSize offset)
	: m_buffer(buffer)
	, m_size(size)
	, m_offset(offset)
	, m_alignment(alignment)
	, m_freeOffset(0)
	, m_memoryContext(EMemoryContextType::Count)
{

}

BufferHandle::~BufferHandle()
{
	for (unsigned int i = 0; i < m_subbuffers.size(); ++i)
		delete m_subbuffers[i];

	m_subbuffers.clear();
}

BufferHandle* BufferHandle::CreateSubbuffer(VkDeviceSize size)
{
	VkDeviceSize bufferOffset = m_freeOffset;
	if ((m_alignment != 0) && (m_freeOffset % m_alignment != 0))
		bufferOffset += m_alignment - (m_freeOffset % m_alignment);

	TRAP(bufferOffset + size <= m_size && "Not enough memory for this sub buffer");
	BufferHandle* hSubBuffer = new BufferHandle(m_buffer, size, m_alignment, bufferOffset);
	hSubBuffer->SetMemoryContext(m_memoryContext);

	m_freeOffset = bufferOffset + size;
	m_subbuffers.push_back(hSubBuffer);
	return hSubBuffer;
}

VkBufferMemoryBarrier BufferHandle::CreateMemoryBarrier(VkAccessFlags srcAccess, VkAccessFlags dstAccess)
{
	VkBufferMemoryBarrier barrier;
	cleanStructure(barrier);
	barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	barrier.srcAccessMask = srcAccess;
	barrier.dstAccessMask = dstAccess;
	barrier.buffer = GetBuffer();
	barrier.offset = GetOffset();
	barrier.size = GetSize();
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

	return barrier;
}

///////////////////////////////////////////////////////////////////////////////////
//MemoryContext::MappedMemory
///////////////////////////////////////////////////////////////////////////////////
MappedMemory::MappedMemory(void* memPtr, MemoryContext* parentContext)
	: m_memPtr(memPtr)
	, m_parentMemoryContext(parentContext)
{

}

MappedMemory::~MappedMemory()
{

}


///////////////////////////////////////////////////////////////////////////////////
//MemoryContext
///////////////////////////////////////////////////////////////////////////////////

MemoryContext::MemoryContext(EMemoryContextType type)
	: m_memory(VK_NULL_HANDLE)
	, m_totalSize(0)
	, m_memoryTypeIndex(-1)
	, m_contextType(type)
	, m_mappedMemory(nullptr)
	, m_memoryFlags(-1)
{

}

MemoryContext::~MemoryContext()
{
	if (m_memory != VK_NULL_HANDLE)
		FreeMemory();
}

void MemoryContext::AllocateMemory(VkDeviceSize size, VkMemoryPropertyFlags flags)
{
	TRAP(m_memory == VK_NULL_HANDLE && "Free memory before allocate another one");
	m_memoryTypeIndex = vk::SVUlkanContext::GetMemTypeIndex(0xFFFFFFFF, flags);
	m_totalSize = size;
	m_memoryFlags = flags;

	VkMemoryAllocateInfo allocInfo;
	cleanStructure(allocInfo);
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = m_totalSize;
	allocInfo.memoryTypeIndex = m_memoryTypeIndex;
	VULKAN_ASSERT(vk::AllocateMemory(vk::g_vulkanContext.m_device, &allocInfo, nullptr, &m_memory));

	m_chunks.insert(Chunk(0, m_totalSize));
}

void MemoryContext::FreeMemory()
{
	if (m_memory == VK_NULL_HANDLE)
		return;

	TRAP(!IsMapped() && "Dont free memory if it's mapped!!");

	VkDevice dev = vk::g_vulkanContext.m_device;

	for (auto e : m_buffersToOffset)
	{
		BufferToOffset_t buffToOffset = e.second;
		vk::DestroyBuffer(dev, buffToOffset.first->GetBuffer(), nullptr);
		delete buffToOffset.first;
	}

	m_chunks.clear();
	m_buffersToOffset.clear();
	vk::FreeMemory(dev, m_memory, nullptr);
	m_memory = VK_NULL_HANDLE;
}

bool MemoryContext::MapMemory()
{
	TRAP(!IsMapped() && "Dont map memory if it is already mapped");
	void* memPtr = nullptr;
	VULKAN_ASSERT(vk::MapMemory(vk::g_vulkanContext.m_device, m_memory, 0, m_totalSize, 0, &memPtr));
	m_mappedMemory = new MappedMemory(memPtr, this);
	return memPtr != nullptr;
}

void MemoryContext::UnmapMemory()
{
	if (!IsMapped())
		return;

	vk::UnmapMemory(vk::g_vulkanContext.m_device, m_memory);
	delete m_mappedMemory;
	m_mappedMemory = nullptr;
}

bool MemoryContext::CanMapMemory()
{
	return m_contextType != EMemoryContextType::DeviceLocal;
}

MemoryContext::Chunk MemoryContext::GetFreeChunk(VkDeviceSize size, VkDeviceSize alignment)
{
	auto found = m_chunks.begin();
	for (; found != m_chunks.end(); ++found)
	{
		if (found->m_size >= size)
			break;
	}
	TRAP(found != m_chunks.end() && "No chunk found for allocation! Too fragmented or not enough memory");
	TRAP(found->m_offset % alignment == 0 && "Something is wrong! All chunks should be stay aligned!! Maybe size is not from MemoryRequirments struct");
	
	Chunk foundChunk = *found;
	m_chunks.erase(found);
	
	//split the chunk
	Chunk allocChunk(foundChunk.m_offset, size);
	Chunk newChunk(foundChunk.m_offset + allocChunk.m_size, foundChunk.m_size - allocChunk.m_size);
	if (newChunk.m_size > 0)
		m_chunks.insert(newChunk);

	return allocChunk;
}

void MemoryContext::FreeChunk(MemoryContext::Chunk chunk)
{
	Chunk newChunk = chunk;
	auto prevChunk = std::find_if(m_chunks.begin(), m_chunks.end(), [&](const Chunk& other)
	{
		return other.m_offset + other.m_size == chunk.m_offset;
	});
	   
	if (prevChunk != m_chunks.end())
	{
		newChunk = Chunk(prevChunk->m_offset, prevChunk->m_size + newChunk.m_size);
		m_chunks.erase(prevChunk);
	}

	auto nextChunk = std::find_if(m_chunks.begin(), m_chunks.end(), [&](const Chunk& other)
	{
		return other.m_offset == chunk.m_offset + chunk.m_size;
	});

	if (nextChunk != m_chunks.end())
	{
		newChunk = Chunk(newChunk.m_offset, newChunk.m_size + nextChunk->m_size);
		m_chunks.erase(nextChunk);
	}
	if (newChunk.m_size > 0)
		m_chunks.insert(newChunk);
}

BufferHandle* MemoryContext::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage)
{
	VkBufferCreateInfo crtInfo;
	cleanStructure(crtInfo);
	crtInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	crtInfo.flags = 0;
	crtInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	crtInfo.usage = usage;
	crtInfo.size = size;

	VkBuffer buffer;
	VULKAN_ASSERT(vk::CreateBuffer(vk::g_vulkanContext.m_device, &crtInfo, nullptr, &buffer));

	VkMemoryRequirements memReq;
	vk::GetBufferMemoryRequirements(vk::g_vulkanContext.m_device, buffer, &memReq);

	uint32_t buffMemIndex = vk::SVUlkanContext::GetMemTypeIndex(memReq.memoryTypeBits, m_memoryFlags);
	TRAP(buffMemIndex == m_memoryTypeIndex && "Memory req for this buffer is not in the same heap");
	std::cout << "For buffer " << buffer << " in context memory " << (unsigned int)m_contextType << " buffer memory index: " << buffMemIndex << " with memory index: " << m_memoryTypeIndex << std::endl;

	Chunk memoryChunk = GetFreeChunk(memReq.size, memReq.alignment);

	//bind buffer to memory
	VULKAN_ASSERT(vk::BindBufferMemory(vk::g_vulkanContext.m_device, buffer, m_memory, memoryChunk.m_offset));
	VkDeviceSize alignment;
	if (usage & (VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT))
		alignment = memReq.alignment; //if is one of the above buffers, we use aligmnent when create sub buffers.
	else
		alignment = 0; // for index and vertex buffers. It seems that these buffers dont have to be aligned when we suballocate

	BufferHandle* hBufferHandle = new BufferHandle(buffer, size, alignment);
	hBufferHandle->SetMemoryContext(m_contextType);

	m_buffersToOffset.emplace(buffer, std::pair<BufferHandle*, Chunk>(hBufferHandle, memoryChunk));

	return hBufferHandle;
}

void MemoryContext::FreeBuffer(BufferHandle* handle)
{
	auto found = m_buffersToOffset.find(handle->GetBuffer());
	if (found == m_buffersToOffset.end())
		return;

	BufferToOffset_t buffToOffset = found->second;

	m_buffersToOffset.erase(found);
	FreeChunk(buffToOffset.second);
	vk::DestroyBuffer(vk::g_vulkanContext.m_device, buffToOffset.first->GetBuffer(), nullptr);
	delete buffToOffset.first;
}

///////////////////////////////////////////////////////////////////////////////////
//MemoryManager
///////////////////////////////////////////////////////////////////////////////////

MemoryManager::MemoryManager()
{
	for (unsigned int i = 0; i < (unsigned int)EMemoryContextType::Count; ++i)
	{
		m_memoryContexts[i] = new MemoryContext((EMemoryContextType)i);
	}

	m_memoryContexts[(unsigned int)EMemoryContextType::DeviceLocal]->AllocateMemory(30 << 20, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
}

MemoryManager::~MemoryManager()
{
	for (unsigned int i = 0; i < (unsigned int)EMemoryContextType::Count; ++i)
		m_memoryContexts[i]->FreeMemory();
}

BufferHandle* MemoryManager::CreateBuffer(EMemoryContextType context, VkDeviceSize size, VkBufferUsageFlags usage)
{
	return m_memoryContexts[(unsigned int)context]->CreateBuffer(size, usage);
}

void MemoryManager::FreeBuffer(EMemoryContextType context, BufferHandle* handle)
{
	m_memoryContexts[(unsigned int)context]->FreeBuffer(handle);
}

void MemoryManager::AllocStaggingMemory(VkDeviceSize size)
{
	m_memoryContexts[(unsigned int)EMemoryContextType::Stagging]->AllocateMemory(size, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
}

void MemoryManager::FreeStagginMemory()
{
	m_memoryContexts[(unsigned int)EMemoryContextType::Stagging]->FreeMemory();
}

bool MemoryManager::MapMemoryContext(EMemoryContextType context)
{
	MemoryContext* memContext = m_memoryContexts[(unsigned int)context];
	if (!memContext->CanMapMemory()) //this is not mappable
		return false;

	return memContext->MapMemory();
}

void MemoryManager::UnmapMemoryContext(EMemoryContextType context)
{
	m_memoryContexts[(unsigned int)context]->UnmapMemory();
}

MappedMemory* MemoryManager::GetMappedMemory(EMemoryContextType context)
{
	MappedMemory* mapMem = m_memoryContexts[(unsigned int)context]->GetMappedMemory();
	TRAP(mapMem && " Memory context is not mapped or cannot be mapped!");
	return mapMem;
}
