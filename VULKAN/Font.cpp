#include "Font.h"

#define FONT_DIR "fonts\\"

//////////////////////////////////////////////////////////////////////////
//CFontImporter
//////////////////////////////////////////////////////////////////////////

CFontImporter::CFontImporter(std::string fntXml)
	: m_fntXml(fntXml)
	, m_importedFont(nullptr)
	, m_xmlContent(nullptr)
	, m_fontTexture(nullptr)
{
}

CFontImporter::~CFontImporter()
{
	delete[] m_xmlContent;
}

CFont2* CFontImporter::GetFont()
{
	TRAP(m_importedFont);
	return m_importedFont;
}

void CFontImporter::BuildFont()
{
	m_importedFont = new CFont2();

	std::string finalFntXml = FONT_DIR + m_fntXml;
	ReadXmlFile(finalFntXml, &m_xmlContent);
	TXmlDocument docoment;
	docoment.parse<rapidxml::parse_default>(m_xmlContent);
	GatherPointsOfInterests(&docoment);
	GatherMetadata();
	LoadFontTexture();
	GetGlyphs(); //font should have the metadata set
}


void CFontImporter::GatherMetadata()
{
	TXmlNode* infoNode = m_xmlPointOfInterests.infoNode;
	TXmlNode* commonNode = m_xmlPointOfInterests.commonNode;
	TXmlNode* charsNode = m_xmlPointOfInterests.charsNode;
	TRAP(infoNode != nullptr && commonNode != nullptr && charsNode != nullptr);

	TXmlAttribute* fontName = infoNode->first_attribute("face", 0, false);
	TRAP(fontName);
	m_fontMetadata.Name = fontName->value();

	TXmlAttribute* textWidth = commonNode->first_attribute("scaleW", 0, false);
	TXmlAttribute* textHeight = commonNode->first_attribute("scaleH", 0, false);
	TRAP(textHeight && textWidth);

	m_fontMetadata.TextWidth = (float)std::atoi(textWidth->value());
	m_fontMetadata.TextHeight = (float)std::atoi(textHeight->value());
	m_fontMetadata.TexelSize = glm::vec2(1.0f, 1.0f) / glm::vec2(m_fontMetadata.TextWidth, m_fontMetadata.TextHeight);

	TXmlAttribute* glyphWH = commonNode->first_attribute("lineHeight", 0, false); //WH = widthHeight
	TRAP(glyphWH);
	m_fontMetadata.GlyphSize = (float)std::atoi(glyphWH->value());

	TXmlAttribute* charCount = charsNode->first_attribute("count", 0, false);
	TRAP(charCount);
	m_fontMetadata.CharsCount = std::atoi(charCount->value());

	//check metadata integrity
	TRAP(!m_fontMetadata.Name.empty());
	TRAP(m_fontMetadata.TextHeight > 0 && m_fontMetadata.TextWidth > 0);
	TRAP(m_fontMetadata.CharsCount > 0);

	m_importedFont->SetMetadata(m_fontMetadata);
}

void CFontImporter::GatherPointsOfInterests(TXmlDocument* doc)
{
	TXmlNode* fontNode = doc->first_node("font", 0, false);
	TRAP(fontNode);

	m_xmlPointOfInterests.infoNode = fontNode->first_node("info", 0, false);
	m_xmlPointOfInterests.commonNode = fontNode->first_node("common", 0, false);
	m_xmlPointOfInterests.pagesNode = fontNode->first_node("pages", 0, false);
	m_xmlPointOfInterests.charsNode = fontNode->first_node("chars", 0, false);
}

