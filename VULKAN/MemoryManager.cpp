#include "MemoryManager.h"
#include "Utils.h"

#include <iostream>
///////////////////////////////////////////////////////////////////////////////////
//BufferHandle
///////////////////////////////////////////////////////////////////////////////////
BufferHandle::BufferHandle(VkBuffer handle, VkDeviceSize size, VkDeviceSize alignment, MemoryContext* context)
	: HandleImpl<VkBuffer>(handle, size, alignment, context)
{

}
BufferHandle::BufferHandle(BufferHandle* parent, VkDeviceSize size, VkDeviceSize alignment, VkDeviceSize offset)
	: HandleImpl<VkBuffer>(parent, size, alignment, offset)
{
}

BufferHandle* BufferHandle::CreateSubbuffer(VkDeviceSize size)
{
	return SubAllocate(this, size);
}

VkBufferMemoryBarrier BufferHandle::CreateMemoryBarrier(VkAccessFlags srcAccess, VkAccessFlags dstAccess)
{
	VkBufferMemoryBarrier barrier;
	cleanStructure(barrier);
	barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	barrier.srcAccessMask = srcAccess;
	barrier.dstAccessMask = dstAccess;
	barrier.buffer = Get();
	barrier.offset = GetOffset();
	barrier.size = GetSize();
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

	return barrier;
}


VkDescriptorBufferInfo BufferHandle::GetDescriptor() const
{
	VkDescriptorBufferInfo descriptor{ m_vkHandle, m_offset, m_size };
	return descriptor;
}

void BufferHandle::FreeResources()
{
	if (m_parent == nullptr)
		vk::DestroyBuffer(vk::g_vulkanContext.m_device, Get(), nullptr);
}

///////////////////////////////////////////////////////////////////////////////////
//ImageHandle
///////////////////////////////////////////////////////////////////////////////////

ImageHandle::ImageHandle(VkImage vkHandle, VkDeviceSize size, VkDeviceSize alignment, const VkImageCreateInfo& imgInfo, MemoryContext* context)
	: HandleImpl<VkImage>(vkHandle, size, alignment, context)
	, m_view(VK_NULL_HANDLE)
	, m_format(imgInfo.format)
	, m_dimensions(imgInfo.extent)
	, m_layers(imgInfo.arrayLayers)
{
	CreateImageView(m_view, Get(), imgInfo);
	if (m_layers > 1)
	{
		m_layersViews.resize(m_layers);
		for (uint32_t i = 0; i < m_layers; ++i)
			CreateImageView(m_layersViews[i], Get(), imgInfo.format, imgInfo.extent, 1, i, imgInfo.mipLevels, 0);
	}
}

void ImageHandle::FreeResources()
{
	if (m_parent == nullptr)
	{
		VkDevice dev = vk::g_vulkanContext.m_device;
		vk::DestroyImageView(dev, m_view, nullptr);
		
		for (auto& layerView : m_layersViews)
			vk::DestroyImageView(dev, layerView, nullptr);

		vk::DestroyImage(dev, Get(), nullptr);
	}
}

ImageHandle::~ImageHandle()
{

}

VkImageMemoryBarrier ImageHandle::CreateMemoryBarrier(VkImageLayout oldLayout, VkImageLayout newLayout, VkAccessFlags srcMask, VkAccessFlags dstMask, VkImageAspectFlags aspectFlags, unsigned int layersCount)
{
	VkImageMemoryBarrier outBarrier;
	cleanStructure(outBarrier);
	outBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	outBarrier.pNext = NULL;
	outBarrier.srcAccessMask = srcMask;
	outBarrier.dstAccessMask = dstMask;
	outBarrier.oldLayout = oldLayout;
	outBarrier.newLayout = newLayout;
	outBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	outBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	outBarrier.image = Get();
	outBarrier.subresourceRange.aspectMask = aspectFlags;
	outBarrier.subresourceRange.baseMipLevel = 0;
	outBarrier.subresourceRange.levelCount = 1;
	outBarrier.subresourceRange.baseArrayLayer = 0;
	outBarrier.subresourceRange.layerCount = layersCount;

	return outBarrier;
}

