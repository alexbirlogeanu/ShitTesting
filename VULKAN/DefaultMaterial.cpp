#include "DefaultMaterial.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//DefaultMaterial
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
	m_textureSlots = indexedTextures;
	WriteTextureIndex(GetAlbedoText(), &m_properties.AlbedoTexture);
}

void DefaultMaterial::OnLoad()
{
	m_properties.Roughness = GetRoughness();
	m_properties.K = GetK();
	m_properties.F0 = GetF0();
	AddTextureSlot(GetAlbedoText());
}