void CFontImporter::LoadFontTexture()
{
	TXmlNode* pagesNode = m_xmlPointOfInterests.pagesNode;
	TRAP(pagesNode);
	//get just first page. export font only in one texture
	TXmlNode* pageNode = pagesNode->first_node();
	TRAP(pageNode);
	TXmlAttribute* file = pageNode->first_attribute("file", 0, false);
	TRAP(file);
	std::string fileName = file->value();
	fileName = FONT_DIR + fileName;
	TRAP(!fileName.empty());
	SImageData imgData;
	Read2DTextureData(imgData, fileName, false);
	m_fontTexture = new CTexture(imgData, true);
	m_importedFont->SetFontTexture(m_fontTexture);
}

void CFontImporter::GetGlyphs()
{
	TXmlNode* charsNode = m_xmlPointOfInterests.charsNode;
	TRAP(charsNode);
	TRAP(m_importedFont);
	for (TXmlNode* curNode = charsNode->first_node(); curNode; curNode = curNode->next_sibling())
		ParseGlyphs(curNode);

	m_importedFont->Validate();
}

void CFontImporter::ParseGlyphs(TXmlNode* charNode)
{
	unsigned char uID;
	unsigned int uX, uY;
	unsigned int uWidth, uHeight;

	TXmlAttribute* id = charNode->first_attribute("id", 0, false);
	TRAP(id);
	uID = std::atoi(id->value());

	TXmlAttribute* x = charNode->first_attribute("x");
	TRAP(x);
	uX = std::atoi(x->value());

	TXmlAttribute* y = charNode->first_attribute("y");
	TRAP(y);
	uY = std::atoi(y->value());

	TXmlAttribute* width = charNode->first_attribute("width");
	TRAP(width);
	uWidth = std::atoi(width->value());

	TXmlAttribute* height = charNode->first_attribute("height");
	TRAP(height);
	uHeight = std::atoi(height->value());

	m_importedFont->AddGlyph(uID, glm::vec2(uX, uY), glm::vec2(uWidth, uHeight));
}

//////////////////////////////////////////////////////////////////////////
//CFont2
//////////////////////////////////////////////////////////////////////////
CFont2::CFont2()
	: m_fontTexture(nullptr)
{
}

CFont2::~CFont2()
{
	delete m_fontTexture;
}

void CFont2::GetUIString(const std::string& text, UIString& uiString)
{
	uiString.reserve(text.size());
	float xOffset = 0;

	for (unsigned int i = 0; i < text.size(); ++i)
	{
		uiString.push_back(GetGlyph(text[i]));
	}
}

void CFont2::AddGlyph(unsigned char id, glm::vec2 pos, glm::vec2 size)
{
	TRAP(m_metaData.TexelSize.x > glm::epsilon<float>() && m_metaData.TexelSize.y > glm::epsilon<float>())
		auto it = m_glyphs.find(id);
	if (it != m_glyphs.end())
		return;

	glm::vec2 texelSize = m_metaData.TexelSize;
	//pos = glm::vec2(pos.x, 1.f - pos.y);

	SGlyph glyph;
	glyph.Id = id;
	glyph.Size = size;
	glyph.SizeUV = size * m_metaData.TexelSize;
	glyph.TopLeftUV = pos * glm::vec2(texelSize.x, -texelSize.y) + glm::vec2(0.f, 1.f);
	/*glyph.TopRightUV = (pos + glm::vec2(size.x, 0.f)) * glm::vec2(texelSize.x, -texelSize.y) + glm::vec2(0.f, 1.f);
	glyph.BotRightUV = (pos + size) * glm::vec2(texelSize.x, -texelSize.y) + glm::vec2(0.f, 1.f);
	glyph.BotLeftUV = (pos + glm::vec2(0.f, size.y)) * glm::vec2(texelSize.x, -texelSize.y) + glm::vec2(0.f, 1.f);*/

	m_glyphs.insert(std::pair<unsigned char, SGlyph>(id, glyph));
}

SGlyph& CFont2::GetGlyph(unsigned char c)
{
	auto it = m_glyphs.find(c);
	TRAP(it != m_glyphs.end());
	return it->second;
}

void CFont2::Validate()
{
	TRAP(m_glyphs.size() == m_metaData.CharsCount);
}
