#pragma once
#include "VulkanLoader.h"

#include "Singleton.h"
#include "defines.h"

#include <vector>
#include <set>
#include <utility>
#include <array>
#include <algorithm>
#include <unordered_map>
#include <string>

enum class EMemoryContextType
{
	StagginBuffer, //used for stagging. After a copy from stage -> device is complete the memory should be freed by application
	DeviceLocalBuffer,
	MeshStaggingBuffer,
	Framebuffers, //device local memory
	Textures, //device local memory
	StaggingTextures, //device local for stagging textures
	UniformBuffers,
	IndirectDrawCmdBuffer,
	BatchStaggingBuffer,
	UI,
	Count
};

class MemoryContext;
class Handle
{
	friend class MemoryContext;
public:
	VkDeviceSize GetSize() const { return m_size; }
	VkDeviceSize GetOffset() const { return m_offset; }
	Handle* GetRootParent() { return ((m_parent)? m_parent->GetRootParent() : this); }
	MemoryContext* GetMemoryContext() { return m_memoryContext; }

	template<class RetType>
	RetType GetPtr();

protected:
	Handle(VkDeviceSize size, VkDeviceSize alignment, MemoryContext* context)
		: m_size(size)
		, m_alignment(alignment)
		, m_offset(0)
		, m_parent(nullptr)
		, m_memoryContext(context)
	{
	}

	Handle(Handle* parrent, VkDeviceSize size, VkDeviceSize alignment, VkDeviceSize offset)
		: m_size(size)
		, m_alignment(alignment)
		, m_offset(offset)
		, m_parent(parrent)
		, m_memoryContext(parrent->m_memoryContext)
	{

	}
	virtual ~Handle()
	{
	}

	virtual void FreeResources()=0;
protected:
	VkDeviceSize				m_size;
	VkDeviceSize				m_offset;
	VkDeviceSize				m_alignment; //if alignment is 0, then is not considered when suballocate
	Handle*						m_parent;

	MemoryContext*				m_memoryContext;
};

template<class VkType>
class HandleImpl : public Handle
{
public:
	const VkType& Get() const { return m_vkHandle; }

protected:
	HandleImpl(VkType handle, VkDeviceSize size, VkDeviceSize alignment, MemoryContext* context)
		: Handle(size, alignment, context)
		, m_vkHandle(handle)
	{

	}
	HandleImpl(HandleImpl<VkType>* parent, VkDeviceSize size, VkDeviceSize alignment, VkDeviceSize offset)
		: Handle(parent, size, alignment, offset)
		, m_vkHandle(parent->Get())
	{

	}
	virtual ~HandleImpl()
	{
		for (unsigned int i = 0; i < m_subDivisions.size(); ++i)
			delete m_subDivisions[i];
	}	

	template<class HandleType>
	HandleType* SubAllocate(HandleType* parent, VkDeviceSize size)
	{
		VkDeviceSize offset = m_freeOffset;
		if ((m_alignment != 0) && (m_freeOffset % m_alignment != 0))
			offset += m_alignment - (m_freeOffset % m_alignment);

		m_freeOffset = offset + size;

		VkDeviceSize absoluteOffest = (m_parent) ? m_parent->GetOffset() : 0; //probably i will forget why is this way ?? I think is getRootParent offset, not parrent
		TRAP(offset + size <= m_size && "Not enough memory for this sub Allocation");
		HandleType* hSubAlloc = new HandleType(parent, size, m_alignment, absoluteOffest + offset);

		m_subDivisions.push_back(hSubAlloc);
		return hSubAlloc;
	}

protected:
	//used for creating subbuffers
	std::vector<HandleImpl<VkType>*>			m_subDivisions;
	VkDeviceSize								m_freeOffset;

	VkType										m_vkHandle;
};

class BufferHandle : public HandleImpl<VkBuffer>
{
	friend class MemoryContext;
	friend class HandleImpl<VkBuffer>;
public:
	BufferHandle* CreateSubbuffer(VkDeviceSize size);
	VkBufferMemoryBarrier CreateMemoryBarrier(VkAccessFlags srcAccess, VkAccessFlags dstAccess);
	VkDescriptorBufferInfo GetDescriptor() const;
protected:
	virtual void FreeResources() override;
private:
	BufferHandle(VkBuffer handle, VkDeviceSize size, VkDeviceSize alignment, MemoryContext* context);
	BufferHandle(BufferHandle* parent, VkDeviceSize size, VkDeviceSize alignment, VkDeviceSize offset);
	virtual ~BufferHandle(){}

	BufferHandle(const BufferHandle&);
	BufferHandle& operator=(const BufferHandle&);
};

class ImageHandle : public HandleImpl<VkImage> //when implementing mips, i think i need a VkImageView object ?? why 
{
	friend class MemoryContext;
	friend class HandleImpl<VkImage>;
public:
	//a method to suballocate ?? 
	VkImageMemoryBarrier CreateMemoryBarrier(VkImageLayout oldLayout, VkImageLayout newLayout, VkAccessFlags srcMask, VkAccessFlags dstMask, VkImageAspectFlags aspectFlags, unsigned int layersCount = VK_REMAINING_ARRAY_LAYERS);
	VkImageMemoryBarrier CreateMemoryBarrierForMips(uint32_t mipLevel, VkImageLayout oldLayout, VkImageLayout newLayout, VkAccessFlags srcMask, VkAccessFlags dstMask, VkImageAspectFlags aspectFlags, uint32_t mipCount = 1);

