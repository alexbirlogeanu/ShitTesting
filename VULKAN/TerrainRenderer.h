#pragma once

#include "Renderer.h"
#include "VulkanLoader.h"
#include "DescriptorsUtils.h"

class Mesh;
class BufferHandle;
class KeyInput;

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
	void CreateGrid();

	bool OnSwitchToWireframe(const KeyInput& key);
private:
	CGraphicPipeline				m_pipeline;
	BufferHandle*					m_terrainParamsBuffer;
	
	Mesh*							m_grid;
	CTexture*						m_texture;
	CTexture*						m_heightMap;

	DescriptorSetLayout				m_descriptorLayout;
	VkDescriptorSet					m_descSet;

	//temporary
	float							m_xDisplacement;
	float							m_yDisplacement;
	glm::vec2						m_heightmapDelta;

	bool							m_drawWireframe;
};