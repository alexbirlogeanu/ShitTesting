#include "Material.h"

#include "Batch.h"
#include "Object.h"

////////////////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////

MaterialLibrary::MaterialLibrary()
	: m_currentPoolIndex(-1)
{
	m_materialTemplates.emplace("default", new MaterialTemplate<DefaultMaterial>("batch.vert", "batch.frag", "default"));
}

MaterialLibrary::~MaterialLibrary()
{

}

void MaterialLibrary::Initialize(CRenderer* renderer)
{
	///CreateDescriptor layouts
	m_descriptorLayouts.resize(DescriptorIndex::Count);
	m_descriptorLayouts[DescriptorIndex::Common].AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);

	m_descriptorLayouts[DescriptorIndex::Specific].AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
	m_descriptorLayouts[DescriptorIndex::Specific].AddBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 12);

	for (unsigned int i = 0; i < DescriptorIndex::Count; ++i)
		m_descriptorLayouts[i].Construct();

	for (auto tmpl : m_materialTemplates)
		tmpl.second->CreatePipeline(renderer);
}

std::vector<VkDescriptorSet> MaterialLibrary::AllocNewDescriptors()
{
	if (m_currentPoolIndex == -1 || !m_descriptorPools[m_currentPoolIndex].CanAllocate(m_descriptorLayouts))
	{
		m_descriptorPools.push_back(DescriptorPool());
		DescriptorPool& pool = m_descriptorPools[++m_currentPoolIndex];
		pool.Construct(m_descriptorLayouts, 10); //magic number again
	}

	VkDescriptorSet set = m_descriptorPools[m_currentPoolIndex].AllocateDescriptorSet(m_descriptorLayouts[0]);
	VkDescriptorSet set2 = m_descriptorPools[m_currentPoolIndex].AllocateDescriptorSet(m_descriptorLayouts[1]);

	std::vector<VkDescriptorSet> newDescSets;
	newDescSets.resize(m_descriptorLayouts.size());
	for (unsigned int i = 0; i < m_descriptorLayouts.size(); ++i)
	{
		newDescSets[i] = m_descriptorPools[m_currentPoolIndex].AllocateDescriptorSet(m_descriptorLayouts[i]);
	}

	return  newDescSets;
}

std::vector<VkDescriptorSetLayout> MaterialLibrary::GetDescriptorLayouts() const
{
	std::vector<VkDescriptorSetLayout> layouts;
	layouts.reserve(m_descriptorLayouts.size());
	for (const auto& layout : m_descriptorLayouts)
	{
		layouts.push_back(layout.Get());
	}

	return layouts;
}

MaterialTemplateBase* MaterialLibrary::GetMaterialByName(const std::string& name) const
{
	auto it = m_materialTemplates.find(name);
	TRAP(it != m_materialTemplates.end() && "Material not found in library");

	return it->second;
}

MaterialTemplateBase::MaterialTemplateBase(const std::string& vertexShader, const std::string& fragmentShader, const std::string& name)
	: m_vertexShader(vertexShader)
	, m_fragmentShader(fragmentShader)
	, m_name(name)
{

}

MaterialTemplateBase::~MaterialTemplateBase()
{

}

void MaterialTemplateBase::CreatePipeline(CRenderer* renderer)
{
	VkPushConstantRange pushConstRange;
	pushConstRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	pushConstRange.offset = 0;
	pushConstRange.size = 256; //max push constant range(can get it from limits)

	m_pipeline.SetVertexInputState(Mesh::GetVertexDesc());
	m_pipeline.AddBlendState(CGraphicPipeline::CreateDefaultBlendState(), GBuffer_InputCnt);
	m_pipeline.SetVertexShaderFile(GetVertexShader());
	m_pipeline.SetFragmentShaderFile(GetFragmentShader());
	m_pipeline.SetCullMode(VK_CULL_MODE_BACK_BIT);
	m_pipeline.AddPushConstant(pushConstRange);
	m_pipeline.CreatePipelineLayout(MaterialLibrary::GetInstance()->GetDescriptorLayouts());
	m_pipeline.Init(renderer, renderer->GetRenderPass(), 0); //0 is a "magic" number. This means that renderpass has just a subpass and the m_pipelines are used in that subpass
}

std::vector<VkDescriptorSet> MaterialTemplateBase::GetNewDescriptorSets()
{
	return MaterialLibrary::GetInstance()->AllocNewDescriptors();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//Material
/////////////////////////////////////////////////////////////////////////////////////////////////////////	



Material::Material(MaterialTemplateBase* matTemplate)
	: m_template(matTemplate)
{

}
Material::~Material()
{

}


uint32_t Material::AddTextureSlot(CTexture* texture)
{
	uint32_t newIndex = (uint32_t)m_textureSlots.size();
	m_textureSlots.push_back(IndexedTexture(texture, -1));
	return newIndex;
}

void Material::WriteTextureIndex(const CTexture* text, uint32_t* addressToWrite)
{
	auto it = std::find_if(m_textureSlots.begin(), m_textureSlots.end(), [text](const IndexedTexture& slot)
	{
		return text == slot.texture;
	});

	TRAP(it != m_textureSlots.end() && "text is not registered in the texture slots.");
	
	if (it != m_textureSlots.end())
	{
		TRAP(it->index != -1 && "text is not indexed. You skip a step, the step of indexing the texture unsing batch");
		*addressToWrite = it->index;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//TEST: DefaultMaterial
/////////////////////////////////////////////////////////////////////////////////////////////////////////	

BEGIN_PROPERTY_MAP(DefaultMaterial)
	IMPLEMENT_PROPERTY(float, Roughness, "Roughness", DefaultMaterial),
	IMPLEMENT_PROPERTY(float, K, "K", DefaultMaterial),
	IMPLEMENT_PROPERTY(float, F0, "F0", DefaultMaterial),
	IMPLEMENT_PROPERTY(CTexture*, AlbedoText, "albedoText", DefaultMaterial)
END_PROPERTY_MAP(DefaultMaterial)

DefaultMaterial::DefaultMaterial(MaterialTemplateBase* matTemplate)
	: Material(matTemplate)
	, SeriableImpl<DefaultMaterial>("DefaultMaterial")
{

}

DefaultMaterial::~DefaultMaterial()
{

}

void DefaultMaterial::SetTextureSlots(const std::vector<IndexedTexture>& indexedTextures)
{
	//HARDCODED we know that the index of albedo texture is 0
	m_textureSlots = indexedTextures;
	WriteTextureIndex(GetAlbedoText(), &m_properties.AlbedoTexture);
}

void DefaultMaterial::SetSpecularProperties(glm::vec4 properties)
{
	m_properties.Roughness = properties.x;
	m_properties.K = properties.y;
	m_properties.F0 = properties.z;
}

void DefaultMaterial::OnLoad()
{
	m_properties.Roughness = GetRoughness();
	m_properties.K = GetK();
	m_properties.F0 = GetF0();
	AddTextureSlot(GetAlbedoText());
}