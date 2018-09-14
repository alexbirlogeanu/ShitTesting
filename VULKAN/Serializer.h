#pragma once

#include "rapidxml/rapidxml.hpp"
#include "glm/glm.hpp"
#include "defines.h"

#include <string>
#include <vector>

class Serializer;
class ISeriable;

class PropertyGeneric
{
public:
	virtual void Save(rapidxml::xml_node<char>* objNode, Serializer* serializer, ISeriable* obj) = 0;
	virtual void Load(rapidxml::xml_node<char>* objNode, Serializer* serializer, ISeriable* obj) = 0;
};

class ISeriable
{
public:
	ISeriable(const std::string& nodeName)
		: m_name(nodeName)
	{}

	virtual bool Serialize(Serializer* serializer) = 0;

	const std::string& GetName() const { return m_name; }
	void SetName(const std::string& name) { m_name = name; }

	virtual void OnSave() {}
	virtual void OnLoad() {}
private:
	std::string m_name;
};


class Serializer
{
public:
	Serializer();
	virtual ~Serializer();

	//Serialize methods
	void SerializeProperty(PropertyGeneric* prop, ISeriable* obj);
	bool BeginSerializing(ISeriable* obj);
	void EndSerializing(ISeriable* obj);

	//Alloc strings/nodes methods
	rapidxml::xml_attribute<char>*	GetNewAttribute(const char* name = "", const char* val = "");
	rapidxml::xml_node<char>* GetNewNode(const char* name = "");
	char* GetNewString(const std::string& str);

	bool HasReachedEof() const { return m_HasReachedEoF; }

	virtual void Save(const std::string& filename) = 0;
	virtual void Load(const std::string& filename) = 0;
protected:
	rapidxml::xml_node<char>*				m_currentNode;
	std::vector<rapidxml::xml_node<char>*>	m_nodeStack;

	rapidxml::xml_document<char>			m_document;
	bool									m_isSaving;
	bool									m_HasReachedEoF;
};

template<class T>
class SeriableImpl : public ISeriable
{
public:
	SeriableImpl(const std::string& nodeName)
		: ISeriable(nodeName)
	{}

	virtual bool Serialize(Serializer* serializer) override
	{
		if (serializer->BeginSerializing(this))
		{
			for (auto prop : T::PropertiesMap)
				serializer->SerializeProperty(prop, this);

			serializer->EndSerializing(this);
			return true;
		}
		return false;
	}


public:
	static std::vector<PropertyGeneric*> PropertiesMap;
};


#define BEGIN_PROPERTY_MAP(CLASSTYPE) std::vector<PropertyGeneric*> SeriableImpl<CLASSTYPE>::PropertiesMap = {

#define END_PROPERTY_MAP(CLASSTYPE) };

#define IMPLEMENT_PROPERTY(PTYPE, PNAME, PLABEL, CLASSTYPE) new Property<PTYPE, CLASSTYPE>(CLASSTYPE::GetMember##PNAME(), PLABEL)

#define DECLARE_PROPERTY(PTYPE, PNAME, CLASSTYPE)  protected: PTYPE m_##PNAME; \
													public: static PTYPE CLASSTYPE::* GetMember##PNAME() { return &CLASSTYPE::m_##PNAME; } \
													const PTYPE Get##PNAME() const { return m_##PNAME; } \
													PTYPE Get##PNAME() { return m_##PNAME; } \
													void Set##PNAME(PTYPE val) { m_##PNAME = val; }

////////////////////////////////Specialization properties////////////////////////////////////////////////
template<typename T, typename BASE> //default int
class Property : public PropertyGeneric
{
public:
	typedef T BASE::* PtmType;
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

		auto prop = serializer->GetNewAttribute(m_label.c_str(), serializer->GetNewString(std::to_string(cobj->*m_ptm)));
		objNode->append_attribute(prop);
	};

	virtual void Load(rapidxml::xml_node<char>* objNode, Serializer* serializer, ISeriable* obj)
	{
		auto prop = objNode->first_attribute(m_label.c_str(), 0, false);
		BASE* cobj = dynamic_cast<BASE*>(obj);
		TRAP(cobj);

		cobj->*m_ptm = std::stoi(prop->value());

	};
