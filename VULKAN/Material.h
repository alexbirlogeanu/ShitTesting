#pragma once

#include "VulkanLoader.h"
#include "Renderer.h"
#include "glm/glm.hpp"
#include "Serializer.h"
#include "Texture.h"
#include "DescriptorsUtils.h"

#include <string>
#include <vector>

class CRenderer;
class CTexture;
class Material;
class MaterialTemplateBase;

enum DescriptorIndex
{
	Common = 0,
	Specific,
	Count
};

//TODO! Need a code refactor
class MaterialLibrary : public Singleton<MaterialLibrary>
{
	friend class Singleton<MaterialLibrary>;
public:
	void Initialize();

	void Setup(VkRenderPass renderPass, uint32_t subpassId);

	std::vector<VkDescriptorSet> AllocNewDescriptors();

	std::vector<VkDescriptorSetLayout> GetDescriptorLayouts() const;
	MaterialTemplateBase* GetMaterialByName(const std::string& name) const;
private:
	MaterialLibrary();
	virtual ~MaterialLibrary();
private:
	std::unordered_map<std::string, MaterialTemplateBase*>		m_materialTemplates;
	
	std::vector<DescriptorPool*>								m_descriptorPools;
	std::vector<DescriptorSetLayout*>							m_descriptorLayouts;
};


class MaterialTemplateBase
{
public:
	MaterialTemplateBase(const std::string& vertexShader, const std::string& fragmentShader, const std::string& name);
	virtual ~MaterialTemplateBase();

	const std::string& GetVertexShader() const { return m_vertexShader; }
	const std::string& GetFragmentShader() const { return m_fragmentShader; }
	const std::string& GetName() const { return m_name; }

	void CreatePipeline(CRenderer* renderer); //TODO delete
	void CreatePipeline(VkRenderPass renderPass, uint32_t subpassId);

	const CGraphicPipeline& GetPipeline() const { return m_pipeline; }

	std::vector<VkDescriptorSet> GetNewDescriptorSets();

	virtual const uint32_t GetDataStride() const = 0;
	virtual Material* Create() = 0;
	virtual Material* Create(Serializer* serializer) = 0;
	virtual void Save(Material* mat, Serializer*) = 0;
protected:

private:
	std::string						m_vertexShader;
	std::string						m_fragmentShader;
	std::string						m_name;

	CGraphicPipeline				m_pipeline;
};

template<class MaterialType>
class MaterialTemplate : public MaterialTemplateBase
{
public:
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
private:
	MaterialTemplate(const std::string& vertexShader, const std::string& fragmentShader, const std::string& name)
		: MaterialTemplateBase(vertexShader, fragmentShader, name)
	{}

	virtual ~MaterialTemplate(){}

	friend class MaterialLibrary;
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

protected:
	//Method that search the text in the texture slot and if that texture exists it writes the index at the address parameter. Usually after the writing, data from that address should be uploaded to GPU (uniform buffer)
	void WriteTextureIndex(const CTexture* text, uint32_t* addressToWrite);
	uint32_t AddTextureSlot(CTexture* texture);
protected:
	MaterialTemplateBase*			m_template;
	std::vector<IndexedTexture>		m_textureSlots;
};

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

		auto prop = serializer->GetNewAttribute(m_label.c_str(), serializer->GetNewString(tmplMaterial->GetName()));
		objNode->append_attribute(prop);

		tmplMaterial->Save(cobj->*m_ptm, serializer);
	};

	virtual void Load(rapidxml::xml_node<char>* objNode, Serializer* serializer, ISeriable* obj)
	{
		auto prop = objNode->first_attribute(m_label.c_str(), 0, false);
		BASE* cobj = dynamic_cast<BASE*>(obj);
		TRAP(cobj);

		cobj->*m_ptm = nullptr;
		if (prop)
		{
			MaterialTemplateBase* tmplMaterial = MaterialLibrary::GetInstance()->GetMaterialByName(prop->value());
			cobj->*m_ptm = tmplMaterial->Create(serializer);
		}
	};

private:
	PtmType						m_ptm;
	std::string					m_label;
};