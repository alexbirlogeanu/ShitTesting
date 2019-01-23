#include "TerrainRenderer.h"

#include "Mesh.h"
#include "defines.h"
#include "glm/glm.hpp"
#include "Texture.h"
#include "Scene.h"
#include "Geometry.h"
#include "ShadowRenderer.h"

#include "Input.h"

struct TerrainParams
{
	glm::mat4 ViewProjMatrix;
	glm::mat4 WorldMatrix;
	glm::vec4 MaterialProp; //x = roughness, y = k, z = F0
	glm::vec4 TesselationParams; //x - outter, y - inner tessellation, z - tessellation factor
	glm::vec4 PatchParams; //xy - number of cells that are in terrain texture patch, zw - total number of cells in a terrain grid
};

struct ShadowTerrainParams
{
	glm::ivec4								NSplits;
	ShadowMapRenderer::SplitsArrayType		Splits;
};

TerrainRenderer::TerrainRenderer(VkRenderPass renderPass)
	: CRenderer(renderPass, "TerrainRenderPass")
	, m_grid(nullptr)
	, m_descSet(VK_NULL_HANDLE)
	, m_shadowDescSet(VK_NULL_HANDLE)
	, m_terrainParamsBuffer(nullptr)
	, m_shadowSplitsBuffer(nullptr)
	, m_splatterTexture(nullptr)
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
	CreateGrid();
	LoadTextures();

	CRenderer::Init();

	m_terrainParamsBuffer = MemoryManager::GetInstance()->CreateBuffer(EMemoryContextType::UniformBuffers, sizeof(TerrainParams), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
	m_shadowSplitsBuffer = MemoryManager::GetInstance()->CreateBuffer(EMemoryContextType::UniformBuffers, sizeof(ShadowTerrainParams), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

	AllocDescriptorSets(m_descriptorPool, m_descriptorLayout.Get(), &m_descSet);
	AllocDescriptorSets(m_descriptorPool, m_shadowDescLayout.Get(), &m_shadowDescSet);

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

void TerrainRenderer::RenderShadows()
{
	glm::mat4 proj;
	PerspectiveMatrix(proj);
	ConvertToProjMatrix(proj);
	m_pushConstants.ShadowProjViewMatrix = g_commonResources.GetAs<glm::mat4>(EResourceType_ShadowProjViewMat);
	m_pushConstants.ViewMatrix = proj * ms_camera.GetViewMatrix();
	VkCommandBuffer cmdBuffer = vk::g_vulkanContext.m_mainCommandBuffer;

	vk::CmdBindPipeline(cmdBuffer, m_shadowPipeline.GetBindPoint(), m_shadowPipeline.Get());
	vk::CmdPushConstants(cmdBuffer, m_shadowPipeline.GetLayout(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT, 0, sizeof(ShadowPushConstants), &m_pushConstants);
	vk::CmdBindDescriptorSets(cmdBuffer, m_shadowPipeline.GetBindPoint(), m_shadowPipeline.GetLayout(), 0, 1, &m_shadowDescSet, 0, nullptr);

	m_grid->Render();
}

void TerrainRenderer::PreRender()
{
	glm::mat4 modelMatrix = glm::scale(glm::translate(glm::mat4(1.0f), Scene::TerrainTranslate), glm::vec3(1.0f));
	m_pushConstants.ModelMatrix = modelMatrix;
	
	ShadowTerrainParams* shadowParams = m_shadowSplitsBuffer->GetPtr<ShadowTerrainParams*>();
	shadowParams->NSplits = glm::ivec4(SHADOWSPLITS);
	shadowParams->Splits = g_commonResources.GetAs<ShadowMapRenderer::SplitsArrayType>(EResourceType_ShadowMapSplits);

	glm::mat4 projMatrix;
	PerspectiveMatrix(projMatrix);
	ConvertToProjMatrix(projMatrix);
	
	TerrainParams* params = m_terrainParamsBuffer->GetPtr<TerrainParams*>();

	params->MaterialProp = glm::vec4(0.90, 0.1, 0.5, 0.0f);
	params->WorldMatrix = modelMatrix;
	params->ViewProjMatrix = projMatrix * ms_camera.GetViewMatrix();

	params->TesselationParams = m_tesselationParameters;
	params->PatchParams = m_terrainPatchParameters;
}

void TerrainRenderer::CreateDescriptorSetLayout()
{
	m_descriptorLayout.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 1);
	m_descriptorLayout.AddBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1);
	m_descriptorLayout.AddBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, (uint32_t)m_terrainTextures.size());

	m_descriptorLayout.Construct();

	m_shadowDescLayout.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_GEOMETRY_BIT, 1);
	m_shadowDescLayout.Construct(); 
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

	VkRenderPass shadowRenderPass = g_commonResources.GetAs<VkRenderPass>(EResourceType_ShadowRenderPass);

	m_shadowPipeline.SetVertexInputState(Mesh::GetVertexDesc());
	m_shadowPipeline.SetViewport(SHADOWW, SHADOWH);
	m_shadowPipeline.SetScissor(SHADOWW, SHADOWH);
	//m_shadowPipeline.SetCullMode(VK_CULL_MODE_BACK_BIT);
	m_shadowPipeline.SetVertexShaderFile("shadow_terrain.vert");
	m_shadowPipeline.SetGeometryShaderFile("shadow_terrain.geom");
	m_shadowPipeline.SetFragmentShaderFile("shadowlineardepth.frag");
	m_shadowPipeline.AddPushConstant({VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT, 0, 256});

	m_shadowPipeline.CreatePipelineLayout(m_shadowDescLayout.Get());
	m_shadowPipeline.Init(this, shadowRenderPass, 0);
}

