#pragma once

#include "Renderer.h"
#include "VulkanLoader.h"
#include "DescriptorsUtils.h"

class Mesh;
class BufferHandle;
class KeyInput;
class MouseInput;

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

	void SwitchToWireframe();
	void SwitchPipeline();
	void ChangeTesselationLevel(int units);
	void ChangeTesselationFactor(int units);

	bool OnEditEnable(const KeyInput& key);

	bool OnMouseInput(const MouseInput& mouseInput);
private:
	CGraphicPipeline				m_tessellatedPipeline;
	CGraphicPipeline				m_simplePipeline;
	CGraphicPipeline*				m_activePipeline;

	BufferHandle*					m_terrainParamsBuffer;
	
	Mesh*							m_grid;
	CTexture*						m_texture;
	CTexture*						m_heightMap;

	DescriptorSetLayout				m_descriptorLayout;
	VkDescriptorSet					m_descSet;

	glm::vec4						m_tesselationParameters;

	bool							m_drawWireframe;
	bool							m_editMode;
};