	const VkImageView& GetView() const { return m_view; };
	const VkImageView& GetLayerView(uint32_t layer) const;
	VkFormat GetFormat() const { return m_format; }
	const VkExtent3D GetDimensions() const { return m_dimensions; }
protected:
	ImageHandle(VkImage vkHandle, VkDeviceSize size, VkDeviceSize alignment, const VkImageCreateInfo& imgInfo, MemoryContext* context);
	ImageHandle(ImageHandle* parent, VkDeviceSize size, VkDeviceSize alignment, VkDeviceSize offset); //dont implement yet
	ImageHandle(const ImageHandle&);
	ImageHandle& operator=(const ImageHandle);

	virtual ~ImageHandle();

	virtual void FreeResources() override;
protected:
	VkImageView						m_view; //image view associated with de img
	std::vector<VkImageView>		m_layersViews; //if a img is multi layers, we create a view for every layer.
	VkFormat						m_format;
	uint32_t						m_layers;
	VkExtent3D						m_dimensions;
};

class MappedMemory
{
	friend class MemoryContext;
public:
	template<class T>
	T GetPtr(Handle* handle);


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

	void AllocateMemory(VkDeviceSize size, VkMemoryPropertyFlags flags);
	void FreeMemory();

	BufferHandle* CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage);
	ImageHandle* CreateImage(const VkImageCreateInfo& crtInfo, const std::string& debugName = std::string());

	void FreeHandle(Handle* handle);

	bool CanMapMemory();
	bool MapMemory();
	void UnmapMemory();
	bool IsBufferMemory() const;

	bool IsMapped() const { return m_mappedMemory != nullptr; }
	MappedMemory* GetMappedMemory() const { return (IsMapped()) ? m_mappedMemory : nullptr; }
private:
	struct Chunk
	{
		Chunk() : m_offset(0), m_size(0){}
		Chunk(VkDeviceSize offset, VkDeviceSize size) : m_offset(offset), m_size(size) {}

		bool operator< (const Chunk& other) const { return m_offset < other.m_offset; }
		bool operator!=(const Chunk& other) const { return m_offset != other.m_offset || m_size != other.m_size; }

		VkDeviceSize	m_offset;
		VkDeviceSize	m_size;
	};

	Chunk GetFreeChunk(VkDeviceSize size, VkDeviceSize alignment);
	void FreeChunk(Chunk chunk);
private:
	std::unordered_map<Handle*, Chunk>					m_allocatedChunks;
	std::set<Chunk>										m_chunks;

	VkDeviceMemory										m_memory;
	VkDeviceSize										m_totalSize;
	VkDeviceSize										m_allocatedSize;
	uint32_t											m_memoryTypeIndex;
	MappedMemory*										m_mappedMemory;
	VkMemoryPropertyFlags								m_memoryFlags;

	EMemoryContextType									m_contextType;
};

template<class T>
T MappedMemory::GetPtr(Handle* handle)
{
	//change this function is shit
	auto& allocatedChunks = m_parentMemoryContext->m_allocatedChunks;
	auto it = allocatedChunks.find(handle->GetRootParent());
	if (it == allocatedChunks.end())
	{
		TRAP(false && "This Handle is not from this memory context");
		return nullptr;
	}

	VkDeviceSize relativeOffset = handle->GetOffset(); //handle can be a child sub allocated from a bigger resource
	const MemoryContext::Chunk& handleAllocatedChunk = it->second;
	VkDeviceSize absoluteOffset = handleAllocatedChunk.m_offset;
	uint8_t* bufferMemPtr = (uint8_t*)m_memPtr + absoluteOffset + relativeOffset;
	return (T)bufferMemPtr;
}

template<class RetType>
RetType Handle::GetPtr()
{
	MappedMemory* mapMem = m_memoryContext->GetMappedMemory();
	TRAP(mapMem && "This memory for this handle is not mapped!!");
	return mapMem->GetPtr<RetType>(this);
}

class MemoryManager : public Singleton<MemoryManager>
{
	friend class Singleton<MemoryManager>;
public:
	//use this function if dont want to suballocate
	BufferHandle* CreateBuffer(EMemoryContextType context, VkDeviceSize size, VkBufferUsageFlags usage);
	//use this function when you want to suballocate. This method will calculate a total size for you
	BufferHandle* CreateBuffer(EMemoryContextType context, std::vector<VkDeviceSize> sizes, VkBufferUsageFlags usage);
	ImageHandle* CreateImage(EMemoryContextType context, const VkImageCreateInfo& imgInfo, const std::string& debugName = std::string());

	void FreeHandle(Handle* handle);

	void AllocMemory(EMemoryContextType type, VkDeviceSize size);
	void FreeMemory(EMemoryContextType type);

	bool MapMemoryContext(EMemoryContextType context); //TODO change to return the new mapped memory
	void UnmapMemoryContext(EMemoryContextType context);
	bool IsMemoryMapped(EMemoryContextType context) const;

	static VkDeviceSize ComputeTotalSize(const std::vector<VkDeviceSize>& sizes);
protected:
	MemoryManager();
	virtual ~MemoryManager();

	MemoryManager(const MemoryManager&);
	MemoryManager& operator= (const MemoryManager&);

private:
	std::array<MemoryContext*, (size_t)EMemoryContextType::Count>	m_memoryContexts;
};