//shit function
void TerrainRenderer::CreateGrid()
{
	SImageData heightMap;
	Read2DTextureData(heightMap, std::string(TEXTDIR) + "terrain3.png", false);
	
	const float heightMax = 7.0f;
	std::vector<SVertex> vertices;
	std::vector<uint32_t> indices;

	const uint32_t xDivision = Scene::TerrainGridSize.x;
	const uint32_t yDivision = Scene::TerrainGridSize.y;
	const float xLength = Scene::TerrainSize.x;
	const float yLength = Scene::TerrainSize.y;

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
				height = height / 256.0f * heightMax;
			}

			glm::vec3 pos(float(x) * xStride, height, float(y) * yStride);
			pos -= glm::vec3(xDisplacement, 1.0f, yDisplacement); //set the grid center in 0,0

			vertices.push_back(SVertex(pos, uv, normal));
		}
	}
	const uint32_t xVerts = xDivision + 1;

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

				P.normal = Geometry::ComputeNormal(P.pos, P1.pos, P0.pos);
				P.normal += Geometry::ComputeNormal(P.pos, P2.pos, P1.pos);
				P.normal += Geometry::ComputeNormal(P.pos, P3.pos, P2.pos);
				P.normal += Geometry::ComputeNormal(P.pos, P0.pos, P3.pos);

				P.normal = glm::normalize(P.normal / 4.0f);
			}
		}

		//now we compute the exceptions: Y = 0 (top row we dont have P0 vertex)
		for (uint32_t x = 1; x < xDivision; ++x)
		{
			SVertex& P = vertices[x];
			const SVertex& P1 = vertices[x + 1];
			const SVertex& P2 = vertices[x];
			const SVertex& P3 = vertices[x - 1];

			P.normal = Geometry::ComputeNormal(P.pos, P2.pos, P1.pos);
			P.normal += Geometry::ComputeNormal(P.pos, P3.pos, P2.pos);

			P.normal = glm::normalize(P.normal / 2.0f);
		}

		//now we compute the exceptions: Y = yDivision (buttom  row we dont have P2 vertex)
		for (uint32_t x = 1; x < xDivision; ++x)
		{
			SVertex& P = vertices[yDivision * xVerts + x];
			const SVertex& P0 = vertices[(yDivision - 1) * xVerts + x];
			const SVertex& P1 = vertices[yDivision * xVerts + x + 1];
			const SVertex& P3 = vertices[yDivision * xVerts + x - 1];

			P.normal = Geometry::ComputeNormal(P.pos, P1.pos, P0.pos);
			P.normal += Geometry::ComputeNormal(P.pos, P0.pos, P3.pos);

			P.normal = glm::normalize(P.normal / 2.0f);
		}
		//now we compute the exceptions: X = 0 (left column we dont have P3 vertex)
		for (uint32_t y = 1; y < yDivision; ++y)
		{
			SVertex& P = vertices[y * xVerts];
			const SVertex& P0 = vertices[(y - 1) * xVerts];
			const SVertex& P1 = vertices[y * xVerts + 1];
			const SVertex& P2 = vertices[(y + 1) * xVerts ];

			P.normal = Geometry::ComputeNormal(P.pos, P1.pos, P0.pos);
			P.normal += Geometry::ComputeNormal(P.pos, P2.pos, P1.pos);

			P.normal = glm::normalize(P.normal / 2.0f);
		}

		//now we compute the exceptions: X = xDivision (right column we dont have P1 vertex)
		for (uint32_t y = 1; y < yDivision; ++y)
		{
			SVertex& P = vertices[y * xVerts + xDivision];
			const SVertex& P0 = vertices[(y - 1) * xVerts + xDivision];
			const SVertex& P2 = vertices[(y + 1) * xVerts + xDivision];
			const SVertex& P3 = vertices[y * xVerts + xDivision - 1];

			P.normal = Geometry::ComputeNormal(P.pos, P3.pos, P2.pos);
			P.normal += Geometry::ComputeNormal(P.pos, P0.pos, P3.pos);

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

	m_terrainPatchParameters = glm::vec4(18.0f, 18.0f, xDivision, yDivision);

	delete[] heightMap.data;
	m_grid = new Mesh(vertices, indices);
}

