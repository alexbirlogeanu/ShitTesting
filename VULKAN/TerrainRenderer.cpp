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
	glm::vec4 extra;
};

TerrainRenderer::TerrainRenderer(VkRenderPass renderPass)
	: CRenderer(renderPass, "TerrainRenderPass")
	, m_grid(nullptr)
	, m_descSet(VK_NULL_HANDLE)
	, m_terrainParamsBuffer(nullptr)
	, m_texture(nullptr)
	, m_heightMap(nullptr)
{

}

TerrainRenderer::~TerrainRenderer()
{
	VkDevice dev = vk::g_vulkanContext.m_device;

}

void TerrainRenderer::Init()
{
	CRenderer::Init();
	CreateGrid();

	m_texture = new CTexture("red.png");
	m_texture->SetSamplerFilter(VK_FILTER_NEAREST);
	ResourceLoader::GetInstance()->LoadTexture(&m_texture);

	m_heightMap = new CTexture("terrain.png");
	m_heightMap->SetIsSRGB(false);
	ResourceLoader::GetInstance()->LoadTexture(&m_heightMap);

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

	glm::mat4 modelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -1.0f, -3.0f));
	glm::mat4 projMatrix;
	PerspectiveMatrix(projMatrix);
	ConvertToProjMatrix(projMatrix);

	params->materialProp = glm::vec4(0.95, 0.05, 0.9, 0.0f);
	params->worldMatrix = modelMatrix;
	params->ViewProjMatrix = projMatrix * ms_camera.GetViewMatrix();

	params->extra = glm::vec4(m_xDisplacement, m_yDisplacement, m_heightmapDelta);
}

void TerrainRenderer::CreateDescriptorSetLayout()
{
	m_descriptorLayout.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT, 1);
	m_descriptorLayout.AddBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1);
	m_descriptorLayout.AddBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 1);

	m_descriptorLayout.Construct();
}

void TerrainRenderer::CreatePipeline()
{
	m_pipeline.SetVertexShaderFile("vert.spv");
	m_pipeline.SetFragmentShaderFile("frag.spv");
	m_pipeline.SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

	//m_pipeline.SetVertexShaderFile("terrain.vert");
	//m_pipeline.SetFragmentShaderFile("terrain.frag");
	//m_pipeline.SetTesselationControlShaderFile("tesselation.tesc");
	//m_pipeline.SetTesselationEvaluationShaderFile("tesselation.tese");
	//m_pipeline.SetTopology(VK_PRIMITIVE_TOPOLOGY_PATCH_LIST);
	//m_pipeline.SetGeometryShaderFile("terrain.geom");
	m_pipeline.AddBlendState(CGraphicPipeline::CreateDefaultBlendState(), 4);
	m_pipeline.SetPolygonMode(VK_POLYGON_MODE_FILL);
	m_pipeline.SetDepthTest(true);
	m_pipeline.SetTesselationPatchSize(3);
	m_pipeline.SetVertexInputState(Mesh::GetVertexDesc());
	m_pipeline.CreatePipelineLayout(m_descriptorLayout.Get());
	m_pipeline.Init(this, m_renderPass, 0);
}

void TerrainRenderer::CreateGrid()
{
	//m_grid = new Mesh("obj\\monkey.mb");
	SImageData heightMap;
	Read2DTextureData(heightMap, std::string(TEXTDIR) + "terrain.png", false);
	

	std::vector<SVertex> vertices;
	std::vector<uint32_t> indices;

	const uint32_t xDivision = 512;
	const uint32_t yDivision = 512;
	const float xLength = 20.0f;
	const float yLength = 20.0f;

	const float xStride = xLength / xDivision;
	const float yStride = yLength / yDivision;
	
	const float xDisplacement = xLength / 2.0f;
	const float yDisplacement = yLength / 2.0f;
	vertices.reserve((xDivision + 1) * (yDivision + 1));

	uint32_t imageDataStride = GetBytesFromFormat(heightMap.format);
	m_xDisplacement = xStride;
	m_yDisplacement = yStride;
	m_heightmapDelta = glm::vec2(1) / glm::vec2(xDivision, yDivision);

	for (uint32_t x = 0; x <= xDivision; ++x)
	{
		for (uint32_t y = 0; y <= yDivision; ++y)
		{
			glm::vec3 normal(0.0f, 1.0f, 0.0f);
			glm::vec2 uv(float(x) / float(xDivision), float(y) / float(yDivision));
			float height;
			{
				//glm::vec2 heightMapUV = uv * glm::vec2(heightMap.width, heightMap.height);
				//height = heightMap.data[imageDataStride * (uint32_t(heightMapUV.y) * heightMap.width + uint32_t(heightMapUV.x))];
				//height = height / 265.0f * 2.0f;
			}
			height = 0.0f;
			glm::vec3 pos(float(x) * xStride, height, float(y) * yStride);
			pos -= glm::vec3(xDisplacement, 1.0f, yDisplacement); //set the grid center in 0,0

			vertices.push_back(SVertex(pos, uv, normal));
		}
	}
	const uint32_t xVerts = xDivision + 1;
	indices.reserve(xDivision * yDivision * 6); //6 indices per square
	for (uint32_t x = 0; x < xDivision; ++x)
	{
		for (uint32_t y = 0; y < yDivision; ++y)
		{
			uint32_t tl = y * xVerts + x;
			uint32_t tr = y * xVerts + x + 1;
			uint32_t br = (y + 1) * xVerts + x + 1;
			uint32_t bl = (y + 1) * xVerts + x;

			indices.push_back(tl);
			indices.push_back(tr);
			indices.push_back(br);

			indices.push_back(br);
			indices.push_back(bl);
			indices.push_back(tl);
		}
	}

	m_grid = new Mesh(vertices, indices);
}

void TerrainRenderer::PopulatePoolInfo(std::vector<VkDescriptorPoolSize>& poolSize, unsigned int& maxSets)
{
	maxSets = 1;
	AddDescriptorType(poolSize, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1);
	AddDescriptorType(poolSize, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2);
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
	wDesc.push_back(InitUpdateDescriptor(m_descSet, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &m_heightMap->GetTextureDescriptor()));

	vk::UpdateDescriptorSets(vk::g_vulkanContext.m_device, (uint32_t)wDesc.size(), wDesc.data(), 0, nullptr);
}