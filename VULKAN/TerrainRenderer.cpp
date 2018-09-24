#include "TerrainRenderer.h"

#include "Mesh.h"
#include "defines.h"
#include "glm/glm.hpp"
#include "Texture.h"

struct TerrainParams
{
	glm::mat4 ViewProjMatrix;
	glm::mat4 worldMatrix;
	glm::vec4 materialProp; //x = roughness, y = k, z = F0
	glm::vec4 viewPos;
};

TerrainRenderer::TerrainRenderer(VkRenderPass renderPass)
	: CRenderer(renderPass, "TerrainRenderPass")
	, m_grid(nullptr)
	, m_descSet(VK_NULL_HANDLE)
	, m_terrainParamsBuffer(nullptr)
	, m_texture(nullptr)
{

}

TerrainRenderer::~TerrainRenderer()
{
	VkDevice dev = vk::g_vulkanContext.m_device;

}

void TerrainRenderer::Init()
{
	CRenderer::Init();
	m_grid = new Mesh("obj\\grid.mb");

	m_texture = new CTexture("red.png");
	ResourceLoader::GetInstance()->LoadTexture(&m_texture);

	m_terrainParamsBuffer = MemoryManager::GetInstance()->CreateBuffer(EMemoryContextType::UniformBuffers, sizeof(TerrainParams), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

	AllocDescriptorSets(m_descriptorPool, m_descriptorLayout.Get(), &m_descSet);

	CreatePipeline();
}

void TerrainRenderer::Render()
{
	VkCommandBuffer cmd = vk::g_vulkanContext.m_mainCommandBuffer;

	StartRenderPass();

	vk::CmdBindPipeline(cmd, m_pipeline.GetBindPoint(), m_pipeline.Get());
	vk::CmdBindDescriptorSets(cmd, m_pipeline.GetBindPoint(), m_pipeline.GetLayout(), 0, 1, &m_descSet, 0, nullptr);

	m_grid->Render();

	EndRenderPass();
}

void TerrainRenderer::PreRender()
{
	TerrainParams* params = m_terrainParamsBuffer->GetPtr<TerrainParams*>();

	glm::mat4 modelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -4.0f));
	glm::mat4 projMatrix;
	PerspectiveMatrix(projMatrix);
	ConvertToProjMatrix(projMatrix);

	params->materialProp = glm::vec4(0.95, 0.05, 0.9, 0.0f);
	params->worldMatrix = modelMatrix;
	params->ViewProjMatrix = projMatrix * ms_camera.GetViewMatrix();

}

void TerrainRenderer::CreateDescriptorSetLayout()
{
	m_descriptorLayout.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, 1);
	m_descriptorLayout.AddBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1);

	m_descriptorLayout.Construct();
}

void TerrainRenderer::CreatePipeline()
{
	m_pipeline.SetVertexShaderFile("vert.spv");
	m_pipeline.SetFragmentShaderFile("frag.spv");
	m_pipeline.SetTesselationControlShaderFile("tesselation.tesc");
	m_pipeline.SetTesselationEvaluationShaderFile("tesselation.tese");
	m_pipeline.AddBlendState(CGraphicPipeline::CreateDefaultBlendState(), 4);
	m_pipeline.SetPolygonMode(VK_POLYGON_MODE_LINE);
	m_pipeline.SetDepthTest(true);
	m_pipeline.SetTopology(VK_PRIMITIVE_TOPOLOGY_PATCH_LIST);
	m_pipeline.SetTesselationPatchSize(3);
	m_pipeline.SetVertexInputState(Mesh::GetVertexDesc());
	m_pipeline.CreatePipelineLayout(m_descriptorLayout.Get());
	m_pipeline.Init(this, m_renderPass, 0);
}

void TerrainRenderer::PopulatePoolInfo(std::vector<VkDescriptorPoolSize>& poolSize, unsigned int& maxSets)
{
	maxSets = 1;
	AddDescriptorType(poolSize, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1);
	AddDescriptorType(poolSize, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1);
}

void TerrainRenderer::UpdateResourceTable()
{

}

void TerrainRenderer::UpdateGraphicInterface()
{
	std::vector<VkWriteDescriptorSet> wDesc;

	VkDescriptorBufferInfo buffInfo = m_terrainParamsBuffer->GetDescriptor();
	VkDescriptorImageInfo textInfo = m_texture->GetTextureDescriptor();

	wDesc.push_back(InitUpdateDescriptor(m_descSet, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &buffInfo));
	wDesc.push_back(InitUpdateDescriptor(m_descSet, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &textInfo));

	vk::UpdateDescriptorSets(vk::g_vulkanContext.m_device, (uint32_t)wDesc.size(), wDesc.data(), 0, nullptr);
}