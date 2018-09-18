#include "DescriptorsUtils.h"

#include "defines.h"
#include "Utils.h"

void NewDescriptorPool(const std::vector<VkDescriptorPoolSize>& poolSize, uint32_t maxSets, VkDescriptorPool* descPool)
{
	VkDescriptorPoolCreateInfo descPoolCi;
	cleanStructure(descPoolCi);
	descPoolCi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descPoolCi.pNext = nullptr;
	descPoolCi.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	descPoolCi.maxSets = maxSets;
	descPoolCi.poolSizeCount = (uint32_t)poolSize.size();
	descPoolCi.pPoolSizes = poolSize.data();

	VULKAN_ASSERT(vk::CreateDescriptorPool(vk::g_vulkanContext.m_device, &descPoolCi, nullptr, descPool));
}

///////////////////////////////////////////////////////////////////////////////////////
//DescriptorSetLayout
///////////////////////////////////////////////////////////////////////////////////////

DescriptorSetLayout::DescriptorSetLayout()
	:m_descSetLayoutHandle(VK_NULL_HANDLE)
{

}

DescriptorSetLayout::~DescriptorSetLayout()
{
	vk::DestroyDescriptorSetLayout(vk::g_vulkanContext.m_device, m_descSetLayoutHandle, nullptr);
}

void DescriptorSetLayout::AddBinding(unsigned int binding, VkDescriptorType type, VkShaderStageFlags flags, unsigned int count)
{
	TRAP(m_descSetLayoutHandle == VK_NULL_HANDLE && "Warning!! Layout is already created, this change will have no effect!");
	m_bindings.push_back(CreateDescriptorBinding(binding, type, flags, count));
}

void DescriptorSetLayout::Construct()
{
	TRAP(m_descSetLayoutHandle == VK_NULL_HANDLE && "Warning!! Layout is already created, this change will have no effect!");
	if (m_descSetLayoutHandle != VK_NULL_HANDLE)
		return;

	NewDescriptorSetLayout(m_bindings, &m_descSetLayoutHandle);
}

///////////////////////////////////////////////////////////////////////////////////////
//DescriptorSetLayout
///////////////////////////////////////////////////////////////////////////////////////

DescriptorPool::DescriptorPool()
	: m_descPoolHandle(VK_NULL_HANDLE)
	, m_remainingSets(0)
{
	for (uint32_t i = 0; i < m_descriptorPoolSizeRemaining.size(); ++i)
		m_descriptorPoolSizeRemaining[i].descriptorCount = 0;
}

DescriptorPool::~DescriptorPool()
{
	vk::DestroyDescriptorPool(vk::g_vulkanContext.m_device, m_descPoolHandle, nullptr);
}

void DescriptorPool::Construct(const DescriptorSetLayout& layoutType, uint32_t maxSets)
{
	std::vector<VkDescriptorSetLayoutBinding> bindings = layoutType.GetBindings();

	for (auto binding : bindings)
	{
		m_descriptorPoolSizeRemaining[binding.descriptorType].descriptorCount += binding.descriptorCount;
		m_descriptorPoolSizeRemaining[binding.descriptorType].type = binding.descriptorType;
	}

	std::vector<VkDescriptorPoolSize> poolSize;

	for (auto& pSize : m_descriptorPoolSizeRemaining)
		if (pSize.descriptorCount != 0)
		{
			pSize.descriptorCount *= maxSets;
			poolSize.push_back(pSize);
		}

	m_remainingSets = maxSets;

	NewDescriptorPool(poolSize, m_remainingSets, &m_descPoolHandle);
}

void DescriptorPool::Construct(const std::vector<DescriptorSetLayout>& layouts, uint32_t maxSets)
{
	for (const auto& layout : layouts)
	{
		std::vector<VkDescriptorSetLayoutBinding> bindings = layout.GetBindings();

		for (auto binding : bindings)
		{
			m_descriptorPoolSizeRemaining[binding.descriptorType].descriptorCount += binding.descriptorCount;
			m_descriptorPoolSizeRemaining[binding.descriptorType].type = binding.descriptorType;
		}
	}

	std::vector<VkDescriptorPoolSize> poolSize;

	for (auto& pSize : m_descriptorPoolSizeRemaining)
		if (pSize.descriptorCount != 0)
		{
			pSize.descriptorCount *= maxSets;
			poolSize.push_back(pSize);
		}

	m_remainingSets = maxSets * layouts.size();

	NewDescriptorPool(poolSize, m_remainingSets, &m_descPoolHandle);
}


void DescriptorPool::Construct(const std::vector<VkDescriptorPoolSize>& poolSize, uint32_t maxSets)
{
	NewDescriptorPool(poolSize, maxSets, &m_descPoolHandle);
}

bool DescriptorPool::CanAllocate(const DescriptorSetLayout& layoutType)
{
	std::vector<VkDescriptorSetLayoutBinding> bindings = layoutType.GetBindings();
	for (auto binding : bindings)
	{
		if (m_descriptorPoolSizeRemaining[binding.descriptorType].descriptorCount - binding.descriptorCount < 0)
			return false;
	}

	return m_remainingSets > 0;
}

bool DescriptorPool::CanAllocate(const std::vector<DescriptorSetLayout>& layoutsType)
{
	for (auto layout : layoutsType)
		if (!CanAllocate(layout))
			return false;

	return (m_remainingSets - layoutsType.size()) > 0;
}

VkDescriptorSet DescriptorPool::AllocateDescriptorSet(const DescriptorSetLayout& layoutType)
{
	VkDescriptorSet newDescSet;
	AllocDescriptorSets(m_descPoolHandle, layoutType.Get(), &newDescSet);
	--m_remainingSets;
	return newDescSet;
}

std::vector<VkDescriptorSet> DescriptorPool::AllocateDescriptorSet(const std::vector<DescriptorSetLayout>& layoutsTypes)
{
	std::vector<VkDescriptorSetLayout> vkLayouts;
	vkLayouts.reserve(layoutsTypes.size());
	for (auto layout : layoutsTypes)
		vkLayouts.push_back(layout.Get());

	std::vector<VkDescriptorSet> newDescSets;
	newDescSets.resize(vkLayouts.size());
	for (unsigned int i = 0; i < layoutsTypes.size(); ++i)
	{
		newDescSets[i] = AllocateDescriptorSet(layoutsTypes[i]);
	}
	return newDescSets;
}
