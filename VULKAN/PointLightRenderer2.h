#pragma once

#include "Renderer.h"
#include "glm/glm.hpp"

#include <vector>

struct PointLightParams;
class PointLight
{
public:
	PointLight(glm::vec3 position);

	void Fill(PointLightParams& params, const glm::mat4& view) const;

private:
	float		m_intensity;
	glm::vec3	m_radiance;
	glm::vec3	m_position; //world space
	float		m_radius;
	glm::vec3	m_attenuationConsts;
};

class PointLightRenderer2 : public CRenderer
{
public:
	PointLightRenderer2(VkRenderPass renderPass);
	virtual ~PointLightRenderer2();

	virtual void Init(); //this is an anti pattern. Fix it
	virtual void Render();

	void InitializeLightGrid(); //this method is for testing. then you should create a way to add point lights to the renderer
protected:
	virtual void CreateDescriptorSetLayout();
	virtual void PopulatePoolInfo(std::vector<VkDescriptorPoolSize>& poolSize, unsigned int& maxSets);

	virtual void UpdateResourceTable();
	virtual void UpdateGraphicInterface();

private:
	void UpdateLightsBuffer();
	void ReallocLightsBuffer();
	
	void CreateLightImage(VkImage& img, VkImageView& view, VkDeviceMemory& memory);
	void PrepareLightImage();

private:
	VkBuffer					m_lightsBuffer;
	VkDeviceMemory				m_lightsMemory;

	VkImage						m_lightImage;
	VkImageView					m_lightImageView;
	VkDeviceMemory				m_lightImageMemory;

	VkImage						m_debugImage; //free this objects  
	VkImageView					m_debugImageView;
	VkDeviceMemory				m_debugImageMemory;

	VkSampler					Sampler;

	CComputePipeline			m_tileShadingPipeline;
	CGraphicPipeline			m_resolvePipeline;

	VkDescriptorSetLayout		m_tileShadingDescLayout;
	VkDescriptorSetLayout		m_resolveDescLayout;

	VkDescriptorSet				m_tileShadingDescSet;
	VkDescriptorSet				m_resolveDescSet;
	Mesh*						m_fullscreenQuad;

	std::vector<PointLight>		m_lights;
	unsigned int				m_allocatedLights;
	glm::mat4					m_proj;
};
