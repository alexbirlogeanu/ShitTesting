#pragma once

#include "VulkanLoader.h"
#include "Renderer.h"
#include "DescriptorsUtils.h"
#include "glm/glm.hpp"

class BufferHandle;
class CTexture;
class Mesh;

class VegetationRenderer : public CRenderer
{
public:
	VegetationRenderer(VkRenderPass renderPass);
	virtual ~VegetationRenderer();

	void Init() override;
	void Render() override;
	void PreRender() override;
private:

	void CreateDescriptorSetLayout() override;
	void PopulatePoolInfo(std::vector<VkDescriptorPoolSize>& poolSize, unsigned int& maxSets) override;

	void UpdateGraphicInterface() override;

	void GenerateVegetation(); //TODO! use terrain as input
	void CreateBuffers();

	void UpdateTextures();
	void CopyBuffers();
private:
	struct PlantDescription
	{
		glm::vec4 Position; //world space
		glm::vec4 Properties; //x - width, y - height of the billboard, z - current angular speed of the billboard, comes from "simulation", texture index
		//probably indexes of the textured used
	};

	struct GlobalParams
	{
		glm::mat4 ProjViewMatrix;
		glm::vec4 CameraPosition;
		glm::vec4 LightDirection;
	} m_globals;

	CGraphicPipeline				m_renderPipeline;
	BufferHandle*					m_staggingBuffer;
	BufferHandle*					m_paramsBuffer;

	DescriptorSetLayout				m_renderDescSetLayout;
	VkDescriptorSet					m_renderDescSet;

	std::vector<PlantDescription>	m_plants;
	//
	std::vector<CTexture*>			m_albedoTextures;
	//std::vector<CTexture*>			m_normalsTextures;
	Mesh*							m_quad;

	const uint32_t					m_maxTextures;
	bool							m_isReady;
};
