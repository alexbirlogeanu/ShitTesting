#pragma once

#include "Renderer.h"
#include "VulkanLoader.h"
#include "DescriptorsUtils.h"

class Mesh;
class BufferHandle;
class TerrainRenderer : public CRenderer
{
public:
	TerrainRenderer(VkRenderPass renderPass);
	virtual ~TerrainRenderer();

	virtual void Init() override;
	virtual void Render() override;
	virtual void PreRender() override;
private:
	virtual void CreateDescriptorSetLayout() override;
	virtual void PopulatePoolInfo(std::vector<VkDescriptorPoolSize>& poolSize, unsigned int& maxSets) override;

	virtual void UpdateResourceTable() override;
	virtual void UpdateGraphicInterface() override;

	void CreatePipeline();
private:
	CGraphicPipeline				m_pipeline;
	BufferHandle*					m_terrainParamsBuffer;
	//Test tesselation part
	Mesh*							m_grid;
	CTexture*						m_texture;

	DescriptorSetLayout				m_descriptorLayout;
	VkDescriptorSet					m_descSet;
};