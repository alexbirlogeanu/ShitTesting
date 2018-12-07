#include "Serializer.h"

#include <fstream>
#include <iostream>
#include "rapidxml/rapidxml_print.hpp"
#include "Utils.h"

Serializer::Serializer()
	: m_isSaving(true)
	, m_HasReachedEoF(false)
	, m_currentNode(nullptr)
{
}

Serializer::~Serializer()
{
}


void Serializer::SerializeProperty(PropertyGeneric* prop, ISeriable* obj)
{
	if (m_isSaving)
		prop->Save(m_currentNode, this, obj);
	else
		prop->Load(m_currentNode, this, obj);

}

bool Serializer::BeginSerializing(ISeriable* obj)
{
	if (m_isSaving)
	{
		auto newNode = GetNewNode(obj->GetName().c_str());
		m_nodeStack.push_back(newNode);
		m_currentNode = newNode;
	}
	else //loading step
	{
		if (!m_currentNode)
		{
			auto root = m_document.first_node();
			m_currentNode = root->first_node(obj->GetName().c_str());
		}
		else
		{
			if (m_nodeStack.empty())
				m_currentNode = m_currentNode->next_sibling(obj->GetName().c_str());
			else
				m_currentNode = m_currentNode->first_node(obj->GetName().c_str());
		}

		if (!m_currentNode)
		{
			return false;
		}

		m_nodeStack.push_back(m_currentNode);
	}
	return true;
}

void Serializer::EndSerializing(ISeriable* obj)
{
	TRAP(!m_nodeStack.empty());
	auto last = m_nodeStack.back();
	TRAP(last == m_currentNode); //sanity check
	m_nodeStack.pop_back();
	auto newCurrentNode = m_currentNode;

	if (m_isSaving)
	{
		obj->OnSave();
		auto parrentNode = (m_nodeStack.empty()) ? m_document.first_node() : m_nodeStack.back();
		parrentNode->append_node(m_currentNode);
		newCurrentNode = (m_nodeStack.empty()) ? nullptr : m_nodeStack.back();
	}
	else
	{
		obj->OnLoad();
		if (m_nodeStack.empty())
		{
			m_HasReachedEoF = m_currentNode->next_sibling() == nullptr;
			newCurrentNode = m_currentNode;
		}
		else
		{
			newCurrentNode = m_nodeStack.back();
		}
	}
	m_currentNode = newCurrentNode;

}

void Serializer::Save(const std::string& filename)
{
	m_isSaving = true;
	auto root = GetNewNode(GetNewString("root"));
	m_document.append_node(root);

	SaveContent();

	std::fstream f(filename, std::ios_base::out);
	f << *m_document.first_node() << std::endl;
	f.close();
	m_document.clear();
}

void Serializer::Load(const std::string& filename)
{
	m_isSaving = false;
	char* xmlContent;
	ReadXmlFile(filename, &xmlContent);

	m_document.parse<rapidxml::parse_default>(xmlContent);

	LoadContent();

	delete xmlContent;
	m_document.clear();
}


rapidxml::xml_attribute<char>*	Serializer::GetNewAttribute(const char* name, const char* val)
{
	return m_document.allocate_attribute(name, val);
}

rapidxml::xml_node<char>* Serializer::GetNewNode(const char* name)
{
	return m_document.allocate_node(rapidxml::node_type::node_element, name);
}

char* Serializer::GetNewString(const std::string& str)
{
	return m_document.allocate_string(str.c_str());
}
