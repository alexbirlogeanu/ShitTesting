#include "TerrainRenderer.h"

#include "Mesh.h"
#include "defines.h"
#include "glm/glm.hpp"
#include "Texture.h"

#include "Input.h"

struct TerrainParams
{
	glm::mat4 ViewProjMatrix;
	glm::mat4 worldMatrix;
	glm::vec4 materialProp; //x = roughness, y = k, z = F0
	glm::vec4 tesselationParams; //x - outter, y - inner tessellation, z - tessellation factor
};

TerrainRenderer::TerrainRenderer(VkRenderPass renderPass)
	: CRenderer(renderPass, "TerrainRenderPass")
	, m_grid(nullptr)
	, m_descSet(VK_NULL_HANDLE)
	, m_terrainParamsBuffer(nullptr)
	, m_texture(nullptr)
	, m_heightMap(nullptr)
	, m_activePipeline(nullptr)
	, m_editMode(false)
	, m_tesselationParameters(13.0f, 7.0f, 0.8f, 0.0f)
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

	m_heightMap = new CTexture("terrain3.png");
	m_heightMap->SetIsSRGB(false);
	ResourceLoader::GetInstance()->LoadTexture(&m_heightMap);

	m_terrainParamsBuffer = MemoryManager::GetInstance()->CreateBuffer(EMemoryContextType::UniformBuffers, sizeof(TerrainParams), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

	AllocDescriptorSets(m_descriptorPool, m_descriptorLayout.Get(), &m_descSet);

	CreatePipeline();

	InputManager::GetInstance()->MapMouseButton(InputManager::MouseButtonsCallback(this, &TerrainRenderer::OnMouseInput));
	InputManager::GetInstance()->MapKeyPressed('2', InputManager::KeyPressedCallback(this, &TerrainRenderer::OnEditEnable));

}

void TerrainRenderer::Render()
{
	VkCommandBuffer cmd = vk::g_vulkanContext.m_mainCommandBuffer;

	StartRenderPass();

	vk::CmdBindPipeline(cmd, m_activePipeline->GetBindPoint(), m_activePipeline->Get());
	vk::CmdBindDescriptorSets(cmd, m_activePipeline->GetBindPoint(), m_activePipeline->GetLayout(), 0, 1, &m_descSet, 0, nullptr);

	m_grid->Render();

	EndRenderPass();
}

void TerrainRenderer::PreRender()
{
	TerrainParams* params = m_terrainParamsBuffer->GetPtr<TerrainParams*>();

	glm::mat4 modelMatrix = glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -5.0f, -3.0f)), glm::vec3(1.0f));
	glm::mat4 projMatrix;
	PerspectiveMatrix(projMatrix);
	ConvertToProjMatrix(projMatrix);

	params->materialProp = glm::vec4(0.90, 0.1, 0.5, 0.0f);
	params->worldMatrix = modelMatrix;
	params->ViewProjMatrix = projMatrix * ms_camera.GetViewMatrix();

	params->tesselationParams = m_tesselationParameters;
}

void TerrainRenderer::CreateDescriptorSetLayout()
{
	m_descriptorLayout.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, 1);
	m_descriptorLayout.AddBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1);
	m_descriptorLayout.AddBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 1);

	m_descriptorLayout.Construct();
}

void TerrainRenderer::CreatePipeline()
{
	m_tessellatedPipeline.SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	m_tessellatedPipeline.SetVertexShaderFile("terrain.vert");
	m_tessellatedPipeline.SetFragmentShaderFile("terrain.frag");
	m_tessellatedPipeline.SetTesselationControlShaderFile("tesselation.tesc");
	m_tessellatedPipeline.SetTesselationEvaluationShaderFile("tesselation.tese");
	m_tessellatedPipeline.SetTopology(VK_PRIMITIVE_TOPOLOGY_PATCH_LIST);
	m_tessellatedPipeline.SetCullMode(VK_CULL_MODE_BACK_BIT);
	m_tessellatedPipeline.AddBlendState(CGraphicPipeline::CreateDefaultBlendState(), 4);
	m_tessellatedPipeline.SetWireframeSupport(true);
	m_tessellatedPipeline.SetDepthTest(true);
	m_tessellatedPipeline.SetTesselationPatchSize(3);
	m_tessellatedPipeline.SetVertexInputState(Mesh::GetVertexDesc());
	m_tessellatedPipeline.CreatePipelineLayout(m_descriptorLayout.Get());
	m_tessellatedPipeline.Init(this, m_renderPass, 0);


	//add support to create a derivative pipeline from another
	m_simplePipeline.SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	m_simplePipeline.SetVertexShaderFile("terrain_debug.vert");
	m_simplePipeline.SetFragmentShaderFile("terrain.frag");
	m_simplePipeline.SetCullMode(VK_CULL_MODE_BACK_BIT);
	m_simplePipeline.AddBlendState(CGraphicPipeline::CreateDefaultBlendState(), 4);
	m_simplePipeline.SetWireframeSupport(true);
	m_simplePipeline.SetDepthTest(true);
	m_simplePipeline.SetVertexInputState(Mesh::GetVertexDesc());
	m_simplePipeline.CreatePipelineLayout(m_descriptorLayout.Get());
	m_simplePipeline.Init(this, m_renderPass, 0);

	m_activePipeline = &m_tessellatedPipeline;
}



