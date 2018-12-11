#pragma once

#include "VulkanLoader.h"
#include "Renderer.h"
#include "DescriptorsUtils.h"
#include "glm/glm.hpp"

class BufferHandle;
class CTexture;
class Mesh;
class KeyInput;
class MouseInput;
class CUIText;

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

	void GenerateVegetation();
	void CreateBuffers();

	void UpdateTextures();
	void CopyBuffers();

	void WindVariation();

	bool OnDebugKey(const KeyInput& key);
	bool OnDebugWindVelocityChange(const MouseInput& mouse);
private:
	struct PlantDescription
	{
		glm::vec4 Position; //world space
		glm::vec4 Properties; //x - width, y - height of the billboard, z - bend factor used in wind simulation, comes from "simulation", texture index
	};

	struct GlobalParams
	{
		glm::mat4 ProjViewMatrix;
		glm::vec4 CameraPosition;
		glm::vec4 LightDirection;
		glm::vec4 WindVelocity;
	} m_globals;

	CGraphicPipeline				m_renderPipeline;
	BufferHandle*					m_staggingBuffer;
	BufferHandle*					m_paramsBuffer;

	DescriptorSetLayout				m_renderDescSetLayout;
	VkDescriptorSet					m_renderDescSet;
	
	std::vector<PlantDescription>	m_plants;
	glm::vec4						m_pushConstant;
	std::vector<CTexture*>			m_albedoTextures;

	Mesh*							m_quad;

	const uint32_t					m_maxTextures;
	bool							m_isReady;
	float							m_elapsedTime;

	float							m_windStrength;
	float							m_angularSpeed;
	glm::vec2						m_windAngleLimits; //in degrees

	//debug
	bool							m_isDebugMode;
	CUIText*						m_debugText;
};
