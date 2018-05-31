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
	, m_freeOffset(0)
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
}

void MemoryContext::FreeMemory()
{
	if (m_memory == VK_NULL_HANDLE)
		return;

	TRAP(!IsMapped() && "Dont free memory if it's mapped!!");

	VkDevice dev = vk::g_vulkanContext.m_device;

	for (auto buff : m_buffersToOffset)
	{
		vk::DestroyBuffer(dev, buff.first->GetBuffer(), nullptr);
		delete buff.first;
	}


	m_buffersToOffset.clear();
	vk::FreeMemory(dev, m_memory, nullptr);
	m_memory = VK_NULL_HANDLE;
}

bool MemoryContext::MapMemory()
{
	TRAP(!IsMapped() && "Dont map memory if it is already mapped");
	void* memPtr = nullptr;
	VULKAN_ASSERT(vk::MapMemory(vk::g_vulkanContext.m_device, m_memory, 0, m_freeOffset, 0, &memPtr));
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

	VkDeviceSize bufferOffset = m_freeOffset;
	if (bufferOffset % memReq.alignment != 0)
		bufferOffset += memReq.alignment - (bufferOffset % memReq.alignment);

	TRAP(bufferOffset + memReq.size <= m_totalSize && "Not enough memory for this buffer");
	m_freeOffset = bufferOffset + memReq.size;

	//bind buffer to memory
	VULKAN_ASSERT(vk::BindBufferMemory(vk::g_vulkanContext.m_device, buffer, m_memory, bufferOffset));
	VkDeviceSize alignment;
	if (usage & (VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT))
		alignment = memReq.alignment; //if is one of the above buffers, we use aligmnent when create sub buffers.
	else
		alignment = 0; // for index and vertex buffers. It seems that these buffers dont have to be aligned when we suballocate

	BufferHandle* hBufferHandle = new BufferHandle(buffer, memReq.size, alignment);
	hBufferHandle->SetMemoryContext(m_contextType);

	m_buffersToOffset.emplace(std::pair<BufferHandle*, VkDeviceSize>(hBufferHandle, bufferOffset));

	return hBufferHandle;
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
