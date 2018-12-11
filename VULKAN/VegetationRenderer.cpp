#include "VegetationRenderer.h"

#include "defines.h"
#include "Utils.h"
#include "MemoryManager.h"
#include "Texture.h"
#include "Mesh.h"
#include "Scene.h"
#include "Serializer.h"
#include "Input.h"
#include "UI.h"

#include <random>

class VegetationTemplate : public SeriableImpl<VegetationTemplate>
{
	DECLARE_PROPERTY(CTexture*, Albedo, VegetationTemplate);
	DECLARE_PROPERTY(glm::vec2, Size, VegetationTemplate);

	VegetationTemplate()
		: SeriableImpl<VegetationTemplate>("VegetationTamplate")
	{}
};

BEGIN_PROPERTY_MAP(VegetationTemplate)
	IMPLEMENT_PROPERTY(CTexture*, Albedo, "Albedo", VegetationTemplate),
	IMPLEMENT_PROPERTY(glm::vec2, Size, "Size", VegetationTemplate)
END_PROPERTY_MAP(VegetationTemplate)

class VegetationTemplateLoader : public Serializer
{
public:
	VegetationTemplateLoader()
	{
	}

	virtual ~VegetationTemplateLoader()
	{
		for (auto t : m_templates)
			delete t;
		m_templates.clear();
	}

	const std::vector<VegetationTemplate*>& GetTemplates() const { return m_templates; }
protected:
	virtual void SaveContent()
	{
		for (auto t : m_templates)
			t->Serialize(this);
	}

	virtual void LoadContent()
	{
		while (!HasReachedEof()) //HAS reached eof is not working properly
		{
			VegetationTemplate* temp = new VegetationTemplate();
			if (temp->Serialize(this))
				m_templates.push_back(temp);
		}
	}
private:
	std::vector<VegetationTemplate*>		m_templates;
};



VegetationRenderer::VegetationRenderer(VkRenderPass renderPass)
	: CRenderer(renderPass, "VegetationPass")
	, m_staggingBuffer(nullptr)
	, m_paramsBuffer(nullptr)
	, m_renderDescSet(VK_NULL_HANDLE)
	, m_maxTextures(4)
	, m_isReady(false)
	, m_quad(nullptr)
	, m_elapsedTime(0.f)
	, m_isDebugMode(false)
	, m_windStrength(0.4f)
	, m_angularSpeed(3.0f)
	, m_windAngleLimits(75.0f, 135.0f)
	, m_debugText(nullptr)
{
	InputManager::GetInstance()->MapKeyPressed('3', InputManager::KeyPressedCallback(this, &VegetationRenderer::OnDebugKey));
	InputManager::GetInstance()->MapMouseButton(InputManager::MouseButtonsCallback(this, &VegetationRenderer::OnDebugWindVelocityChange));
}

VegetationRenderer::~VegetationRenderer()
{

}

void VegetationRenderer::Init()
{
	CRenderer::Init();
	
	GenerateVegetation();
	AllocDescriptorSets(m_descriptorPool, m_renderDescSetLayout.Get(), &m_renderDescSet);
	CreateBuffers();

	//m_quad = new Mesh("obj\\veg_plane.mb");
	m_quad = CreateFullscreenQuad();

	TRAP(sizeof(GlobalParams) <= 256);
	VkPushConstantRange pushRange{ VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(GlobalParams) };

	m_renderPipeline.SetVertexShaderFile("grass.vert");
	m_renderPipeline.SetFragmentShaderFile("grass.frag");
	m_renderPipeline.SetVertexInputState(Mesh::GetVertexDesc());
	m_renderPipeline.SetDepthTest(true);
	m_renderPipeline.SetDepthWrite(true);
	m_renderPipeline.AddPushConstant(pushRange);
	//m_renderPipeline.SetCullMode(VK_CULL_MODE_BACK_BIT);
	m_renderPipeline.AddBlendState(CGraphicPipeline::CreateDefaultBlendState(), 4);
	m_renderPipeline.CreatePipelineLayout(m_renderDescSetLayout.Get());
	m_renderPipeline.Init(this, m_renderPass, 0);

	//init wind velocity
	m_globals.WindVelocity = glm::vec4(-1.0f, 0.0f, 1.0f, 0.0f);
}

