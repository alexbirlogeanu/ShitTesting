#pragma once

#include "VulkanLoader.h"
#include "Renderer.h"
#include "glm/glm.hpp"

#include <string>
#include <vector>

class CRenderer;
class CTexture;
class Material;

class MaterialTemplateBase
{
public:
	MaterialTemplateBase(const std::string& vertexShader, const std::string& fragmentShader);
	virtual ~MaterialTemplateBase();

	const std::string& GetVertexShader() const { return m_vertexShader; }
	const std::string& GetFragmentShader() const { return m_fragmentShader; }

	virtual const uint32_t GetDataStride() const = 0;
	virtual Material* Create() = 0;
private:
	std::string		m_vertexShader;
	std::string		m_fragmentShader;
};

template<class MaterialType>
class MaterialTemplate : public MaterialTemplateBase
{
public:
	MaterialTemplate(const std::string& vertexShader, const std::string& fragmentShader)
		: MaterialTemplateBase(vertexShader, fragmentShader)
	{}

	virtual ~MaterialTemplate(){}

	const uint32_t GetDataStride() const override { return sizeof(MaterialType::PropertiesType); }

	Material* Create() { return new MaterialType(this); }
};

struct IndexedTexture
{
	CTexture*	texture;
	uint32_t	index; //this member should be filled with the global index used by the batch

	IndexedTexture()
		: texture(nullptr)
		, index(-1){}

	IndexedTexture(CTexture* _texture, uint32_t _index)
		: texture(_texture)
		, index(_index)
	{}
};

class Material
{
public:
	Material(MaterialTemplateBase* matTemplate);
	virtual ~Material();

	MaterialTemplateBase* GetTemplate() { return m_template; }

	virtual void* GetData() = 0;
	virtual uint32_t GetDataStride() = 0;

	std::vector<IndexedTexture>& GetTextureSlots() { return m_textureSlots; }
	virtual void SetTextureSlots(const std::vector<IndexedTexture>& indexedTexture) = 0; //the material should implement this method to write the texture indexes to the shader data

	uint32_t AddTextureSlot(CTexture* texture);
protected:
	
	MaterialTemplateBase*			m_template;
	std::vector<IndexedTexture>		m_textureSlots;
};


/////////////////TEST///////////////////

struct MaterialProperties
{
	float		Roughness;
	float		K;
	float		F0;
	uint32_t	AlbedoTexture;
};

class StandardMaterial : public Material
{
public:
	StandardMaterial(MaterialTemplateBase* matTemplate);
	virtual ~StandardMaterial();

	void* GetData() override { return &m_properties; }
	uint32_t GetDataStride() override { return sizeof(MaterialProperties); }

	virtual void SetTextureSlots(const std::vector<IndexedTexture>& indexedTexture) override; //the material should implement this method to write the texture indexes to the shader data
	void SetAlbedoTexture(CTexture* texture);
	void SetSpecularProperties(glm::vec4 properties);

	typedef MaterialProperties PropertiesType;
private:
	MaterialProperties	m_properties;
	CTexture*			m_albedoTexture; //this should be serialized, but because is a test class we dont have to
};