VkImageMemoryBarrier ImageHandle::CreateMemoryBarrierForMips(uint32_t mipLevel, VkImageLayout oldLayout, VkImageLayout newLayout, VkAccessFlags srcMask, VkAccessFlags dstMask, VkImageAspectFlags aspectFlags, uint32_t mipCount)
{
	VkImageMemoryBarrier outBarrier;
	cleanStructure(outBarrier);
	outBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	outBarrier.pNext = NULL;
	outBarrier.srcAccessMask = srcMask;
	outBarrier.dstAccessMask = dstMask;
	outBarrier.oldLayout = oldLayout;
	outBarrier.newLayout = newLayout;
	outBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	outBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	outBarrier.image = Get();
	outBarrier.subresourceRange.aspectMask = aspectFlags;
	outBarrier.subresourceRange.baseMipLevel = mipLevel;
	outBarrier.subresourceRange.levelCount = mipCount;
	outBarrier.subresourceRange.baseArrayLayer = 0;
	outBarrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

	return outBarrier;
}

const VkImageView& ImageHandle::GetLayerView(uint32_t layer) const
{
	if (m_layers == 1) //if image is not a layered one ignore the param
		return m_view;

	TRAP(layer < m_layers);
	TRAP(!m_layersViews.empty() && "Layers not initialized!");
	return m_layersViews[layer];
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
	, m_allocatedSize(0)
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
	uint32_t bitsType = -1;
	VkDevice dev = vk::g_vulkanContext.m_device;

	if (IsBufferMemory())
	{
		VkBufferCreateInfo dummyCrtInfo;
		cleanStructure(dummyCrtInfo);
		dummyCrtInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		dummyCrtInfo.flags = 0;
		dummyCrtInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		dummyCrtInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
		dummyCrtInfo.size = size;

		VkBuffer dummyBuffer;
		VULKAN_ASSERT(vk::CreateBuffer(dev, &dummyCrtInfo, nullptr, &dummyBuffer));

		VkMemoryRequirements memReq;
		vk::GetBufferMemoryRequirements(dev, dummyBuffer, &memReq);
		vk::DestroyBuffer(dev, dummyBuffer, nullptr);

		bitsType = memReq.memoryTypeBits;
	}
	else //for images
	{
		VkImageCreateInfo dummyCrtInfo;
		cleanStructure(dummyCrtInfo);
		dummyCrtInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		dummyCrtInfo.pNext = nullptr;
		dummyCrtInfo.flags = 0;
		dummyCrtInfo.imageType = VK_IMAGE_TYPE_2D;
		dummyCrtInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
		dummyCrtInfo.extent.width = 8;
		dummyCrtInfo.extent.height = 8;
		dummyCrtInfo.extent.depth = 1;
		dummyCrtInfo.mipLevels = 1;
		dummyCrtInfo.arrayLayers = 1;
		dummyCrtInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		dummyCrtInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		dummyCrtInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		dummyCrtInfo.queueFamilyIndexCount = 0;
		dummyCrtInfo.pQueueFamilyIndices = NULL;
		dummyCrtInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VkImage dummyImage;
		VULKAN_ASSERT(vk::CreateImage(dev, &dummyCrtInfo, nullptr, &dummyImage));
		VkMemoryRequirements memReq;
		vk::GetImageMemoryRequirements(dev, dummyImage, &memReq);
		vk::DestroyImage(dev, dummyImage, nullptr);

		bitsType = memReq.memoryTypeBits;
	}
	TRAP(m_memory == VK_NULL_HANDLE && "Free memory before allocate another one");
	m_memoryTypeIndex = vk::SVUlkanContext::GetMemTypeIndex(bitsType, flags);
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

	for (auto c : m_allocatedChunks)
	{
		Handle* h = c.first;
		h->FreeResources();
		delete h;
	}
	m_allocatedChunks.clear();
	m_chunks.clear();

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

bool MemoryContext::IsBufferMemory() const
{ 
	switch (m_contextType)
	{
	case EMemoryContextType::Framebuffers:
	case EMemoryContextType::Textures:
		return false;
	default:
		return true;
	}
}
bool MemoryContext::CanMapMemory()
{
	return IsBufferMemory() && m_contextType != EMemoryContextType::DeviceLocalBuffer;
}

MemoryContext::Chunk MemoryContext::GetFreeChunk(VkDeviceSize size, VkDeviceSize alignment)
{
	auto found = m_chunks.begin();
	for (; found != m_chunks.end(); ++found)
	{
		if (found->m_size >= size)
			break; 
	}
	TRAP(found != m_chunks.end() && "No chunk found for allocation! Too fragmented or not enough memory"); //try to resolve the fragmentation problem
	TRAP(found->m_offset % alignment == 0 && "Something is wrong! All chunks should be stay aligned!! Maybe size is not from MemoryRequirments struct");
	
	if (size % alignment != 0)
		size += alignment - (size % alignment);

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
	//std::cout << "For buffer " << buffer << " in context memory " << (unsigned int)m_contextType << " buffer memory index: " << buffMemIndex << " with memory index: " << m_memoryTypeIndex << std::endl;

	Chunk memoryChunk = GetFreeChunk(memReq.size, memReq.alignment);

	//bind buffer to memory
	VULKAN_ASSERT(vk::BindBufferMemory(vk::g_vulkanContext.m_device, buffer, m_memory, memoryChunk.m_offset));
	VkDeviceSize alignment;
	if (usage & (VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT))
		alignment = memReq.alignment; //if is one of the above buffers, we use aligmnent when create sub buffers.
	else
		alignment = 0; // for index and vertex buffers. It seems that these buffers dont have to be aligned when we suballocate

	BufferHandle* hBufferHandle = new BufferHandle(buffer, size, alignment, this);

	m_allocatedChunks.emplace(hBufferHandle, memoryChunk);
	m_allocatedSize += memReq.size;

	return hBufferHandle;
}

ImageHandle* MemoryContext::CreateImage(const VkImageCreateInfo& crtInfo, const std::string& debugName)
{
	VkDevice dev = vk::g_vulkanContext.m_device;
	VkImage image;
	vk::CreateImage(dev, &crtInfo, nullptr, &image);

	VkMemoryRequirements memReq;
	vk::GetImageMemoryRequirements(dev, image, &memReq);

	uint32_t imgMemIndex = vk::SVUlkanContext::GetMemTypeIndex(memReq.memoryTypeBits, m_memoryFlags);
	TRAP(imgMemIndex == m_memoryTypeIndex && "Memory req for this image is not in the same heap");
	
	Chunk memoryChunk = GetFreeChunk(memReq.size, memReq.alignment);
	//bind image to memory
	VULKAN_ASSERT(vk::BindImageMemory(dev, image, m_memory, memoryChunk.m_offset));

	ImageHandle* hImageHandle = new ImageHandle(image, memReq.size, memReq.alignment, crtInfo, this);
	m_allocatedChunks.emplace(hImageHandle, memoryChunk);
	m_allocatedSize += memReq.size;

	if (!debugName.empty())
		SetObjectDebugName(image, debugName);

	return hImageHandle;
}

void MemoryContext::FreeHandle(Handle* handle)
{
	auto found = m_allocatedChunks.find(handle->GetRootParent());
	if (found == m_allocatedChunks.end())
	{
		TRAP(false && "The handle is not allocated from this memory or a child handle is passed for freeing");
		return;
	}

	Chunk chunk = found->second;
	FreeChunk(chunk);
	m_allocatedSize -= chunk.m_size;
	m_allocatedChunks.erase(found);
	handle->FreeResources();
	delete handle;
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
	m_memoryContexts[(unsigned int)EMemoryContextType::StagginBuffer]->AllocateMemory(4 << 20, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	m_memoryContexts[(unsigned int)EMemoryContextType::DeviceLocalBuffer]->AllocateMemory(64 << 20, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	m_memoryContexts[(unsigned int)EMemoryContextType::Framebuffers]->AllocateMemory(256 << 20, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	m_memoryContexts[(unsigned int)EMemoryContextType::Textures]->AllocateMemory(256 << 20, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	m_memoryContexts[(unsigned int)EMemoryContextType::UniformBuffers]->AllocateMemory(64 << 20, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	m_memoryContexts[(unsigned int)EMemoryContextType::IndirectDrawCmdBuffer]->AllocateMemory(2 << 20, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	m_memoryContexts[(unsigned int)EMemoryContextType::UI]->AllocateMemory(2 << 20, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
}

MemoryManager::~MemoryManager()
{
	for (unsigned int i = 0; i < (unsigned int)EMemoryContextType::Count; ++i)
	{
		m_memoryContexts[i]->FreeMemory();
		delete m_memoryContexts[i];
	}
}

BufferHandle* MemoryManager::CreateBuffer(EMemoryContextType context, VkDeviceSize size, VkBufferUsageFlags usage)
{
	MemoryContext* memContext = m_memoryContexts[(unsigned int)context];
	if (memContext->IsBufferMemory())
		return memContext->CreateBuffer(size, usage);
	
	TRAP(false && "Trying to create a buffer in an image memory context!");
	return nullptr;
}

BufferHandle* MemoryManager::CreateBuffer(EMemoryContextType context, std::vector<VkDeviceSize> sizes, VkBufferUsageFlags usage)
{
	return CreateBuffer(context, MemoryManager::ComputeTotalSize(sizes), usage);
}

VkDeviceSize MemoryManager::ComputeTotalSize(const std::vector<VkDeviceSize>& sizes)
{
	VkDeviceSize totalSize = 0;
	VkDeviceSize minAllign = vk::g_vulkanContext.m_limits.minUniformBufferOffsetAlignment; //this is not a good align. there are multiple types of buffers
	for (auto size : sizes)
		totalSize += (size / minAllign + 1) * minAllign;

	return totalSize;
}

ImageHandle* MemoryManager::CreateImage(EMemoryContextType context, const VkImageCreateInfo& imgInfo, const std::string& debugName)
{
	MemoryContext* memContext = m_memoryContexts[(unsigned int)context];
	if (!memContext->IsBufferMemory())
		return memContext->CreateImage(imgInfo, debugName);

	TRAP(false && "Trying to create an image in a buffer memory context!");
	return nullptr;
}

void MemoryManager::FreeHandle(Handle* handle)
{
	handle->GetMemoryContext()->FreeHandle(handle);
}

void MemoryManager::AllocMemory(EMemoryContextType context, VkDeviceSize size)
{
	m_memoryContexts[(unsigned int)context]->AllocateMemory(size, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT); //Let it be. for now its ok
}

void MemoryManager::FreeMemory(EMemoryContextType context)
{
	m_memoryContexts[(unsigned int)context]->FreeMemory();
}

bool MemoryManager::MapMemoryContext(EMemoryContextType context)
{
	MemoryContext* memContext = m_memoryContexts[(unsigned int)context];
	TRAP(memContext->CanMapMemory() && "Memory context cannot be mapped!!");
	if (!memContext->CanMapMemory()) //this is not mappable
		return false;

	return memContext->MapMemory();
}

void MemoryManager::UnmapMemoryContext(EMemoryContextType context)
{
	m_memoryContexts[(unsigned int)context]->UnmapMemory();
}

bool MemoryManager::IsMemoryMapped(EMemoryContextType context) const
{
	return m_memoryContexts[(unsigned int)context]->IsMapped();
}