void VegetationRenderer::Render()
{
	if (!m_isReady)
	{
		CopyBuffers();
		m_isReady = true; //kinda iffy. We know that the data will be on the gpu next frame
		return;
	}

	if (m_isReady && m_staggingBuffer)
	{
		MemoryManager::GetInstance()->FreeHandle(m_staggingBuffer);
		m_staggingBuffer = nullptr;
		UpdateTextures();
	}

	StartRenderPass();
	VkCommandBuffer cmdBuff = vk::g_vulkanContext.m_mainCommandBuffer;

	vk::CmdBindPipeline(cmdBuff, m_renderPipeline.GetBindPoint(), m_renderPipeline.Get());
	vk::CmdBindDescriptorSets(cmdBuff, m_renderPipeline.GetBindPoint(), m_renderPipeline.GetLayout(), 0, 1, &m_renderDescSet, 0, nullptr);
	vk::CmdPushConstants(cmdBuff, m_renderPipeline.GetLayout(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(GlobalParams), &m_globals);

	m_quad->Render(-1, (uint32_t)m_plants.size());

	EndRenderPass();
}

void VegetationRenderer::PreRender()
{
	m_elapsedTime += GetDeltaTime();

	glm::mat4 proj;
	PerspectiveMatrix(proj);
	ConvertToProjMatrix(proj);
	WindVariation();

	m_globals.ProjViewMatrix = proj * ms_camera.GetViewMatrix();
	m_globals.CameraPosition = glm::vec4(ms_camera.GetPos(), 1.0f);
	m_globals.LightDirection = glm::vec4(directionalLight.GetDirection());
}

void VegetationRenderer::UpdateTextures()
{
	std::vector<VkWriteDescriptorSet> wDesc;
	std::vector<VkDescriptorImageInfo> colorInfos;
	std::vector<VkDescriptorImageInfo> normalInfos;

	auto fillTextures = [&](const std::vector<CTexture*>& textures, uint32_t bindingIndex, std::vector<VkDescriptorImageInfo>& imageInfos){

		for (auto t : textures)
			imageInfos.push_back(t->GetTextureDescriptor());

		TRAP(!textures.empty());

		for (auto i = textures.size(); i < m_maxTextures; ++i)
			imageInfos.push_back(textures[0]->GetTextureDescriptor());

		wDesc.push_back(InitUpdateDescriptor(m_renderDescSet, bindingIndex, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, imageInfos));

	};

	fillTextures(m_albedoTextures, 1, colorInfos);

	vk::UpdateDescriptorSets(vk::g_vulkanContext.m_device, (uint32_t)wDesc.size(), wDesc.data(), 0, nullptr);
}

void VegetationRenderer::CreateDescriptorSetLayout()
{
	m_renderDescSetLayout.AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
	m_renderDescSetLayout.AddBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, m_maxTextures);

	m_renderDescSetLayout.Construct();
}

void VegetationRenderer::PopulatePoolInfo(std::vector<VkDescriptorPoolSize>& poolSize, unsigned int& maxSets)
{
	AddDescriptorType(poolSize, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, m_maxTextures);
	AddDescriptorType(poolSize, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2);

	maxSets = 2;
}

void VegetationRenderer::UpdateGraphicInterface()
{
	std::vector<VkWriteDescriptorSet> wDesc;
	VkDescriptorBufferInfo bufferInfo = m_paramsBuffer->GetDescriptor();

	wDesc.push_back(InitUpdateDescriptor(m_renderDescSet, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferInfo));

	vk::UpdateDescriptorSets(vk::g_vulkanContext.m_device, (uint32_t)wDesc.size(), wDesc.data(), 0, nullptr);
}

void VegetationRenderer::GenerateVegetation()
{
	SImageData vegetationDistribution;
	Read2DTextureData(vegetationDistribution, std::string(TEXTDIR) + "veg_distr.png", false);

	uint32_t width = vegetationDistribution.width;
	uint32_t height = vegetationDistribution.height;

	const float plantsPerCell = 3.0f;//TODO parameterize this renderer with a serializer
	const float redDistr = 1.0f;
	const float greenDistr = 0.25f;
	const float blueDistr = 0.0f;

	std::vector<uint32_t> numberOfPlantsPerCell;
	uint32_t totalPlants = 0;
	for (uint32_t x = 0; x < width; ++x)
		for (uint32_t y = 0; y < height; ++y)
		{
			uint32_t plants = uint32_t((redDistr * vegetationDistribution.GetRed(x, y) + greenDistr * vegetationDistribution.GetGreen(x, y) + blueDistr * vegetationDistribution.GetBlue(x, y)) * plantsPerCell);

			numberOfPlantsPerCell.push_back(plants);
			totalPlants += plants;
		}

	std::vector<glm::vec3> plantsPosition;
	plantsPosition.reserve(totalPlants);

	CScene::CalculatePlantsPositions(glm::uvec2(width, height), numberOfPlantsPerCell, plantsPosition);

	delete[] vegetationDistribution.data;

	VegetationTemplateLoader loader;
	loader.Load("vegetation.xml");

	auto templates = loader.GetTemplates();

	for (auto temp : templates)
		m_albedoTextures.push_back(temp->GetAlbedo());

	TRAP(m_albedoTextures.size() < m_maxTextures && "Need to increase the limit. Also increase in the grass.frag too");

	unsigned int seed = 67834;
	std::mt19937 generator(seed);
	std::uniform_int_distribution<uint32_t> texturIndexDistr(0, m_albedoTextures.size() - 1);
	std::uniform_real_distribution<float> bendFactorDistr(0.05f, 0.15f);
	
	for (auto p : plantsPosition)
	{
		uint32_t index = texturIndexDistr(generator);
		float bendFactor = bendFactorDistr(generator);
		VegetationTemplate* vegTemplate = templates[index];

		m_plants.push_back({ glm::vec4(p, 1.0f), glm::vec4(glm::vec2(vegTemplate->GetSize()), bendFactor, float(index)) });
	}
}

