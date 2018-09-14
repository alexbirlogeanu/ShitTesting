#pragma once

#include "VulkanLoader.h"
#include "Renderer.h"
#include "glm/glm.hpp"
#include "Serializer.h"
#include "Texture.h"

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
	const uint32_t GetID() const { return 2; } //find a way to identify a template material

	virtual const uint32_t GetDataStride() const = 0;
	virtual Material* Create() = 0;
	virtual Material* Create(Serializer* serializer) = 0;
	virtual void Save(Material* mat, Serializer*) = 0;
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

	Material* Create(Serializer* serializer) override
	{
		MaterialType* material = new MaterialType(this);
		material->Serialize(serializer);
		return material; 
	}
	Material* Create() override
	{
		return new MaterialType(this);
	}

	virtual void Save(Material* mat, Serializer* serializer) override
	{
		MaterialType* material = dynamic_cast<MaterialType*>(mat);
		TRAP(material);
		material->Serialize(serializer);
	}
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

class StandardMaterial : public Material, public SeriableImpl<StandardMaterial>
{
public:
	StandardMaterial(MaterialTemplateBase* matTemplate);
	virtual ~StandardMaterial();

	void* GetData() override { return &m_properties; }
	uint32_t GetDataStride() override { return sizeof(MaterialProperties); }

	virtual void SetTextureSlots(const std::vector<IndexedTexture>& indexedTexture) override; //the material should implement this method to write the texture indexes to the shader data
	void SetAlbedoTexture(CTexture* texture);
	void SetSpecularProperties(glm::vec4 properties);

	virtual void OnSave() override;

	typedef MaterialProperties PropertiesType;
private:
	MaterialProperties	m_properties;

	DECLARE_PROPERTY(float, Roughness, StandardMaterial);
	DECLARE_PROPERTY(float, K, StandardMaterial);
	DECLARE_PROPERTY(float, F0, StandardMaterial);
	DECLARE_PROPERTY(CTexture*, AlbedoText, StandardMaterial);

	CTexture*			m_albedoTexture; //this should be serialized, but because is a test class we dont have to
};

//debug
extern MaterialTemplateBase*  s_TestTemplate;

template<typename BASE>
class Property<Material*, BASE> : public PropertyGeneric
{
public:
	typedef Material* BASE::* PtmType;
	Property()
	{}
	Property(PtmType offset, const std::string& label)
		: m_ptm(offset)
		, m_label(label)
	{}

	virtual void Save(rapidxml::xml_node<char>* objNode, Serializer* serializer, ISeriable* obj)
	{
		BASE* cobj = dynamic_cast<BASE*>(obj);
		TRAP(cobj);
		MaterialTemplateBase* tmplMaterial = (cobj->*m_ptm)->GetTemplate();

		auto prop = serializer->GetNewAttribute(m_label.c_str(), serializer->GetNewString(std::to_string(tmplMaterial->GetID())));
		objNode->append_attribute(prop);

		tmplMaterial->Save(cobj->*m_ptm, serializer);
	};

	virtual void Load(rapidxml::xml_node<char>* objNode, Serializer* serializer, ISeriable* obj)
	{
		auto prop = objNode->first_attribute(m_label.c_str(), 0, false);
		BASE* cobj = dynamic_cast<BASE*>(obj);
		TRAP(cobj);

		cobj->*m_ptm = s_TestTemplate->Create(serializer);
	};

private:
	PtmType						m_ptm;
	std::string					m_label;
};