#pragma once

#include "Material.h"
#include "Serializer.h"

struct NormalMapMaterialProperties
{
	float		Roughness;
	float		K;
	float		F0;
	uint32_t	AlbedoTexture;
	uint32_t	NormalMapTexture;
	uint32_t	Padding[3]; //not used
};

class NormalMapMaterial : public Material, public SeriableImpl<NormalMapMaterial>
{
public:
	typedef NormalMapMaterialProperties PropertiesType;

	NormalMapMaterial(MaterialTemplateBase* matTemplate);
	virtual ~NormalMapMaterial();

	void* GetData() override { return &m_properties; }
	uint32_t GetDataStride() override { return sizeof(NormalMapMaterialProperties); }
	void SetTextureSlots(const std::vector<IndexedTexture>& indexedTexture) override;

	void OnLoad() override;
private:
	DECLARE_PROPERTY(float, Roughness, NormalMapMaterial);
	DECLARE_PROPERTY(float, K, NormalMapMaterial);
	DECLARE_PROPERTY(float, F0, NormalMapMaterial);
	DECLARE_PROPERTY(CTexture*, AlbedoText, NormalMapMaterial);
	DECLARE_PROPERTY(CTexture*, NormalMapText, NormalMapMaterial);

	PropertiesType						m_properties;
};