void TerrainRenderer::LoadTextures()
{
	m_splatterTexture = new CTexture("terrain_splatter.png");
	m_splatterTexture->SetIsSRGB(false);
	ResourceLoader::GetInstance()->LoadTexture(&m_splatterTexture);

	std::string textFilename[] = {"grass2.png", "rock3.png", "sand.png"};
	for (const auto& fn : textFilename)
	{
		CTexture* text = new CTexture(fn);
		ResourceLoader::GetInstance()->LoadTexture(&text);
		m_terrainTextures.push_back(text);
	}
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

void TerrainRenderer::ChangePatchSize(int units)
{
	auto computeNewSize = [](int oldSize, int delta, int maxSize)
	{
		return glm::clamp(oldSize + delta, 1, maxSize);
	};

	glm::vec2 maxSize(m_terrainPatchParameters.z, m_terrainPatchParameters.w);
	m_terrainPatchParameters.x = (float)computeNewSize(int(m_terrainPatchParameters.x), units, int(maxSize.x));
	m_terrainPatchParameters.y = (float)computeNewSize(int(m_terrainPatchParameters.y), units, int(maxSize.y));
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
		else if (mouseInput.IsSpecialKeyPressed(SpecialKey::Ctrl))
			ChangePatchSize(mouseInput.GetWheelDelta());
		else
			ChangeTesselationLevel(mouseInput.GetWheelDelta());
	}

	return true;
}

void TerrainRenderer::PopulatePoolInfo(std::vector<VkDescriptorPoolSize>& poolSize, unsigned int& maxSets)
{
	maxSets = 2;
	AddDescriptorType(poolSize, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1);
	AddDescriptorType(poolSize, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1);
	AddDescriptorType(poolSize, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, (uint32_t)m_terrainTextures.size());
	AddDescriptorType(poolSize, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1); //shadow
}

void TerrainRenderer::UpdateResourceTable()
{

}

void TerrainRenderer::UpdateGraphicInterface()
{
	std::vector<VkWriteDescriptorSet> wDesc;

	VkDescriptorBufferInfo buffInfo = m_terrainParamsBuffer->GetDescriptor();
	VkDescriptorBufferInfo splitsInfo = m_shadowSplitsBuffer->GetDescriptor();
	std::vector<VkDescriptorImageInfo> textInfos;

	for (auto text : m_terrainTextures)
		textInfos.push_back(text->GetTextureDescriptor());

	wDesc.push_back(InitUpdateDescriptor(m_descSet, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &buffInfo));
	wDesc.push_back(InitUpdateDescriptor(m_descSet, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &m_splatterTexture->GetTextureDescriptor()));
	wDesc.push_back(InitUpdateDescriptor(m_descSet, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, textInfos));
	wDesc.push_back(InitUpdateDescriptor(m_shadowDescSet, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &splitsInfo));

	vk::UpdateDescriptorSets(vk::g_vulkanContext.m_device, (uint32_t)wDesc.size(), wDesc.data(), 0, nullptr);
}
