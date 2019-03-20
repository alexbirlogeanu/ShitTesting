#include "Material.h"

#include "Batch.h"
#include "Object.h"

#include "DefaultMaterial.h"
#include "NormalMapMaterial.h"

////////////////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////

MaterialLibrary::MaterialLibrary()
{
	m_materialTemplates.emplace("default", new MaterialTemplate<DefaultMaterial>("batch.vert", "defaultmaterial.frag", "default"));
	m_materialTemplates.emplace("normalmap", new MaterialTemplate<NormalMapMaterial>("normalmapmaterial.vert", "normalmapmaterial.frag", "normalmap"));
}

MaterialLibrary::~MaterialLibrary()
{
	for (auto entry : m_materialTemplates)
		delete entry.second;
}

void MaterialLibrary::Initialize()
{
	//CreateDescriptor layouts
	m_descriptorLayouts.resize(DescriptorIndex::Count);

	m_descriptorLayouts[DescriptorIndex::Common] = new DescriptorSetLayout();
	m_descriptorLayouts[DescriptorIndex::Common]->AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);

	m_descriptorLayouts[DescriptorIndex::Specific] = new DescriptorSetLayout();
	m_descriptorLayouts[DescriptorIndex::Specific]->AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
	m_descriptorLayouts[DescriptorIndex::Specific]->AddBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, BATCH_MAX_TEXTURE);

	for (unsigned int i = 0; i < DescriptorIndex::Count; ++i)
		m_descriptorLayouts[i]->Construct();
}

void MaterialLibrary::Setup(VkRenderPass renderPass, uint32_t subpassId)
{
	for (auto tmpl : m_materialTemplates)
		tmpl.second->CreatePipeline(renderPass, subpassId);
}

std::vector<VkDescriptorSet> MaterialLibrary::AllocNewDescriptors()
{
	DescriptorPool* pool = nullptr; //used to alloc new desc
	for (unsigned int i = 0; i < m_descriptorPools.size(); ++i)
	{
		if (m_descriptorPools[i]->CanAllocate(m_descriptorLayouts))
		{
			pool = m_descriptorPools[i];
			break;
		}
	}

	if (!pool)
	{
		pool = new DescriptorPool();
		pool->Construct(m_descriptorLayouts, 10);//magic number again
	}

	TRAP(pool);

	std::vector<VkDescriptorSet> newDescSets;
	newDescSets.resize(m_descriptorLayouts.size());
	for (unsigned int i = 0; i < m_descriptorLayouts.size(); ++i)
	{
		newDescSets[i] = pool->AllocateDescriptorSet(*m_descriptorLayouts[i]); //TODO! I have to look into the array version of this method and why it doesn't work
	}

	return  newDescSets;
}

std::vector<VkDescriptorSetLayout> MaterialLibrary::GetDescriptorLayouts() const
{
	std::vector<VkDescriptorSetLayout> layouts;
	layouts.reserve(m_descriptorLayouts.size());
	for (const auto& layout : m_descriptorLayouts)
	{
		layouts.push_back(layout->Get());
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
	pushConstRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT;
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

void MaterialTemplateBase::CreatePipeline(VkRenderPass renderPass, uint32_t subpassId)
{
	VkPushConstantRange pushConstRange;
	pushConstRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT;
	pushConstRange.offset = 0;
	pushConstRange.size = 256; //max push constant range(can get it from limits)

	m_pipeline.SetVertexInputState(Mesh::GetVertexDesc());
	m_pipeline.AddBlendState(CGraphicPipeline::CreateDefaultBlendState(), GBuffer_InputCnt);
	m_pipeline.SetVertexShaderFile(GetVertexShader());
	m_pipeline.SetFragmentShaderFile(GetFragmentShader());
	m_pipeline.SetCullMode(VK_CULL_MODE_BACK_BIT);
	m_pipeline.AddPushConstant(pushConstRange);
	m_pipeline.CreatePipelineLayout(MaterialLibrary::GetInstance()->GetDescriptorLayouts());
	m_pipeline.Setup(renderPass, subpassId);
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
