#pragma once

#include "Material.h"
#include "Serializer.h"

struct DefaultMaterialProperties
{
	float		Roughness;
	float		K;
	float		F0;
	uint32_t	AlbedoTexture;
};

class DefaultMaterial : public Material, public SeriableImpl<DefaultMaterial>
{
public:
	DefaultMaterial(MaterialTemplateBase* matTemplate);
	virtual ~DefaultMaterial();

	void* GetData() override { return &m_properties; }
	uint32_t GetDataStride() override { return sizeof(DefaultMaterialProperties); }

	virtual void SetTextureSlots(const std::vector<IndexedTexture>& indexedTexture) override; //the material should implement this method to write the texture indexes to the shader data

	virtual void OnLoad() override;

	typedef DefaultMaterialProperties PropertiesType;
private:
	DefaultMaterialProperties	m_properties;

	DECLARE_PROPERTY(float, Roughness, DefaultMaterial);
	DECLARE_PROPERTY(float, K, DefaultMaterial);
	DECLARE_PROPERTY(float, F0, DefaultMaterial);
	DECLARE_PROPERTY(CTexture*, AlbedoText, DefaultMaterial);

};