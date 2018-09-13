#include "Material.h"

#include "Batch.h"
#include "Object.h"

MaterialTemplateBase::MaterialTemplateBase(const std::string& vertexShader, const std::string& fragmentShader)
	: m_vertexShader(vertexShader)
	, m_fragmentShader(fragmentShader)
{

}

MaterialTemplateBase::~MaterialTemplateBase()
{

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
	uint32_t newIndex = m_textureSlots.size();
	m_textureSlots.push_back(IndexedTexture(texture, -1));
	return newIndex;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//TEST: StandardMaterial
/////////////////////////////////////////////////////////////////////////////////////////////////////////	

StandardMaterial::StandardMaterial(MaterialTemplateBase* matTemplate)
	: Material(matTemplate)
{

}

StandardMaterial::~StandardMaterial()
{

}

void StandardMaterial::SetAlbedoTexture(CTexture* texture) 
{ 
	m_albedoTexture = texture; 
	AddTextureSlot(m_albedoTexture);
}

void StandardMaterial::SetTextureSlots(const std::vector<IndexedTexture>& indexedTextures)
{
	//HARDCODED we know that the index of albedo texture is 0
	m_textureSlots = indexedTextures;
	m_properties.AlbedoTexture = m_textureSlots[0].index;
}

void StandardMaterial::SetSpecularProperties(glm::vec4 properties)
{
	m_properties.Roughness = properties.x;
	m_properties.K = properties.y;
	m_properties.F0 = properties.z;
}