#pragma once

#include "VulkanLoader.h"

#include <vector>
#include <array>

class DescriptorSetLayout
{
public:
	DescriptorSetLayout();
	~DescriptorSetLayout();

	const VkDescriptorSetLayout& Get() const { return m_descSetLayoutHandle; }
	VkDescriptorSetLayout& Get() { return m_descSetLayoutHandle; }

	const std::vector<VkDescriptorSetLayoutBinding>& GetBindings() const { return m_bindings; }

	//Construct
	void AddBinding(unsigned int binding, VkDescriptorType type, VkShaderStageFlags flags, unsigned int count = 1);
	void Construct();

	bool IsValid() const { return m_descSetLayoutHandle != VK_NULL_HANDLE; }
private:
	std::vector<VkDescriptorSetLayoutBinding>	m_bindings;
	VkDescriptorSetLayout						m_descSetLayoutHandle;
};

class DescriptorPool
{
public:
	DescriptorPool();
	virtual ~DescriptorPool();

	const VkDescriptorPool& Get() const { m_descPoolHandle; }

	void Construct(const DescriptorSetLayout& layoutType, uint32_t maxSets);
	void Construct(const std::vector<DescriptorSetLayout>& layouts, uint32_t maxSets);
	void Construct(const std::vector<VkDescriptorPoolSize>& poolSize, uint32_t maxSets); //backwards compatibility
	
	//need methods to check for multiples sets
	bool CanAllocate(const DescriptorSetLayout& layoutType);
	bool CanAllocate(const std::vector<DescriptorSetLayout>& layoutsType);
	VkDescriptorSet AllocateDescriptorSet(const DescriptorSetLayout& layoutType);
	std::vector<VkDescriptorSet> AllocateDescriptorSet(const std::vector<DescriptorSetLayout>& layoutsTypes);
private:
	VkDescriptorPool																	m_descPoolHandle;
	std::array<VkDescriptorPoolSize, VkDescriptorType::VK_DESCRIPTOR_TYPE_RANGE_SIZE>	m_descriptorPoolSizeRemaining;
	uint32_t																			m_remainingSets;
};