void VegetationRenderer::CopyBuffers()
{
	TRAP(m_staggingBuffer && m_paramsBuffer);

	StartDebugMarker("VegetationCopyBuffers");
	VkBufferCopy region;
	region.srcOffset = m_staggingBuffer->GetOffset();
	region.dstOffset = m_paramsBuffer->GetOffset();
	region.size = m_staggingBuffer->GetSize();

	vk::CmdCopyBuffer(vk::g_vulkanContext.m_mainCommandBuffer, m_staggingBuffer->Get(), m_paramsBuffer->Get(), 1, &region);

	EndDebugMarker("VegetationCopyBuffers");
}

void VegetationRenderer::CreateBuffers()
{
	TRAP(!m_plants.empty() && "Heeey maybe put some plants you dumb fuck");

	if (m_plants.empty())
		return;

	VkDeviceSize totalSize = m_plants.size() * sizeof(PlantDescription);
	m_staggingBuffer = MemoryManager::GetInstance()->CreateBuffer(EMemoryContextType::StagginBuffer, totalSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
	m_paramsBuffer = MemoryManager::GetInstance()->CreateBuffer(EMemoryContextType::DeviceLocalBuffer, totalSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);

	bool wasMemMapped = MemoryManager::GetInstance()->IsMemoryMapped(EMemoryContextType::StagginBuffer);
	if (!wasMemMapped)
		MemoryManager::GetInstance()->MapMemoryContext(EMemoryContextType::StagginBuffer);

	PlantDescription* memPtr = m_staggingBuffer->GetPtr<PlantDescription*>();
	memcpy(memPtr, m_plants.data(), totalSize);

	if (!wasMemMapped)
		MemoryManager::GetInstance()->UnmapMemoryContext(EMemoryContextType::StagginBuffer);

}

void VegetationRenderer::WindVariation()
{
	float as = glm::sin(m_elapsedTime * m_angularSpeed) + glm::sin(2.1f + m_elapsedTime * m_angularSpeed / 2.0f) + glm::sin(0.5f + m_elapsedTime * 2.0f * m_angularSpeed); //[-3, 3]
	glm::vec2 limits(glm::radians(m_windAngleLimits.x), glm::radians(m_windAngleLimits.y));

	float angle = (as + 3.0f) / 6.0f * (limits.y - limits.x) + limits.x; // [-3, 3] -> [limits.x, limits.y]

	m_globals.WindVelocity = glm::vec4(glm::cos(angle), 0.0f, -glm::sin(angle), 0.0f) * m_windStrength;
}

bool VegetationRenderer::OnDebugKey(const KeyInput& key)
{
	if (key.GetKeyPressed() == '3')
	{
		m_isDebugMode = !m_isDebugMode;

		if (m_isDebugMode)
		{
			TRAP(!m_debugText);
			m_debugText = CUIManager::GetInstance()->CreateTextItem("Vegetation: Wheel (WindStrength) + Shift (Speed) / Ctr(AngleLimits). Press 3 to close", glm::uvec2(10, 50));
		}
		else
		{
			TRAP(m_debugText);
			CUIManager::GetInstance()->DestroyTextItem(m_debugText);
			m_debugText = nullptr;

		}
		return true;
	}

	return false;
}

bool VegetationRenderer::OnDebugWindVelocityChange(const MouseInput& mouse)
{
	if (!m_isDebugMode)
		return false;

	if (mouse.GetWheelDelta() != 0)
	{
		float modifier = (mouse.GetWheelDelta() > 0) ? 0.2f : -0.2f;
		if (mouse.IsSpecialKeyPressed(SpecialKey::Shift))
		{
			m_angularSpeed += modifier;
			m_angularSpeed = glm::clamp(m_angularSpeed, 0.1f, 4.0f);

		}
		else if (mouse.IsSpecialKeyPressed(SpecialKey::Ctrl))
		{
			float angleDelta = (mouse.GetWheelDelta() > 0.0f) ? 5.0f : -5.0f;
			m_windAngleLimits.y += angleDelta;
			m_windAngleLimits.y = glm::clamp(m_windAngleLimits.y, m_windAngleLimits.x + 5.0f, 180.0f);
		}
		else
		{
			m_windStrength += modifier;
			m_windStrength = glm::clamp(m_windStrength, 0.2f, 10.0f);
		}

		return true;
	}

	return false;
}
