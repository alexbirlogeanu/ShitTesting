#pragma once

#include "Renderer.h"
#include "VulkanLoader.h"
#include "DescriptorsUtils.h"

class Mesh;
class BufferHandle;
class KeyInput;
class MouseInput;

class TerrainRenderer : public Renderer
{
public:
	TerrainRenderer();
	virtual ~TerrainRenderer();

	void Setup(VkRenderPass renderPass, uint32_t subpassId);
	void Render();
	
	void SetupShadows(VkRenderPass renderPass, uint32_t subpassId);
	void RenderShadows();

	void PreRender();
	
protected:
	void InitInternal() override;

	void CreateDescriptorSetLayouts() override;
	void UpdateGraphicInterface() override;
	void AllocateDescriptorSets() override;

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
	struct ShadowPushConstants
	{
		glm::mat4 ShadowProjViewMatrix;
		glm::mat4 ViewMatrix;
		glm::mat4 ModelMatrix;
	};

	CGraphicPipeline				m_tessellatedPipeline;
	CGraphicPipeline				m_simplePipeline;
	CGraphicPipeline*				m_activePipeline;
	CGraphicPipeline				m_shadowPipeline;

	BufferHandle*					m_terrainParamsBuffer;
	BufferHandle*					m_shadowSplitsBuffer;

	Mesh*							m_grid;
	std::vector<CTexture*>			m_terrainTextures;
	CTexture*						m_splatterTexture;

	DescriptorSetLayout				m_descriptorLayout;
	DescriptorSetLayout				m_shadowDescLayout;

	VkDescriptorSet					m_descSet;
	VkDescriptorSet					m_shadowDescSet;

	glm::vec4						m_tesselationParameters;
	glm::vec4						m_terrainPatchParameters; //use for texturing. Packs following data: xy - number of cells that are in a patch, zw - total number of cells that are in a terrain grid
	ShadowPushConstants				m_pushConstants;

	bool							m_drawWireframe;
	bool							m_editMode;
};
