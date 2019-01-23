#include "TestRenderer.h"

#include "defines.h"
#include "MemoryManager.h"
#include "Mesh.h"
#include "Utils.h"

struct ShaderParams
{
	glm::vec4 TestVector;
	glm::mat4 Proj;
};

TestRenderer::TestRenderer(VkRenderPass renderPass)
	: CRenderer(renderPass, "TestRenderpass")
	, m_uniformBuffer(nullptr)
{}

TestRenderer::~TestRenderer()
{

}

void TestRenderer::Init()
{
	CRenderer::Init();

	m_quad = CreateFullscreenQuad();

	m_pipeline.SetVertexShaderFile("testlayered.vert");
	m_pipeline.SetGeometryShaderFile("testlayered.geom");
	m_pipeline.SetVertexInputState(Mesh::GetVertexDesc());
	m_pipeline.SetCullMode(VK_CULL_MODE_NONE);
	m_pipeline.AddPushConstant({ VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT, 0, sizeof(ShaderParams)});
	m_pipeline.CreatePipelineLayout(m_descLayout.Get());
	m_pipeline.Init(this, m_renderPass, 0);
}

void TestRenderer::Render()
{
	VkCommandBuffer cmd = vk::g_vulkanContext.m_mainCommandBuffer;
	glm::mat4 projMatrix;

	PerspectiveMatrix(projMatrix);
	ConvertToProjMatrix(projMatrix);

	StartRenderPass();

	ShaderParams params;
	cleanStructure(params);
	params.TestVector = glm::vec4(1.0f, 2.0, 3.0f, -1.0f);
	params.Proj = projMatrix;

	vk::CmdBindPipeline(cmd, m_pipeline.GetBindPoint(), m_pipeline.Get());
	vk::CmdPushConstants(cmd, m_pipeline.GetLayout(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT, 0, sizeof(ShaderParams), &params);

	m_quad->Render();
	EndRenderPass();
}

void TestRenderer::PreRender()
{

}

void TestRenderer::UpdateGraphicInterface()
{

}

void TestRenderer::CreateDescriptorSetLayout()
{
	//m_descLayout.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT);
	m_descLayout.Construct();
}

void TestRenderer::PopulatePoolInfo(std::vector<VkDescriptorPoolSize>& poolSize, unsigned int& maxSets)
{
	//AddDescriptorType(poolSize, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1);
	maxSets = 0;
}