//shit function
void TerrainRenderer::CreateGrid()
{
	//m_grid = new Mesh("obj\\trig.mb");
	//m_grid = new Mesh("obj\\monkey.mb");
	//return;
	SImageData heightMap;
	Read2DTextureData(heightMap, std::string(TEXTDIR) + "terrain3.png", false);
	
	const float heightMax = 7.0f;
	std::vector<SVertex> vertices;
	std::vector<uint32_t> indices;

	const uint32_t xDivision = 32;
	const uint32_t yDivision = 32;
	const float xLength = 20.0f;
	const float yLength = 20.0f;

	const float xStride = xLength / xDivision;
	const float yStride = yLength / yDivision;
	
	const float xDisplacement = xLength / 2.0f;
	const float yDisplacement = yLength / 2.0f;
	vertices.reserve((xDivision + 1) * (yDivision + 1));

	uint32_t imageDataStride = GetBytesFromFormat(heightMap.format);
	for (uint32_t x = 0; x <= xDivision; ++x)
	{
		for (uint32_t y = 0; y <= yDivision; ++y)
		{
			glm::vec3 normal(0.0f, 1.0f, 0.0f); //latter we compute normals too
			glm::vec2 uv(float(x) / float(xDivision), float(y) / float(yDivision));
			float height;
			{
				glm::vec2 heightMapUV = uv * glm::vec2(heightMap.width, heightMap.height);
				height = heightMap.data[imageDataStride * (uint32_t(heightMapUV.y) * heightMap.width + uint32_t(heightMapUV.x))];
				height = height / 265.0f * heightMax;
			}

			glm::vec3 pos(float(x) * xStride, height, float(y) * yStride);
			pos -= glm::vec3(xDisplacement, 1.0f, yDisplacement); //set the grid center in 0,0

			vertices.push_back(SVertex(pos, uv, normal));
		}
	}
	const uint32_t xVerts = xDivision + 1;
	auto computeNormal = [](const glm::vec3& center, const glm::vec3& p1, const glm::vec3& p2)
	{
		glm::vec3 e1 = p2 - center;
		glm::vec3 e2 = p1 - center;

		glm:: vec3 normal = glm::vec3(e1.y * e2.z, e1.z * e2.x, e1.x * e2.y) - glm::vec3(e1.z * e2.y, e1.x * e2.z, e1.y * e2.x);

		return glm::normalize(normal);
	};

	//now we compute normals
	/*To get the normal of a vertex, first have to compute the normal of the adjancent triangles

		the neighbours vertexes are numbered as follows :
			0
			|
		3 - P - 1
			|
			2

		We will get the normals of the triangles that contains pair of vertexes : P - 0, P - 1, P - 2, P - 3\
	*/
	{
		//first compute the normals for all the vertexes with 4 neighbours
		for (uint32_t x = 1; x < xDivision; ++x)
		{
			for (uint32_t y = 1; y < yDivision; ++y)
			{
				SVertex& P = vertices[y * xVerts + x];
				const SVertex& P0 = vertices[(y - 1) * xVerts + x];
				const SVertex& P1 = vertices[y * xVerts + x + 1];
				const SVertex& P2 = vertices[(y + 1) * xVerts + x];
				const SVertex& P3 = vertices[y * xVerts + x - 1];

				P.normal = computeNormal(P.pos, P1.pos, P0.pos);
				P.normal += computeNormal(P.pos, P2.pos, P1.pos);
				P.normal += computeNormal(P.pos, P3.pos, P2.pos);
				P.normal += computeNormal(P.pos, P0.pos, P3.pos);

				P.normal = glm::normalize(P.normal / 4.0f);
			}
		}

		//now we compute the exceptions: Y = 0 (top row we dont have P0 vertex)
		for (int x = 1; x < xDivision; ++x)
		{
			SVertex& P = vertices[x];
			const SVertex& P1 = vertices[x + 1];
			const SVertex& P2 = vertices[x];
			const SVertex& P3 = vertices[x - 1];

			P.normal = computeNormal(P.pos, P2.pos, P1.pos);
			P.normal += computeNormal(P.pos, P3.pos, P2.pos);

			P.normal = glm::normalize(P.normal / 2.0f);
		}

		//now we compute the exceptions: Y = yDivision (buttom  row we dont have P2 vertex)
		for (int x = 1; x < xDivision; ++x)
		{
			SVertex& P = vertices[yDivision * xVerts + x];
			const SVertex& P0 = vertices[(yDivision - 1) * xVerts + x];
			const SVertex& P1 = vertices[yDivision * xVerts + x + 1];
			const SVertex& P3 = vertices[yDivision * xVerts + x - 1];

			P.normal = computeNormal(P.pos, P1.pos, P0.pos);
			P.normal += computeNormal(P.pos, P0.pos, P3.pos);

			P.normal = glm::normalize(P.normal / 2.0f);
		}
		//now we compute the exceptions: X = 0 (left column we dont have P3 vertex)
		for (int y = 1; y < yDivision; ++y)
		{
			SVertex& P = vertices[y * xVerts];
			const SVertex& P0 = vertices[(y - 1) * xVerts];
			const SVertex& P1 = vertices[y * xVerts + 1];
			const SVertex& P2 = vertices[(y + 1) * xVerts ];

			P.normal = computeNormal(P.pos, P1.pos, P0.pos);
			P.normal += computeNormal(P.pos, P2.pos, P1.pos);

			P.normal = glm::normalize(P.normal / 2.0f);
		}

		//now we compute the exceptions: X = xDivision (right column we dont have P1 vertex)
		for (int y = 1; y < yDivision; ++y)
		{
			SVertex& P = vertices[y * xVerts + xDivision];
			const SVertex& P0 = vertices[(y - 1) * xVerts + xDivision];
			const SVertex& P2 = vertices[(y + 1) * xVerts + xDivision];
			const SVertex& P3 = vertices[y * xVerts + xDivision - 1];

			P.normal = computeNormal(P.pos, P3.pos, P2.pos);
			P.normal += computeNormal(P.pos, P0.pos, P3.pos);

			P.normal = glm::normalize(P.normal / 2.0f);

		}

		//the 4 corner vertex are not processed. i'm lazy
	}
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
	delete[] heightMap.data;
	m_grid = new Mesh(vertices, indices);
}

