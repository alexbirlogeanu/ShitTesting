#pragma once

#include "VulkanLoader.h"
#include "rapidxml/rapidxml.hpp"
#include "defines.h"
#include "glm/glm.hpp"
#include "SVertex.h"
#include "Texture.h"

#include <string>
#include <vector>

typedef rapidxml::xml_document<char>  TXmlDocument;
typedef rapidxml::xml_node<char>      TXmlNode;
typedef rapidxml::xml_attribute<char> TXmlAttribute;


struct SFontMetadata
{
	std::string     Name;
	float           TextWidth;
	float           TextHeight;
	float           GlyphSize;
	unsigned int    CharsCount;
	glm::vec2       TexelSize;
};

class CFont2;
class CFontImporter
{
public:
	CFontImporter(std::string fntXml);
	virtual ~CFontImporter();

	CFont2* GetFont();
	void BuildFont();

private:
	void GatherPointsOfInterests(TXmlDocument* doc);
	void GatherMetadata();
	void LoadFontTexture();
	void GetGlyphs();
	void ParseGlyphs(TXmlNode* charNode);
private:
	struct xmlPOI
	{
		xmlPOI()
		{
			charsNode = commonNode = infoNode = pagesNode = nullptr;
		}

		TXmlNode* charsNode;
		TXmlNode* infoNode;
		TXmlNode* commonNode;
		TXmlNode* pagesNode;
	};

	xmlPOI          m_xmlPointOfInterests;

	CFont2*          m_importedFont;
	std::string     m_fntXml;
	char*           m_xmlContent;
	SFontMetadata   m_fontMetadata;
	CTexture*       m_fontTexture;

};

struct SGlyph
{
	unsigned char   Id;
	glm::vec2       Size; //in pixels
	glm::vec2		SizeUV;
	glm::vec2       TopLeftUV;
};

class CFont2
{
public:
	typedef std::vector<SGlyph> UIString;
	virtual ~CFont2();

	float GetGlyphPixelsSize() const { return m_metaData.GlyphSize; }
	void GetUIString(const std::string& text, UIString& uiString);

	VkImageView  GetFontImageView() const { return m_fontTexture->GetImageView(); }
private:
	CFont2();

	void SetMetadata(const SFontMetadata& metadata) { m_metaData = metadata; };
	void SetFontTexture(CTexture* text) { TRAP(text); m_fontTexture = text; }
	void AddGlyph(unsigned char id, glm::vec2 pos, glm::vec2 size);
	SGlyph& GetGlyph(unsigned char c);

	void Validate();

	friend class CFontImporter;
private:
	SFontMetadata                               m_metaData;
	CTexture*                                   m_fontTexture;

	std::unordered_map<unsigned char, SGlyph>   m_glyphs;
};