private:
	PtmType						m_ptm;
	std::string					m_label;
};

template<typename BASE>
class Property<bool, BASE> : public PropertyGeneric
{
public:
	typedef bool BASE::* PtmType;
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

		std::string stBool = (cobj->*m_ptm) ? "true" : "false";
		auto prop = serializer->GetNewAttribute(m_label.c_str(), serializer->GetNewString(stBool));
		objNode->append_attribute(prop);
	};

	virtual void Load(rapidxml::xml_node<char>* objNode, Serializer* serializer, ISeriable* obj)
	{
		auto prop = objNode->first_attribute(m_label.c_str(), 0, false);
		BASE* cobj = dynamic_cast<BASE*>(obj);
		TRAP(cobj);

		cobj->*m_ptm = std::string(prop->value()) == std::string("true");

	};
private:
	PtmType						m_ptm;
	std::string					m_label;
};

template<typename BASE>
class Property<float, BASE> : public PropertyGeneric
{
public:
	typedef float BASE::* PtmType;
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

		auto prop = serializer->GetNewAttribute(m_label.c_str(), serializer->GetNewString(std::to_string(cobj->*m_ptm)));
		objNode->append_attribute(prop);
	};

	virtual void Load(rapidxml::xml_node<char>* objNode, Serializer* serializer, ISeriable* obj)
	{
		auto prop = objNode->first_attribute(m_label.c_str(), 0, false);
		BASE* cobj = dynamic_cast<BASE*>(obj);
		TRAP(cobj);
		cobj->*m_ptm = std::stof(prop->value());

	};
private:
	PtmType						m_ptm;
	std::string					m_label;
};

template<typename BASE>
class Property<glm::vec3, BASE> : public PropertyGeneric
{
public:
	typedef glm::vec3 BASE::* PtmType;
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

		auto node = serializer->GetNewNode(m_label.c_str());

		node->append_attribute(serializer->GetNewAttribute("x", serializer->GetNewString(std::to_string((cobj->*m_ptm)[0]))));
		node->append_attribute(serializer->GetNewAttribute("y", serializer->GetNewString(std::to_string((cobj->*m_ptm)[1]))));
		node->append_attribute(serializer->GetNewAttribute("z", serializer->GetNewString(std::to_string((cobj->*m_ptm)[2]))));

		objNode->append_node(node);
	};

	virtual void Load(rapidxml::xml_node<char>* objNode, Serializer* serializer, ISeriable* obj)
	{
		auto node = objNode->first_node(m_label.c_str(), 0, false);
		TRAP(node);
		BASE* cobj = dynamic_cast<BASE*>(obj);
		TRAP(cobj);

		(cobj->*m_ptm)[0] = std::stof(node->first_attribute("x")->value()); //need some checks for null
		(cobj->*m_ptm)[1] = std::stof(node->first_attribute("y")->value());
		(cobj->*m_ptm)[2] = std::stof(node->first_attribute("z")->value());
	};
private:
	PtmType						m_ptm;
	std::string					m_label;
};

template<typename BASE>
class Property<std::string, BASE> : public PropertyGeneric
{
public:
	typedef std::string BASE::* PtmType;
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

		auto prop = serializer->GetNewAttribute(m_label.c_str(), serializer->GetNewString(cobj->*m_ptm));
		objNode->append_attribute(prop);
	};

	virtual void Load(rapidxml::xml_node<char>* objNode, Serializer* serializer, ISeriable* obj)
	{
		auto prop = objNode->first_attribute(m_label.c_str(), 0, false);
		BASE* cobj = dynamic_cast<BASE*>(obj);
		TRAP(cobj);
		cobj->*m_ptm = std::string(prop->value());
	};
private:
	PtmType						m_ptm;
	std::string					m_label;
};