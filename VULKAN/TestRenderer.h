#pragma once

#include "DescriptorsUtils.h"
#include "Renderer.h"
#include "VulkanLoader.h"

class Mesh;
class BufferHandle;
class TestRenderer : public CRenderer
{
public:
	TestRenderer(VkRenderPass renderPass);
	virtual ~TestRenderer();

	void Init() override;
	void Render() override;
	void PreRender() override;
protected:
	void UpdateGraphicInterface() override;
	void CreateDescriptorSetLayout() override;
	void PopulatePoolInfo(std::vector<VkDescriptorPoolSize>& poolSize, unsigned int& maxSets) override;
private:
	Mesh*					m_quad;
	CGraphicPipeline		m_pipeline;
	BufferHandle*			m_uniformBuffer; //currently not used. I use push constants for that matter
	DescriptorSetLayout		m_descLayout;

};