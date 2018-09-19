#include "NormalMapMaterial.h"

BEGIN_PROPERTY_MAP(NormalMapMaterial)
	IMPLEMENT_PROPERTY(float, Roughness, "Roughness", NormalMapMaterial),
	IMPLEMENT_PROPERTY(float, K, "K", NormalMapMaterial),
	IMPLEMENT_PROPERTY(float, F0, "F0", NormalMapMaterial),
	IMPLEMENT_PROPERTY(CTexture*, AlbedoText, "albedoText", NormalMapMaterial),
	IMPLEMENT_PROPERTY(CTexture*, NormalMapText, "normalMapText", NormalMapMaterial)

END_PROPERTY_MAP(NormalMapMaterial)

NormalMapMaterial::NormalMapMaterial(MaterialTemplateBase* matTemplate)
	: Material(matTemplate)
	, SeriableImpl<NormalMapMaterial>("NormalMapMaterial")
{

}

NormalMapMaterial::~NormalMapMaterial()
{

}

void NormalMapMaterial::SetTextureSlots(const std::vector<IndexedTexture>& indexedTextures)
{
	m_textureSlots = indexedTextures;
	WriteTextureIndex(GetAlbedoText(), &m_properties.AlbedoTexture);
	WriteTextureIndex(GetNormalMapText(), &m_properties.NormalMapTexture);
}

void NormalMapMaterial::OnLoad()
{
	m_properties.Roughness = GetRoughness();
	m_properties.K = GetK();
	m_properties.F0 = GetF0();
	AddTextureSlot(GetAlbedoText());
	AddTextureSlot(GetNormalMapText());
}