void TerrainRenderer::SwitchToWireframe()
{
	m_drawWireframe = !m_drawWireframe;
	m_activePipeline->SwitchWireframe(m_drawWireframe);
}

void TerrainRenderer::SwitchPipeline()
{
	static bool useDebug = true;
	m_activePipeline = (useDebug) ? &m_simplePipeline : &m_tessellatedPipeline;
	useDebug = !useDebug;

	m_activePipeline->SwitchWireframe(m_drawWireframe);
}

void TerrainRenderer::ChangeTesselationLevel(int units)
{
	float outterLevel = m_tesselationParameters.x;
	outterLevel += float(units) * 2.0f;
	outterLevel = glm::clamp(outterLevel, 1.0f, 33.0f);

	float innerLevel = outterLevel / 2.0f + 1.0f;

	m_tesselationParameters.x = outterLevel;
	m_tesselationParameters.y = innerLevel;
}

void TerrainRenderer::ChangeTesselationFactor(int units)
{
	float factor = m_tesselationParameters.z;
	m_tesselationParameters.z = glm::clamp(factor + float(units) * 0.1f, 0.0f, 1.0f);
}

bool TerrainRenderer::OnEditEnable(const KeyInput& key)
{
	m_editMode = !m_editMode;
	return true;
}

bool TerrainRenderer::OnMouseInput(const MouseInput& mouseInput)
{
	if (!m_editMode)
		return false;

	if (mouseInput.IsButtonUp(MouseInput::Button::Left))
		SwitchPipeline();

	if (mouseInput.IsButtonUp(MouseInput::Button::Right))
		SwitchToWireframe();

	if (mouseInput.GetWheelDelta() != 0)
	{
		if (mouseInput.IsSpecialKeyPressed(SpecialKey::Shift))
			ChangeTesselationFactor(mouseInput.GetWheelDelta());
		else
			ChangeTesselationLevel(mouseInput.GetWheelDelta());
	}

	return true;
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