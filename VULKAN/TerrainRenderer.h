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
	void LoadTextures();

	void SwitchToWireframe();
	void SwitchPipeline();
	void ChangeTesselationLevel(int units);
	void ChangeTesselationFactor(int units);
	void ChangePatchSize(int units);

	bool OnEditEnable(const KeyInput& key);

	bool OnMouseInput(const MouseInput& mouseInput);
private:
	CGraphicPipeline				m_tessellatedPipeline;
	CGraphicPipeline				m_simplePipeline;
	CGraphicPipeline*				m_activePipeline;

	BufferHandle*					m_terrainParamsBuffer;
	
	Mesh*							m_grid;
	std::vector<CTexture*>			m_terrainTextures;
	CTexture*						m_splatterTexture;

	DescriptorSetLayout				m_descriptorLayout;
	VkDescriptorSet					m_descSet;

	glm::vec4						m_tesselationParameters;
	glm::vec4						m_terrainPatchParameters; //use for texturing. Packs following data: xy - number of cells that are in a patch, zw - total number of cells that are in a terrain grid

	bool							m_drawWireframe;
	bool							m_editMode;
};
